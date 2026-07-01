# Native Python package for NFT marker creation — Design (issue #25)

Status: **design / pre-implementation**. Branch: `feat/python-package`.

## Understanding summary

- **What:** a native Python package (pybind11) that generates NFT markers
  (`.iset` / `.fset` / `.fset3`, optionally `.zft`) from an image, wrapping the
  existing C++ core (`createNftDataSet`) — compiled natively, **no WASM**.
- **Why:** a first-class Python API with real OS threads, no WASM/JS bridge, and
  direct image/bytes input — for Python/AR pipelines that don't want Node.
- **Who:** Python developers and AR tooling that need marker generation.
- **Reality check:** marker generation is **memory-bandwidth-bound** (see #29), so
  native is not dramatically faster in raw compute than WASM. The wins are: no
  WASM overhead, real threads, native I/O, and a Pythonic API.
- **Non-goals:** replacing the Node/WASM CLI; the runtime tracker; (for the MVP)
  threading, `.zft`, NumPy input, and quality scoring.

## Key code insight

`markerCreator.cpp` already contains the runtime-agnostic core `createNftDataSet`
(it writes output via `fopen`-based `ar2WriteImageSet` / `ar2SaveFeatureSet` /
`kpmSaveRefDataSet` — portable). The only Emscripten coupling is:
1. `#include <emscripten/emscripten.h>` (line 45) — **vestigial / unused**.
2. `#include "markerCreator_bindings.cpp"` at the bottom — the WASM entry point.

So the core is essentially already portable; only the entry-point glue is
Emscripten-specific, and it lives in a **separate** file.

## Decision log

| Decision | Choice | Alternatives | Why |
|---|---|---|---|
| Location | Same repo, `python/` subdir | Separate repo | Shares C++ sources + submodules; single maintenance point |
| MVP scope | Minimal (image → iset/fset/fset3, single-thread) | Full-featured first | De-risk + value fast; YAGNI |
| Build system | scikit-build-core + CMake | setuptools + pybind11 | Best fit for compiling the AR2/KPM/Eigen C/C++ tree + wheels |
| Core/binding split (MVP) | Minimal `#ifdef __EMSCRIPTEN__` guard in `markerCreator.cpp` | Full core extraction now | Lowest risk to the working WASM build; keep it byte-identical |
| Core extraction | Deferred to a follow-up issue (`markerCreatorCore.{h,cpp}`) | Do it now | Cleaner SRP, but a bigger refactor; do after the native path is proven |
| Dev/build env | Linux (WSL Ubuntu-24.04 or Docker) | Native Windows/MSVC | Matches the project's Linux build workflow; native gcc + libjpeg/zlib |
| Release wheels | `cibuildwheel` in GitHub Actions (Linux/macOS/Windows) | Local builds | CI runners supply all 3 OSes; no Mac/Windows toolchain needed locally |
| Distribution | **PyPI now** | Docker now | Standard Python channel first |
| Docker image | **Deferred / undecided** | A=PyPI-only, B=extend Node image, C=slim python image | Decide later; leaning C (separate slim image) but not committed |

## Architecture (MVP)

**Existing code changed — only `markerCreator.cpp`, 2 minimal edits:**
1. Remove the dead `#include <emscripten/emscripten.h>` (unused → cleanup).
2. Guard the bottom binding include with `#ifdef __EMSCRIPTEN__ … #endif`.
   → WASM build stays **byte-identical** (emcc defines `__EMSCRIPTEN__`).

**New files (all under `python/`):**
- `python/src/markerCreator_py.cpp` — pybind11 binding (parallel to
  `markerCreator_bindings.cpp`); `extern` declares + calls `createNftDataSet`.
- `python/CMakeLists.txt` — compiles the same source list as `tools/makem.js`
  (AR2 + KPM/FreakMatcher + `markerCreator.cpp` + `markerCompress.cpp` + the
  pybind11 binding); links native `libjpeg` (`-ljpeg`), `zlib` (`-lz`),
  `pybind11`; include dirs mirror `makem.js`.
- `python/pyproject.toml` — scikit-build-core backend + metadata.
- `python/nft_marker_creator/__init__.py` — thin wrapper: decode the image with
  Pillow → raw bytes + (xsize, ysize, nc, dpi) → call the extension.

### MVP Python API (proposed)
```python
import nft_marker_creator as nmc

paths = nmc.create("pinball.jpg", output_dir="output")
# decodes image (Pillow), calls createNftDataSet, returns the written file paths
# kwargs map to the existing cmdStr flags, e.g. dpi=, level=, leveli=
```
The binding builds the existing `cmdStr` (`"-level=2 -leveli=1 ..."`) from kwargs
so it reuses the core's argument parsing unchanged.

### Two faces: library + standalone CLI
The same package ships both a **library** (`import nft_marker_creator`) and a
**standalone CLI** via a `[project.scripts]` entry point in `pyproject.toml`,
e.g. `nft-marker-creator -i pinball.jpg` → a small `argparse` wrapper calling the
same `create()`. This mirrors today's `node NFTMarkerCreator.js` and costs only a
few lines. Planned as the **immediate follow-up to the MVP library** (not in the
first spike). Heavier "standalone app" forms — a frozen single-file binary
(PyInstaller) or a GUI — are later, deliberate steps, not committed here.

## De-risk spike (first concrete step)

Goal: prove native compile + link + a one-function binding before building the
package. Run in **WSL Ubuntu-24.04 or a Linux Docker container**
(`build-essential`, `python3-dev`, `libjpeg-dev`, `zlib1g-dev`, `pybind11`).

1. Apply the 2 `markerCreator.cpp` guards.
2. Minimal `CMakeLists.txt` compiling the `makem.js` source list natively.
3. Minimal `markerCreator_py.cpp` exposing `createNftDataSet`.
4. `pip install ./python` (or cmake build) → `python -c "import nft_marker_creator; nft_marker_creator.create('src/pinball.jpg')"`.
5. Confirm `.iset/.fset/.fset3` are produced and match the Node tool's output for
   the same image (byte-identical or documented differences).

**If the spike compiles + links + runs → proceed to the MVP package. If native
compilation reveals blockers (missing deps, emcc-specific assumptions) → report
and adjust before investing further.**

## Testing strategy
- Spike: produce a marker from `pinball.jpg`, compare to the Node CLI output.
- MVP: a pytest that runs `nmc.create()` on the test image and asserts the three
  files exist (and, ideally, match a reference / the Node output).
- CI: build the wheel(s) with cibuildwheel; run the pytest on the built wheel.

## Spike result — VALIDATED ✅ (2026-07-01)

The de-risk spike succeeded end-to-end: `import nft_marker_creator; create()`
generated valid `.iset/.fset/.fset3` from the test image natively (~42.8s
single-threaded, vs ~51.9s for the WASM tool — the no-WASM-overhead win is real).

**Native vs WASM (fair: same container, Node 22, back-to-back, test image):**

| Threads | WASM | Native | Native advantage |
|--------:|-----:|-------:|:---:|
| 1 | 66.2s | 47.9s | 1.38× |
| 4 | 25.8s | 17.4s | 1.48× |
| 8 | 24.5s | 16.9s | 1.45× |

Native is ~1.4× faster than WASM at every thread count; both hit the memory-bound
plateau at ~4 threads (#29). Threaded output is byte-identical to single-threaded
(md5-verified across `.iset/.fset/.fset3`).

**Proven build recipe:**
- **clang** (not GCC): GCC 12 fails on the vendored Eigen's `isfinite_impl`.
- **C++17** (not the WASM build's C++11): Eigen needs `std::integer_sequence`.
- **Source list** = AR2 + KPM **plus** the full AR core the ARToolKit5 build links
  (`Makefile.in` recurses AR, ARICP, AR2, KPM, ARUtil): AR matrix (`m*.c`,`v*.c`)
  + param (`param*.c` minus `paramGL.c`) + `arUtil.c` + `arPattLoad.c` + `ARICP/icp*.c`
  + ARUtil (`log`, `file_utils`, `thread_sub`, `unzip`, `ioapi`, `crypt`, `zip`).
  Native linking is strict; emcc tolerated the never-called tracker symbols.
- **Link**: `libjpeg`, `zlib`, `pthread`.
- **Core edit**: only the 2 `markerCreator.cpp` guards (WASM stays byte-identical).

## Risks — status after spike
- ~~Native compilation of AR2/KPM/Eigen~~ → **resolved** (clang + C++17).
- ~~libjpeg/zlib native linkage~~ → **resolved** (`JPEG::JPEG` / `ZLIB::ZLIB`).
- `markerCompress.cpp` / `.zft` — still deferred (post-MVP; hardcoded temp path).
- **Output parity** — still to confirm: does native output match the WASM tool
  byte-for-byte? (MVP test task.)
- **`exit()` on error** — the core calls `exit()` on failures, which would kill the
  host Python process. Acceptable for the spike; the MVP should surface errors as
  exceptions (relates to the core-extraction refactor #31).

## Open follow-ups
- Core extraction into `markerCreatorCore.{h,cpp}` (issue #31).
- **Standalone CLI** entry point (`[project.scripts]`) — immediate follow-up to
  the MVP library; low cost, mirrors the Node CLI.
- Heavier standalone app: frozen binary (PyInstaller) or GUI — later, deliberate.
- Docker image for the Python package (option C leaning), decision deferred.
- Post-MVP: threading, `.zft`, NumPy input, ported confidence/entropy scoring,
  cibuildwheel multi-platform + PyPI publish.
