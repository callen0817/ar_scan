#!/usr/bin/env python3
"""
AV Scan — Backward-compatibility wrapper for LingBot-Map Streaming Backend.
Architectural Note:
LingBot has been promoted from the VITURE-specific driver directory to the
generic AV Scan reconstruction layer at:
  core/reconstruction/lingbot/lingbot_streaming_backend.py

This file imports and re-exports LingBotStreamingBackend to preserve existing
driver bindings and test contracts without breaking M5 qualification.
"""

import os
import sys

# Ensure repository root is in python path
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../.."))
if REPO_ROOT not in sys.path:
    sys.path.insert(0, REPO_ROOT)

from core.reconstruction.lingbot.lingbot_streaming_backend import (
    LingBotStreamingBackend,
    HAS_LINGBOT,
    LINGBOT_REPO_PATH
)

__all__ = ["LingBotStreamingBackend", "HAS_LINGBOT", "LINGBOT_REPO_PATH"]
