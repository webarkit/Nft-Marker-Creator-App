"""Native NFT marker creator (WebARKitLib) — spike wrapper.

MVP: decode an image with Pillow and generate the .iset/.fset/.fset3 marker
files via the native core. Single-threaded; .zft, threading, NumPy input and
quality scoring are post-MVP (see python/DESIGN.md).
"""

from __future__ import annotations

import os

from PIL import Image

from . import _core

__all__ = ["create"]


def create(image, output_dir: str = ".", dpi: float | None = None,
           level: int = 2, leveli: int = 1, threads: int = 1) -> list[str]:
    """Generate NFT marker files from ``image``.

    ``threads`` > 1 runs the bounded worker pool (real OS threads). Output is
    byte-identical regardless of thread count. Returns the written file paths
    (``<name>.iset/.fset/.fset3``).
    """
    img = Image.open(image).convert("RGB")
    xsize, ysize = img.size
    raw = img.tobytes()  # interleaved RGB, length == xsize*ysize*3

    if dpi is None:
        dpi = float(img.info.get("dpi", (150,))[0])
    cmd = f"-level={level} -leveli={leveli} -dpi={float(dpi)}"
    if threads and threads > 1:
        cmd += f" --threaded {int(threads)}"

    os.makedirs(output_dir, exist_ok=True)
    prev = os.getcwd()
    os.chdir(output_dir)
    try:
        _core.create_nft_dataset(raw, float(dpi), xsize, ysize, 3, cmd)
    finally:
        os.chdir(prev)

    base = os.path.splitext(os.path.basename(str(image)))[0]
    outputs: list[str] = []
    for ext in ("iset", "fset", "fset3"):
        src = os.path.join(output_dir, f"tempFilename.{ext}")
        dst = os.path.join(output_dir, f"{base}.{ext}")
        if os.path.exists(src):
            os.replace(src, dst)
            outputs.append(dst)
    return outputs
