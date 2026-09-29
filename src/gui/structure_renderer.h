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

#pragma once

#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QDebug>
#include <QMatrix4x4>
#include <QtMath>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

#include <vector>

#include "../data/model_loader.h"
#include "../data/frame.h"
#include "scene.h"
#include "shader_program_manager.h"

class StructureRenderer {
private:
    // sphere facets
    std::vector<glm::vec3> sphere_vertices;
    std::vector<glm::vec3> sphere_normals;
    std::vector<unsigned int> sphere_indices;

    // cylinder facets
    std::vector<glm::vec3> cylinder_vertices;
    std::vector<glm::vec3> cylinder_normals;
    std::vector<unsigned int> cylinder_indices;

    // vao and vbo for rendering
    QOpenGLVertexArrayObject vao_sphere;
    QOpenGLBuffer vbo_sphere[3];

    QOpenGLVertexArrayObject vao_cylinder;
    QOpenGLBuffer vbo_cylinder[3];

    QOpenGLVertexArrayObject vao_unitcell;
    QOpenGLBuffer vbo_unitcell[2];

    QOpenGLVertexArrayObject vao_line;
    QOpenGLBuffer vbo_line[2];

    QOpenGLVertexArrayObject vao_plane;
    QOpenGLBuffer vbo_plane[2];

    std::shared_ptr<Scene> scene;
    std::shared_ptr<ShaderProgramManager> shader_manager;

    // models
    std::shared_ptr<Model> axis_model;

    unsigned int sphere_tesselation_level = 4;

public:
    /**
     * @brief      Constructs a new instance.
     *
     * @param[in]  _scene           The scene
     * @param[in]  _shader_manager  The shader manager
     * @param[in]  _user_action     The user action
     */
    StructureRenderer(const std::shared_ptr<Scene>& _scene,
                      const std::shared_ptr<ShaderProgramManager>& _shader_manager);

    /**
     * @brief      Draw the structure
     */
    void draw(const Frame *frame);

    /**
     * @brief      Draws coordinate axes.
     */
    void draw_coordinate_axes();

    void set_sphere_tesselation_level(unsigned int tesselation_level);

    unsigned int get_sphere_tesselation_level() const {
        return this->sphere_tesselation_level;
    }

private:
    /**
     * @brief      Set lighting uniforms.
     *
     * @param      shader  The shader
     */
    void set_lighting_uniforms(ShaderProgram* shader, const LightingSettings& settings) const;

    /**
     * @brief      Draws atoms.
     *
     * @param[in]  atoms           The atoms
     * @param[in]  structure       The structure
     * @param[in]  periodicity_xy  The periodicity xy
     * @param[in]  periodicity_z   The periodicity z
     */
    void draw_atoms(const Structure* structure);

    /**
     * @brief      Draws bonds.
     *
     * @param[in]  structure  The structure
     */
    void draw_bonds(const Structure* structure);

    /**
     * @brief      Draw single object
     *
     * @param[in]  structure  The structure
     * @param[in]  shader     Which shader to use
     */
    void draw_single_object(const std::shared_ptr<Model>& obj, ShaderProgram* shader);

    /**
     * @brief      Generate coordinates of a sphere
     *
     * @param[in]  tesselation_level  The tesselation level
     */
    void generate_sphere_coordinates(unsigned int tesselation_level);

    /**
     * @brief      Generate coordinates for a default cylinder (radius 1, height 1)
     *
     * @param[in]  stack_count  The stack count
     * @param[in]  slice_count  The slice count
     */
    void generate_cylinder_coordinates(unsigned int stack_count, unsigned int slice_count);

    /**
     * @brief      Load all data to a vertex array object
     */
    void load_sphere_to_vao();

    /**
     * @brief      Load all data to a vertex array object
     */
    void load_cylinder_to_vao();

    /**
     * @brief      Load simple line data to vertex array object
     */
    void load_line_to_vao();

    /**
     * @brief      Load simple plane data to vertex array object
     */
    void load_plane_to_vao();

    /**
     * @brief      Loads an arrow model.
     */
    void load_arrow_model();

    /**
     * @brief      Darken color
     *
     * @param[in]  color   The color
     * @param[in]  amount  The amount
     *
     * @return     The 3D vector.
     */
    QVector3D darken(const QVector3D& color, float amount) const;

    /**
     * @brief      Lighten color
     *
     * @param[in]  color   The color
     * @param[in]  amount  The amount
     *
     * @return     The 3D vector.
     */
    QVector3D lighten(const QVector3D& color, float amount) const;

    /**
     * @brief      Mix colors
     *
     * @param[in]  color1  First color
     * @param[in]  color2  Second color
     * @param[in]  amount  The amount
     *
     * @return     The 3D vector.
     */
    QVector3D mix(const QVector3D& color1, const QVector3D& color2, float amount) const;
};
