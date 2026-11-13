"""Read placement tables from an extracted Tears of the Kingdom RomFS."""
import json
import os
from pathlib import Path
import struct
from compression import zstd


class RomFS:
    def __init__(self, root):
        self.root = Path(root)
        index = self.root / "index.json"
        self.image = None
        if index.is_file() and (self.root / "romfs.bin").is_file():
            entries = json.loads(index.read_text())["files"]
            self.index = {row["path"]: (row["offset"], row["size"]) for row in entries}
            self.image = (self.root / "romfs.bin").open("rb")
        else:
            self.index = {p.relative_to(self.root).as_posix(): None
                          for p in self.root.rglob("*") if p.is_file()}
        self.dictionaries = {}
        for payload in sarc_files(self.read("Pack/ZsDic.pack.zs")):
            dictionary = zstd.ZstdDict(payload)
            self.dictionaries[dictionary.dict_id] = dictionary

    def __enter__(self):
        return self

    def __exit__(self, *_):
        if self.image:
            self.image.close()

    def read(self, name):
        if self.image:
            offset, size = self.index[name]
            data = os.pread(self.image.fileno(), size, offset)
            if len(data) != size:
                raise ValueError(f"Truncated RomFS entry {name}")
        else:
            data = (self.root / name).read_bytes()
        if name.endswith(".zs"):
            info = zstd.get_frame_info(data)
            if info.dictionary_id:
                return zstd.decompress(data, zstd_dict=self.dictionaries[info.dictionary_id])
            return zstd.decompress(data)
        return data

    def byml(self, name):
        return Byml(self.read(name)).read()


def sarc_files(data):
    if data[:4] != b"SARC" or data[6:8] != b"\xff\xfe":
        raise ValueError("Expected a little-endian SARC")
    header, = struct.unpack_from("<H", data, 4)
    start, = struct.unpack_from("<I", data, 12)
    if data[header:header + 4] != b"SFAT":
        raise ValueError("Missing SARC file table")
    size, count = struct.unpack_from("<HH", data, header + 4)
    for i in range(count):
        low, high = struct.unpack_from("<II", data, header + size + i * 16 + 8)
        if not 0 <= low <= high <= len(data) - start:
            raise ValueError("Invalid SARC member")
        yield data[start + low:start + high]


class Byml:
    def __init__(self, data):
        if data[:2] not in (b"YB", b"BY"):
            raise ValueError("Invalid BYML header")
        self.data = data
        self.byteorder = "little" if data[:2] == b"YB" else "big"
        self.endian = "<" if self.byteorder == "little" else ">"
        keys, strings, self.root = struct.unpack_from(self.endian + "III", data, 4)
        self.keys = self.strings(keys)
        self.values = self.strings(strings)

    def strings(self, offset):
        if not offset:
            return []
        if self.data[offset] != 0xC2:
            raise ValueError("Invalid BYML string table")
        count = self.u24(offset + 1)
        table = struct.unpack_from(f"{self.endian}{count + 1}I", self.data, offset + 4)
        return [self.data[offset + a:offset + b].split(b"\0", 1)[0].decode("utf-8")
                for a, b in zip(table, table[1:])]

    def u24(self, offset):
        return int.from_bytes(self.data[offset:offset + 3], self.byteorder)

    def u32(self, offset):
        return struct.unpack_from(self.endian + "I", self.data, offset)[0]

    def read(self):
        return self.container(self.root)

    def container(self, offset):
        kind = self.data[offset]
        count = self.u24(offset + 1)
        start = offset + 4
        if kind == 0xC0:
            values = start + ((count + 3) & ~3)
            return [self.value(self.data[start + i], values + 4 * i) for i in range(count)]
        if kind == 0xC1:
            return {self.keys[self.u24(row)]: self.value(self.data[row + 3], row + 4)
                    for row in range(start, start + 8 * count, 8)}
        raise ValueError(f"Unsupported BYML container 0x{kind:02X}")

    def value(self, kind, offset):
        word = self.u32(offset)
        if kind in (0xC0, 0xC1):
            return self.container(word)
        if kind == 0xA0:
            return self.values[word]
        if kind == 0xD0:
            return bool(word)
        if kind == 0xD3:
            return word
        formats = {0xD1: "i", 0xD2: "f", 0xD4: "q", 0xD5: "Q", 0xD6: "d"}
        if kind in formats:
            return struct.unpack_from(self.endian + formats[kind], self.data,
                                      word if kind >= 0xD4 else offset)[0]
        if kind == 0xFF:
            return None
        raise ValueError(f"Unsupported BYML value 0x{kind:02X}")
