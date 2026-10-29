# autobattler

- [한국어](#한국어)
- [English](#english)

## 한국어

메카벨리움 스타일의 싱글플레이 오토배틀러(`docs/specs/battle.md`). nv 엔진(`chromedays/engine`) 위에서 브라우저(WebAssembly,
WebGPU)와 Windows(Direct3D 12)로 한 소스에서 빌드된다. 엔진은 Git submodule `engine/`이다.

처음에는 엔진 저장소의 `autobattler/` 폴더였고, 2026-10-29에 이 저장소로 옮겼다. 그 폴더와 테스트, 문서, 빌드 대시보드의
커밋 기록은 `git filter-repo`로 가져왔다(`autobattler/`는 `src/`, `autobattler/data/`는 `data/`).

빌드 대시보드는 master와 PR이 열린 브랜치의 Release·Debug 빌드를 보여 준다(`docs/specs/build_dashboard.md`,
`tools/dashboard/README.md`).

### 빌드

Emscripten SDK(6.0.10에서 확인), CMake 3.30 이상, Git LFS가 필요하다.

```sh
git clone --recursive https://github.com/chromedays/autobattler.git
cd autobattler
git -C engine lfs pull --include "assets/fonts/*"   # 게임이 읽는 엔진 에셋은 UI 글꼴뿐이다
emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure          # tests/, Node로 실행
python3 -m http.server -d build 8000                # http://localhost:8000/
```

Windows는 `AGENTS.md`의 "On Windows"와 `justfile`(`just setup`, `just run Release`)을 본다.

### 엔진 올리기

`engine/`은 엔진의 커밋 하나에 고정되어 있다. 새 엔진을 쓰려면 `git -C engine fetch && git -C engine checkout <커밋>` 뒤에 두
빌드를 확인하고 submodule 변경을 커밋한다. 게임에 필요한 엔진 변경은 먼저 `chromedays/engine`의 PR로 들어간다.

## English

A single-player auto-battler in the style of Mechabellum (`docs/specs/battle.md`). It is built on the nv engine
(`chromedays/engine`) for the browser (WebAssembly, WebGPU) and Windows (Direct3D 12) from one source. The engine is the Git submodule
`engine/`.

It was first the engine repository's `autobattler/` folder and moved to this repository on 2026-10-29. The history of that folder, its
tests, docs and the build dashboard came along through `git filter-repo` (`autobattler/` is now `src/`, `autobattler/data/` is
`data/`).

The build dashboard shows the Release and Debug builds of master and of every branch with an open pull request
(`docs/specs/build_dashboard.md`, `tools/dashboard/README.md`).

### Building

Needs the Emscripten SDK (tested with 6.0.10), CMake 3.30+ and Git LFS.

```sh
git clone --recursive https://github.com/chromedays/autobattler.git
cd autobattler
git -C engine lfs pull --include "assets/fonts/*"   # the only engine asset the game reads is the UI font
emcmake cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure          # tests/, run under Node
python3 -m http.server -d build 8000                # http://localhost:8000/
```

For Windows, see "On Windows" in `AGENTS.md` and the `justfile` (`just setup`, `just run Release`).

### Moving the engine

`engine/` is pinned to one engine commit. To take a newer engine, `git -C engine fetch && git -C engine checkout <commit>`, check
both builds and commit the submodule change. An engine change the game needs goes into `chromedays/engine` as a pull request first.
