# Map pack format v1

The pack is UTF-8 JSON. Its root contains `schema`, the title ID `game`, `version`, `build_id`, and `layers`. The three layers are `surface`, `sky`, and `depths`.

Each point has a unique string `id`, a string `category`, eastward `x`, northward `north`, and world-space `height`. Coordinates are in internal game meters. For player-facing heights, use the game map's own projection. Korok deliveries can include a `destination` with the same coordinate fields.

Categories use stable v1 keys, which happen to be Simplified Chinese strings. The [locale catalogs](../locales/) map those keys to presentation labels. Store and compare the keys in point data, saved category selections, and preset rules so that changing a label or language leaves the selection intact. Unknown keys may be shown verbatim. A single pack works in every supported language.

A point may include `progress`. Its `key` is a decimal string to preserve 64-bit precision in JavaScript.

| `kind` | State source | Display |
| --- | --- | --- |
| `bool` | 32-bit GameData key and value | Hide when it matches `value` |
| `enum` | 32-bit GameData key and value | Hide when it matches any value in `values` |
| `guid` | 64-bit one-time state | Hide when true |
| `respawn` | 64-bit resettable state | Dim when true, restore after reset |

The adapter provides the current layer, visibility, map center, scale, viewport, and a read-only progress snapshot. When progress is unavailable, show normal markers. Fast travel uses the game's own map controls, and native markers take precedence for activated travel points. All categories start enabled; a platform adapter saves the user's selection locally.
