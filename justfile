# The Windows build (engine/docs/specs/d3d12_port.md): the Visual Studio generator, every configuration in one build folder.
# `just` lists the recipes; `just build Release`, `just run`, `just test Release`. The web build has its own two, `just web` and
# `just serve`, with a folder for each configuration (build-release, build-debug); their lines use no shell syntax, so they run
# under sh and PowerShell alike.

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

# Needs the Emscripten SDK on the PATH (emsdk_env) and Ninja; the folder is configured with emcmake the first time.
# Build the web game of one configuration (Release or Debug) into build-release or build-debug.
web config="Release":
    {{ if path_exists(justfile_directory() / ("build-" + lowercase(config)) / "CMakeCache.txt") == "true" { "cmake -E true" } else { "emcmake cmake -S . -B build-" + lowercase(config) + " -G Ninja -DCMAKE_BUILD_TYPE=" + config } }}
    cmake --build build-{{lowercase(config)}}

# Only on this computer: WebGPU needs a secure context, which plain HTTP is only on localhost and 127.0.0.1, so another
# device could not run the page anyway (and 0.0.0.0, Python's default, prints a link Chrome refuses WebGPU on).
# Build the web game, then serve it at http://127.0.0.1:<port>/ (Ctrl+C stops it).
serve config="Release" port="8000": (web config)
    python3 -m http.server --bind 127.0.0.1 -d build-{{lowercase(config)}} {{port}}
