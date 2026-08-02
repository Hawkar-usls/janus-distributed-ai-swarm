#!/usr/bin/env python3
"""Assemble ordered JANUS Arduino source fragments into IDE-ready sketches."""

from __future__ import annotations

import argparse
from pathlib import Path

TARGETS = {
    "anchor": ("firmware/anchor", "Anchor", "Anchor_part_*.ino"),
    "golcron": ("firmware/golcron", "Golcron", "Golcron_part_*.ino"),
}


def assemble(repo_root: Path, output_root: Path, target: str) -> Path:
    source_dir_rel, sketch_name, fragment_pattern = TARGETS[target]
    source_dir = repo_root / source_dir_rel
    primary = source_dir / f"{sketch_name}.ino"
    fragments = sorted(source_dir.glob(fragment_pattern))

    if not primary.is_file():
        raise FileNotFoundError(f"missing primary sketch: {primary}")
    if not fragments:
        raise FileNotFoundError(f"no fragments matched: {source_dir / fragment_pattern}")

    output_dir = output_root / sketch_name
    output_dir.mkdir(parents=True, exist_ok=True)
    output_file = output_dir / f"{sketch_name}.ino"

    sources = [primary, *fragments]
    with output_file.open("w", encoding="utf-8", newline="\n") as out:
        for index, source in enumerate(sources):
            text = source.read_text(encoding="utf-8-sig")
            if index:
                out.write("\n")
            out.write(text.rstrip("\r\n"))
            out.write("\n")

    assembled = output_file.read_text(encoding="utf-8")
    if assembled.count("void setup()") != 1:
        raise RuntimeError(f"{target}: expected exactly one setup()")
    if assembled.count("void loop()") != 1:
        raise RuntimeError(f"{target}: expected exactly one loop()")

    print(
        f"assembled {target}: {len(sources)} source files -> {output_file} "
        f"({assembled.count(chr(10))} lines, {output_file.stat().st_size} bytes)"
    )
    return output_file


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--target", choices=["all", *TARGETS], default="all")
    parser.add_argument("--output-root", type=Path, default=Path("build"))
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    selected = TARGETS if args.target == "all" else {args.target: TARGETS[args.target]}
    for target in selected:
        assemble(repo_root, args.output_root.resolve(), target)


if __name__ == "__main__":
    main()
