"""Standalone CLI for the native NFT marker creator.

Exposed as the ``nft-marker-creator`` command (see ``[project.scripts]`` in
pyproject.toml) and via ``python -m nft_marker_creator``. Thin argparse wrapper
over :func:`nft_marker_creator.create`, mirroring the Node CLI.
"""

from __future__ import annotations

import argparse
import sys

from . import create


def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="nft-marker-creator",
        description="Generate NFT marker files (.iset/.fset/.fset3) from an image.",
    )
    p.add_argument("-i", "--input", required=True, help="input image (jpg/jpeg/png)")
    p.add_argument("-o", "--output", default=".", help="output directory (default: current dir)")
    p.add_argument("--dpi", type=float, default=None, help="override DPI (default: image metadata, else 150)")
    p.add_argument("--level", type=int, default=2, help="tracking extraction level 0-4 (default: 2)")
    p.add_argument("--leveli", type=int, default=1, help="initialization extraction level 0-3 (default: 1)")
    p.add_argument("--threads", type=int, default=1, help="worker threads (default: 1)")
    return p


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        paths = create(
            args.input,
            output_dir=args.output,
            dpi=args.dpi,
            level=args.level,
            leveli=args.leveli,
            threads=args.threads,
        )
    except Exception as exc:  # native core raises RuntimeError on failure
        print(f"nft-marker-creator: error: {exc}", file=sys.stderr)
        return 1
    for path in paths:
        print(path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
