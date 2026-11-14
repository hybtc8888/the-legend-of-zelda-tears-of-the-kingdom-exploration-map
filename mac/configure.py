"""Choose the Mac exploration map categories and install its generated data."""
import argparse
import json
import os
import re
from pathlib import Path
import tempfile

from map_locale import GROUPS, LANGUAGES, load_catalogs, system_language


def write(path, data):
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.")
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def select(groups, catalog):
    messages = catalog["installer"]
    labels = catalog["groups"]
    print(messages["select"])
    for index, name in enumerate(groups, 1):
        print(f"{index:2} [x] {labels.get(name, name)}")
    try:
        excluded = {int(word) for word in input("> ").split()}
        if not excluded.issubset(range(1, len(groups) + 1)):
            raise ValueError
    except ValueError:
        raise ValueError(messages["invalid_selection"]) from None
    return [name for index, name in enumerate(groups, 1) if index not in excluded]


def install_icons(pack, source, target):
    if not pack.get("details"):
        return
    filenames = {point[key] for points in pack["layers"].values() for point in points
                 for key in ("icon", "selected_icon") if point.get(key)}
    if any(not re.fullmatch(r"[0-9a-f]{20}\.png", name) for name in filenames):
        raise ValueError("The map pack contains an invalid marker icon filename.")
    files = {name: (source / "map-icons" / name).read_bytes() for name in filenames}
    icons = target / "map-icons"
    icons.mkdir(exist_ok=True)
    for name, data in files.items():
        write(icons / name, data)


def main():
    languages = ("auto", *LANGUAGES)
    language_parser = argparse.ArgumentParser(add_help=False)
    language_parser.add_argument("--language", choices=languages)
    # Read the language first so even the help and category prompts are localized.
    language, _ = language_parser.parse_known_args()
    target = Path.home() / "Library/Application Support/MacNativeGames/TOTK"
    language_file = target / "map-language.txt"
    try:
        saved = language_file.read_text(encoding="utf-8").strip()
    except FileNotFoundError:
        saved = "auto"
    chosen = language.language or saved
    catalogs = load_catalogs()
    catalog = catalogs.get(chosen) or catalogs[system_language()]
    messages = catalog["installer"]

    parser = argparse.ArgumentParser(description=messages["description"])
    parser.add_argument("pack", type=Path, help=messages["pack_help"])
    parser.add_argument("--all", action="store_true", help=messages["all_help"])
    parser.add_argument("--language", choices=languages, help=messages["language_help"])
    args = parser.parse_args()
    data = args.pack.read_bytes()
    pack = json.loads(data)
    if pack.get("schema") != 1 or pack.get("version") != "1.4.3" or pack.get("game") != "0100F2C0115B6000":
        parser.error(messages["invalid_pack"])
    groups = [group["id"] for group in GROUPS]
    try:
        selected = groups if args.all else select(groups, catalog)
    except ValueError as error:
        parser.error(str(error))
    target.mkdir(parents=True, exist_ok=True)
    try:
        install_icons(pack, args.pack.parent, target)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    write(target / "map-points-v1.json", data)
    write(target / "map-groups-v1.json",
          (json.dumps({group["id"]: group["id"] in selected for group in GROUPS}) + "\n").encode())
    if args.language:
        write(language_file, (args.language + "\n").encode())
    print(messages["installed"].format(selected=len(selected), total=len(groups)))


if __name__ == "__main__":
    main()
