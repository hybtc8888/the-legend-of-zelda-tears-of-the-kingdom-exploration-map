# Translation catalogs

Each JSON file is named for a language code and contains its display name, the in-game interface, installer prompts and category labels. The installer reads the catalogs directly. The Mac adapter embeds the menu and panel strings when `mac/integrate.py` runs.

Keep the keys the same across languages. Category keys are shared with the [v1 map pack](../docs/map-pack.md) and saved settings. Translate the values and keep the installer placeholders `{selected}` and `{total}` intact.
