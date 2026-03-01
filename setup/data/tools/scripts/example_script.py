#!/usr/bin/env python3
"""
Example Python script for Radiant level editor.

Run this from the Python Script Editor (Tools > Python Script Editor, or Ctrl+Alt+Y).

The radiant module provides path helpers when the script is run from Radiant.
"""

def main():
    try:
        import radiant
        print("Radiant paths:")
        print("  App path:", radiant.app_path())
        print("  Engine path:", radiant.engine_path())
        print("  Game path:", radiant.game_path())
        print("  Maps path:", radiant.maps_path())
        print("  Current map:", radiant.current_map())
        print("  Current map path:", radiant.current_map_path())
    except ImportError:
        print("Note: 'radiant' module not found. Run this script from Radiant's Python Script Editor")
        print("      for path helpers. Or run with PYTHONPATH including Radiant's scripts directory.")
    print("\nHello from Radiant Python!")

if __name__ == "__main__":
    main()
