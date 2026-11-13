"""Choose the Mac exploration map categories and install its generated data."""
import argparse
import json
import os
from pathlib import Path
import tempfile


def write(path, data):
    fd, name = tempfile.mkstemp(dir=path.parent, prefix=f".{path.name}.")
    with os.fdopen(fd, "wb") as stream:
        stream.write(data)
    try:
        os.replace(name, path)
    finally:
        Path(name).unlink(missing_ok=True)


def select(categories):
    print("默认全开。输入要取消的编号，用空格分隔，或按回车全部保留。")
    for index, name in enumerate(categories, 1):
        print(f"{index:2} [x] {name}")
    excluded = {int(word) for word in input("> ").split()}
    if not excluded.issubset(range(1, len(categories) + 1)):
        raise ValueError("分类编号超出范围")
    return [name for index, name in enumerate(categories, 1) if index not in excluded]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    parser.add_argument("--all", action="store_true", help="enable all categories without a prompt")
    args = parser.parse_args()
    data = args.pack.read_bytes()
    pack = json.loads(data)
    if pack.get("schema") != 1 or pack.get("version") != "1.4.3" or pack.get("game") != "0100F2C0115B6000":
        parser.error("Expected a Tears of the Kingdom 1.4.3 map pack")
    categories = sorted({point["category"] for rows in pack["layers"].values() for point in rows})
    try:
        selected = categories if args.all else select(categories)
    except ValueError as error:
        parser.error(str(error))
    target = Path.home() / "Library/Application Support/MacNativeGames/TOTK"
    target.mkdir(parents=True, exist_ok=True)
    write(target / "map-points-v1.json", data)
    write(target / "map-points-selection-v1.json",
          (json.dumps(selected, ensure_ascii=False) + "\n").encode())
    print(f"已安装地图数据，启用 {len(selected)}/{len(categories)} 个分类。")


if __name__ == "__main__":
    main()
