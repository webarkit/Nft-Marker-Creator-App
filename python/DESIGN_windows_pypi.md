# Design: Real-PyPI publishing + Windows wheels

Status: approved (brainstorming) — 2026-07-03
Branch: `kalwalt/ci/pypi-release-and-cross-platform-wheels` (from `dev`)

## Understanding summary

- **Goal 1 — Real PyPI**: extend `.github/workflows/python-wheels.yml` so **prerelease**
  tags publish to **TestPyPI** and **final** tags publish to **real PyPI**, both via
  Trusted Publishing (OIDC, no API tokens).
- **Goal 2 — Windows wheels first** (maintainer is on Windows 11 and can dogfood;
  macOS deferred to a tracked follow-up because it can't be tested locally).
- Windows is a **CMake + CI** change, not a source port: the vendored code already
  supports Win32.
- **Non-goals**: macOS wheels, Windows-ARM64, musllinux, any change to the working
  Linux build. First real-PyPI version: **0.1.0**.

## Key source findings (why Windows is tractable)

- `lib/SRC/ARUtil/thread_sub.c` has a complete Win32 threading path
  (`_beginthread`, `CRITICAL_SECTION`, `CONDITION_VARIABLE`) enabled by defining
  **`ARUTIL_DISABLE_PTHREADS`**. No pthread port needed.
- `lib/SRC/ARUtil/ioapi.h` already maps `fopen64`→`fopen`, `ftello64`→`_ftelli64`,
  `fseeko64`→`_fseeki64` on Windows/MSVC. minizip compiles as-is; the Linux
  `_LARGEFILE64_SOURCE`/`_FILE_OFFSET_BITS` defines must be **omitted** on Windows.

## Assumptions

1. **Windows compiler**: try **MSVC first** (cibuildwheel default). The "clang required"
   rule was a _Linux GCC_ codegen bug; MSVC is a different compiler with good Eigen/C++17
   support. **clang-cl is the fallback**. _(Highest risk — validate first.)_
2. **Windows deps**: statically link zlib + libjpeg-turbo via CMake `FetchContent`.
   Self-contained `.pyd`, **no delvewheel**, avoids the auditwheel/patchelf problem class.
3. Windows **AMD64**, CPython **3.8–3.13** (matching Linux).
4. Prerelease vs final detected by a `check-release` job parsing the tag.
5. Linux build untouched; new CMake logic lives under `if(WIN32)` / `if(NOT MSVC)`.
6. Maintainer does the one-time real-PyPI pending-publisher setup (environment `pypi`).

## Final design

### Part 1 — PyPI publishing (tag-driven routing, single workflow)

```
check-release (parse tag → outputs.target = testpypi | pypi | none)
build_wheels (matrix: ubuntu-latest, windows-latest)
build_sdist  (ubuntu-latest)
      → publish_testpypi  (if needs.check-release.outputs.target == 'testpypi')
      → publish_pypi      (if needs.check-release.outputs.target == 'pypi')
```

Tag rules (PEP 440):

- `python-v0.1.0rc1` / `...a1` / `...b1` / `...dev1` → **TestPyPI**
- `python-v0.1.0` (clean `X.Y.Z`) → **real PyPI**
- `workflow_dispatch` → build + test only, no publish.

`check-release` is a bash-regex job exposing `target`. `publish_pypi` mirrors the
TestPyPI job but drops `repository-url` (defaults to PyPI) and uses `environment: pypi`.
Both keep `permissions: id-token: write`.

### Part 2 — Windows wheels

CMake (additive, guarded):

```cmake
if(WIN32)
  target_compile_definitions(_core PRIVATE ARUTIL_DISABLE_PTHREADS)
  # omit _LARGEFILE64_SOURCE/_FILE_OFFSET_BITS; omit Threads::Threads
  # FetchContent + static-link zlib and libjpeg-turbo
else()
  # existing Linux defines + Threads::Threads + system JPEG/ZLIB
endif()
if(NOT MSVC)
  # -O2 Release override, -fno-strict-aliasing, -fno-strict-overflow,
  # visibility presets, LINKER:-z,noseparate-code
endif()
```

cibuildwheel:

```toml
[tool.cibuildwheel.windows]
archs = ["AMD64"]
```

No repair step (static). `build`/`skip`/`test-command` already cross-platform.

Workflow: `build_wheels` gets `matrix.os: [ubuntu-latest, windows-latest]`,
`runs-on: ${{ matrix.os }}`, per-OS artifact names merged at publish.

### Risk register — all resolved via local spike (VS2022 + CMake 4.1.2 + Py 3.11)

1. **MSVC miscompile** — ✅ RESOLVED. Plain MSVC builds _and_ runs correctly
   (import + threaded generation + real feature detection). No clang-cl needed.
2. **zlib/libjpeg-turbo provisioning** — ✅ zlib via `FetchContent_MakeAvailable`
   (`zlibstatic`); libjpeg-turbo refuses `add_subdirectory`, so via
   `ExternalProject_Add` + imported static target (`add_dependencies(_core jpeg_ext)`).
3. **pthreads** — ✅ our pool uses portable `std::thread`; only WebARKitLib
   `thread_sub.c` uses pthreads, handled by `ARUTIL_DISABLE_PTHREADS`.
4. **`NOMINMAX`** (found during spike) — `<windows.h>` min/max macros broke
   `std::min({...})`; added to Windows defs.

## Decision log

| Decision                               | Alternatives                            | Why                                                                    |
| -------------------------------------- | --------------------------------------- | ---------------------------------------------------------------------- |
| Prerelease→TestPyPI, final→PyPI        | publish both always; replace TestPyPI   | Keeps a staging gate before every real release.                        |
| Single workflow + `check-release` gate | second workflow; scattered `contains()` | One source of truth, auditable classification.                         |
| Windows before macOS                   | mac-first; both together                | Maintainer can dogfood Windows locally; no Mac to test on.             |
| Static-link libjpeg/zlib on Windows    | delvewheel + dynamic DLLs               | Self-contained `.pyd`; avoids the auditwheel/patchelf failure class.   |
| MSVC first, clang-cl fallback          | clang-cl first; require clang           | "clang required" was Linux-GCC-specific; MSVC is the simplest default. |
| Guard via `if(WIN32)`/`if(NOT MSVC)`   | separate CMakeLists                     | Keep the proven Linux build byte-identical.                            |

```

```
