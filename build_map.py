"""Generate a local map pack from extracted Tears of the Kingdom 1.4.3 files."""
import argparse
from collections import Counter
import json
from pathlib import Path
import re

from flag_hash import name_hash
from romfs import RomFS

GAME_BUILD = "277178B7DBA1B6D4949A84778D4ABC58B31B34F5"
WORLD_FIELDS = ("MainField", "MinusField")
LOCATIONS = {
    "Shrine": "神庙", "CheckPoint": "树根", "Tower": "鸟望台", "Dungeon": "神殿",
    "CaveEntranceNormal": "洞穴入口", "CaveEntranceSpecial": "地洞入口",
    "CaveEntranceWell": "井", "DragonTears": "龙之泪", "City": "村庄",
    "Stable": "驿站", "ShopArmor": "防具店", "ShopDye": "染色店", "ShopGeneral": "杂货店",
    "ShopInn": "旅馆", "ShopJewelry": "珠宝店", "SkyArchipelago": "天空群岛",
}
BOSSES = {
    "Golem": "岩石巨人", "Giant": "独眼巨人", "Lynel": "人马", "Drake": "古栗欧克",
    "Mogurudo": "巨霸伽马", "Sandworm": "莫尔德拉吉克", "Zonau": "方块魔像",
}


def get_layer(path, height):
    if path.startswith("Banc/MinusField/"):
        return "depths"
    if "/Sky/" in path or height >= 700:
        return "sky"
    return "surface"


def flag(name, kind, value=1):
    return {"key": str(name_hash(name)), "kind": kind, "value": value}


def build(fs):
    if "RSDB/Location.Product.143.rstbl.byml.zs" not in fs.index:
        raise ValueError("The map generator requires the 1.4.3 RomFS")
    game = fs.byml("GameData/GameDataList.Product.140.byml.zs")["Data"]
    bools = {r["Hash"] for r in game["Bool"] if r["SaveFileIndex"] == 0}
    enums = {r["Hash"]: r for r in game["Enum"] if r["SaveFileIndex"] == 0}
    lightroots = game["Enum"][:120]
    if len(lightroots) != 120 or any(row["RawValues"] != ["Close", "Open"] for row in lightroots):
        raise ValueError("Unexpected 1.4.3 lightroot state table")
    resets = {r["Hash"]: r["ResetTypeValue"] for r in game["Bool64bitKey"]}
    layers = {layer: [] for layer in ("surface", "sky", "depths")}
    actors = {}

    def add(layer, category, key, position, progress=None, **extra):
        x, y, z = position
        point = {"id": str(key), "category": category,
                 "x": round(x, 3), "height": round(y, 3), "north": round(-z, 3), **extra}
        if progress:
            point["progress"] = progress
        layers[layer].append(point)

    def known_bool(name):
        progress = flag(name, "bool")
        return progress if int(progress["key"]) in bools else None

    def known_enum(name, states):
        key = name_hash(name)
        if key in enums:
            raw = enums[key]["RawValues"]
            values = enums[key]["Values"]
            return {"key": str(key), "kind": "enum", "values": [values[raw.index(s)] for s in states]}
        return None

    for field in WORLD_FIELDS:
        prefix = f"Banc/{field}/"
        location_file = f"{prefix}LocationArea/{field}.locationarea.byml.zs"
        for kind, rows in fs.byml(location_file).items():
            if kind not in LOCATIONS and not kind.startswith("Spot"):
                continue
            for location_index, row in enumerate(rows):
                resource = row["LocationName"]
                name = Path(resource).name.split(".", 1)[0]
                if kind == "Shrine":
                    progress = known_enum(f"DungeonState.{name}", ("Open", "Enter", "Clear"))
                elif kind == "CheckPoint":
                    root = lightroots[location_index]
                    progress = {"key": str(root["Hash"]), "kind": "enum", "values": [root["Values"][1]]}
                else:
                    progress = known_bool(f"IsOpenCannon.{name}" if kind == "Tower" else f"IsVisitLocation.{name}")
                for key, position in zip(row["InstanceID"], row["Trans"], strict=True):
                    layer = get_layer(prefix, position[1])
                    if kind == "SkyArchipelago":
                        layer = "sky"
                    category = LOCATIONS.get(kind, "地名")
                    add(layer, category, f"location:{key}", position, progress, game_name=name)
                for i, marker in enumerate(row.get("CaveMarkerInfo", [])):
                    position = marker["Trans"]
                    progress = known_bool(f"IsGetCaveMasterMedal.{name}")
                    if progress:
                        add(get_layer(prefix, position[1]), "魔犹伊", f"bubbul:{name}:{i}", position, progress)
        for key, row in fs.byml(f"{prefix}RecycleBox/{field}.recyclebox.byml.zs").items():
            position = row["Trans"]
            add(get_layer(prefix, position[1]), "左纳乌制造机", f"dispenser:{key}", position,
                contents=row["Contents"])
        for path in sorted(fs.index):
            if not path.startswith(prefix) or not path.endswith(".bcett.byml.zs") or "/Merged/" in path:
                continue
            for a in fs.byml(path).get("Actors", []):
                if "Translate" in a:
                    actors[a["Hash"]] = (path, a)

    koroks = fs.byml("Banc/MainField/HiddenKorok/MainField.hiddenkorok.byml.zs")
    for key, position in koroks["HiddenKorok"].items():
        path, actor = actors[int(key)]
        layer = get_layer(path, position[1])
        progress = known_bool(f"IsAppearKorok.{key}")
        if not progress:
            raise ValueError(f"Missing Korok flag {key}")
        add(layer, "呀哈哈", f"korok:{key}", position, progress, seeds=1,
            puzzle=actor.get("Dynamic", {}).get("HideType"))
    passengers = {}
    for path, actor in actors.values():
        if actor["Gyaml"] != "KorokCarryPassenger_Pair":
            continue
        for link in actor.get("Links", []):
            destination = actors.get(link["Dst"])
            if destination and destination[1]["Gyaml"] == "KorokCarryProgressKeeper":
                passengers[str(link["Dst"])] = actor["Translate"]
    for key, destination in koroks["CarryKorok"].items():
        progress = known_enum(f"KorokCarryProgress.{key}", ("Clear",))
        if not progress or key not in passengers:
            raise ValueError(f"Incomplete Korok route {key}")
        position = passengers[key]
        x, y, z = destination
        add("surface", "搬运呀哈哈", f"korok-carry:{key}", position, progress, seeds=2,
            destination={"x": round(x, 3), "height": round(y, 3), "north": round(-z, 3)})

    for key, (path, actor) in actors.items():
        name = actor["Gyaml"]
        position = actor["Translate"]
        layer = get_layer(path, position[1])
        drop = actor.get("Dynamic", {}).get("Drop__DropActor", "")
        reset = resets.get(key)
        progress = {"key": str(key), "kind": "guid", "value": 1} if reset == 80 else None
        category = None
        if name.startswith("TBox_") and reset == 80:
            category = "宝箱"
            if drop.startswith("Armor_"):
                category = "防具"
                progress = known_bool(f"IsGet.{drop}") or progress
            elif drop == "Obj_SageWill":
                category = "贤者的遗志"
            elif drop.startswith("Obj_TreasureMap"):
                category = "残旧的地图"
            elif drop.startswith("Obj_AutoBuilderDraftAssassin_"):
                category = "依盖队设计图"
                progress = known_bool(f"ReadyMadeAutoBuilderDraft.{drop}.IsOwned") or progress
        elif name == "Npc_CraftSignboard001":
            category = "卡邦达立牌"
        elif re.fullmatch(r"Npc_DressFairy_\d{2}", name):
            category = "大妖精"
        elif name.startswith("Enemy_") and reset in (88, 208):
            family = name.split("_")[1]
            category = BOSSES.get(family)
            if ("_Little" in name or "_Baby" in name or
                    family == "Zonau" and name_hash(f"IsDefeatBossEnemy.{key}") not in bools):
                category = None
            progress = {"key": str(key), "kind": "respawn", "value": 1}
        elif name.startswith("Obj_AutoBuilderDraft"):
            category = "设计图石板"
            progress = known_bool(f"ReadyMadeAutoBuilderDraft.{name}.IsOwned")
        if category:
            add(layer, category, f"actor:{key}", position, progress, actor=name, item=drop)

    points = [point for layer in layers.values() for point in layer]
    counts = Counter(point["category"] for point in points)
    expected = {"神庙": 152, "树根": 120, "鸟望台": 15, "呀哈哈": 800, "搬运呀哈哈": 100, "左纳乌制造机": 30}
    if any(counts[key] != value for key, value in expected.items()):
        raise ValueError(f"Unexpected 1.4.3 counts: {dict(counts)}")
    if len({p["id"] for p in points}) != len(points) or sum(p.get("seeds", 0) for p in points) != 1000:
        raise ValueError("Duplicate point identifiers or unexpected Korok seed total")
    return {"schema": 1, "game": "0100F2C0115B6000", "version": "1.4.3",
            "build_id": GAME_BUILD, "layers": layers}, counts


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("romfs", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    with RomFS(args.romfs) as fs:
        data, counts = build(fs)
    with args.output.open("x", encoding="utf-8") as output:
        json.dump(data, output, ensure_ascii=False, separators=(",", ":"))
        output.write("\n")
    print(json.dumps({"total": sum(counts.values()), "categories": dict(sorted(counts.items()))}, ensure_ascii=False))


if __name__ == "__main__":
    main()
