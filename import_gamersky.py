"""Build a local detailed map from the public Gamersky viewer and your own v1 pack."""
import argparse
from collections import Counter, defaultdict
from concurrent.futures import ThreadPoolExecutor
from html.parser import HTMLParser
import json
from math import hypot, isfinite
from pathlib import Path
import re
from urllib.request import Request, urlopen

from map_names import game_names
from minigames import CATEGORY as MINIGAMES
from romfs import RomFS

SITE = "https://www.gamersky.com"
VIEWER = SITE + "/tools/map/zeldabotw2/"
MAPS = {18: "surface", 19: "depths", 20: "sky"}
ALIASES = {"呀哈哈*2": "搬运呀哈哈", "魔犹伊遗失物": "魔犹伊", "大妖精之泉": "大妖精",
           "人马（莱尼尔）": "人马", "独眼巨人（西诺克斯）": "独眼巨人"}
NATIVE_MAP_CATEGORIES = {"神庙", "树根", "鸟望台", "神殿", "大妖精", "地名", "天空群岛", "洞穴入口", "地洞入口",
                         "井", "村庄", "驿站", "龙之泪", "残旧的地图", "左纳乌制造机", MINIGAMES}
HEADERS = {"Origin": SITE, "Referer": VIEWER, "User-Agent": "Mozilla/5.0"}


class PointText(HTMLParser):
    """Keep visible text and paragraph breaks from a marker description."""
    def __init__(self):
        super().__init__()
        self.parts = []

    def handle_starttag(self, tag, _attrs):
        if tag in {"br", "div", "p", "li"}:
            self.parts.append("\n")

    def handle_endtag(self, tag):
        if tag in {"div", "p", "li"}:
            self.parts.append("\n")

    def handle_data(self, data):
        self.parts.append(data)


def plain(text):
    parser = PointText()
    parser.feed(text or "")
    parser.close()
    return "\n".join(line for part in "".join(parser.parts).splitlines() if (line := part.strip()))


def request(route, body):
    req = Request("https://mapapi.gamersky.com" + route, data=json.dumps(body).encode(),
                  headers={**HEADERS, "Content-Type": "application/json"})
    with urlopen(req, timeout=20) as response:
        data = json.load(response)
    if data.get("error") is not None:
        raise ValueError(f"{route}: {data['error']}")
    return data


def read_layer(map_id):
    meta = request("/map/getMap", {"gameMapId": map_id})["map"]
    catalogs = {cat["id"]: cat for group in meta["landmarkCatalogGroups"]
                if group["landmarkCatalogType"] == 0 for cat in group["landmarkCatalogs"]}
    rows = request("/landmark/getLandmarkList", {
        "gameMapId": map_id, "keyword": None, "catalogIdsSelected": sorted(catalogs),
        "userLabelTypeName": None, "userId": 0, "isConditionNoneEnable": False,
    })["landmarks"]
    counts = Counter(row["landmarkCatalogId"] for row in rows)
    if len(rows) != len({row["id"] for row in rows}) or any(row["gameMapId"] != map_id for row in rows):
        raise ValueError(f"Unexpected markers for map {map_id}")
    if any(counts[id] != (cat["landmarksCount"] or 0) for id, cat in catalogs.items()):
        raise ValueError(f"Marker counts disagree with the catalog for map {map_id}")

    points = []
    for row in rows:
        x = 42.665 * row["y"] - 6255.787
        north = 42.659 * row["x"] + 7046.931
        if any(not isfinite(v) or abs(v) > 6500 for v in (x, north)):
            raise ValueError(f"Unexpected coordinates for marker {row['id']}")
        cat = catalogs[row["landmarkCatalogId"]]
        points.append({
            "id": f"gs:{row['id']}", "category": ALIASES.get(cat["name"], cat["name"]),
            "name": plain(row["name"]), "notes": plain(row["description"] or cat["defaultIntro"]),
            "x": round(x, 2), "north": round(north, 2), "group": cat["groupName"],
        })
    return MAPS[map_id], points


def comparable(text):
    return re.sub(r"[\s·()（）]", "", text)


def match_points(native, external):
    """Match one-to-one within categories. Loose matches require distinct names."""
    groups = defaultdict(list)
    for index, (_, point) in enumerate(native):
        groups[point["category"]].append(index)
    pairs = []
    for index, (layer, source) in enumerate(external):
        category = source["category"]
        for other in groups[category]:
            native_layer, point = native[other]
            anchor = point.get("destination", point)
            distance = hypot(source["x"] - anchor["x"], source["north"] - anchor["north"])
            title = comparable(source["name"])
            name_match = bool(title and title != comparable(category) and
                              title in {comparable(point.get(field, "")) for field in ("name", "reward")})
            if native_layer != layer and category not in {"搬运呀哈哈", "呀哈哈", "洞穴入口", "地洞入口"}:
                if category != "神庙" or not name_match or distance > 65:
                    continue
            limit = {"呀哈哈": 125, "搬运呀哈哈": 125}.get(category, 30)
            if category in {"树根", "神庙", "岩石巨人", "人马", "古栗欧克", "巨霸伽马",
                            "方块魔像", "独眼巨人", "莫尔德拉吉克"}:
                limit = 65
            if distance <= limit or (name_match and distance <= 550):
                score = distance - (350 if name_match else 0) + (8 if layer != native_layer else 0)
                pairs.append((score, index, other))
    matches = {}
    taken = set()
    for _, index, other in sorted(pairs):
        if index not in matches and other not in taken:
            matches[index] = other
            taken.add(other)
    return matches, taken


def build(pack, romfs):
    with ThreadPoolExecutor(max_workers=len(MAPS)) as pool:
        source = dict(pool.map(read_layer, MAPS))
    external = [(layer, point) for layer, points in source.items() for point in points]
    if len(external) != len({point["id"] for _, point in external}):
        raise ValueError("Duplicate marker IDs across maps")

    native = [(layer, point.copy()) for layer, points in pack["layers"].items() for point in points]
    if romfs:
        with RomFS(romfs) as fs:
            game_names(fs, [point for _, point in native])
    matches, taken = match_points(native, external)
    layers = {layer: [] for layer in pack["layers"]}
    for index, (layer, point) in enumerate(external):
        if index in matches:
            native_layer, original = native[matches[index]]
            # The game's icon and the guide card must use the same coordinates.
            point["source_id"] = point.pop("id")
            for field in ("id", "progress", "item", "reward", "supplies", "puzzle", "seeds", "activity"):
                if field in original:
                    point[field] = original[field]
            if point["category"] in NATIVE_MAP_CATEGORIES:
                layer = native_layer
                for field in ("x", "north", "height", "game_name"):
                    if field in original:
                        point[field] = original[field]
            if "destination" in original:
                point["origin"] = {field: original[field] for field in ("x", "north", "height")}
                point["destination"] = original["destination"]
        layers[layer].append(point)

    for index, (layer, point) in enumerate(native):
        if index not in taken:
            layers[layer].append(point)

    return {**pack, "layers": layers, "details": {
        "source": VIEWER, "marker_count": len(external), "matched": len(matches),
        "new_categories": sorted({p["category"] for _, p in external} - {p["category"] for _, p in native}),
    }}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--romfs", type=Path, required=True)
    args = parser.parse_args()
    if args.output.exists():
        parser.error("Choose a new output filename.")
    pack = json.loads(args.pack.read_bytes())
    if pack.get("schema") != 1 or pack.get("version") != "1.4.3" or pack.get("details"):
        parser.error("Expected an unmodified v1 map pack for Tears of the Kingdom 1.4.3.")
    result = build(pack, args.romfs)
    with args.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, separators=(",", ":"))
        stream.write("\n")
    print(result["details"])


if __name__ == "__main__":
    main()
