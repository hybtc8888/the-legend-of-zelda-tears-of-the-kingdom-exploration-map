"""Read player-facing fallback names directly from the local Simplified Chinese game archive."""
import re
from struct import unpack_from

from romfs import sarc_files


def message_files(data):
    table, = unpack_from("<H", data, 4)
    table_size, count = unpack_from("<HH", data, table + 4)
    nodes = table + table_size
    names = nodes + count * 16
    name_size, = unpack_from("<H", data, names + 4)
    for index, payload in enumerate(sarc_files(data)):
        flags, = unpack_from("<I", data, nodes + 16 * index + 4)
        if flags >> 24:
            start = names + name_size + (flags & 0xFFFFFF) * 4
            yield data[start:data.index(b"\0", start)].decode(), payload


def messages(data):
    if data[:10] != b"MsgStdBn\xff\xfe" or data[12] != 1:
        raise ValueError("Expected a little-endian UTF-16 message table")
    sections = {}
    offset = 32
    for _ in range(unpack_from("<H", data, 14)[0]):
        size, = unpack_from("<I", data, offset + 4)
        sections[data[offset:offset + 4]] = data[offset + 16:offset + 16 + size]
        offset += (16 + size + 15) & ~15
    labels = sections[b"LBL1"]
    texts = sections[b"TXT2"]
    values = unpack_from(f"<{unpack_from('<I', texts)[0]}I", texts, 4)
    for index in range(unpack_from("<I", labels)[0]):
        count, start = unpack_from("<II", labels, 4 + index * 8)
        for _ in range(count):
            length = labels[start]
            name = labels[start + 1:start + 1 + length].decode()
            entry, = unpack_from("<I", labels, start + 1 + length)
            start += length + 5
            raw = texts[values[entry]:values[entry + 1] if entry + 1 < len(values) else len(texts)]
            text = []
            at = 0
            while at + 2 <= len(raw):
                char, = unpack_from("<H", raw, at)
                if not char:
                    break
                if char == 14 and at + 8 <= len(raw):
                    arg_size, = unpack_from("<H", raw, at + 6)
                    at += 8 + arg_size
                    continue
                if char == 15:
                    at += 6
                    continue
                text.append(raw[at:at + 2])
                at += 2
            yield name, b"".join(text).decode("utf-16-le", errors="replace").strip()


def game_names(fs, points):
    archive = max(path for path in fs.index if path.startswith("Mals/CNzh.Product.") and path.endswith(".sarc.zs"))
    wanted = {"LocationMsg/Location.msbt", "LocationMsg/Dungeon.msbt", "ActorMsg/PouchContent.msbt",
              "ActorMsg/PictureBook.msbt", "ActorMsg/AutoBuilderDraft.msbt", "ActorMsg/Npc.msbt"}
    lookup = {}
    for filename, payload in message_files(fs.read(archive)):
        if filename in wanted:
            lookup.update(messages(payload))

    def find(code):
        while code:
            value = lookup.get(code + "_Name") or lookup.get(code)
            if value:
                return value
            code, separator, _ = code.rpartition("_")
            if not separator:
                break
        return ""

    for point in points:
        actor = point.get("actor", "")
        name = lookup.get(point.get("game_name", "")) or find(actor)
        reward = find(point.get("item", ""))
        if name or reward:
            point["name"] = name or reward
        if reward:
            point["reward"] = reward
        if "contents" in point:
            supplies = [find(re.sub(r"_A_\d+$", "_A_01", code)) for code in point["contents"]]
            point["supplies"] = "、".join(filter(None, supplies))
