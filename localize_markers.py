"""Add offline translations to a locally generated map pack."""
import argparse
from collections import defaultdict
import json
from pathlib import Path
import re
import sys

from map_names import message_files, messages
from romfs import RomFS


FIELDS = ("name", "notes", "reward", "supplies")
MAP_PACK_LIMIT = 8 * 1024 * 1024
LANGUAGES = {
    "en": ("USen", "eng_Latn"),
    "ja": ("JPja", "jpn_Jpan"),
    "es": ("EUes", "spa_Latn"),
    "ru": ("EUru", "rus_Cyrl"),
    "fr": ("EUfr", "fra_Latn"),
    "de": ("EUde", "deu_Latn"),
}
LOCALES = Path(__file__).resolve().parent / "locales"
QUANTITY = re.compile(r"\s*[x×]\s*(\d+)$", re.IGNORECASE)


def game_labels(fs, region):
    archive = max(name for name in fs.index
                  if name.startswith(f"Mals/{region}.Product.") and name.endswith(".sarc.zs"))
    for filename, payload in message_files(fs.read(archive)):
        prefix = filename.split("/", 1)[0]
        if prefix not in {"ActorMsg", "LocationMsg", "ChallengeMsg"}:
            continue
        for key, value in messages(payload):
            if not value or "\n" in value:
                continue
            if prefix == "ActorMsg" and not key.endswith("_Name"):
                continue
            if prefix == "ChallengeMsg" and key != "Name":
                continue
            yield (filename, key), value


def exact_names(source, target):
    matches = defaultdict(set)
    for key, original in source.items():
        if translated := target.get(key):
            matches[original].add(translated)
    return {original: next(iter(matches)) for original, matches in matches.items() if len(matches) == 1}


def resolved_name(original, names):
    if original in names:
        return names[original]
    quantity = QUANTITY.search(original)
    if quantity and (name := names.get(original[:quantity.start()])):
        return f"{name} ×{quantity[1]}"
    return None


def localized_names(source, target, category_labels):
    names = {**category_labels, **exact_names(source, target)}
    names["锅"] = category_labels["烹饪点"]
    names["运输呀哈哈"] = category_labels["搬运呀哈哈"]
    if korok := names.get("克洛格"):
        names["呀哈哈"] = korok
    if seed := names.get("克洛格的果实"):
        names["呀哈哈种子"] = seed
        names["呀哈哈果实"] = seed
    return names


def marker_catalog(values, names, translator, tokenizer, language, code):
    translated = {}
    pending = set()
    for key in FIELDS:
        translations = translated[key] = {}
        for original in values[key]:
            if key == "notes":
                pending.update(line.strip() for line in original.split("\n") if line.strip())
            elif key == "supplies":
                pending.update(part for part in original.split("、") if not resolved_name(part, names))
            elif name := resolved_name(original, names):
                translations[original] = name
            else:
                pending.add(original)

    lines = {}
    queue = sorted(pending, key=lambda line: (len(line), line))
    print(f"{code}: translating {len(queue)} distinct strings", flush=True)
    for start in range(0, len(queue), 256):
        batch = queue[start:start+256]
        inputs = [["zho_Hans", *tokenizer.encode(line, out_type=str), "</s>"] for line in batch]
        results = translator.translate_batch(inputs, target_prefix=[[language]] * len(batch),
                                             max_batch_size=24, beam_size=1, max_decoding_length=128)
        for line, result in zip(batch, results, strict=True):
            translation = tokenizer.decode(result.hypotheses[0][1:]).strip()
            if not translation:
                raise ValueError(f"{code}: empty translation for {line!r}")
            lines[line] = translation

    for key in FIELDS:
        for original in values[key] - translated[key].keys():
            if key == "notes":
                value = "\n".join(lines[line.strip()] if line.strip() else "" for line in original.split("\n"))
            elif key == "supplies":
                separator = "、" if code == "ja" else ", "
                value = separator.join(resolved_name(part, names) or lines[part] for part in original.split("、"))
            else:
                value = lines[original]
            translated[key][original] = value
    return translated


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pack", type=Path)
    parser.add_argument("output", type=Path, help="Save alongside the pack's map-icons directory for installation.")
    parser.add_argument("--romfs", required=True, type=Path)
    parser.add_argument("--model", required=True, type=Path, help="CTranslate2 NLLB model with sentencepiece.bpe.model.")
    parser.add_argument("--threads", type=int, default=4)
    args = parser.parse_args()
    if args.threads < 1:
        parser.error("--threads must be positive.")

    try:
        import ctranslate2
        import sentencepiece
    except ImportError as error:
        parser.error(f"Install ctranslate2 and sentencepiece first ({error}).")
    pack = json.loads(args.pack.read_bytes())
    if pack.get("schema") != 1:
        parser.error("Expected a v1 map pack.")
    values = {key: {point[key] for layer in pack["layers"].values() for point in layer if point.get(key)}
              for key in FIELDS}
    translator = ctranslate2.Translator(str(args.model), compute_type="int8", intra_threads=args.threads)
    tokenizer = sentencepiece.SentencePieceProcessor(model_file=str(args.model / "sentencepiece.bpe.model"))
    with RomFS(args.romfs) as fs:
        source = dict(game_labels(fs, "CNzh"))
        marker_text = {}
        for code, (region, language) in LANGUAGES.items():
            labels = json.loads((LOCALES / f"{code}.json").read_bytes())["categories"]
            names = localized_names(source, dict(game_labels(fs, region)), labels)
            marker_text[code] = marker_catalog(values, names, translator, tokenizer, language, code)
    pack["marker_text"] = marker_text
    data = json.dumps(pack, ensure_ascii=False, separators=(",", ":")).encode("utf-8")
    if len(data) > MAP_PACK_LIMIT:
        parser.error("Translated pack exceeds the Mac runtime's 8 MiB limit.")
    args.output.write_bytes(data)
    print(f"Wrote {args.output}", flush=True)


if __name__ == "__main__":
    main()
