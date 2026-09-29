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

#include <QApplication>
#include <QCommandLineParser>
#include <QSurfaceFormat>
#include <QTimer>

#include "config.h"
#include "gui/main_window.h"

int main(int argc, char* argv[]) {
    QCoreApplication::setOrganizationName("IMC");
    QCoreApplication::setOrganizationDomain("tue.nl");
    QCoreApplication::setApplicationName(PROGRAM_NAME);
    QCoreApplication::setApplicationVersion(PROGRAM_VERSION);

    // OpenGL 3.3 core profile (required on macOS)
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSwapBehavior(QSurfaceFormat::DoubleBuffer);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(argc, argv);

    QCommandLineParser parser;
    parser.setApplicationDescription("Graphical user interface for the PyQInt Hartree-Fock program");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", "Molecule (.xyz) or result (.json) to open");
    QCommandLineOption opt_screenshot("screenshot",
        "Render the main window to an image file and quit (used for documentation).", "image");
    parser.addOption(opt_screenshot);
    QCommandLineOption opt_tab("results-tab",
        "Tab of the results panel to show (summary, orbitals, diagram, charges, matrices, optimization).", "tab");
    parser.addOption(opt_tab);
    parser.process(app);

    MainWindow window;
    window.show();

    const QStringList files = parser.positionalArguments();
    if(!files.isEmpty()) {
        const QString file = files.front();
        const QString tab = parser.value(opt_tab);
        QTimer::singleShot(250, &window, [&window, file, tab]() {
            window.open_file(file);
            if(!tab.isEmpty()) {
                window.show_results_tab(tab);
            }
        });
    }

    if(parser.isSet(opt_screenshot)) {
        const QString image = parser.value(opt_screenshot);
        QTimer::singleShot(4000, &window, [&window, image]() {
            window.grab().save(image);
            QApplication::quit();
        });
    }

    return app.exec();
}
