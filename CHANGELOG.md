# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

## [Unreleased]

Initial version (0.1.0), tested with PyQInt 1.4.3, Python 3.12 and uv 0.12.20.

### Added
- Added a Qt 6 desktop application for setting up, running and analysing PyQInt Hartree-Fock calculations without writing Python
- Added a managed Python environment: on first launch the bundled uv downloads a standalone Python interpreter and installs the tested PyQInt version into a private per-user folder, leaving other Python installations untouched
- Added an environment dialog (Python → Manage environment) to install, repair, reset, return to the tested PyQInt version, upgrade to the latest PyQInt release, or use a custom Python interpreter
- Added restricted and unrestricted Hartree-Fock single-point calculations, geometry optimizations and Foster-Boys localization, with the STO-3G, STO-6G, 3-21G, 6-31G and aug-cc-pVXZ basis sets
- Added validation of charge, multiplicity and method before a calculation is started, with automatic selection of a compatible spin state and plain-language explanations
- Added a stand-alone job script (`job.py`) per calculation that uses only the public PyQInt API, together with `result.json`, `output.log` and the molecule in a dedicated job folder
- Added live SCF convergence and geometry-optimization energy plots, and cancellation of running calculations
- Added a molecule library with the molecules shipped with PyQInt, and loading and saving of `.xyz` files
- Added 3D molecular-orbital isosurfaces evaluated in C++ from the basis set and coefficients, with an automatic isovalue enclosing a chosen fraction of the density, a manual isovalue, adjustable grid resolution and opacity
- Added results tabs for a summary (energies, energy decomposition, frontier orbitals, timings), orbitals (including the largest basis-function contributions), an MO energy-level diagram, Mulliken and Löwdin charges, all intermediate matrices, and the geometry-optimization trajectory
- Added stereoscopic (anaglyph and interlaced) rendering and camera controls derived from Managlyph
- Added File → Save image, and the `--screenshot` and `--results-tab` command-line options for producing documentation images
- Added Windows (NSIS) and Apple Silicon macOS (disk image) packaging with the pinned, checksum-verified uv executable
- Added automatic download of uv next to the executable for builds from source (CMake option `PYQINT_GUI_FETCH_UV`)
- Added a GitHub Actions workflow that builds and tests on Linux, Windows and macOS, runs an environment-installation integration test, and attaches the installers to tagged releases
- Added unit tests for molecule parsing, job validation, script generation, result parsing, orbital evaluation against PyQInt reference values and marching cubes, plus opt-in end-to-end tests

### Fixed
Fixes relative to the renderer adopted from Managlyph:
- Fixed heap corruption on exit caused by the OpenGL context invoking `cleanup()` on an already destroyed viewer widget
- Fixed a crash when the OpenGL context is recreated (for example after re-docking), by releasing all GPU resources on context loss and re-uploading models afterwards
- Fixed stale shader programs being kept after re-initialization because existing programs were never replaced
- Fixed a memory leak on every atom-color lookup
- Fixed an uninitialized variable and data race when determining the largest atom distance in a structure
