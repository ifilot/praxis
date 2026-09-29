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

#ifndef FRAME_H
#define FRAME_H

#include <array>
#include <memory>
#include <optional>
#include <vector>
#include <string>

#include "structure.h"
#include "model.h"

/**
 * @brief The Frame class
 */
class Frame
{
private:
    std::shared_ptr<Structure> structure;
    std::vector<std::shared_ptr<Model>> models;
    std::string description;
    std::optional<std::array<float, 9>> unit_cell;

public:
    /**
     * @brief Frame constructor
     * @param atomic structure
     * @param description of the frame
     */
    Frame(const std::shared_ptr<Structure>& struc,
          const std::string& description);

    /**
     * @brief Get the frame description
     * @return frame description
     */
    inline const auto& get_description() const {
        return this->description;
    }

    /**
     * @brief Add a model (e.g. molecular orbital) to the frame
     * @param model
     */
    inline void add_model(const std::shared_ptr<Model>& model) {
        this->models.push_back(model);
    }

    /**
     * @brief Get the structure from the frame
     * @return structure pointer
     */
    inline std::shared_ptr<Structure> get_structure() const {
        return this->structure;
    }

    /**
     * @brief Get all models
     * @return
     */
    inline const auto& get_models() const {
        return this->models;
    }

    /**
     * @brief Set optional unit-cell matrix for this frame
     * @param frame_unit_cell row-major 3x3 matrix
     */
    inline void set_unit_cell(const std::optional<std::array<float, 9>>& frame_unit_cell) {
        this->unit_cell = frame_unit_cell;
    }

    /**
     * @brief Get optional unit-cell matrix for this frame
     * @return optional row-major 3x3 matrix
     */
    inline const auto& get_unit_cell() const {
        return this->unit_cell;
    }
};

#endif // FRAME_H
