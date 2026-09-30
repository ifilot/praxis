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

#include <QString>

class QTextStream;

#include "job_spec.h"

/**
 * @brief Generates a stand-alone Python script for a job
 *
 * The script only uses the public PyQInt (Hartree-Fock) or PyDFT (density
 * functional theory) API plus the small helper module
 * pyqint_gui_export.py (copied next to the script) for writing the JSON
 * result file and reporting progress. Students can open the script to see
 * exactly which PyQInt calls were made.
 */
class JobScriptWriter {
public:
    static constexpr const char* RESULT_FILENAME = "result.json";
    static constexpr const char* SCRIPT_FILENAME = "job.py";
    static constexpr const char* LOCALIZATION_SCRIPT_FILENAME = "localize.py";
    static constexpr const char* HELPER_FILENAME = "pyqint_gui_export.py";
    static constexpr const char* HELPER_RESOURCE = ":/assets/python/pyqint_gui_export.py";

    /**
     * @brief Generate the Python source of the job script
     */
    static QString generate(const JobSpec& spec);

    /**
     * @brief Write job.py and the helper module into a directory
     *
     * @return empty string on success, otherwise an error message
     */
    static QString write_job_directory(const JobSpec& spec, const QString& directory);

    /**
     * @brief Generate a script that adds Foster-Boys localized orbitals to an
     *        existing result file, without repeating the SCF calculation
     *
     * @param result_filename  name of the result file (in the same folder)
     */
    static QString generate_localization(const QString& result_filename, int seed, int runners);

    /**
     * @brief Write localize.py and the helper module next to a result file
     *
     * @return empty string on success, otherwise an error message
     */
    static QString write_localization_script(const QString& result_file, int seed, int runners);

    /**
     * @brief Quote a string as a Python string literal
     */
    static QString python_string(const QString& str);

private:
    static QString generate_dft(const JobSpec& spec);
    static void write_header(QTextStream& out, const JobSpec& spec, const QString& program);
    static void write_molecule(QTextStream& out, const JobSpec& spec);
    static QString python_float(double v);
    static QString write_helper(const QString& directory);
};
