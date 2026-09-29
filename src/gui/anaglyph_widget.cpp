/**************************************************************************
 *   This file is part of MANAGLYPH.                                      *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   MANAGLYPH is free software:                                          *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   MANAGLYPH is distributed in the hope that it will be useful,         *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "anaglyph_widget.h"

#include <QSettings>
#include <QOpenGLExtraFunctions>

namespace {
int normalize_msaa_samples(int samples) {
    switch(samples) {
    case 1:
    case 2:
    case 4:
    case 8:
        return samples;
    default:
        return 4;
    }
}

int normalize_sphere_tesselation_level(int level) {
    return std::clamp(level, 0, 6);
}
}

/**
 * @brief      Constructs a new instance.
 *
 * @param      parent  The parent
 */
AnaglyphWidget::AnaglyphWidget(QWidget *parent)
    : QOpenGLWidget(parent) {
    const QString operatingsystem = QSysInfo::productType();

    this->shader_manager = std::make_shared<ShaderProgramManager>();

    this->scene = std::make_shared<Scene>();
    this->scene->camera_position = QVector3D(0.0, -10.0f, 0.0);

    auto pTimer = new QTimer(this);
    pTimer->start(1000 / 60.0);

    // set default matrix orientation on start-up
    this->reset_matrices();

    // enable mouse tracking
    this->setMouseTracking(true);

    this->load_lighting_settings();
}

/**
 * @brief Rotate the scene
 * @param angle
 */
void AnaglyphWidget::rotate_scene(float angle) {
    this->scene->rotate_z(angle);
    this->update();
}

void AnaglyphWidget::reset_panning() {
    this->pan_offset = QVector3D(0.0f, 0.0f, 0.0f);
    this->update();
}

/**
 * @brief Set lighting settings for atoms and bonds.
 *
 * @param ambient_strength
 * @param specular_strength
 * @param shininess
 */
void AnaglyphWidget::set_atom_lighting_settings(float ambient,
                                                float diffuse,
                                                float specular,
                                                float shininess,
                                                float edge_strength,
                                                float edge_power) {
    this->scene->atom_lighting.ambient_strength = ambient;
    this->scene->atom_lighting.diffuse_strength = diffuse;
    this->scene->atom_lighting.specular_strength = specular;
    this->scene->atom_lighting.shininess = shininess;
    this->scene->atom_lighting.edge_strength = edge_strength;
    this->scene->atom_lighting.edge_power = edge_power;
    QSettings settings;
    settings.setValue("lighting/atoms/ambient_strength", ambient);
    settings.setValue("lighting/atoms/diffuse_strength", diffuse);
    settings.setValue("lighting/atoms/specular_strength", specular);
    settings.setValue("lighting/atoms/shininess", shininess);
    settings.setValue("lighting/atoms/edge_strength", edge_strength);
    settings.setValue("lighting/atoms/edge_power", edge_power);
    this->update();
}

/**
 * @brief Set lighting settings for objects.
 *
 * @param ambient_strength
 * @param specular_strength
 * @param shininess
 */
void AnaglyphWidget::set_object_lighting_settings(float ambient,
                                                  float diffuse,
                                                  float specular,
                                                  float shininess,
                                                  float edge_strength,
                                                  float edge_power) {
    this->scene->object_lighting.ambient_strength = ambient;
    this->scene->object_lighting.diffuse_strength = diffuse;
    this->scene->object_lighting.specular_strength = specular;
    this->scene->object_lighting.shininess = shininess;
    this->scene->object_lighting.edge_strength = edge_strength;
    this->scene->object_lighting.edge_power = edge_power;
    QSettings settings;
    settings.setValue("lighting/objects/ambient_strength", ambient);
    settings.setValue("lighting/objects/diffuse_strength", diffuse);
    settings.setValue("lighting/objects/specular_strength", specular);
    settings.setValue("lighting/objects/shininess", shininess);
    settings.setValue("lighting/objects/edge_strength", edge_strength);
    settings.setValue("lighting/objects/edge_power", edge_power);
    this->update();
}

/**
 * @brief Get lighting settings for atoms and bonds.
 *
 * @return lighting settings
 */
LightingSettings AnaglyphWidget::get_atom_lighting_settings() const {
    return LightingSettings{
        this->scene->atom_lighting.ambient_strength,
        this->scene->atom_lighting.diffuse_strength,
        this->scene->atom_lighting.specular_strength,
        this->scene->atom_lighting.shininess,
        this->scene->atom_lighting.edge_strength,
        this->scene->atom_lighting.edge_power
    };
}

/**
 * @brief Get lighting settings for objects.
 *
 * @return lighting settings
 */
LightingSettings AnaglyphWidget::get_object_lighting_settings() const {
    return LightingSettings{
        this->scene->object_lighting.ambient_strength,
        this->scene->object_lighting.diffuse_strength,
        this->scene->object_lighting.specular_strength,
        this->scene->object_lighting.shininess,
        this->scene->object_lighting.edge_strength,
        this->scene->object_lighting.edge_power
    };
}

void AnaglyphWidget::load_lighting_settings() {
    QSettings settings;
    this->scene->atom_lighting.ambient_strength =
        settings.value("lighting/atoms/ambient_strength", this->scene->atom_lighting.ambient_strength).toFloat();
    this->scene->atom_lighting.diffuse_strength =
        settings.value("lighting/atoms/diffuse_strength", this->scene->atom_lighting.diffuse_strength).toFloat();
    this->scene->atom_lighting.specular_strength =
        settings.value("lighting/atoms/specular_strength", this->scene->atom_lighting.specular_strength).toFloat();
    this->scene->atom_lighting.shininess =
        settings.value("lighting/atoms/shininess", this->scene->atom_lighting.shininess).toFloat();
    this->scene->atom_lighting.edge_strength =
        settings.value("lighting/atoms/edge_strength", this->scene->atom_lighting.edge_strength).toFloat();
    this->scene->atom_lighting.edge_power =
        settings.value("lighting/atoms/edge_power", this->scene->atom_lighting.edge_power).toFloat();
    this->scene->object_lighting.ambient_strength =
        settings.value("lighting/objects/ambient_strength", this->scene->object_lighting.ambient_strength).toFloat();
    this->scene->object_lighting.diffuse_strength =
        settings.value("lighting/objects/diffuse_strength", this->scene->object_lighting.diffuse_strength).toFloat();
    this->scene->object_lighting.specular_strength =
        settings.value("lighting/objects/specular_strength", this->scene->object_lighting.specular_strength).toFloat();
    this->scene->object_lighting.shininess =
        settings.value("lighting/objects/shininess", this->scene->object_lighting.shininess).toFloat();
    this->scene->object_lighting.edge_strength =
        settings.value("lighting/objects/edge_strength", this->scene->object_lighting.edge_strength).toFloat();
    this->scene->object_lighting.edge_power =
        settings.value("lighting/objects/edge_power", this->scene->object_lighting.edge_power).toFloat();

    this->msaa_samples = normalize_msaa_samples(settings.value("rendering/msaa_samples", this->msaa_samples).toInt());
    this->sphere_tesselation_level = normalize_sphere_tesselation_level(
        settings.value("rendering/sphere_tesselation_level", this->sphere_tesselation_level).toInt());
}

/**
 * @brief      Sets the camera alignment.
 *
 * @param[in]  direction  The direction
 */
void AnaglyphWidget::set_camera_alignment(int direction) {
    QVector3D dirvec;

    switch((CameraAlignment)direction) {
    case CameraAlignment::DEFAULT:
        this->scene->rotation_matrix.setToIdentity();
        this->scene->rotation_matrix.rotate(20.0, QVector3D(1,0,0));
        this->scene->rotation_matrix.rotate(30.0, QVector3D(0,0,1));
        this->update();
        return;
    case CameraAlignment::TOP:
        dirvec = QVector3D(0.0f, 0.0f, 1.0f);
        emit signal_message_statusbar("Change camera alignment to TOP");
        break;
    case CameraAlignment::BOTTOM:
        dirvec = QVector3D(0.0f, 0.0f, -1.0f);
        emit signal_message_statusbar("Change camera alignment to BOTTOM");
        break;
    case CameraAlignment::LEFT:
        dirvec = QVector3D(-1.0f, 0.0f, 0.0f);
        emit signal_message_statusbar("Change camera alignment to LEFT");
        break;
    case CameraAlignment::RIGHT:
        dirvec = QVector3D(1.0f, 0.0f, 0.0f);
        emit signal_message_statusbar("Change camera alignment to RIGHT");
        break;
    case CameraAlignment::FRONT:
        dirvec = QVector3D(0.0f, 1.0f, 0.0f);
        emit signal_message_statusbar("Change camera alignment to FRONT");
        break;
    case CameraAlignment::BACK:
        dirvec = QVector3D(0.0f, -1.0f, 0.0f);
        emit signal_message_statusbar("Change camera alignment to BACK");
        break;
    }

    QVector3D axis;
    float angle;

    // avoid gimball locking
    if (fabs(dirvec[1]) > .999) {
        if(dirvec[1] < 0.0) {
            axis = QVector3D(0.0, 0.0, 1.0);
            angle = -M_PI;
        } else {
            axis = QVector3D(0.0, 0.0, 1.0);
            angle = 0.0;
        }
    } else {
        axis = QVector3D::crossProduct(QVector3D(0.0, 1.0, 0.0), dirvec);
        angle = std::acos(dirvec[1]);
    }

    this->scene->rotation_matrix.setToIdentity();
    this->scene->rotation_matrix.rotate(qRadiansToDegrees(angle), axis);
    this->update();
}

/**
 * @brief      Sets the camera mode.
 *
 * @param[in]  mode  The mode
 */
void AnaglyphWidget::set_camera_mode(int mode) {
    float w = (float)this->scene->canvas_width;
    float h = (float)this->scene->canvas_height;

    switch((CameraMode)mode) {
    case CameraMode::PERSPECTIVE:
        this->scene->camera_mode = CameraMode::PERSPECTIVE;
        this->scene->projection.setToIdentity();
        this->scene->projection.perspective(45.0f, w / h, 0.01f, 1000.0f);
        
        break;
    case CameraMode::ORTHOGRAPHIC:
        this->scene->camera_mode = CameraMode::ORTHOGRAPHIC;
        float w = (float)this->scene->canvas_width;
        float h = (float)this->scene->canvas_height;
        float ratio = w/h;
        float zoom = -this->scene->camera_position[1];
        this->scene->projection.setToIdentity();
        this->scene->projection.ortho(-zoom/2.0f, zoom/2.0f, -zoom / ratio /2.0f, zoom / ratio / 2.0f, 0.01f, 1000.0f);
        
        break;
    }

    this->update();
}


void AnaglyphWidget::set_msaa_samples(int samples) {
    const int normalized_samples = normalize_msaa_samples(samples);

    if (this->msaa_samples == normalized_samples) {
        return;
    }

    this->msaa_samples = normalized_samples;

    QSettings settings;
    settings.setValue("rendering/msaa_samples", this->msaa_samples);

    if (!this->context()) {
        return;
    }

    makeCurrent();
    this->destroy_framebuffers();
    this->build_framebuffers();
    doneCurrent();

    this->update();
}
void AnaglyphWidget::set_sphere_tesselation_level(int tesselation_level) {
    const int normalized_level = normalize_sphere_tesselation_level(tesselation_level);

    if (this->sphere_tesselation_level == normalized_level) {
        return;
    }

    this->sphere_tesselation_level = normalized_level;

    QSettings settings;
    settings.setValue("rendering/sphere_tesselation_level", this->sphere_tesselation_level);

    if (this->structure_renderer) {
        makeCurrent();
        this->structure_renderer->set_sphere_tesselation_level(this->sphere_tesselation_level);
        doneCurrent();
    }

    this->update();
}

void AnaglyphWidget::reset_lighting_settings_to_defaults() {
    const LightingSettings defaults;

    this->set_atom_lighting_settings(defaults.ambient_strength,
                                     defaults.diffuse_strength,
                                     defaults.specular_strength,
                                     defaults.shininess,
                                     defaults.edge_strength,
                                     defaults.edge_power);

    this->set_object_lighting_settings(defaults.ambient_strength,
                                       defaults.diffuse_strength,
                                       defaults.specular_strength,
                                       defaults.shininess,
                                       defaults.edge_strength,
                                       defaults.edge_power);

    this->set_msaa_samples(4);
    this->set_sphere_tesselation_level(4);
}



/**
 * @brief      Destroys the object.
 */
AnaglyphWidget::~AnaglyphWidget() {
    this->makeCurrent();
    this->frame.reset();
    this->doneCurrent();
    cleanup();

    // the base class destructor destroys the context, which would otherwise
    // call cleanup() on this (by then destroyed) object
    if(this->context() != nullptr) {
        disconnect(this->context(), &QOpenGLContext::aboutToBeDestroyed, this, &AnaglyphWidget::cleanup);
    }
}

/**
 * @brief      Set the minimum window size
 *
 * @return     Minimimum window size
 */
QSize AnaglyphWidget::minimumSizeHint() const {
    return QSize(50, 50);
}

/**
 * @brief      Provide default window size
 *
 * @return     Default window size
 */
QSize AnaglyphWidget::sizeHint() const {
    return QSize(400, 400);
}

/**
 * @brief      Clean the widget
 */
void AnaglyphWidget::cleanup() {
    // called when the OpenGL context is about to be destroyed (e.g. when the
    // widget is re-parented); release everything that lives on the GPU so
    // that initializeGL() can start with a clean slate
    makeCurrent();
    this->destroy_framebuffers();
    if(this->frame) {
        for(const auto& model : this->frame->get_models()) {
            model->release_gpu();
        }
    }
    this->structure_renderer.reset();
    this->shader_manager->clear();
    if(this->quad_vbo.isCreated()) {
        this->quad_vbo.destroy();
    }
    if(this->quad_vao.isCreated()) {
        this->quad_vao.destroy();
    }
    doneCurrent();
}

/**
 * @brief      Initialize OpenGL environment
 */
void AnaglyphWidget::initializeGL() {
    qDebug() << "Connecting to OpenGL Context";
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, &AnaglyphWidget::cleanup);

    qDebug() << "Initialize OpenGL functions";
    this->initializeOpenGLFunctions();

    glClearColor(this->tint, this->tint, this->tint, 1.0f);

    qDebug() << "Load shaders";
    this->load_shaders();

    qDebug() << "Create structure renderer object";
    this->structure_renderer = std::make_unique<StructureRenderer>(this->scene,
                                                                   this->shader_manager);
    this->structure_renderer->set_sphere_tesselation_level(this->sphere_tesselation_level);

    qDebug() << "Build Framebuffers";
    this->build_framebuffers();

    qDebug() << "Emit openGL ready";
    emit(opengl_ready());
}

QSize AnaglyphWidget::render_size() {
    const int width = std::max(1, this->scene->canvas_width * supersample_scale);
    const int height = std::max(1, this->scene->canvas_height * supersample_scale);
    return QSize(width, height);
}

void AnaglyphWidget::set_render_viewport() {
    const QSize target = render_size();
    glViewport(0, 0, target.width(), target.height());
}

void AnaglyphWidget::set_screen_viewport() {
    glViewport(0, 0, this->scene->canvas_width, this->scene->canvas_height);
}

/**
 * @brief      Render scene
 */
void AnaglyphWidget::paintGL() {
    this->set_screen_viewport();

    // paint coordinate axes to its own framebuffer
    if(this->flag_axis_enabled) {
        this->bind_render_framebuffer(FrameBuffer::COORDINATE_AXES);
        this->set_render_viewport();
        glEnable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        this->structure_renderer->draw_coordinate_axes();
        this->resolve_framebuffer(FrameBuffer::COORDINATE_AXES);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    // first perform a structure drawing call
    if (this->stereographic_type_name == "NONE") {
        this->paint_regular();
    } else {
        this->paint_stereographic();
    }

    // then combine everything
    if(this->flag_axis_enabled) {
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        glBlendEquation(GL_FUNC_ADD);

        ShaderProgram *shader = this->shader_manager->get_shader_program("simple_canvas_shader");
        shader->bind();

        // update screen coordinates
        shader->set_uniform("regular_texture", 0);

        // draw quad on screen
        this->set_screen_viewport();
        this->quad_vao.bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::COORDINATE_AXES]);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        this->quad_vao.release();

        shader->release();
    }
}

/**
 * @brief      Paint the models in the models vector to the screen
 */
void AnaglyphWidget::draw_structure() {
    if(this->frame) {
        this->structure_renderer->draw(this->frame.get());
    }
}

/**
 * @brief Load a new frame
 * @param _structure
 */
void AnaglyphWidget::set_frame(const std::shared_ptr<Frame>& _frame) {
    // replacing the frame may release models that live on the GPU
    this->makeCurrent();
    this->frame = _frame;
    this->doneCurrent();
    this->update();
}

void AnaglyphWidget::set_frame_conservative(const std::shared_ptr<Frame>& _frame) {
    this->set_frame(_frame);
}

/**
 * @brief      Place the camera such that the whole structure is visible
 *
 * @param[in]  radius  Radius of the sphere encompassing the structure
 */
void AnaglyphWidget::fit_camera(float radius) {
    const float distance = std::max(8.0f, 2.8f * radius + 6.0f);
    this->scene->camera_position = QVector3D(0.0f, -distance, 0.0f);
    this->pan_offset = QVector3D(0.0f, 0.0f, 0.0f);
    this->set_camera_mode((int)this->scene->camera_mode);
}

/**
 * @brief      Resize window
 *
 * @param[in]  width   screen width
 * @param[in]  height  screen height
 */
void AnaglyphWidget::resizeGL(int w, int h) {
    this->scene->projection.setToIdentity();
    this->scene->projection.perspective(45.0f, GLfloat(w) / h, 0.01f, 1000.0f);

    // store sizes
    this->scene->canvas_width = w;
    this->scene->canvas_height = h;

    this->set_screen_viewport();

    const QSize target = render_size();

    // resize textures and render buffers
    for (unsigned int i = 0; i < FrameBuffer::NR_FRAMEBUFFERS; ++i) {
        glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, target.width(), target.height(), 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

        glBindRenderbuffer(GL_RENDERBUFFER, this->rbo[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, target.width(), target.height());

        if (this->msaa_enabled) {
            glBindRenderbuffer(GL_RENDERBUFFER, this->rbo_msaa_color[i]);
            QOpenGLContext::currentContext()->extraFunctions()->glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->msaa_samples, GL_RGBA8, target.width(), target.height());

            glBindRenderbuffer(GL_RENDERBUFFER, this->rbo_msaa_depth_stencil[i]);
            QOpenGLContext::currentContext()->extraFunctions()->glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->msaa_samples, GL_DEPTH24_STENCIL8, target.width(), target.height());
        }
    }
}

/**
 * @brief      Parse mouse press event
 *
 * @param      event  The event
 */
void AnaglyphWidget::mousePressEvent(QMouseEvent *event) {
    // handle dragging
    if (event->buttons() & Qt::LeftButton) {
        // enable arcball rotation mode
        this->arcball_rotation_flag = true;

        // store positions of mouse
        this->m_lastPos = event->pos();
    }

    if (event->buttons() & Qt::RightButton) {
        this->pan_flag = true;
        this->m_lastPos = event->pos();
    }
}

/**
 * @brief      Parse mouse release event
 *
 * @param      event  The event
 */
void AnaglyphWidget::mouseReleaseEvent(QMouseEvent *event) {
    if (this->arcball_rotation_flag && !(event->buttons() & Qt::LeftButton) ) {
        // make arcball rotation permanent (note that the multiplication order for matrices matters)
        this->scene->rotation_matrix = this->scene->arcball_rotation * this->scene->rotation_matrix;

        // reset the arcball rotation matrix
        this->scene->arcball_rotation.setToIdentity();

        // unset arcball rotation mode
        this->arcball_rotation_flag = false;
    }

    if (this->pan_flag && !(event->buttons() & Qt::RightButton)) {
        this->pan_flag = false;
    }
}

/**
 * @brief      Parse mouse move event
 *
 * @param      event  The event
 */
void AnaglyphWidget::mouseMoveEvent(QMouseEvent *event) {
    this->setFocus(Qt::MouseFocusReason);

    if(this->arcball_rotation_flag) { // drag event
        // implementation adapted from
        // https://en.wikibooks.org/wiki/OpenGL_Programming/Modern_OpenGL_Tutorial_Arcball

        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            double ex = event->position().x();
            double ey = event->position().y();
        #else
            double ex = event->pos().x();
            double ey = event->pos().y();
        #endif
        if(ex != this->m_lastPos.x() || ey != this->m_lastPos.y()) {

            // calculate arcball vectors
            QVector3D va = this->get_arcball_vector(this->m_lastPos.x(), this->m_lastPos.y());
            QVector3D vb = this->get_arcball_vector(ex, ey);

            // calculate angle between vectors
            float dotprod = QVector3D::dotProduct(va, vb);
            if (qFabs(dotprod) > 0.9999f) {
                return;
            }
            float angle = qAcos(qMin(1.0f, dotprod));

            // determine rotation vector in camera space
            QVector4D axis_cam_space = QVector4D(QVector3D::crossProduct(va, vb).normalized());

            // calculate matrix to change basis from camera to model space
            QMatrix3x3 camera_to_model_trans = this->scene->view.inverted().toGenericMatrix<3,3>();

            // determine rotation vector in model space
            QVector4D axis_model_space = QMatrix4x4(camera_to_model_trans) * axis_cam_space;

            // set the rotation
            this->set_arcball_rotation(qRadiansToDegrees(angle), axis_model_space);
        }
    } else if(this->pan_flag) {
        #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            double ex = event->position().x();
            double ey = event->position().y();
        #else
            double ex = event->pos().x();
            double ey = event->pos().y();
        #endif

        const float dx = static_cast<float>(ex - this->m_lastPos.x());
        const float dy = static_cast<float>(ey - this->m_lastPos.y());
        const float zoom = std::max(1.0f, -this->scene->camera_position[1]);
        const float pan_scale = zoom * 0.0012f;

        this->pan_offset += QVector3D(-dx * pan_scale, 0.0f, dy * pan_scale);
        this->m_lastPos = QPoint(static_cast<int>(ex), static_cast<int>(ey));
        this->update();
    }
}

/**
 * @brief      Calculate the arcball vector for mouse rotation
 *
 * @param      int x    x-position of the mouse cursor on the screen
 * @param      int y    y-position of the mouse cursor on the screen
 */
QVector3D AnaglyphWidget::get_arcball_vector(int x, int y) {
    QVector3D P = QVector3D(1.0f * (float)x / (float)this->geometry().width() * 2.0f - 1.0f,
                            1.0f * (float)y / (float)this->geometry().height() * 2.0f - 1.0f,
                            0.0f);
    P[1] = -P[1];

    float OP_squared = P[0] * P[0] + P[1] * P[1];

    if(OP_squared <= 1.0f)
        P[2] = sqrt(1.0f - OP_squared);
    else
        P = P.normalized();
    return P;
}

/**
 * @brief      Sets the arcball rotation.
 *
 * @param[in]  arcball_angle   The arcball angle
 * @param[in]  arcball_vector  The arcball vector
 */
void AnaglyphWidget::set_arcball_rotation(float arcball_angle, const QVector4D& arcball_vector) {
    this->scene->arcball_rotation.setToIdentity();
    this->scene->arcball_rotation.rotate(arcball_angle, QVector3D(arcball_vector));
    this->update();
}


/**
 * @brief      Parse mouse wheel event
 *
 * @param      event  The event
 */
void AnaglyphWidget::wheelEvent(QWheelEvent *event) {

    this->scene->camera_position += event->angleDelta().ry() * 0.01f * QVector3D(0, 1, 0);
    if(this->scene->camera_position[1] > -5.0) {
        this->scene->camera_position[1] = -5.0;
    }

    float w = (float)this->scene->canvas_width;
    float h = (float)this->scene->canvas_height;
    float ratio = w/h;
    float zoom = -this->scene->camera_position[1];

    if(this->scene->camera_mode == CameraMode::ORTHOGRAPHIC) {
        this->scene->projection.setToIdentity();
        this->scene->projection.ortho(-zoom/2.0f, zoom/2.0f, -zoom / ratio /2.0f, zoom / ratio / 2.0f, 0.01f, 1000.0f);
    }

    this->update();
}

/**
 * @brief Update when moving window
 */
void AnaglyphWidget::window_move_event() {
    this->top_left = mapToGlobal(QPoint(0, 0));
    this->update();
}

/**
 * @brief Set stereographic projection type
 * @param stereographic projection name
 */
void AnaglyphWidget::set_stereo(QString stereo_name) {
    if (stereo_name.startsWith("stereo")) {
        this->stereographic_type_name = stereo_name;
    } else {
        this->stereographic_type_name = "NONE";
    }

    this->update();
}

/**
 * PRIVATE FUNCTIONS
 */

/**
 * @brief      Load OpenGL shaders
 */
void AnaglyphWidget::load_shaders() {
    // create regular shaders
    shader_manager->create_shader_program("atombond_shader", ShaderProgramType::ModelShader, ":/assets/shaders/phong.vs", ":/assets/shaders/phong.fs");
    shader_manager->create_shader_program("object_shader", ShaderProgramType::ModelShader, ":/assets/shaders/phong.vs", ":/assets/shaders/phong.fs");
    shader_manager->create_shader_program("axes_shader", ShaderProgramType::AxesShader, ":/assets/shaders/axes.vs", ":/assets/shaders/axes.fs");
    shader_manager->create_shader_program("silhouette_shader", ShaderProgramType::SilhouetteShader, ":/assets/shaders/silhouette.vs", ":/assets/shaders/silhouette.fs");

    // create shaders for the stereographic projections
    shader_manager->create_shader_program("stereo_anaglyph_red_cyan", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_anaglyph_red_cyan.fs");
    shader_manager->create_shader_program("stereo_interlaced_checkerboard_lr", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_checkerboard_lr.fs");
    shader_manager->create_shader_program("stereo_interlaced_checkerboard_rl", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_checkerboard_rl.fs");
    shader_manager->create_shader_program("stereo_interlaced_columns_lr", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_columns_lr.fs");
    shader_manager->create_shader_program("stereo_interlaced_columns_rl", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_columns_rl.fs");
    shader_manager->create_shader_program("stereo_interlaced_rows_lr", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_rows_lr.fs");
    shader_manager->create_shader_program("stereo_interlaced_rows_rl", ShaderProgramType::StereoscopicShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/stereo_interlaced_rows_rl.fs");

    // load shader for the Canvas
    shader_manager->create_shader_program("canvas_shader", ShaderProgramType::CanvasShader, ":/assets/shaders/stereo.vs", ":/assets/shaders/canvas.fs");
    shader_manager->create_shader_program("simple_canvas_shader", ShaderProgramType::SimpleCanvasShader, ":/assets/shaders/simplecanvas.vs", ":/assets/shaders/simplecanvas.fs");
}

/**
 * @brief      Build a canvas used for stereographic rendering
 */
void AnaglyphWidget::build_framebuffers() {
    const int render_width = std::max(1, this->geometry().width() * supersample_scale);
    const int render_height = std::max(1, this->geometry().height() * supersample_scale);

    this->msaa_enabled = this->msaa_samples > 1;

    // initialize frame buffers
    glGenFramebuffers(FrameBuffer::NR_FRAMEBUFFERS, this->framebuffers);
    glGenTextures(FrameBuffer::NR_FRAMEBUFFERS, this->texture_color_buffers);
    glGenRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo);

    if (this->msaa_enabled) {
        glGenFramebuffers(FrameBuffer::NR_FRAMEBUFFERS, this->framebuffers_msaa);
        glGenRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo_msaa_color);
        glGenRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo_msaa_depth_stencil);
    }

    for (unsigned int i = 0; i < FrameBuffer::NR_FRAMEBUFFERS; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers[i]);
        glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, render_width, render_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->texture_color_buffers[i], 0);

        glBindRenderbuffer(GL_RENDERBUFFER, this->rbo[i]);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, render_width, render_height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->rbo[i]);

        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            qWarning() << "Framebuffer is not complete.";
        }

        if (this->msaa_enabled) {
            glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers_msaa[i]);

            glBindRenderbuffer(GL_RENDERBUFFER, this->rbo_msaa_color[i]);
            QOpenGLContext::currentContext()->extraFunctions()->glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->msaa_samples, GL_RGBA8, render_width, render_height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, this->rbo_msaa_color[i]);

            glBindRenderbuffer(GL_RENDERBUFFER, this->rbo_msaa_depth_stencil[i]);
            QOpenGLContext::currentContext()->extraFunctions()->glRenderbufferStorageMultisample(GL_RENDERBUFFER, this->msaa_samples, GL_DEPTH24_STENCIL8, render_width, render_height);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, this->rbo_msaa_depth_stencil[i]);

            if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
                qWarning() << "MSAA framebuffer is not complete, disabling MSAA.";
                this->msaa_enabled = false;
                break;
            }
        }
    }

    if (this->msaa_enabled) {
        qDebug() << "MSAA enabled with" << this->msaa_samples << "samples.";
    }

    this->framebuffers_initialized = true;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // create screen quad vao
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    // vertex attributes for a quad that fills the entire screen in Normalized Device Coordinates.
    float quadvecs[] = {
        // positions
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        // texCoords
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    if (!this->quad_vao.isCreated()) {
        this->quad_vao.create();
        this->quad_vao.bind();

        this->quad_vbo.create();
        this->quad_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
        this->quad_vbo.bind();
        this->quad_vbo.allocate(quadvecs, sizeof(quadvecs));
        f->glEnableVertexAttribArray(0);
        f->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
        f->glEnableVertexAttribArray(1);
        f->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

        this->quad_vao.release();
    }
}


void AnaglyphWidget::bind_render_framebuffer(FrameBuffer buffer) {
    if (this->msaa_enabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers_msaa[buffer]);
    } else {
        glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers[buffer]);
    }
}

void AnaglyphWidget::resolve_framebuffer(FrameBuffer buffer) {
    if (!this->msaa_enabled) {
        return;
    }

    glBindFramebuffer(GL_READ_FRAMEBUFFER, this->framebuffers_msaa[buffer]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, this->framebuffers[buffer]);

    const QSize target = render_size();
    QOpenGLContext::currentContext()->extraFunctions()->glBlitFramebuffer(0, 0,
                      target.width(), target.height(),
                      0, 0,
                      target.width(), target.height(),
                      GL_COLOR_BUFFER_BIT, GL_NEAREST);

    glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers[buffer]);
}

void AnaglyphWidget::destroy_framebuffers() {
    if (!this->framebuffers_initialized) {
        return;
    }

    glDeleteFramebuffers(FrameBuffer::NR_FRAMEBUFFERS, this->framebuffers);
    glDeleteTextures(FrameBuffer::NR_FRAMEBUFFERS, this->texture_color_buffers);
    glDeleteRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo);

    glDeleteFramebuffers(FrameBuffer::NR_FRAMEBUFFERS, this->framebuffers_msaa);
    glDeleteRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo_msaa_color);
    glDeleteRenderbuffers(FrameBuffer::NR_FRAMEBUFFERS, this->rbo_msaa_depth_stencil);

    std::fill(std::begin(this->framebuffers), std::end(this->framebuffers), 0u);
    std::fill(std::begin(this->texture_color_buffers), std::end(this->texture_color_buffers), 0u);
    std::fill(std::begin(this->rbo), std::end(this->rbo), 0u);
    std::fill(std::begin(this->framebuffers_msaa), std::end(this->framebuffers_msaa), 0u);
    std::fill(std::begin(this->rbo_msaa_color), std::end(this->rbo_msaa_color), 0u);
    std::fill(std::begin(this->rbo_msaa_depth_stencil), std::end(this->rbo_msaa_depth_stencil), 0u);

    this->framebuffers_initialized = false;
}


/**
 * @brief      Reset rotation matrices
 */
void AnaglyphWidget::reset_matrices() {
    this->scene->rotation_matrix.setToIdentity();
    this->scene->rotation_matrix.rotate(20.0, QVector3D(1,0,0));
    this->scene->rotation_matrix.rotate(30.0, QVector3D(0,0,1));
    this->scene->arcball_rotation.setToIdentity();
    this->pan_offset = QVector3D(0.0f, 0.0f, 0.0f);
}

/**
 * @brief      Regular draw call
 */
void AnaglyphWidget::paint_regular() {
    // set draw settings
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    // set view matrix
    QVector3D lookat = QVector3D(0.0f, 1.0f, 0.0f) + this->pan_offset;
    this->scene->view.setToIdentity();
    this->scene->view.lookAt(this->scene->camera_position + this->pan_offset, lookat, QVector3D(0.0, 0.0, 1.0));

    // regular draw call to the STRUCTURE_NORMAL framebuffer
    this->bind_render_framebuffer(FrameBuffer::STRUCTURE_NORMAL);
    this->set_render_viewport();
    glEnable(GL_DEPTH_TEST);
    glClearColor(this->tint, this->tint, this->tint, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    this->draw_structure();
    this->resolve_framebuffer(FrameBuffer::STRUCTURE_NORMAL);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // bundle both framebuffers and draw on the canvas
    this->set_screen_viewport();
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    ShaderProgram *canvas_shader = this->shader_manager->get_shader_program("canvas_shader");
    canvas_shader->bind();

    // update screen coordinates
    canvas_shader->set_uniform("regular_texture", 0);

    // draw quad on screen
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    this->quad_vao.bind();
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::STRUCTURE_NORMAL]);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::SILHOUETTE_NORMAL]);
    f->glDrawArrays(GL_TRIANGLES, 0, 6);
    this->quad_vao.release();

    // release shader
    canvas_shader->release();
}

/**
 * @brief      Stereographic draw call
 */
void AnaglyphWidget::paint_stereographic() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
    glBlendEquation(GL_FUNC_ADD);

    // set convergence point and intra-ocular separation
    QVector3D lookat = QVector3D(0.0f, 1.0f, 0.0f) + this->pan_offset;
    const QVector3D camera_position = this->scene->camera_position + this->pan_offset;
    float dist = 1.0f - this->scene->camera_position[1];
    float eye_sep = dist / 30.0f;

    // draw structure for left eye
    this->scene->view.setToIdentity();
    this->scene->view.lookAt(camera_position - QVector3D(eye_sep / 2.0, 0.0, 0.0), lookat, QVector3D(0.0, 0.0, 1.0));
    this->bind_render_framebuffer(FrameBuffer::STRUCTURE_LEFT);
    this->set_render_viewport();
    glEnable(GL_DEPTH_TEST);
    glClearColor(this->tint, this->tint, this->tint, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    this->draw_structure();
    this->resolve_framebuffer(FrameBuffer::STRUCTURE_LEFT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // draw structure for right eye
    this->scene->view.setToIdentity();
    this->scene->view.lookAt(camera_position + QVector3D(eye_sep / 2.0, 0.0, 0.0), lookat, QVector3D(0.0, 0.0, 1.0));
    this->bind_render_framebuffer(FrameBuffer::STRUCTURE_RIGHT);
    this->set_render_viewport();
    glEnable(GL_DEPTH_TEST);
    glClearColor(this->tint, this->tint, this->tint, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    this->draw_structure();
    this->resolve_framebuffer(FrameBuffer::STRUCTURE_RIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // combine L+L
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers[FrameBuffer::ANAGLYPH_LEFT]);
    this->set_render_viewport();
    ShaderProgram *canvas_shader = this->shader_manager->get_shader_program("canvas_shader");
    canvas_shader->bind();

    // update screen coordinates
    canvas_shader->set_uniform("regular_texture", 0);

    // draw quad on screen
    QOpenGLFunctions *f = QOpenGLContext::currentContext()->functions();

    this->quad_vao.bind();
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::STRUCTURE_LEFT]);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::SILHOUETTE_LEFT]);
    f->glDrawArrays(GL_TRIANGLES, 0, 6);
    this->quad_vao.release();
    canvas_shader->release();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // combine R+R
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);
    glBindFramebuffer(GL_FRAMEBUFFER, this->framebuffers[FrameBuffer::ANAGLYPH_RIGHT]);
    this->set_render_viewport();
    canvas_shader->bind();

    // update screen coordinates
    canvas_shader->set_uniform("regular_texture", 0);

    // draw quad on screen
    this->quad_vao.bind();
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::STRUCTURE_RIGHT]);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::SILHOUETTE_RIGHT]);
    f->glDrawArrays(GL_TRIANGLES, 0, 6);
    this->quad_vao.release();
    canvas_shader->release();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // combine left and right for anaglyph
    this->set_screen_viewport();
    glDisable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT);

    ShaderProgram *stereographic_shader = this->shader_manager->get_shader_program(this->stereographic_type_name.toUtf8().constData());
    stereographic_shader->bind();

    // update screen coordinates
    stereographic_shader->set_uniform("left_eye_texture", 0);
    stereographic_shader->set_uniform("right_eye_texture", 1);
    stereographic_shader->set_uniform("screen_x", this->top_left.x());
    stereographic_shader->set_uniform("screen_y", this->top_left.y());

    // draw quad on screen
    this->quad_vao.bind();
    f->glActiveTexture(GL_TEXTURE0);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::ANAGLYPH_LEFT]);
    f->glActiveTexture(GL_TEXTURE1);
    f->glBindTexture(GL_TEXTURE_2D, this->texture_color_buffers[FrameBuffer::ANAGLYPH_RIGHT]);
    f->glDrawArrays(GL_TRIANGLES, 0, 6);
    this->quad_vao.release();

    stereographic_shader->release();
}
