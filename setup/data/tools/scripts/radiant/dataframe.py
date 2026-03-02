"""
Pandas integration for Radiant Python scripts.

Provides helpers to load CSV/spreadsheet data as pandas DataFrames.
Requires: pip install pandas

Example:
    import radiant.dataframe as rdf
    if rdf.available():
        df = rdf.read_csv("path/to/data.csv")
        print(df.describe())
"""


def available() -> bool:
    """Return True if pandas is installed."""
    try:
        import pandas as pd  # noqa: F401
        return True
    except ImportError:
        return False


def read_csv(path: str, **kwargs):
    """
    Load a CSV file as a pandas DataFrame.

    Args:
        path: Path to the CSV file.
        **kwargs: Passed to pandas.read_csv() (e.g. sep, encoding).

    Returns:
        pandas.DataFrame if pandas is available.

    Raises:
        ImportError: If pandas is not installed.
    """
    import pandas as pd
    return pd.read_csv(path, **kwargs)


def read_spreadsheet(path: str, **kwargs):
    """
    Alias for read_csv. Load spreadsheet/CSV as DataFrame.
    """
    return read_csv(path, **kwargs)


def to_csv_safe(df, path: str, **kwargs) -> bool:
    """
    Save DataFrame to CSV. Returns False if pandas unavailable or error.
    """
    try:
        import pandas as pd
        df.to_csv(path, **kwargs)
        return True
    except (ImportError, Exception):
        return False
