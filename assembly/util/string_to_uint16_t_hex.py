#!/usr/bin/env python3
import sys

def string_to_uint16_t_hex(string: str) -> list[str]:
    uint16_t_hexes = []
    for char in string:
        uint16_t_hexes.append(f'00{ord(char):x}'.upper())
    uint16_t_hexes.append('0000')
    return uint16_t_hexes

def main() -> None:
    if len(sys.argv) != 2:
        sys.exit("usage: string_to_uint16_t_hex.py <string>")
    string = sys.argv[1]
    uint16_t_hexes = string_to_uint16_t_hex(string)
    for uint16_t_hex in uint16_t_hexes:
        print(uint16_t_hex)

if __name__ == "__main__":
    main()
