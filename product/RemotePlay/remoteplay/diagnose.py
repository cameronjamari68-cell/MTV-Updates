"""Loader for the protected diagnostic: python -m remoteplay.diagnose"""
import sys

from ._diagnose_core import main


if __name__ == "__main__":
    sys.exit(main())
