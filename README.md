# The Legend of Zelda: Tears of the Kingdom — Exploration Map

**English** · [简体中文](docs/zh-Hans/README.md) · [日本語](docs/ja/README.md) · [Español](docs/es/README.md) · [Русский](docs/ru/README.md) · [Français](docs/fr/README.md) · [Deutsch](docs/de/README.md)

An in-game exploration overlay for Tears of the Kingdom **1.4.3** on Apple silicon Macs running MacNativeGames. The overlay follows the game's Sky, Surface and Depths maps as you switch layers, pan and zoom. Activated shrines and Lightroots retain their native markers and fast travel controls.

- **4,428 points in the base pack.** Includes all **1,000 Korok Seeds** across 800 single-seed puzzles and 100 two-seed deliveries. The data records both ends of each delivery.
- **Live progress.** Tracked pickups disappear when collected. Defeated respawning bosses and Lynels dim until they return. Turn off the progress filter to revisit completed locations.
- **Custom filters.** Every category is enabled by default. Use eight color-coded groups and toggle each independently. Marker glyphs and the panel legend share the same group colors. Equipment remains in its own group when found in a chest.
- **Seven languages.** The in-game menu, shortcut panel, category names and installer prompts are available in English, Simplified Chinese, Japanese, Spanish, Russian, French and German.

| Points of interest | Examples |
| --- | --- |
| Landmarks | 152 shrines, 120 Lightroots, 15 Skyview Towers, temples, settlements, stables and sky archipelagos |
| Entrances and services | Caves, chasms, wells, Great Fairies, 30 Zonai Device Dispensers, inns and shops |
| Collectibles | Koroks, chests, armor, Sage’s Wills, Old Maps, Yiga Schematics, Dragon’s Tears, Bubbulfrogs and Addison’s signs |
| Enemies | Taluses, Hinoxes, Lynels, Gleeoks, Froxes, Flux Constructs and Moldugas |

The base pack covers fixed placements. The optional import adds guide markers such as quest starting points and gathering spots.

## Install on Mac

You need an Apple silicon Mac, Python 3.14, a MacNativeGames source checkout, and your own base game and **1.4.3** update. Follow the [MacNativeGames build guide](https://github.com/ly05010419/MacNativeGames/blob/main/docs/install.md) for the game files and App build.

1. Download this project's source and run from its root directory:

   ```sh
   python3.14 mac/integrate.py "/path/to/MacNativeGames"
   ```

2. Build or update the **1.4.3** Mac App from that MacNativeGames checkout. Quit the App first if you are updating. The integration can be run again over an existing installation made with this project's integration script.
3. Generate the shared map pack from your extracted 1.4.3 RomFS. In the MacNativeGames build workspace it is typically under `totk/extracted/v143/romfs` and contains `index.json` and `romfs.bin`.

   ```sh
   python3.14 build_map.py "/path/to/romfs" map-points.json
   ```

4. Install the pack. Press Return to keep all eight groups, or enter the numbers you want to exclude, separated by spaces. Add `--all` to skip the prompt.

   ```sh
   python3.14 mac/configure.py map-points.json --language en
   ```

   Use `--language auto` to follow the system, or choose `en`, `zh-Hans`, `ja`, `es`, `ru`, `fr` or `de`. Omit the flag to keep a saved language. After installation, switch anytime from **View → Exploration Map → Language** in the Mac menu bar. Switching languages there preserves your category selection and applies immediately.

The map pack is generated locally from your game files. Progress is read from the running game without editing saves.

## Controls

With the default bindings, **Tab** or **M** opens the native map. Press **3** toward Sky or **5** toward Depths. On a controller, open the map with **−** and change layers with **↑ / ↓**.

| Keyboard | Action |
| --- | --- |
| **⌘⇧L** | Show or hide the category and shortcut panel |
| **⌘⇧M** | Toggle the overlay |
| **⌘⇧P** | Toggle the progress filter |
| **⌘⌥1–8** | Toggle a group and save its visibility |
| **⌘⇧1–8** | Temporarily show only the matching group |
| **⌘⇧0** | Restore saved group visibility |

Groups 1–8 are Landmarks & exploration, Collectibles & materials, Enemies & bosses, Treasure chests, Koroks, Equipment, Quests, and Towns & services.

On a controller, tap **R3** to toggle the panel or hold it for about **0.7 seconds** to toggle the overlay. In the panel, use **↑ / ↓** to select a row, **← / →** to page, and the game's confirm and back buttons to toggle an option or close the panel. The panel also supports mouse and wheel input. On keyboard use the arrows, Return, and Esc. Select the same preset again or leave the game map to restore your custom categories.

## Repository

| Path | Purpose |
| --- | --- |
| [`docs/`](docs/) | Localized player guides and [map pack format](docs/map-pack.md) |
| [`locales/`](locales/) | Translation catalogs shared by the installer and Mac UI |
| [`mac/`](mac/) | MacNativeGames integration, installer and runtime |
| [`build_map.py`](build_map.py) | Platform-independent map pack generator |

The runtime adapter currently targets macOS. Community ports can use the [map pack format and runtime contract](docs/map-pack.md). Source code is available under the [MIT License](LICENSE).

## Optional map marker details

Hover over a map marker with the mouse or move the game's map reticle onto it. The card shows the name, category, coordinates, and any available notes and obtainable items. The optional import reads the public marker icons and Simplified Chinese descriptions from the Gamersky interactive map. The game supplies additional markers and progress states.

```sh
python3.14 import_gamersky.py map-points.json map-points-detailed.json --romfs "/path/to/romfs"
python3.14 mac/configure.py map-points-detailed.json
```

[Gamersky interactive map](https://www.gamersky.com/tools/map/zeldabotw2/)
