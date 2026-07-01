# nft-marker-creator (Python)

Native Python package for creating **NFT markers** (`.iset` / `.fset` / `.fset3`)
for WebARKitLib / ARToolKit-based AR — a pybind11 wrapper around the same C++
core as the WASM tool, compiled natively (no WebAssembly). Ships both a
**library** and a **command-line tool**.

The generated markers are compatible with jsartoolkitNFT, ARnft, and AR.js.

## Install

```bash
pip install nft-marker-creator
```

Prebuilt wheels are provided for Linux, macOS, and Windows across supported
Python versions, so **no compiler or toolchain is required** — `pip` just
downloads the right wheel. The only runtime dependency is
[Pillow](https://pypi.org/project/pillow/) (installed automatically).

<details>
<summary>Building from source (contributors / unsupported platforms)</summary>

A source build compiles the native extension locally and needs: a **C++17
compiler (clang recommended)**, **CMake ≥ 3.18**, and **libjpeg** + **zlib**
development headers, plus the `WebARKitLib` git submodule.

```bash
git clone --recurse-submodules https://github.com/webarkit/Nft-Marker-Creator-App
cd Nft-Marker-Creator-App
# Debian/Ubuntu: sudo apt install build-essential clang cmake libjpeg-dev zlib1g-dev
CC=clang CXX=clang++ pip install ./python
```

</details>

## Usage

### Command line

```bash
nft-marker-creator -i pinball.jpg -o output --threads 4
```

Prints the paths of the written marker files. Options:

| Option           | Description                               | Default            |
| ---------------- | ----------------------------------------- | ------------------ |
| `-i`, `--input`  | Input image (jpg/jpeg/png) **(required)** | —                  |
| `-o`, `--output` | Output directory                          | current directory  |
| `--dpi`          | Override DPI                              | image metadata/150 |
| `--level`        | Tracking extraction level (0–4)           | 2                  |
| `--leveli`       | Initialization extraction level (0–3)     | 1                  |
| `--threads`      | Worker threads                            | 1                  |

Also runnable as `python -m nft_marker_creator ...`.

### Library

```python
import nft_marker_creator as nmc

paths = nmc.create("pinball.jpg", output_dir="output", threads=4)
print(paths)  # ['output/pinball.iset', 'output/pinball.fset', 'output/pinball.fset3']
```

`create(image, output_dir=".", dpi=None, level=2, leveli=1, threads=1)` returns
the list of written file paths and raises `RuntimeError` on failure (it never
terminates the host process). `threads > 1` uses real OS threads; the output is
byte-identical regardless of thread count.

## Notes

- Output files: `<name>.iset`, `<name>.fset`, `<name>.fset3`.
- Marker creation is memory-bandwidth-bound; threading speedup typically plateaus
  around 4 threads.
- This package is an additional, native distribution alongside the Node/WASM CLI
  in the same repository — not a replacement.

## License

LGPL-3.0 (same as the parent project).
