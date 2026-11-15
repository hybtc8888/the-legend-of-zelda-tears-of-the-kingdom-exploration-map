# The Legend of Zelda: Tears of the Kingdom — Exploration Map

**English** · [简体中文](docs/zh-Hans/README.md) · [日本語](docs/ja/README.md) · [Español](docs/es/README.md) · [Русский](docs/ru/README.md) · [Français](docs/fr/README.md) · [Deutsch](docs/de/README.md)

An in-game exploration map for Tears of the Kingdom **1.4.3** on Apple silicon Macs running MacNativeGames. It expands the native Sky, Surface and Depths maps with discoverable landmarks, live collection tracking and guide cards. Shrines, Lightroots, towers, caves, the five temples, Great Fairy Fountains and other supported locations use the game's own markers. The overlay adds original glyphs where the game lacks a marker. Fast travel follows the game's own controls and unlocks.

- **4,436 points in the base pack.** Includes all **1,000 Korok Seeds** across 800 single-seed puzzles and 100 two-seed deliveries. The data records both ends of each delivery.
- **Live progress.** Tracked pickups and activities dim when completed and can be hidden with one switch. Respawning bosses and Lynels stay dim until they return.
- **Ten filters.** Every group is enabled by default. Treasure chests, equipment, Koroks, Great Fairies and minigames each have their own switch.
- **19 minigame starts.** Native place labels cover supported venues. Additional pins mark registration locations. The game handles active objectives and progress.
- **Seven languages.** The in-game menu, shortcut panel, category names and installer prompts are available in English, Simplified Chinese, Japanese, Spanish, Russian, French and German.

| Points of interest | Examples |
| --- | --- |
| Landmarks | 152 shrines, 120 Lightroots, 15 Skyview Towers, temples, settlements, stables and sky archipelagos |
| Entrances and services | Caves, chasms, wells, Great Fairies, 30 Zonai Device Dispensers, inns and shops |
| Collectibles | Koroks, chests, armor, Sage’s Wills, Old Maps, Yiga Schematics, Dragon’s Tears, Bubbulfrogs and Addison’s signs |
| Enemies | Taluses, Hinoxes, Lynels, Gleeoks, Froxes, Flux Constructs and Moldugas |
| Minigames | Shield surfing, Tarrey Town and Lurelin rallies, diving challenges and mine-cart courses |

The base pack covers fixed placements. The optional import adds guide markers such as quest starting points and gathering spots.

## Install on Mac

You need an Apple silicon Mac, Python 3.14, a MacNativeGames source checkout, and your own base game and **1.4.3** update. Follow the [MacNativeGames build guide](https://github.com/ly05010419/MacNativeGames/blob/main/docs/install.md) for the game files and App build.

1. Download this project's source and run from its root directory:

   ```sh
   python3.14 mac/integrate.py "/path/to/MacNativeGames"
   ```

2. Build or update the **1.4.3** Mac App from that MacNativeGames checkout. Quit the App first if you are updating. The integration can be run again over an existing installation made with this project's integration script.
3. Generate the shared map pack from your extracted 1.4.3 RomFS. In the MacNativeGames build workspace it is typically under `totk/extracted/v143/romfs` and contains `index.json` and `romfs.bin`.

   Regenerate an older pack to add the minigame locations.

   ```sh
   python3.14 build_map.py "/path/to/romfs" map-points.json
   ```

4. Install the pack. Press Return to keep all ten groups, or enter the numbers you want to exclude, separated by spaces. Add `--all` to skip the prompt.

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
| **⌘⇧P** | Show or hide all completed points, including collected Koroks |
| **⌘⌥1–9** | Toggle groups 1–9 and save their visibility |
| **⌘⌥0** | Toggle minigames |

Groups 1–9 are Landmarks & exploration, Collectibles & materials, Enemies & bosses, Treasure chests, Koroks, Equipment, Quests, Towns & services, and Great Fairies.

The minigame switch controls the added pins and guide cards. The game's place labels and live objective markers follow the native map.

Select **语言Language：…** at the bottom of the exploration map panel with the mouse, keyboard, or controller. The setting is saved immediately and also applies to marker details when the pack includes translations.

On a controller, tap **R3** to toggle the panel or hold it for about **0.7 seconds** to toggle the overlay. In the panel, use **↑ / ↓** to select a row, **← / →** to page, and the game's confirm and back buttons to toggle an option or close the panel. The panel also supports mouse and wheel input. On keyboard use the arrows, Return, and Esc.

## Repository

| Path | Purpose |
| --- | --- |
| [`docs/`](docs/) | Localized player guides and [map pack format](docs/map-pack.md) |
| [`locales/`](locales/) | Translation catalogs shared by the installer and Mac UI |
| [`mac/`](mac/) | MacNativeGames integration, installer and runtime |
| [`build_map.py`](build_map.py) | Platform-independent map pack generator |
| [`minigames.py`](minigames.py) | Minigame locations and registration actor IDs |

The runtime adapter currently targets macOS. Community ports can use the [map pack format and runtime contract](docs/map-pack.md). Source code is available under the [MIT License](LICENSE).

## Optional map marker details

Hover over a map marker with the mouse or move the game's map reticle onto it. The card shows the name, category, coordinates, and any available notes and obtainable items. The optional import adds Simplified Chinese descriptions from the Gamersky interactive map. Matched native landmarks use the game's coordinates and layers.

```sh
python3.14 import_gamersky.py map-points.json map-points-detailed.json --romfs "/path/to/romfs"
python3.14 mac/configure.py map-points-detailed.json
```

To translate marker names, notes, rewards and device dispenser contents locally, add translations to the detailed pack before installing it. Install `ctranslate2` and `sentencepiece` in your Python environment. The generator uses official names from your game files and a local [CTranslate2 NLLB model](https://opennmt.net/CTranslate2/guides/transformers.html#nllb) with `sentencepiece.bpe.model` for the remaining text.

```sh
python3.14 localize_markers.py map-points-detailed.json map-points-localized.json \
  --romfs "/path/to/romfs" --model "/path/to/ctranslate2-nllb"
python3.14 mac/configure.py map-points-localized.json
```

[Gamersky interactive map](https://www.gamersky.com/tools/map/zeldabotw2/)
