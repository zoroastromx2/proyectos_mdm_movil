# AGENTS.md

Desktop GIS utility (C++17 / Qt 6.8.3 QML) that manages GeoPackage files via GDAL/OGR and generates QGIS 3.40 `.qgz` project files.

## Environment prerequisites

| Requirement | Notes |
|---|---|
| MSVC 2022 64-bit | Only tested compiler |
| Qt 6.8.3 | Core, Gui, Quick, Qml, QuickControls2 |
| CMake ≥ 3.21 | |
| Ninja | Hardcoded to `D:/Strawberry/c/bin/ninja.exe` in Qt Creator; override with `-DCMAKE_MAKE_PROGRAM=` on other machines |
| vcpkg | `VCPKG_ROOT` env var must be set; manages GDAL, libzip, sqlite3 automatically |
| GDAL runtime | `GDAL_DATA`, `GDAL_DRIVER_PATH`, `PROJ_LIB` must be set at runtime or the GPKG driver will not load (app runs but `.qgz` generation silently fails) |

Use the OSGeo4W Python at `E:\OSGeo4W\apps\Python312\python.exe` — the system Python has no `osgeo` module.

## Build commands

```bat
# Configure (Debug)
cmake -S . -B build\Debug -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_PREFIX_PATH=<Qt6_install_dir> ^
  -DCMAKE_GENERATOR=Ninja

# Build
cmake --build build\Debug

# Run
build\Debug\proyectos_mdm_movil.exe
```

Qt Creator names its build dirs `build\Desktop_Qt_6_8_3_MSVC2022_64bit_Debug\` etc. There is no `CMakePresets.json`; `cmake --preset` does not work.

## No tests, no CI, no linter

There are zero automated tests, no CI pipeline, no `.clang-format`, no pre-commit hooks. Manual testing only: run the app and exercise the two panels.

## Architecture

- Single `CMakeLists.txt` at root — no subdirectory CMake files.
- `src/geomanager.h/.cpp` — GeoPackage CRUD via GDAL/OGR, exposed to QML as `QML_ELEMENT`.
- `src/qgisprojectgenerator.h/.cpp` — `.qgz` generation (ZIP + QGIS 3.40 XML), exposed to QML.
- `qml/main.qml` — root `ApplicationWindow`; injects C++ objects into panels as `required property`.
- QML module URI is `App` 1.0; the main QML resource path is `"qrc:/App/qml/main.qml"` — if the URI changes, this path must change too.
- `main.cpp` includes both backend headers only to force linking of auto-generated QML registration TUs. **Do not remove these includes.**

## Key conventions

- **File paths from QML `FileDialog`** arrive as `file:///` URIs. Always call `GeoManager.urlToPath()` or `urlListToPaths()` before passing to GDAL — GDAL cannot open `file:///` URIs.
- **Error pattern:** operations return `bool`; on failure set `lastError` string and reset `busy` to false.
- **`GDALAllRegister()`** is called in both the `GeoManager` constructor and inside `buildQgsXml()`. The second call is intentional (diagnostic block) and harmless.
- `buildQgsXml()` contains extensive `qInfo()`/`qWarning()` diagnostic logging (GDAL version, driver status, env vars, test open). This is live production code, not leftover debug — do not remove without understanding the context.
- `.qgz` datasource references use **absolute paths** (`<gpkgPath>|layername=<name>`). Moving the `.gpkg` or `.qgz` after generation breaks QGIS layer references.
- Layer IDs are `<name>_<16 random hex chars>` — not stable across runs.
- All GDAL operations run **synchronously on the UI thread**; long operations will freeze the UI. The `busy` property signals blocking but does not run anything async.

## vcpkg / DLL deployment

vcpkg installs dependencies into `vcpkg_installed/` (git-ignored). Do not manually add DLLs. A post-build step copies vcpkg runtime DLLs and runs `windeployqt --qmldir qml --no-translations` automatically.

## Misc

- `README.md` is essentially empty; this file is the real documentation.
- The app name is spelled **"Provectos"** (with a P) in `setApplicationName` and `vcpkg.json`. The repo folder uses "proyectos". Both spellings are intentional.
- `.qtcreator/CMakeLists.txt.user` contains machine-specific paths; do not commit it.
