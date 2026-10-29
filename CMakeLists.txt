cmake_minimum_required(VERSION 3.30) # the engine's ozz-animation needs 3.30
project(autobattler LANGUAGES C CXX)

# The engine is a Git submodule (engine/, README.md). As a subproject it builds only the nv library and hands us the platform
# (NV_WEB, NV_WINDOWS), the C and C++ standards and the options our code compiles with.
add_subdirectory(engine)
add_compile_options(${NV_COMPILE_OPTIONS})
add_compile_definitions(${NV_COMPILE_DEFINITIONS})

add_executable(autobattler src/main.c src/battle.c src/battle_view.c src/defs.c src/strings.c)
target_link_libraries(autobattler PRIVATE nv)
# A package of only what it reads (docs/specs/battle.md, "Executable"): the engine's UI font and the definition files.
nv_setup_executable(autobattler ROOT COMPONENT autobattler
    PRELOAD "${PROJECT_SOURCE_DIR}/engine/assets/fonts@/assets/fonts" "${PROJECT_SOURCE_DIR}/data@/data")

# The build label names this repository's commit and the build type (nv_version.h, NV_BUILD_NAME).
nv_add_version(autobattler)

enable_testing()
add_subdirectory(tests)
