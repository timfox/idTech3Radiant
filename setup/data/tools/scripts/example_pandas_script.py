#!/usr/bin/env python3
"""
Example script using pandas with Radiant.

Requires: pip install pandas

Run from Python Script Editor (Tools > Python Script Editor, Ctrl+Alt+Y).
Uses radiant.dataframe to load CSV/spreadsheet data as DataFrames.
"""

def main():
    import radiant
    import radiant.dataframe as rdf

    if not rdf.available():
        print("Pandas not installed. Run: pip install pandas")
        return

    print("Radiant paths:")
    print("  Maps path:", radiant.maps_path())
    print("  Game path:", radiant.game_path())

    # Example: load a CSV from the maps or game directory
    import os
    maps = radiant.maps_path()
    if maps and os.path.isdir(maps):
        # Look for any CSV in maps (e.g. exported entity data)
        for name in os.listdir(maps):
            if name.endswith(".csv"):
                path = os.path.join(maps, name)
                try:
                    df = rdf.read_csv(path)
                    print(f"\nLoaded {name}:")
                    print(df.head())
                    print(f"\nShape: {df.shape}")
                    return
                except Exception as e:
                    print(f"Could not load {name}: {e}")

    print("\nNo CSV files found in maps directory.")
    print("Use Spreadsheet (Ctrl+Alt+E) > right-click > Copy as pandas code")
    print("to get code that loads your spreadsheet as a DataFrame.")

if __name__ == "__main__":
    main()
