/**************************************************************************
 *   This file is part of PRAXIS.                                         *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   PRAXIS is free software:                                             *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   PRAXIS is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "orbital_gallery_widget.h"

#include <algorithm>
#include <cmath>

#include <QMouseEvent>
#include <QOpenGLFramebufferObject>
#include <QOpenGLPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

#include "shader_program_manager.h"
#include "structure_renderer.h"

OrbitalGalleryWidget::OrbitalGalleryWidget(QWidget* parent) :
    QOpenGLWidget(parent) {
    QSurfaceFormat fmt = QSurfaceFormat::defaultFormat();
    fmt.setSamples(4);
    this->setFormat(fmt);

    this->shader_manager = std::make_shared<ShaderProgramManager>();
    this->scene = std::make_shared<Scene>();
    this->scene->rotation_matrix = Scene::alignment_rotation(CameraAlignment::DEFAULT);
    this->scene->canvas_width = this->tile_size;
    this->scene->canvas_height = this->tile_size;
    this->setMinimumSize(200, 200);
}

OrbitalGalleryWidget::~OrbitalGalleryWidget() {
    this->cleanup();
    if(this->context() != nullptr) {
        disconnect(this->context(), &QOpenGLContext::aboutToBeDestroyed, this, &OrbitalGalleryWidget::cleanup);
    }
}

void OrbitalGalleryWidget::cleanup() {
    // models release their buffers when destroyed; this requires the context
    if(this->context() == nullptr) {
        this->tiles.clear();
        this->placeholder.reset();
        return;
    }
    this->makeCurrent();
    for(auto& t : this->tiles) {
        t.frame.reset();
    }
    this->placeholder.reset();
    this->renderer.reset();
    this->shader_manager->clear();
    this->doneCurrent();
}

QSize OrbitalGalleryWidget::sizeHint() const {
    return QSize(1000, 700);
}

// ---------------------------------------------------------------------------
// content
// ---------------------------------------------------------------------------
void OrbitalGalleryWidget::set_tiles(std::vector<Tile> _tiles, std::shared_ptr<Frame> _placeholder) {
    if(this->context() != nullptr) {
        this->makeCurrent();
    }
    this->tiles = std::move(_tiles);
    this->placeholder = std::move(_placeholder);
    if(this->context() != nullptr) {
        this->doneCurrent();
    }
    this->selected = -1;
    this->scroll = std::clamp(this->scroll, 0, std::max(0, this->content_height() - this->height()));
    emit layout_changed();
    this->update();
}

void OrbitalGalleryWidget::set_tile(int index, std::shared_ptr<Frame> frame, const QString& subtitle) {
    if(index < 0 || index >= (int)this->tiles.size()) {
        return;
    }
    if(this->context() != nullptr) {
        this->makeCurrent();
    }
    this->tiles[index].frame = std::move(frame);
    if(this->context() != nullptr) {
        this->doneCurrent();
    }
    if(!subtitle.isEmpty()) {
        this->tiles[index].subtitle = subtitle;
    }
    this->update();
}

void OrbitalGalleryWidget::clear() {
    this->set_tiles({}, nullptr);
}

void OrbitalGalleryWidget::set_tile_size(int size) {
    this->tile_size = std::max(80, size);
    this->scroll = std::clamp(this->scroll, 0, std::max(0, this->content_height() - this->height()));
    emit layout_changed();
    this->update();
}

void OrbitalGalleryWidget::set_scroll(int _scroll) {
    this->scroll = std::max(0, _scroll);
    this->update();
}

void OrbitalGalleryWidget::set_rotation(const QMatrix4x4& rotation) {
    this->scene->rotation_matrix = rotation;
    this->scene->arcball_rotation.setToIdentity();
    this->update();
}

QMatrix4x4 OrbitalGalleryWidget::get_rotation() const {
    return this->scene->arcball_rotation * this->scene->rotation_matrix;
}

void OrbitalGalleryWidget::set_camera_mode(CameraMode mode) {
    this->scene->camera_mode = mode;
    this->update();
}

void OrbitalGalleryWidget::set_camera_distance(float distance) {
    this->camera_distance = std::clamp(distance, 3.0f, 500.0f);
    this->update();
}

// ---------------------------------------------------------------------------
// layout
// ---------------------------------------------------------------------------
int OrbitalGalleryWidget::columns() const {
    return std::max(1, (this->width() - gap) / (this->tile_size + gap));
}

int OrbitalGalleryWidget::cell_size() const {
    // stretch the tiles such that the rows fill the width
    const int ncol = this->columns();
    return std::max(40, (this->width() - gap) / ncol - gap);
}

int OrbitalGalleryWidget::content_height() const {
    const int ncol = this->columns();
    const int nrows = ((int)this->tiles.size() + ncol - 1) / ncol;
    return gap + nrows * (this->cell_size() + gap);
}

QRect OrbitalGalleryWidget::tile_rect(int index) const {
    const int ncol = this->columns();
    const int sz = this->cell_size();
    const int row = index / ncol;
    const int col = index % ncol;
    return QRect(gap + col * (sz + gap), gap + row * (sz + gap) - this->scroll, sz, sz);
}

int OrbitalGalleryWidget::tile_at(const QPoint& pos) const {
    for(int i = 0; i < (int)this->tiles.size(); ++i) {
        if(this->tile_rect(i).contains(pos)) {
            return i;
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// rendering
// ---------------------------------------------------------------------------
void OrbitalGalleryWidget::initializeGL() {
    connect(this->context(), &QOpenGLContext::aboutToBeDestroyed, this, &OrbitalGalleryWidget::cleanup);
    this->initializeOpenGLFunctions();

    this->shader_manager->create_shader_program("atombond_shader", ShaderProgramType::ModelShader, ":/assets/shaders/phong.vs", ":/assets/shaders/phong.fs");
    this->shader_manager->create_shader_program("object_shader", ShaderProgramType::ModelShader, ":/assets/shaders/phong.vs", ":/assets/shaders/phong.fs");
    this->renderer = std::make_unique<StructureRenderer>(this->scene, this->shader_manager);
}

void OrbitalGalleryWidget::resizeGL(int, int) {
    this->scroll = std::clamp(this->scroll, 0, std::max(0, this->content_height() - this->height()));
    emit layout_changed();
}

void OrbitalGalleryWidget::setup_camera() {
    // every tile is square; all tiles share the same camera
    this->scene->camera_position = QVector3D(0.0f, -this->camera_distance, 0.0f);
    this->scene->projection.setToIdentity();
    if(this->scene->camera_mode == CameraMode::ORTHOGRAPHIC) {
        // same scale as the perspective view at the center of the molecule
        const float half = this->camera_distance * std::tan(qDegreesToRadians(22.5f));
        this->scene->projection.ortho(-half, half, -half, half, 0.01f, 1000.0f);
    } else {
        this->scene->projection.perspective(45.0f, 1.0f, 0.01f, 1000.0f);
    }
    this->scene->view.setToIdentity();
    this->scene->view.lookAt(this->scene->camera_position, QVector3D(0.0f, 1.0f, 0.0f), QVector3D(0.0f, 0.0f, 1.0f));
}

void OrbitalGalleryWidget::draw_frame(const Frame* frame) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);
    if(frame != nullptr) {
        this->renderer->draw(frame);
    }
}

void OrbitalGalleryWidget::make_opaque() {
    // translucent surfaces lower the alpha of the framebuffer; the widget
    // (and exported images) must be opaque
    glDisable(GL_SCISSOR_TEST);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
}

void OrbitalGalleryWidget::paintGL() {
    const qreal dpr = this->devicePixelRatioF();
    const QColor window = this->palette().color(QPalette::Window);

    // QPainter leaves the depth mask disabled
    glDepthMask(GL_TRUE);
    glDisable(GL_SCISSOR_TEST);
    glClearColor(window.redF(), window.greenF(), window.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if(!this->renderer) {
        return;
    }

    this->setup_camera();
    const int widget_h = this->height();

    std::vector<int> visible;
    for(int i = 0; i < (int)this->tiles.size(); ++i) {
        const QRect r = this->tile_rect(i);
        if(r.bottom() < 0 || r.top() > widget_h) {
            continue;
        }
        visible.push_back(i);

        const GLint x = (GLint)std::lround(r.x() * dpr);
        const GLint y = (GLint)std::lround((widget_h - r.y() - r.height()) * dpr);
        const GLsizei s = (GLsizei)std::lround(r.width() * dpr);
        glViewport(x, y, s, s);
        glEnable(GL_SCISSOR_TEST);
        glScissor(x, y, s, s);
        glDepthMask(GL_TRUE);
        glClearColor(tint, tint, tint, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        const Tile& t = this->tiles[i];
        this->draw_frame(t.frame ? t.frame.get() : this->placeholder.get());
    }

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glViewport(0, 0, (GLsizei)std::lround(this->width() * dpr), (GLsizei)std::lround(widget_h * dpr));
    this->make_opaque();

    // labels; painting on a paint device of the framebuffer rather than on
    // the widget keeps them in grabbed images (QWidget::grab() redirects
    // painters on the widget itself)
    QOpenGLPaintDevice device(QSize((int)std::lround(this->width() * dpr), (int)std::lround(widget_h * dpr)));
    device.setDevicePixelRatio(dpr);
    QPainter painter(&device);
    painter.setRenderHint(QPainter::Antialiasing);
    const QPalette pal = this->palette();
    QFont bold = this->font();
    bold.setBold(true);
    const QFontMetrics fm(this->font());
    for(int i : visible) {
        const QRect r = this->tile_rect(i);
        const Tile& t = this->tiles[i];

        const QString subtitle = t.frame ? t.subtitle : QString("Evaluating...");
        const int wtext = std::max(QFontMetrics(bold).horizontalAdvance(t.title), fm.horizontalAdvance(subtitle));
        const QRectF box(r.left() + 4, r.top() + 4, std::min(r.width() - 8, wtext + 12), 2 * fm.height() + 8);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 190));
        painter.drawRoundedRect(box, 4, 4);

        painter.setPen(QColor(30, 30, 30));
        painter.setFont(bold);
        painter.drawText(box.adjusted(6, 4, -6, 0), Qt::AlignLeft | Qt::AlignTop, t.title);
        painter.setFont(this->font());
        painter.setPen(QColor(70, 70, 70));
        painter.drawText(box.adjusted(6, 4 + fm.height(), -6, 0), Qt::AlignLeft | Qt::AlignTop, subtitle);

        if(i == this->selected) {
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(pal.color(QPalette::Highlight), 3));
            painter.drawRect(QRectF(r).adjusted(1.5, 1.5, -1.5, -1.5));
        }
    }

    if(this->tiles.empty()) {
        painter.setPen(pal.color(QPalette::WindowText));
        painter.drawText(this->rect(), Qt::AlignCenter, "No orbitals to show");
    }
}

QImage OrbitalGalleryWidget::render_tile(int index, int pixels, const QColor& background) {
    if(!this->renderer || index < 0 || index >= (int)this->tiles.size()) {
        return QImage();
    }

    this->makeCurrent();

    QOpenGLFramebufferObjectFormat fmt;
    fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
    fmt.setSamples(8);
    QOpenGLFramebufferObject fbo(pixels, pixels, fmt);
    fbo.bind();

    glViewport(0, 0, pixels, pixels);
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glClearColor(background.redF(), background.greenF(), background.blueF(), 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    this->setup_camera();
    const Tile& t = this->tiles[index];
    this->draw_frame(t.frame ? t.frame.get() : this->placeholder.get());
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    this->make_opaque();

    fbo.release();
    const QImage image = fbo.toImage().convertToFormat(QImage::Format_RGB32);
    this->doneCurrent();
    return image;
}

// ---------------------------------------------------------------------------
// interaction
// ---------------------------------------------------------------------------
QVector3D OrbitalGalleryWidget::arcball_vector(const QPoint& pos) const {
    // arcball relative to the tile in which the drag started
    const QRect& r = this->drag_rect;
    QVector3D p(2.0f * (pos.x() - r.left()) / (float)r.width() - 1.0f,
                1.0f - 2.0f * (pos.y() - r.top()) / (float)r.height(),
                0.0f);
    const float op2 = p.x() * p.x() + p.y() * p.y();
    if(op2 <= 1.0f) {
        p.setZ(std::sqrt(1.0f - op2));
    } else {
        p.normalize();
    }
    return p;
}

void OrbitalGalleryWidget::mousePressEvent(QMouseEvent* event) {
    if(event->button() != Qt::LeftButton) {
        return;
    }
    const int idx = this->tile_at(event->pos());
    this->press_pos = event->pos();
    this->last_pos = event->pos();
    this->rotating = idx >= 0;
    if(idx >= 0) {
        this->drag_rect = this->tile_rect(idx);
    }
}

void OrbitalGalleryWidget::mouseMoveEvent(QMouseEvent* event) {
    if(!this->rotating || event->pos() == this->last_pos) {
        return;
    }

    const QVector3D va = this->arcball_vector(this->press_pos);
    const QVector3D vb = this->arcball_vector(event->pos());
    this->last_pos = event->pos();
    const float dot = QVector3D::dotProduct(va, vb);
    if(std::abs(dot) > 0.9999f) {
        return;
    }
    const float angle = std::acos(std::min(1.0f, dot));

    // rotation axis from camera to model space
    this->setup_camera();
    const QVector4D axis_cam(QVector3D::crossProduct(va, vb).normalized());
    const QMatrix3x3 cam_to_model = this->scene->view.inverted().toGenericMatrix<3, 3>();
    const QVector4D axis_model = QMatrix4x4(cam_to_model) * axis_cam;

    this->scene->arcball_rotation.setToIdentity();
    this->scene->arcball_rotation.rotate(qRadiansToDegrees(angle * rotation_sensitivity), QVector3D(axis_model));
    this->update();
}

void OrbitalGalleryWidget::mouseReleaseEvent(QMouseEvent* event) {
    if(event->button() != Qt::LeftButton) {
        return;
    }
    if(this->rotating) {
        this->scene->rotation_matrix = this->scene->arcball_rotation * this->scene->rotation_matrix;
        this->scene->arcball_rotation.setToIdentity();
        this->rotating = false;
    }
    if((event->pos() - this->press_pos).manhattanLength() <= 3) {
        this->selected = this->tile_at(event->pos());
    }
    this->update();
}

void OrbitalGalleryWidget::mouseDoubleClickEvent(QMouseEvent* event) {
    const int idx = this->tile_at(event->pos());
    if(idx >= 0) {
        this->selected = idx;
        this->update();
        emit tile_activated(idx);
    }
}

void OrbitalGalleryWidget::wheelEvent(QWheelEvent* event) {
    if(event->modifiers() & Qt::ControlModifier) {
        const float steps = event->angleDelta().y() / 120.0f;
        this->set_camera_distance(this->camera_distance * std::pow(0.9f, steps));
    } else {
        emit scroll_requested(-event->angleDelta().y());
    }
    event->accept();
}
