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

#include "marching_cubes.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

#include "edgetable.h"
#include "triangletable.h"

namespace {

// corner offsets using the convention of Paul Bourke's lookup tables
constexpr int corner_offset[8][3] = {
    {0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}
};

// corners connected by each of the twelve cube edges
constexpr int edge_corners[12][2] = {
    {0, 1}, {1, 2}, {2, 3}, {3, 0},
    {4, 5}, {5, 6}, {6, 7}, {7, 4},
    {0, 4}, {1, 5}, {2, 6}, {3, 7}
};

} // namespace

IsoMesh marching_cubes(const ScalarField& field, float isovalue) {
    IsoMesh mesh;

    const auto& dims = field.get_dims();
    if(dims[0] < 2 || dims[1] < 2 || dims[2] < 2) {
        return mesh;
    }

    // normals point away from the enclosed region, see header
    const float normal_sign = isovalue >= 0.0f ? -1.0f : 1.0f;

    // vertices are shared between cubes; key them on the grid edge
    std::unordered_map<uint64_t, uint32_t> edge_vertex;

    auto get_vertex = [&](unsigned int i, unsigned int j, unsigned int k, int edge) -> uint32_t {
        const int* ca = corner_offset[edge_corners[edge][0]];
        const int* cb = corner_offset[edge_corners[edge][1]];

        const unsigned int ia = i + ca[0], ja = j + ca[1], ka = k + ca[2];
        const unsigned int ib = i + cb[0], jb = j + cb[1], kb = k + cb[2];

        const float va = field.get(ia, ja, ka);
        const float vb = field.get(ib, jb, kb);
        float t = 0.5f;
        if(std::abs(vb - va) > 1e-12f) {
            t = (isovalue - va) / (vb - va);
            t = std::min(std::max(t, 0.0f), 1.0f);
        }

        // unique key: lowest corner of the edge plus the edge direction; a
        // vertex coinciding with a grid point is keyed on that grid point so
        // that it is shared by all edges meeting there (this avoids
        // zero-area triangles and keeps the mesh closed)
        constexpr float snap = 1e-6f;
        uint64_t key;
        if(t < snap) {
            t = 0.0f;
            key = ((uint64_t)field.index(ia, ja, ka) << 2) | 3u;
        } else if(t > 1.0f - snap) {
            t = 1.0f;
            key = ((uint64_t)field.index(ib, jb, kb) << 2) | 3u;
        } else {
            const unsigned int il = std::min(ia, ib), jl = std::min(ja, jb), kl = std::min(ka, kb);
            const unsigned int axis = (ia != ib) ? 0 : ((ja != jb) ? 1 : 2);
            key = ((uint64_t)field.index(il, jl, kl) << 2) | axis;
        }

        auto got = edge_vertex.find(key);
        if(got != edge_vertex.end()) {
            return got->second;
        }

        const glm::vec3 pa = field.position(ia, ja, ka);
        const glm::vec3 pb = field.position(ib, jb, kb);
        const glm::vec3 ga = field.gradient(ia, ja, ka);
        const glm::vec3 gb = field.gradient(ib, jb, kb);

        glm::vec3 n = normal_sign * (ga + t * (gb - ga));
        const float len = glm::length(n);
        n = len > 0.0f ? n / len : glm::vec3(0.0f);

        const uint32_t idx = (uint32_t)mesh.positions.size();
        mesh.positions.push_back(pa + t * (pb - pa));
        mesh.normals.push_back(n);
        edge_vertex.emplace(key, idx);
        return idx;
    };

    for(unsigned int k = 0; k < dims[2] - 1; ++k) {
        for(unsigned int j = 0; j < dims[1] - 1; ++j) {
            for(unsigned int i = 0; i < dims[0] - 1; ++i) {
                unsigned int cubeindex = 0;
                for(int c = 0; c < 8; ++c) {
                    const float v = field.get(i + corner_offset[c][0],
                                              j + corner_offset[c][1],
                                              k + corner_offset[c][2]);
                    if(v < isovalue) {
                        cubeindex |= (1u << c);
                    }
                }

                if(edge_table[cubeindex] == 0) {
                    continue;
                }

                for(int t = 0; triangle_table[cubeindex][t] != -1; t += 3) {
                    uint32_t v0 = get_vertex(i, j, k, triangle_table[cubeindex][t]);
                    uint32_t v1 = get_vertex(i, j, k, triangle_table[cubeindex][t + 1]);
                    uint32_t v2 = get_vertex(i, j, k, triangle_table[cubeindex][t + 2]);

                    // skip degenerate triangles
                    if(v0 == v1 || v1 == v2 || v0 == v2) {
                        continue;
                    }

                    // enforce counter-clockwise winding with respect to the
                    // outward pointing normals
                    const glm::vec3& p0 = mesh.positions[v0];
                    const glm::vec3 face = glm::cross(mesh.positions[v1] - p0, mesh.positions[v2] - p0);
                    const glm::vec3 nsum = mesh.normals[v0] + mesh.normals[v1] + mesh.normals[v2];
                    if(glm::dot(face, nsum) < 0.0f) {
                        std::swap(v1, v2);
                    }

                    mesh.indices.push_back(v0);
                    mesh.indices.push_back(v1);
                    mesh.indices.push_back(v2);
                }
            }
        }
    }

    // vertices without a well-defined gradient get the average face normal
    std::vector<glm::vec3> face_normals(mesh.positions.size(), glm::vec3(0.0f));
    bool need_face_normals = false;
    for(const auto& n : mesh.normals) {
        if(n == glm::vec3(0.0f)) {
            need_face_normals = true;
            break;
        }
    }
    if(need_face_normals) {
        for(size_t t = 0; t < mesh.indices.size(); t += 3) {
            const uint32_t a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
            const glm::vec3 fn = glm::cross(mesh.positions[b] - mesh.positions[a],
                                            mesh.positions[c] - mesh.positions[a]);
            face_normals[a] += fn;
            face_normals[b] += fn;
            face_normals[c] += fn;
        }
        for(size_t v = 0; v < mesh.normals.size(); ++v) {
            if(mesh.normals[v] == glm::vec3(0.0f) && glm::length(face_normals[v]) > 0.0f) {
                mesh.normals[v] = glm::normalize(face_normals[v]);
            }
        }
    }

    return mesh;
}

IsoMesh sphere_mesh(const glm::vec3& center, float radius, unsigned int stacks, unsigned int slices) {
    constexpr float pi = 3.14159265358979323846f;
    IsoMesh mesh;
    stacks = std::max(2u, stacks);
    slices = std::max(3u, slices);

    auto add_vertex = [&](const glm::vec3& n) {
        mesh.positions.push_back(center + radius * n);
        mesh.normals.push_back(n);
    };

    // north pole, rings (without seam duplicates), south pole
    add_vertex(glm::vec3(0.0f, 0.0f, 1.0f));
    for(unsigned int i = 1; i < stacks; ++i) {
        const float theta = pi * (float)i / (float)stacks;
        for(unsigned int j = 0; j < slices; ++j) {
            const float phi = 2.0f * pi * (float)j / (float)slices;
            add_vertex(glm::vec3(std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)));
        }
    }
    add_vertex(glm::vec3(0.0f, 0.0f, -1.0f));

    const uint32_t south = (uint32_t)mesh.positions.size() - 1;
    auto ring = [slices](unsigned int i, unsigned int j) {     // i = 1 .. stacks-1
        return (uint32_t)(1 + (i - 1) * slices + (j % slices));
    };
    auto triangle = [&mesh](uint32_t a, uint32_t b, uint32_t c) {
        mesh.indices.insert(mesh.indices.end(), {a, b, c});
    };

    // counter-clockwise when seen from outside: moving down (increasing
    // theta) and then along increasing phi
    for(unsigned int j = 0; j < slices; ++j) {
        triangle(0, ring(1, j), ring(1, j + 1));
        for(unsigned int i = 1; i + 1 < stacks; ++i) {
            triangle(ring(i, j), ring(i + 1, j), ring(i + 1, j + 1));
            triangle(ring(i, j), ring(i + 1, j + 1), ring(i, j + 1));
        }
        triangle(ring(stacks - 1, j), south, ring(stacks - 1, j + 1));
    }

    return mesh;
}
