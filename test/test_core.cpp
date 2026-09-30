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

#include <cmath>
#include <map>
#include <utility>

#include <QDir>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "calculation/job_result.h"
#include "calculation/job_runner.h"
#include "calculation/population_analysis.h"
#include "calculation/python_environment.h"
#include "config.h"
#include "calculation/job_script_writer.h"
#include "calculation/job_spec.h"
#include "data/molecule.h"
#include "gui/scene.h"
#include "orbitals/marching_cubes.h"
#include "orbitals/orbital_builder.h"
#include "orbitals/scalar_field.h"

namespace {

QString data_file(const QString& name) {
    return QDir(TEST_DATA_DIR).filePath(name);
}

Molecule water() {
    return Molecule::from_xyz_string("3\nwater\n"
                                     "O 0.0 0.0 0.1368220\n"
                                     "H 0.0 0.7697660 -0.5472890\n"
                                     "H 0.0 -0.7697660 -0.5472890\n", "water");
}

/**
 * @brief Check that a mesh is closed, correctly oriented and lies on a sphere
 */
void verify_sphere_mesh(const IsoMesh& mesh, float radius, float tol) {
    QVERIFY(!mesh.empty());
    QCOMPARE(mesh.positions.size(), mesh.normals.size());
    QCOMPARE(mesh.indices.size() % 3, (size_t)0);

    for(size_t i = 0; i < mesh.positions.size(); ++i) {
        const glm::vec3& p = mesh.positions[i];
        QVERIFY2(std::abs(glm::length(p) - radius) < tol, "vertex does not lie on sphere");
        QVERIFY2(glm::dot(mesh.normals[i], p) > 0.0f, "normal does not point outward");
        QVERIFY(std::abs(glm::length(mesh.normals[i]) - 1.0f) < 1e-4f);
    }

    // every edge is shared by exactly two triangles with opposite direction
    std::map<std::pair<uint32_t, uint32_t>, int> edges;
    for(size_t t = 0; t < mesh.indices.size(); t += 3) {
        const uint32_t a = mesh.indices[t], b = mesh.indices[t + 1], c = mesh.indices[t + 2];
        const glm::vec3 face = glm::cross(mesh.positions[b] - mesh.positions[a],
                                          mesh.positions[c] - mesh.positions[a]);
        QVERIFY2(glm::dot(face, mesh.positions[a]) > 0.0f, "triangle is wound clockwise");
        edges[{a, b}]++;
        edges[{b, c}]++;
        edges[{c, a}]++;
    }
    for(const auto& e : edges) {
        QVERIFY2(e.second == 1, "directed edge used more than once");
        QVERIFY2(edges.count({e.first.second, e.first.first}) == 1, "mesh is not closed");
    }
}

} // namespace

class TestCore : public QObject {
    Q_OBJECT

private slots:
    // ------------------------------------------------------------------
    // molecules
    // ------------------------------------------------------------------
    void molecule_parse_xyz() {
        const Molecule mol = water();
        QCOMPARE(mol.get_name(), QString("water"));
        QCOMPARE(mol.size(), (size_t)3);
        QCOMPARE(mol.formula(), QString("H2O"));
        QCOMPARE(mol.nuclear_charge(), 10);
        QVERIFY(std::abs(mol.get_atoms()[1].position.y - 0.7697660) < 1e-12);
    }

    void molecule_formula_hill() {
        Molecule mol("ethanol");
        mol.add_atom("O", glm::dvec3(0.0));
        mol.add_atom("H", glm::dvec3(0.0));
        mol.add_atom("C", glm::dvec3(0.0));
        mol.add_atom("C", glm::dvec3(0.0));
        QCOMPARE(mol.formula(), QString("C2HO"));
    }

    void molecule_formula_html() {
        QCOMPARE(water().formula_html(), QString("H<sub>2</sub>O"));
        const Molecule benzene = Molecule::from_xyz_file(":/assets/molecules/benzene.xyz");
        QCOMPARE(benzene.formula_html(), QString("C<sub>6</sub>H<sub>6</sub>"));
    }

    void molecule_parse_errors() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Molecule::from_xyz_string("", "x"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Molecule::from_xyz_string("2\nx\nH 0 0 0\n", "x"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Molecule::from_xyz_string("1\nx\nXx 0 0 0\n", "x"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, Molecule::from_xyz_string("1\nx\nH 0 a 0\n", "x"));
    }

    void molecule_library_resources() {
        const QStringList files = QDir(":/assets/molecules").entryList({"*.xyz"});
        QVERIFY(files.size() > 10);
        for(const auto& f : files) {
            const Molecule mol = Molecule::from_xyz_file(":/assets/molecules/" + f);
            QVERIFY2(!mol.empty(), qPrintable(f));
        }
    }

    // ------------------------------------------------------------------
    // job specification
    // ------------------------------------------------------------------
    void jobspec_validation() {
        JobSpec spec;
        spec.molecule = water();
        QVERIFY(spec.validate().isEmpty());

        // doublet water cation is fine with UHF but not RHF
        spec.charge = 1;
        spec.multiplicity = 2;
        QVERIFY(!spec.validate().isEmpty());
        spec.method = HFMethod::Unrestricted;
        QVERIFY(spec.validate().isEmpty());

        // parity mismatch
        spec.multiplicity = 1;
        QVERIFY(!spec.validate().isEmpty());

        // Foster-Boys only for RHF
        spec.charge = 0;
        spec.foster_boys = true;
        QVERIFY(!spec.validate().isEmpty());
        spec.method = HFMethod::Restricted;
        QVERIFY(spec.validate().isEmpty());

        // no geometry optimization for a single atom
        spec.molecule = Molecule::from_xyz_string("1\nhe\nHe 0 0 0\n", "he");
        spec.foster_boys = false;
        spec.type = JobType::GeometryOptimization;
        QVERIFY(!spec.validate().isEmpty());
    }

    // ------------------------------------------------------------------
    // script generation
    // ------------------------------------------------------------------
    void script_python_string() {
        QCOMPARE(JobScriptWriter::python_string("abc"), QString("'abc'"));
        QCOMPARE(JobScriptWriter::python_string("it's"), QString("'it\\'s'"));
        QCOMPARE(JobScriptWriter::python_string("a\\b"), QString("'a\\\\b'"));
        QCOMPARE(JobScriptWriter::python_string("a\nb"), QString("'a\\nb'"));
    }

    void script_generation() {
        JobSpec spec;
        spec.molecule = water();
        spec.foster_boys = true;
        QString src = JobScriptWriter::generate(spec);
        QVERIFY(src.contains("HF(mol, 'sto3g').rhf("));
        QVERIFY(src.contains("FosterBoys(res"));
        QVERIFY(src.contains("fosterboys=fb"));
        QVERIFY(!src.contains("GeometryOptimization"));
        QCOMPARE(src.count("mol.add_atom("), 3);

        spec.foster_boys = false;
        spec.method = HFMethod::Unrestricted;
        src = JobScriptWriter::generate(spec);
        QVERIFY(src.contains(".uhf("));
        QVERIFY(src.contains("multiplicity=JOB['multiplicity']"));

        spec.method = HFMethod::Restricted;
        spec.type = JobType::GeometryOptimization;
        src = JobScriptWriter::generate(spec);
        QVERIFY(src.contains("GeometryOptimization(mol, 'sto3g'"));
        QVERIFY(src.contains("geomopt=opt"));
    }

    void script_write_directory() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        JobSpec spec;
        spec.molecule = water();
        QVERIFY(JobScriptWriter::write_job_directory(spec, dir.path()).isEmpty());
        QVERIFY(QFileInfo::exists(dir.filePath("job.py")));
        QVERIFY(QFileInfo::exists(dir.filePath("pyqint_gui_export.py")));
        QVERIFY(QFileInfo::exists(dir.filePath("molecule.xyz")));
    }

    void script_localization() {
        const QString src = JobScriptWriter::generate_localization("result.json", 7, 3);
        QVERIFY(src.contains("from pyqint import FosterBoys"));
        QVERIFY(src.contains("res = load_result('result.json')"));
        QVERIFY(src.contains("FosterBoys(res, seed=7).run(nr_runners=3)"));
        QVERIFY(src.contains("add_localization('result.json', fb"));

        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QVERIFY(!JobScriptWriter::write_localization_script(dir.filePath("result.json"), 42, 1).isEmpty());
        QVERIFY(QFile::copy(data_file("h2o_rhf_fb.json"), dir.filePath("result.json")));
        QVERIFY(JobScriptWriter::write_localization_script(dir.filePath("result.json"), 42, 1).isEmpty());
        QVERIFY(QFileInfo::exists(dir.filePath("localize.py")));
        QVERIFY(QFileInfo::exists(dir.filePath("pyqint_gui_export.py")));
    }

    /**
     * @brief End-to-end test: run a generated script with a real PyQInt
     *
     * Only executed when PYQINT_GUI_TEST_PYTHON points to a Python
     * interpreter with PyQInt installed.
     */
    void script_end_to_end() {
        const QString python = qEnvironmentVariable("PYQINT_GUI_TEST_PYTHON");
        if(python.isEmpty()) {
            QSKIP("PYQINT_GUI_TEST_PYTHON not set");
        }

        QTemporaryDir dir;
        JobSpec spec;
        spec.molecule = water();
        spec.foster_boys = true;
        QVERIFY(JobScriptWriter::write_job_directory(spec, dir.path()).isEmpty());

        QProcess proc;
        proc.setWorkingDirectory(dir.path());
        proc.start(python, {"-u", "job.py"});
        QVERIFY(proc.waitForFinished(120000));
        QVERIFY2(proc.exitCode() == 0, proc.readAllStandardError().constData());

        auto res = JobResult::load(dir.filePath("result.json"));
        QVERIFY(std::abs(res->total_energy() - (-74.9623204342175)) < 1e-6);
        QCOMPARE(res->orbital_sets.size(), (size_t)2);
    }

    /**
     * @brief Localize the orbitals of an existing result with a real PyQInt
     *
     * Only executed when PYQINT_GUI_TEST_PYTHON is set (see script_end_to_end).
     */
    void localization_end_to_end() {
        const QString python = qEnvironmentVariable("PYQINT_GUI_TEST_PYTHON");
        if(python.isEmpty()) {
            QSKIP("PYQINT_GUI_TEST_PYTHON not set");
        }

        QTemporaryDir dir;
        JobSpec spec;
        spec.molecule = water();
        QVERIFY(JobScriptWriter::write_job_directory(spec, dir.path()).isEmpty());

        QProcess proc;
        proc.setWorkingDirectory(dir.path());
        proc.start(python, {"-u", "job.py"});
        QVERIFY(proc.waitForFinished(120000));
        QVERIFY2(proc.exitCode() == 0, proc.readAllStandardError().constData());
        auto before = JobResult::load(dir.filePath("result.json"));
        QCOMPARE(before->orbital_sets.size(), (size_t)1);

        QVERIFY(JobScriptWriter::write_localization_script(dir.filePath("result.json"), 42, 1).isEmpty());
        proc.start(python, {"-u", "localize.py"});
        QVERIFY(proc.waitForFinished(120000));
        QVERIFY2(proc.exitCode() == 0, proc.readAllStandardError().constData());

        auto after = JobResult::load(dir.filePath("result.json"));
        QCOMPARE(after->orbital_sets.size(), (size_t)2);
        QVERIFY(after->localization.present);
        QVERIFY(after->job["foster_boys"].toBool());
        QCOMPARE(after->total_energy(), before->total_energy());

        // the sum over the occupied orbitals is invariant under localization
        using namespace PopulationAnalysis;
        const double canonical = evaluate(*after, 0, 0, 1, Kind::Hamilton).occupied_sum;
        const double localized = evaluate(*after, 1, 0, 1, Kind::Hamilton).occupied_sum;
        QVERIFY(std::abs(canonical - localized) < 1e-8);
    }

    /**
     * @brief Full workflow: bootstrap the managed environment with uv and
     *        run a job through JobRunner
     *
     * Downloads Python and PyQInt, hence only executed when
     * PYQINT_GUI_TEST_UV points to a uv executable.
     */
    void environment_install_and_run() {
        const QString uv = qEnvironmentVariable("PYQINT_GUI_TEST_UV");
        if(uv.isEmpty()) {
            QSKIP("PYQINT_GUI_TEST_UV not set");
        }

        QStandardPaths::setTestModeEnabled(true);
        QDir(PythonEnvironment::root_directory()).removeRecursively();
        QSettings().setValue("python/uv_path", uv);
        PythonEnvironment::set_interpreter_override(QString());

        PythonEnvironment env;

        // nothing installed yet
        QSignalSpy check_spy(&env, &PythonEnvironment::finished);
        env.check();
        QCOMPARE(env.get_state(), PythonEnvironment::State::Missing);

        // install
        QSignalSpy install_spy(&env, &PythonEnvironment::finished);
        env.install();
        QVERIFY(install_spy.wait(600000));
        QVERIFY2(install_spy.front().at(0).toBool(), qPrintable(install_spy.front().at(1).toString()));
        QCOMPARE(env.get_state(), PythonEnvironment::State::Ready);
        QCOMPARE(env.get_pyqint_version(), QString(PYQINT_PINNED_VERSION));
        QVERIFY(env.get_python_version().startsWith(PYTHON_MANAGED_VERSION));

        // the virtual environment must be based on the standalone Python
        // downloaded by uv, not on a Python installation of the system
        QVERIFY(QFileInfo(env.python_executable()).absoluteFilePath().startsWith(PythonEnvironment::root_directory()));
        const QDir interpreters(QDir(PythonEnvironment::root_directory()).filePath("interpreters"));
        QVERIFY2(!interpreters.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty(), "no managed interpreter");
        QFile cfg(QDir(PythonEnvironment::env_directory()).filePath("pyvenv.cfg"));
        QVERIFY(cfg.open(QIODevice::ReadOnly));
        const QString cfgtext = QString::fromUtf8(cfg.readAll());
        QVERIFY2(cfgtext.contains(interpreters.absolutePath()), qPrintable(cfgtext));

        // run a UHF job on the water cation through the job runner
        JobRunner runner(&env);
        JobSpec spec;
        spec.molecule = water();
        spec.charge = 1;
        spec.multiplicity = 2;
        spec.method = HFMethod::Unrestricted;
        QSignalSpy iter_spy(&runner, &JobRunner::scf_iteration);
        QSignalSpy job_spy(&runner, &JobRunner::finished);
        QVERIFY(runner.start(spec).isEmpty());
        QVERIFY(job_spy.wait(120000));
        QVERIFY2(job_spy.front().at(0).toBool(), qPrintable(job_spy.front().at(1).toString()));
        QVERIFY(iter_spy.size() > 3);

        auto res = JobResult::load(job_spy.front().at(2).toString());
        QVERIFY(res->is_unrestricted());
        QCOMPARE(res->nelec, 9);
        QCOMPARE(res->nalpha, 5);
        QCOMPARE(res->nbeta, 4);
        QVERIFY(QFileInfo::exists(QDir(runner.get_job_directory()).filePath("output.log")));

        QDir(PythonEnvironment::root_directory()).removeRecursively();
        QDir(JobRunner::jobs_directory()).removeRecursively();
    }

    // ------------------------------------------------------------------
    // result parsing
    // ------------------------------------------------------------------
    void result_rhf() {
        auto res = JobResult::load(data_file("h2o_rhf_fb.json"));
        QCOMPARE(res->method, QString("rhf"));
        QCOMPARE(res->nelec, 10);
        QCOMPARE(res->positions.size(), (size_t)3);
        QCOMPARE(res->basis->size(), (size_t)7);
        QVERIFY(std::abs(res->total_energy() - (-74.9623204342175)) < 1e-10);

        QCOMPARE(res->orbital_sets.size(), (size_t)2);
        QCOMPARE(res->orbital_sets[0].label, QString("Canonical"));
        QCOMPARE(res->orbital_sets[0].homo(), 4);
        QCOMPARE(res->orbital_sets[1].label, QString("Foster-Boys"));
        QVERIFY(res->localization.present);

        // neutral molecule: charges sum to zero
        double qm = 0.0, ql = 0.0;
        for(double q : res->mulliken) qm += q;
        for(double q : res->lowdin) ql += q;
        QVERIFY(std::abs(qm) < 1e-8);
        QVERIFY(std::abs(ql) < 1e-8);

        QVERIFY(res->matrices.size() >= 6);
        QCOMPARE(res->matrices.front().first, QString("overlap"));
        const DenseMatrix& S = res->matrices.front().second;
        QCOMPARE(S.rows, 7);
        for(int i = 0; i < 7; ++i) {
            QVERIFY(std::abs(S(i, i) - 1.0) < 1e-6);
        }

        QVERIFY(!res->optimization);
    }

    void result_uhf() {
        auto res = JobResult::load(data_file("o2_uhf.json"));
        QVERIFY(res->is_unrestricted());
        QCOMPARE(res->multiplicity, 3);
        QCOMPARE(res->nalpha, 9);
        QCOMPARE(res->nbeta, 7);
        QCOMPARE(res->orbital_sets.size(), (size_t)2);
        QCOMPARE(res->orbital_sets[0].spin, QString("alpha"));
        QCOMPARE(res->orbital_sets[0].homo(), 8);
        QCOMPARE(res->orbital_sets[1].homo(), 6);
    }

    void result_geomopt() {
        auto res = JobResult::load(data_file("h2o_geomopt.json"));
        QVERIFY(res->optimization != nullptr);
        QVERIFY(res->optimization->frames.size() > 2);
        QCOMPARE(res->optimization->frames.size(), res->optimization->energies.size());
        // optimization lowers the energy
        QVERIFY(res->optimization->energies.back() < res->optimization->energies.front());
    }

    void result_errors() {
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, JobResult::parse("not json"));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, JobResult::parse("{\"schema\": 99}"));
    }

    // ------------------------------------------------------------------
    // orbital evaluation (reference values computed with PyQInt 1.4.3)
    // ------------------------------------------------------------------
    void basis_functions_match_pyqint() {
        auto res = JobResult::load(data_file("h2o_rhf_fb.json"));

        const std::vector<std::pair<glm::dvec3, std::vector<double>>> reference = {
            {{0.3, -0.2, 0.5}, {4.273799176987582e-01, 3.891130251512668e-01, 4.950376301549634e-01,
                                -3.300250867699756e-01, 3.984127380390816e-01, 4.536241963068757e-02,
                                6.418954439821872e-02}},
            {{-0.5, 0.1, 0.0}, {1.577040578294883e-01, 3.857203883009980e-01, -5.947967758066635e-01,
                                1.189593551613327e-01, -3.075766564572279e-01, 8.642397596189072e-02,
                                7.044264950321390e-02}},
        };

        for(const auto& ref : reference) {
            for(size_t i = 0; i < res->basis->size(); ++i) {
                const double v = BasisSet::evaluate_function((*res->basis)[i], ref.first);
                QVERIFY2(std::abs(v - ref.second[i]) < 1e-12, qPrintable(QString("basis function %1").arg(i)));
            }
        }

        // molecular orbitals: HOMO and LUMO
        const auto& set = res->orbital_sets[0];
        QVERIFY(std::abs(res->basis->evaluate({0.3, -0.2, 0.5}, set.coefficients[4]) - (-4.950376301549570e-01)) < 1e-12);
        QVERIFY(std::abs(res->basis->evaluate({0.3, -0.2, 0.5}, set.coefficients[5]) - 1.487280998491095e-01) < 1e-12);
        QVERIFY(std::abs(res->basis->evaluate({-0.5, 0.1, 0.0}, set.coefficients[5]) - (-3.934128678474843e-01)) < 1e-12);
    }

    void orbital_normalization_on_grid() {
        auto res = JobResult::load(data_file("h2o_rhf_fb.json"));
        const Molecule mol = res->get_molecule();
        glm::vec3 offset;
        auto structure = mol.to_structure(&offset);

        std::vector<glm::vec3> atoms;
        for(const auto& a : mol.get_atoms()) {
            atoms.push_back(glm::vec3(a.position) - offset);
        }

        OrbitalBuilder builder(res->basis, atoms, offset);
        OrbitalGridSettings settings;
        settings.padding = 4.0f;
        settings.spacing = 0.05f;

        // HOMO (O 2p lone pair) integrates to one
        auto field = builder.build_field(res->orbital_sets[0].coefficients[4], settings);
        double sum = 0.0;
        for(float v : field->data()) {
            sum += (double)v * v;
        }
        const double dv = std::pow(settings.spacing * 1.8897259886, 3);
        QVERIFY2(std::abs(sum * dv - 1.0) < 1e-3, qPrintable(QString("norm = %1").arg(sum * dv)));

        // suggested isovalue encloses the requested fraction
        const float iso = OrbitalBuilder::suggest_isovalue(*field, 0.9f);
        QVERIFY(iso > 0.0f && iso < field->max_abs());

        auto meshes = OrbitalBuilder::build_meshes(*field, iso);
        QVERIFY(!meshes.positive.empty());
        QVERIFY(!meshes.negative.empty());
    }

    // ------------------------------------------------------------------
    // marching cubes
    // ------------------------------------------------------------------
    void marching_cubes_sphere() {
        const float h = 0.05f;
        const unsigned int n = 61;
        ScalarField field(glm::vec3(-1.5f), h, {n, n, n});
        for(unsigned int k = 0; k < n; ++k) {
            for(unsigned int j = 0; j < n; ++j) {
                for(unsigned int i = 0; i < n; ++i) {
                    const glm::vec3 p = field.position(i, j, k);
                    field.set(i, j, k, 1.0f - glm::dot(p, p));
                }
            }
        }

        // positive isovalue: region f > 0.5 is a sphere with radius sqrt(0.5)
        verify_sphere_mesh(marching_cubes(field, 0.5f), std::sqrt(0.5f), 0.01f);

        // negative isovalue on the negated field gives the same sphere
        for(auto& v : field.data()) {
            v = -v;
        }
        verify_sphere_mesh(marching_cubes(field, -0.5f), std::sqrt(0.5f), 0.01f);
    }

    void sphere_mesh_closed() {
        verify_sphere_mesh(sphere_mesh(glm::vec3(0.0f), 1.3f), 1.3f, 1e-4f);
        verify_sphere_mesh(sphere_mesh(glm::vec3(0.0f), 0.5f, 2, 3), 0.5f, 1e-4f);
    }

    // ------------------------------------------------------------------
    // population analysis (reference values: pyqint.PopulationAnalysis of
    // PyQInt 1.4.3, applied to the same result file)
    // ------------------------------------------------------------------
    void population_analysis_matches_pyqint() {
        using namespace PopulationAnalysis;
        auto res = JobResult::load(data_file("h2o_rhf_fb.json"));

        const std::vector<std::pair<Kind, std::vector<double>>> reference = {
            {Kind::Hamilton, {0.010564272477614252, -0.17690957386029055, -0.20225887625011651,
                              0.06465670972825072, 0.0, 1.2546056460312225, 0.5292282637842698}},
            {Kind::Overlap, {-0.0005850653331922144, 0.1164840014884288, 0.15362552584584527,
                             -0.026278002411380414, 0.0, -0.7466838433755053, -0.4019747949939212}},
            {Kind::BondIndex, {0.00017394459037625858, -0.000515505309857355, 0.31325173518340066,
                               0.2513698309347904, 0.0, -0.492174775870477, -0.8196509098246463}},
        };
        for(const auto& ref : reference) {
            const PairPopulation pop = evaluate(*res, 0, 0, 1, ref.first);
            QCOMPARE(pop.values.size(), ref.second.size());
            QCOMPARE(pop.spin_factor, 2.0);
            double occ = 0.0;
            for(size_t k = 0; k < ref.second.size(); ++k) {
                QVERIFY2(std::abs(pop.values[k] - ref.second[k]) < 1e-10, qPrintable(short_name(ref.first)));
                if(k < 5) {
                    occ += ref.second[k];
                }
            }
            QVERIFY(std::abs(pop.occupied_sum - occ) < 1e-10);
        }

        // O-H bonding: negative MOHP, positive MOOP summed over occupied orbitals
        QVERIFY(evaluate(*res, 0, 0, 1, Kind::Hamilton).occupied_sum < 0.0);
        QVERIFY(evaluate(*res, 0, 0, 1, Kind::Overlap).occupied_sum > 0.0);

        // symmetric in the two atoms, and invariant under localization
        for(Kind k : {Kind::Hamilton, Kind::Overlap, Kind::BondIndex}) {
            const PairPopulation ab = evaluate(*res, 0, 0, 1, k);
            const PairPopulation ba = evaluate(*res, 0, 1, 0, k);
            const PairPopulation fb = evaluate(*res, 1, 0, 1, k);
            for(size_t i = 0; i < ab.values.size(); ++i) {
                QVERIFY(std::abs(ab.values[i] - ba.values[i]) < 1e-12);
            }
            QVERIFY(std::abs(ab.occupied_sum - fb.occupied_sum) < 1e-8);
        }

        QVERIFY_THROWS_EXCEPTION(std::runtime_error, evaluate(*res, 0, 1, 1, Kind::Hamilton));
        QVERIFY_THROWS_EXCEPTION(std::runtime_error, evaluate(*res, 0, 0, 3, Kind::Hamilton));
        QCOMPARE(basis_functions_on_atom(*res->basis, 0).size(), (size_t)5);   // O: 1s 2s 2p
        QCOMPARE(basis_functions_on_atom(*res->basis, 1).size(), (size_t)1);   // H: 1s
    }

    void population_analysis_uhf() {
        using namespace PopulationAnalysis;
        auto res = JobResult::load(data_file("o2_uhf.json"));
        const PairPopulation alpha = evaluate(*res, 0, 0, 1, Kind::Hamilton);
        const PairPopulation beta = evaluate(*res, 1, 0, 1, Kind::Hamilton);
        QCOMPARE(alpha.matrix_key, QString("fock_alpha"));
        QCOMPARE(beta.matrix_key, QString("fock_beta"));
        QCOMPARE(alpha.spin_factor, 1.0);
        QVERIFY(alpha.occupied_sum + beta.occupied_sum < 0.0);     // O2 is bound
        QCOMPARE(evaluate(*res, 0, 0, 1, Kind::BondIndex).matrix_key, QString("density_alpha"));
    }

    void orbital_frontier_labels() {
        auto res = JobResult::load(data_file("h2o_rhf_fb.json"));
        const OrbitalSet& set = res->orbital_sets[0];
        QCOMPARE(set.frontier_label(4), QString("HOMO"));
        QCOMPARE(set.frontier_label(5), QString("LUMO"));
        QCOMPARE(set.frontier_label(2), QString("HOMO-2"));
        QCOMPARE(set.frontier_label(6), QString("LUMO+1"));
        QCOMPARE(res->find_orbital_set("Foster-Boys"), 1);
        QCOMPARE(res->find_orbital_set("nope"), -1);
        QVERIFY(res->find_matrix("fock") != nullptr);
        QVERIFY(res->find_matrix("fock_alpha") == nullptr);
    }

    // ------------------------------------------------------------------
    // camera orientation
    // ------------------------------------------------------------------
    void principal_axes_orientation() {
        // planar "molecule" in a tilted plane with normal n; widest along a
        const QVector3D n = QVector3D(1.0f, 1.0f, 1.0f).normalized();
        const QVector3D a = QVector3D(1.0f, -1.0f, 0.0f).normalized();
        const QVector3D b = QVector3D::crossProduct(n, a);
        std::vector<QVector3D> points;
        for(int i = 0; i < 6; ++i) {
            const float phi = (float)i * (float)M_PI / 3.0f;
            points.push_back(3.0f * std::cos(phi) * a + 1.5f * std::sin(phi) * b);
        }

        // camera looks along +y with +z up: face-on puts the normal along y
        const QMatrix4x4 face = Scene::principal_axes_rotation(points, true);
        QVERIFY(std::abs(std::abs(face.mapVector(n).y()) - 1.0f) < 1e-4f);
        QVERIFY(std::abs(std::abs(face.mapVector(a).x()) - 1.0f) < 1e-4f);

        // edge-on puts the normal along the vertical axis of the screen
        const QMatrix4x4 edge = Scene::principal_axes_rotation(points, false);
        QVERIFY(std::abs(std::abs(edge.mapVector(n).z()) - 1.0f) < 1e-4f);

        // proper rotations
        for(const QMatrix4x4& m : {face, edge}) {
            QVERIFY(std::abs(m.determinant() - 1.0) < 1e-4);
        }

        // fixed alignments: TOP looks down the z-axis
        const QMatrix4x4 top = Scene::alignment_rotation(CameraAlignment::TOP);
        QVERIFY(std::abs(std::abs(top.mapVector(QVector3D(0, 0, 1)).y()) - 1.0f) < 1e-4f);
    }

    void marching_cubes_empty() {
        ScalarField field(glm::vec3(0.0f), 0.1f, {10, 10, 10});
        QVERIFY(marching_cubes(field, 0.5f).empty());
    }
};

QTEST_MAIN(TestCore)
#include "test_core.moc"
