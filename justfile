# The Windows build (engine/docs/specs/d3d12_port.md): the Visual Studio generator, every configuration in one build folder.
# `just` lists the recipes; `just build Release`, `just run`, `just test Release`.

set windows-shell := ["powershell.exe", "-NoLogo", "-NoProfile", "-Command"]

build_dir := "build-win"
generator := "Visual Studio 18 2026"

default:
    @just --list

# Fetch the engine submodule and the one LFS asset the game reads, the UI font.
setup:
    git submodule update --init
    git -C engine lfs pull --include "assets/fonts/*"

# Generate the Visual Studio solution (the build does this itself when the folder has none).
configure:
    cmake -S . -B {{build_dir}} -G "{{generator}}" -A x64

# Build every target of one configuration (Debug or Release).
build config="Debug":
    if (-not (Test-Path {{build_dir}}/CMakeCache.txt)) { just configure }
    cmake --build {{build_dir}} --config {{config}} -j

# Build, then run the game; the arguments after the configuration go to it.
run config="Debug" *args: (build config)
    & {{build_dir}}/{{config}}/autobattler.exe {{args}}

# Run a Debug build's script (engine/engine/debug_script.h); exits with 1 when the log holds an error.
script file out=(build_dir / "script"): (build "Debug")
    & {{build_dir}}/Debug/autobattler.exe --script "{{file}}" --out "{{out}}"

# Run the C tests of one configuration.
test config="Debug": (build config)
    ctest --test-dir {{build_dir}} -C {{config}} --output-on-failure

# Remove one configuration's build output (the solution and the downloaded tools stay).
clean config="Debug":
    cmake --build {{build_dir}} --config {{config}} --target clean
