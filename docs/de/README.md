# The Legend of Zelda: Tears of the Kingdom — Erkundungskarte für Hyrule

[English](../../README.md) · [简体中文](../zh-Hans/README.md) · [日本語](../ja/README.md) · [Español](../es/README.md) · [Русский](../ru/README.md) · [Français](../fr/README.md) · **Deutsch**

Diese Mod ergänzt die Karte von **1.4.3** in MacNativeGames auf Macs mit Apple-Chip um eine Erkundungsebene. Die Markierungen folgen der Spielkarte beim Wechsel zwischen Himmel, Oberfläche und Untergrund sowie beim Verschieben und Zoomen. Aktivierte Schreine und Lichtwurzeln behalten ihre Spielsymbole und Schnellreise.

- **4.428 Punkte im Basispaket**, darunter alle **1.000 Krog-Samen**: 800 Einzelrätsel und 100 Transporte mit je zwei Samen. Das Kartenpaket speichert Start und Ziel jedes Transports.
- Enthalten sind 152 Schreine, 120 Lichtwurzeln, 15 Kartografietürme, 30 Sonau-Kapselspender sowie Höhlen, Brunnen, Truhen, Rüstungen, Drachentränen und Bosse.
- Punkte mit Fortschrittserfassung verschwinden nach Abschluss. Besiegte Bosse und Leunen, die wieder erscheinen können, werden abgedunkelt und danach wieder normal angezeigt. Schalte den Fortschrittsfilter aus, um abgeschlossene Punkte zu sehen.
- Alle Kategorien sind anfangs aktiv. Speichere deine Auswahl oder nutze vorübergehende Filter für Orientierungspunkte, Sammelobjekte oder Bosse und Leunen. Quest-Startpunkte und dynamische Sammelstellen sind derzeit nicht enthalten.

## Installation auf dem Mac

Du brauchst einen Mac mit Apple-Chip, Python 3.14, den MacNativeGames-Quellcode, dein eigenes Basisspiel und das Update **1.4.3**. Siehe die [englische MacNativeGames-Bauanleitung](https://github.com/ly05010419/MacNativeGames/blob/main/docs/install.md).

1. Führe im Hauptordner dieses Projekts `python3.14 mac/integrate.py "/path/to/MacNativeGames"` aus.
2. Erstelle oder aktualisiere die Mac-App für 1.4.3 aus diesem Quellcode. Beende das Spiel vor einem Update.
3. Erzeuge mit der entpackten RomFS von 1.4.3, meist unter `totk/extracted/v143/romfs`, das Kartenpaket: `python3.14 build_map.py "/path/to/romfs" map-points.json`.
4. Installiere es mit `python3.14 mac/configure.py map-points.json --language de`. Drücke Eingabe für alle Kategorien oder tippe die auszuschließenden Nummern mit Leerzeichen dazwischen ein. Mit `--all` überspringst du die Auswahl.

Wechsle die Sprache in der Mac-Menüleiste unter **View → Erkundungskarte → Sprache**. `--language auto` folgt der Systemsprache. Die Kategorieauswahl bleibt beim Sprachwechsel erhalten. Das Kartenpaket wird aus deinen Spieldateien erstellt. Der Fortschritt wird aus dem laufenden Spiel gelesen, ohne Spielstände zu bearbeiten.

## Steuerung

Öffne die Spielkarte mit **Tab / M** oder **−** am Controller. Wechsle mit **3 / 5** oder **↑ / ↓** am Steuerkreuz die Ebene.

| Tasten | Aktion |
| --- | --- |
| **⌘⇧L** | Kategorien und Tastenkürzel ein- oder ausblenden |
| **⌘⇧M** | Erkundungsebene ein- oder ausschalten |
| **⌘⇧P** | Fortschrittsfilter umschalten |
| **⌘⌥1–8** | Gruppe umschalten und Sichtbarkeit speichern |
| **⌘⇧1–8** | Vorübergehend nur diese Gruppe anzeigen |
| **⌘⇧0** | Gespeicherte Gruppensichtbarkeit wiederherstellen |

1. Orte & Erkundung · 2. Sammelobjekte & Materialien · 3. Gegner & Bosse · 4. Schatztruhen · 5. Krogs · 6. Ausrüstung · 7. Aufgaben · 8. Siedlungen & Dienste

Drücke **R3** kurz, um das Panel zu öffnen oder zu schließen, oder halte es etwa 0,7 Sekunden, um die Ebene umzuschalten. Im Panel wählst du mit hoch/runter eine Zeile, blätterst mit links/rechts und verwendest die Bestätigen- und Zurück-Tasten des Spiels. Auch Pfeiltasten, Eingabe, Esc, Maus und Mausrad funktionieren. Wähle denselben Filter erneut oder verlasse die Karte, um zu deiner gespeicherten Auswahl zurückzukehren.

[Englische Kartenpaket-Spezifikation](../map-pack.md) · [MIT-Lizenz](../../LICENSE)
