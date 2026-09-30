/**************************************************************************
 *   This file is part of PYQINT-GUI.                                     *
 *                                                                        *
 *   Author: Ivo Filot <ivo@ivofilot.nl>                                  *
 *                                                                        *
 *   PYQINT-GUI is free software:                                         *
 *   you can redistribute it and/or modify it under the terms of the      *
 *   GNU General Public License as published by the Free Software         *
 *   Foundation, either version 3 of the License, or (at your option)     *
 *   any later version.                                                   *
 *                                                                        *
 *   PYQINT-GUI is distributed in the hope that it will be useful,        *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#pragma once

#include <memory>
#include <vector>

#include <QImage>
#include <QOpenGLFunctions>
#include <QOpenGLWidget>

#include "data/frame.h"
#include "scene.h"

class ShaderProgramManager;
class StructureRenderer;

/**
 * @brief Grid of molecular orbitals sharing a single camera
 *
 * All tiles are drawn by one OpenGL widget, each in its own viewport, such
 * that rotating one orbital rotates all of them. Dragging rotates, the mouse
 * wheel scrolls (Ctrl + wheel zooms) and double-clicking activates a tile.
 */
class OrbitalGalleryWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT

public:
    struct Tile {
        QString title;                  // e.g. "MO 5 · HOMO"
        QString subtitle;               // e.g. "-0.3912 Ht · occ. 2"
        std::shared_ptr<Frame> frame;   // molecule + isosurfaces; nullptr while evaluating
    };

private:
    static constexpr float rotation_sensitivity = 2.0f;     // see AnaglyphWidget
    static constexpr int gap = 6;                           // between tiles (logical px)
    static constexpr float tint = 235.0f / 255.0f;          // background of a tile

    std::shared_ptr<Scene> scene;
    std::shared_ptr<ShaderProgramManager> shader_manager;
    std::unique_ptr<StructureRenderer> renderer;

    std::vector<Tile> tiles;
    std::shared_ptr<Frame> placeholder;     // molecule only, shown while evaluating

    int tile_size = 240;                    // requested tile size (logical px)
    int scroll = 0;                         // vertical scroll offset (logical px)
    int selected = -1;
    float camera_distance = 12.0f;

    bool rotating = false;
    QRect drag_rect;
    QPoint press_pos;
    QPoint last_pos;

public:
    explicit OrbitalGalleryWidget(QWidget* parent = nullptr);
    ~OrbitalGalleryWidget();

    /**
     * @brief Replace all tiles
     *
     * @param tiles         tiles (frames may still be missing)
     * @param placeholder   shown in tiles without frame
     */
    void set_tiles(std::vector<Tile> tiles, std::shared_ptr<Frame> placeholder);

    /**
     * @brief Set the frame (and optionally the subtitle) of a tile
     */
    void set_tile(int index, std::shared_ptr<Frame> frame, const QString& subtitle);

    void clear();

    inline int count() const {
        return (int)this->tiles.size();
    }

    inline const Tile& tile(int index) const {
        return this->tiles[index];
    }

    void set_tile_size(int size);

    void set_scroll(int scroll);

    /**
     * @brief Total height of all rows (logical px)
     */
    int content_height() const;

    void set_rotation(const QMatrix4x4& rotation);
    QMatrix4x4 get_rotation() const;

    void set_camera_mode(CameraMode mode);

    /**
     * @brief Distance of the camera; all tiles use the same scale
     */
    void set_camera_distance(float distance);

    /**
     * @brief Render a single tile to an image of pixels x pixels
     */
    QImage render_tile(int index, int pixels, const QColor& background);

    QSize sizeHint() const override;

signals:
    void layout_changed();
    void scroll_requested(int delta);
    void tile_activated(int index);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void cleanup();

private:
    int columns() const;
    int cell_size() const;
    QRect tile_rect(int index) const;       // logical px, scrolled
    int tile_at(const QPoint& pos) const;

    void setup_camera();
    void draw_frame(const Frame* frame);
    void make_opaque();
    QVector3D arcball_vector(const QPoint& pos) const;
};
