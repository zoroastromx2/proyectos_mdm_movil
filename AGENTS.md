# AGENTS.md

Desktop GIS utility (C++17 / Qt 6.8.3 QML) that manages GeoPackage files via GDAL/OGR and generates QGIS 3.40 `.qgz` project files. Licensed GPL v3.

## Environment prerequisites

| Requirement | Notes |
|---|---|
| MSVC 2022 64-bit | Only tested compiler |
| Qt ≥ 6.5 (tested on 6.8.3) | Core, Gui, Quick, Qml, QuickControls2; Material style hardcoded |
| CMake ≥ 3.21 | |
| Ninja | Hardcoded to `D:/Strawberry/c/bin/ninja.exe` in Qt Creator; override with `-DCMAKE_MAKE_PROGRAM=` on other machines |
| vcpkg | `VCPKG_ROOT` env var must be set; `vcpkg.json` enables GDAL with `tools` + `libspatialite` features, plus libzip and sqlite3 |
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
- `src/geomanager.h/.cpp` — GeoPackage CRUD via GDAL/OGR, registered to QML as `QML_ELEMENT`.
- `src/qgisprojectgenerator.h/.cpp` — `.qgz` generation (ZIP + QGIS 3.40 XML), registered to QML as `QML_ELEMENT`.
- `qml/main.qml` — root `ApplicationWindow`; instantiates both C++ types inline (`GeoManager { id: geoManager }`) and passes them to panels via property binding.
- `qml/GeoPackagePanel.qml` — CRUD UI (~342 lines); receives `required property var geoMgr` (weakly typed).
- `qml/QgisGeneratorPanel.qml` — generator UI (~93 lines); receives `required property GeoManager geoMgr` and `required property QgisProjectGenerator qgisMgr` (strongly typed — must `import App 1.0`).
- QML module URI is `App` 1.0; the main QML resource path is `"qrc:/App/qml/main.qml"` — if the URI changes, this path must change too.
- `main.cpp` includes both backend headers only to force linking of auto-generated QML registration TUs. **Do not remove these includes.**

## Key conventions

- **File paths from QML `FileDialog`** arrive as `file:///` URIs. Always call `GeoManager.urlToPath()` or `urlListToPaths()` before passing to GDAL — GDAL cannot open `file:///` URIs.
- **Error pattern:** operations return `bool`; on failure set `lastError` string and reset `busy` to false.
- **`GDALAllRegister()`** is called in both the `GeoManager` constructor and inside `buildQgsXml()`. The second call is intentional (diagnostic block) and harmless.
- `buildQgsXml()` opens the GeoPackage **twice**: once as a diagnostic test (`testDs`), closes it, then opens it again for real work. The extensive `qInfo()`/`qWarning()` logging in that function is live production code — do not remove without understanding the context.
- **Layer name from SHP base name:** `createGeoPackage` and `addLayers` derive the GPKG layer name from `QFileInfo::baseName()`. Two SHPs with the same base name will conflict silently.
- **`createGeoPackage`/`addLayers` failure is non-fatal per file:** the loop skips failing SHPs and continues; `lastError` may be set even on overall success. Returns `false` only if *no* layers were imported.
- **GDAL RAII:** `geomanager.cpp` uses a local `GdalDatasetGuard` RAII wrapper for `GDALClose`. `qgisprojectgenerator.cpp` manages `GDALDataset*` manually — keep this asymmetry in mind when adding new GDAL code.
- `.qgz` datasource references use **absolute paths** (`<gpkgPath>|layername=<name>`). Moving the `.gpkg` or `.qgz` after generation breaks QGIS layer references.
- `.qgz` ZIP entry is always named `"project.qgs"` regardless of the output filename — QGIS requires this.
- **Project CRS is always hardcoded to WGS84 (EPSG:4326)** in the generated `.qgz`; initial map canvas extent is always full-globe. Individual layer SRS blocks attempt to read actual CRS but fall back to WGS84.
- **OGR geometry mapping** (`ogrGeomToQgis()` in `qgisprojectgenerator.cpp`) collapses Multi* types to their single-type equivalents (e.g., MultiPolygon → Polygon).
- Layer IDs are `<name>_<16 random hex chars>` — not stable across runs.
- **Success signals differ:** `GeoManager` emits `operationSucceeded()`; `QgisProjectGenerator` emits `generationSucceeded(outputPath)`. Handled by two separate `Connections` blocks in `main.qml`.
- All GDAL operations run **synchronously on the UI thread**; long operations will freeze the UI. The `busy` property signals blocking but does not run anything async.

## vcpkg / DLL deployment

vcpkg installs dependencies into `vcpkg_installed/` (git-ignored). Do not manually add DLLs. A post-build step copies vcpkg runtime DLLs and runs `windeployqt --qmldir qml --no-translations` automatically.

## Misc

- `README.md` is essentially empty; this file is the real documentation.
- `setApplicationName` uses `"Proyectos MDM Móvil"`. The vcpkg package name is `provectos-mdm-movil` (with a v) — this discrepancy is pre-existing.
- `.qtcreator/CMakeLists.txt.user` is machine-specific and already covered by `.gitignore`'s `*.user` rule.
