"""
Radiant Python API - Helper module for scripts run from the level editor.

This module provides utilities for Python scripts executed via the
Python Script Editor workbench. Scripts run in a subprocess and can
use these helpers for path resolution and common tasks.

Example:
    import radiant
    print(radiant.app_path())
    print(radiant.engine_path())
"""

import os
import sys


def _env_path(key: str, default: str = "") -> str:
    """Get path from RADIANT_* environment variable set by the editor."""
    return os.environ.get(key, default)


def app_path() -> str:
    """Path to the Radiant editor installation (scripts, plugins, etc.)."""
    return _env_path("RADIANT_APP_PATH", "")


def engine_path() -> str:
    """Path to the game engine / base game directory."""
    return _env_path("RADIANT_ENGINE_PATH", "")


def game_path() -> str:
    """Path to the current game/mod directory."""
    return _env_path("RADIANT_GAME_PATH", "")


def maps_path() -> str:
    """Path to the maps directory."""
    return _env_path("RADIANT_MAPS_PATH", "")


def scripts_path() -> str:
    """Path to the scripts directory (entity defs, etc.)."""
    app = app_path()
    return os.path.join(app, "scripts") if app else ""


def current_map() -> str:
    """Name of the currently loaded map (without path)."""
    return _env_path("RADIANT_CURRENT_MAP", "")


def current_map_path() -> str:
    """Full path to the currently loaded map file."""
    return _env_path("RADIANT_CURRENT_MAP_PATH", "")
