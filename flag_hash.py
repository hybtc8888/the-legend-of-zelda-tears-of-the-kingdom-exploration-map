"""GameData name hashes, using the 32-bit MurmurHash3 algorithm."""


def name_hash(name):
    data = name.encode("utf-8")
    mask = 0xFFFFFFFF

    def rotate(value, bits):
        return ((value << bits) | (value >> (32 - bits))) & mask

    def mix(value):
        return rotate(value * 0xCC9E2D51 & mask, 15) * 0x1B873593 & mask

    h = 0
    complete = len(data) & ~3
    for i in range(0, complete, 4):
        h = rotate(h ^ mix(int.from_bytes(data[i:i + 4], "little")), 13)
        h = (5 * h + 0xE6546B64) & mask
    h ^= mix(int.from_bytes(data[complete:], "little")) ^ len(data)
    h = ((h ^ (h >> 16)) * 0x85EBCA6B) & mask
    h = ((h ^ (h >> 13)) * 0xC2B2AE35) & mask
    return h ^ (h >> 16)
