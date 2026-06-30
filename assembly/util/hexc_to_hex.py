#!/usr/bin/env python3
"""Strip a .hexc file (hex words + `;` comments) down to compact hex.

Removes everything from the first `;` on each line, drops blank lines, and
writes one cleaned word per line. Bit-field spacing inside a word (e.g.
`E 000 ? ???? ????`) is preserved so you can still see the boundaries while
filling in label offsets.

Usage:
    uv run scratchpad/hexc_to_hex.py character_count_in_string.hexc
    uv run scratchpad/hexc_to_hex.py in.hexc out.hex
"""
import sys
from pathlib import Path


def hexc_to_hex(text: str) -> str:
    lines = []
    for line in text.splitlines():
        line = line.split(";", 1)[0].strip()  # drop comment, trim
        if line:                               # skip blank / comment-only lines
            lines.append(line)
    return "\n".join(lines) + "\n"


def main() -> None:
    if len(sys.argv) < 2:
        sys.exit("usage: hexc_to_hex.py <in.hexc> [out.hex]")
    src = Path(sys.argv[1])
    dst = Path(sys.argv[2]) if len(sys.argv) > 2 else src.with_suffix(".hex")
    dst.write_text(hexc_to_hex(src.read_text()))
    print(f"wrote {dst}")


if __name__ == "__main__":
    main()
