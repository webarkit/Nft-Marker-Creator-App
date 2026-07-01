"""Enable ``python -m nft_marker_creator``."""

import sys

from .cli import main

if __name__ == "__main__":
    sys.exit(main())
