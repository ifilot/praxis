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
#include <QVector4D>

#include <chrono>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <exception>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/norm.hpp>

class Model {
private:
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<uint32_t> indices;
    QVector4D color;

    bool flag_loaded_vao = false;

    QOpenGLVertexArrayObject vao;
    QOpenGLBuffer vbo[4];

public:

    Model(std::vector<glm::vec3> positions, std::vector<glm::vec3> normals, std::vector<uint32_t> indices);

    /**
     * @brief      Draw the model
     */
    void draw();

    /**
     * @brief      Destroys the object.
     */
    ~Model();

    /**
     * @brief      Get maximum vector
     *
     * @return     The maximum vector distance
     */
    glm::vec3 get_max_dim() const;

    /**
     * @brief      Load all data to a vertex array object
     */
    void load_to_vao();

    /**
     * @brief      Release the GPU buffers (requires the OpenGL context that
     *             created them to be current); the model is uploaded again
     *             on the next draw call
     */
    void release_gpu();

    inline void set_color(const QVector4D& _color) {
        this->color = _color;
    }

    const auto& get_color() const {
        return this->color;
    }

    inline size_t get_num_vertices() const {
        return this->positions.size();
    }

    inline size_t get_num_normals() const {
        return this->normals.size();
    }

    inline size_t get_num_indices() const {
        return this->indices.size();
    }

    const std::vector<glm::vec3>& get_positions() const { 
        return positions; 
    }

    inline bool is_loaded() const {
        return this->flag_loaded_vao;
    }
};
