"""Choose the Mac exploration map categories and install its generated data."""
import argparse
import json
import os
from pathlib import Path
import tempfile

from map_locale import LANGUAGES, load_catalogs, system_language


def write(path, data):
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.")
    try:
        with os.fdopen(fd, "wb") as stream:
            stream.write(data)
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def select(categories, catalog):
    messages = catalog["installer"]
    labels = catalog["categories"]
    print(messages["select"])
    for index, name in enumerate(categories, 1):
        print(f"{index:2} [x] {labels.get(name, name)}")
    try:
        excluded = {int(word) for word in input("> ").split()}
        if not excluded.issubset(range(1, len(categories) + 1)):
            raise ValueError
    except ValueError:
        raise ValueError(messages["invalid_selection"]) from None
    return [name for index, name in enumerate(categories, 1) if index not in excluded]


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
    categories = sorted({point["category"] for rows in pack["layers"].values() for point in rows})
    try:
        selected = categories if args.all else select(categories, catalog)
    except ValueError as error:
        parser.error(str(error))
    target.mkdir(parents=True, exist_ok=True)
    write(target / "map-points-v1.json", data)
    write(target / "map-points-selection-v1.json",
          (json.dumps(selected, ensure_ascii=False) + "\n").encode())
    if args.language:
        write(language_file, (args.language + "\n").encode())
    print(messages["installed"].format(selected=len(selected), total=len(categories)))


if __name__ == "__main__":
    main()
