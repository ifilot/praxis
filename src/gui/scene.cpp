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

#include "scene.h"

#include <algorithm>
#include <array>

#include <QtMath>

namespace {

/**
 * @brief Eigenvectors of a symmetric 3x3 matrix (cyclic Jacobi method)
 *
 * @return eigenvectors (columns of v) sorted by decreasing eigenvalue
 */
std::array<QVector3D, 3> symmetric_eigenvectors(double a[3][3]) {
    double v[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};
    for(int sweep = 0; sweep < 50; ++sweep) {
        const double off = a[0][1] * a[0][1] + a[0][2] * a[0][2] + a[1][2] * a[1][2];
        if(off < 1e-20) {
            break;
        }
        for(int p = 0; p < 2; ++p) {
            for(int q = p + 1; q < 3; ++q) {
                if(std::abs(a[p][q]) < 1e-30) {
                    continue;
                }
                const double theta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double t = (theta >= 0 ? 1.0 : -1.0) / (std::abs(theta) + std::sqrt(theta * theta + 1.0));
                const double c = 1.0 / std::sqrt(t * t + 1.0);
                const double s = t * c;
                for(int k = 0; k < 3; ++k) {       // A <- J^T A J
                    const double akp = a[k][p], akq = a[k][q];
                    a[k][p] = c * akp - s * akq;
                    a[k][q] = s * akp + c * akq;
                }
                for(int k = 0; k < 3; ++k) {
                    const double apk = a[p][k], aqk = a[q][k];
                    a[p][k] = c * apk - s * aqk;
                    a[q][k] = s * apk + c * aqk;
                }
                for(int k = 0; k < 3; ++k) {       // V <- V J
                    const double vkp = v[k][p], vkq = v[k][q];
                    v[k][p] = c * vkp - s * vkq;
                    v[k][q] = s * vkp + c * vkq;
                }
            }
        }
    }

    std::array<int, 3> order = {0, 1, 2};
    std::sort(order.begin(), order.end(), [&a](int i, int j) { return a[i][i] > a[j][j]; });

    std::array<QVector3D, 3> result;
    for(int i = 0; i < 3; ++i) {
        QVector3D e((float)v[0][order[i]], (float)v[1][order[i]], (float)v[2][order[i]]);
        // fix the arbitrary sign: largest component positive
        int imax = 0;
        for(int k = 1; k < 3; ++k) {
            if(std::abs(e[k]) > std::abs(e[imax])) {
                imax = k;
            }
        }
        result[i] = e[imax] < 0.0f ? -e : e;
    }
    return result;
}

} // namespace

Scene::Scene() {
}

QMatrix4x4 Scene::alignment_rotation(CameraAlignment alignment, const std::vector<QVector3D>& positions) {
    QMatrix4x4 rotation;
    QVector3D dirvec;

    switch(alignment) {
    case CameraAlignment::DEFAULT:
        rotation.rotate(20.0, QVector3D(1, 0, 0));
        rotation.rotate(30.0, QVector3D(0, 0, 1));
        return rotation;
    case CameraAlignment::FACE_ON:
        return principal_axes_rotation(positions, true);
    case CameraAlignment::EDGE_ON:
        return principal_axes_rotation(positions, false);
    case CameraAlignment::TOP:    dirvec = QVector3D(0.0f, 0.0f, 1.0f); break;
    case CameraAlignment::BOTTOM: dirvec = QVector3D(0.0f, 0.0f, -1.0f); break;
    case CameraAlignment::LEFT:   dirvec = QVector3D(-1.0f, 0.0f, 0.0f); break;
    case CameraAlignment::RIGHT:  dirvec = QVector3D(1.0f, 0.0f, 0.0f); break;
    case CameraAlignment::FRONT:  dirvec = QVector3D(0.0f, 1.0f, 0.0f); break;
    case CameraAlignment::BACK:   dirvec = QVector3D(0.0f, -1.0f, 0.0f); break;
    }

    QVector3D axis;
    float angle;

    // avoid gimbal locking
    if(std::fabs(dirvec[1]) > .999) {
        axis = QVector3D(0.0, 0.0, 1.0);
        angle = dirvec[1] < 0.0 ? -M_PI : 0.0;
    } else {
        axis = QVector3D::crossProduct(QVector3D(0.0, 1.0, 0.0), dirvec);
        angle = std::acos(dirvec[1]);
    }

    rotation.rotate(qRadiansToDegrees(angle), axis);
    return rotation;
}

QMatrix4x4 Scene::principal_axes_rotation(const std::vector<QVector3D>& positions, bool face_on) {
    if(positions.size() < 2) {
        return alignment_rotation(CameraAlignment::DEFAULT);
    }

    QVector3D ctr(0.0f, 0.0f, 0.0f);
    for(const auto& p : positions) {
        ctr += p;
    }
    ctr /= (float)positions.size();

    double cov[3][3] = {};
    for(const auto& p : positions) {
        const QVector3D d = p - ctr;
        for(int i = 0; i < 3; ++i) {
            for(int j = 0; j < 3; ++j) {
                cov[i][j] += (double)d[i] * d[j];
            }
        }
    }

    const auto axes = symmetric_eigenvectors(cov);

    // rows of the rotation matrix are the model-frame axes that end up
    // along screen-right (x), viewing direction (y) and screen-up (z)
    QVector3D right = axes[0];
    const QVector3D depth = face_on ? axes[2] : axes[1];
    const QVector3D up = face_on ? axes[1] : axes[2];
    if(QVector3D::dotProduct(QVector3D::crossProduct(right, depth), up) < 0.0f) {
        right = -right;     // keep a proper rotation (determinant +1)
    }

    return QMatrix4x4(right.x(), right.y(), right.z(), 0.0f,
                      depth.x(), depth.y(), depth.z(), 0.0f,
                      up.x(),    up.y(),    up.z(),    0.0f,
                      0.0f,      0.0f,      0.0f,      1.0f);
}

/**
 * @brief Rotate scene around z-axis
 * @param angle
 */
void Scene::rotate_z(float angle) {
    this->rotation_matrix.rotate(angle, QVector3D(0,0,1));
}

/**
 * @brief       calculate a ray originating based on mouse position and current view
 *
 * @param       mouse position
 * @param       pointer to vector holding ray origin
 * @param       pointer to vector holding ray direction
 * @return      void
 */
void Scene::calculate_ray(const QPoint& mouse_position, QVector3D* ray_origin, QVector3D* ray_direction) {
    const float screen_width = (float)this->canvas_width;
    const float screen_height = (float)this->canvas_height;

    const QVector3D ray_nds = QVector3D((2.0f * (float)mouse_position.x()) / screen_width - 1.0f,
                                         1.0f - (2.0f * (float)mouse_position.y()) / screen_height,
                                         1.0);

    if(this->camera_mode == CameraMode::ORTHOGRAPHIC) {
        const QVector4D ray_clip(ray_nds[0], ray_nds[1], 0.0, 1.0);

        // the position on the 'camera screen' determines the origin of the
        // ray vector in orthographic projection
        QVector4D ray_eye = this->projection.inverted() * ray_clip;
        ray_eye = QVector4D(ray_eye[0], ray_eye[1], 0.0, 0.0);
        *ray_origin = this->camera_position + (this->view.inverted() * ray_eye).toVector3D();

        // if the projection is orthographic, the ray vector is the same
        // as the view direction of the camera (in world space)
        *ray_direction = -this->camera_position.normalized();
    } else if(this->camera_mode == CameraMode::PERSPECTIVE) {
        const QVector4D ray_clip(ray_nds[0], ray_nds[1], -1.0, 1.0);

        QVector4D ray_eye = this->projection.inverted() * ray_clip;
        ray_eye = QVector4D(ray_eye[0], ray_eye[1], -1.0, 0.0);
        *ray_direction = (this->view.inverted() * ray_eye).toVector3D().normalized();

        // the origin of the ray in perspective projection is simply the position
        // of the camera in world space
        *ray_origin = this->camera_position;
    } else {
        throw std::logic_error("Invalid camera mode");
    }
}

/**
 * @brief      Calculates the point of intersection of a ray with a plane
 *
 * @param[in]  ray_origin    The ray origin
 * @param[in]  ray_vector    The ray vector
 * @param[in]  plane_origin  The plane origin
 * @param[in]  plane_normal  The plane normal
 *
 * @return     The ray plane intersection.
 */
QVector3D Scene::calculate_ray_plane_intersection(const QVector3D& ray_origin,
                                                  const QVector3D& ray_vector,
                                                  const QVector3D& plane_origin,
                                                  const QVector3D& plane_normal) {

    float dotprod = QVector3D::dotProduct(ray_vector, plane_normal);

    if(std::fabs(dotprod) < 0.001) {
        return QVector3D(-1, -1, -1);
    } else {
        float t = QVector3D::dotProduct(plane_origin - ray_origin, plane_normal) / dotprod;
        return ray_origin + t * ray_vector;
    }
}
