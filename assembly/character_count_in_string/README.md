## Order of business

1. Write `assembly/character_count_in_string/code.asm`
2. Write `assembly/character_count_in_string/code.hexc`, with the help of `assembly/util/string_to_uint16_t_hex.py`
3. Write `assembly/character_count_in_string/code_with_labels.hexc`
4. Produce `assembly/character_count_in_string/code_with_labels.hex` with

```sh
assembly/util/hexc_to_hex.py assembly/character_count_in_string/code_with_labels.hexc
```

5. Produce `assembly/character_count_in_string/code_with_labels.obj` with

```sh
assembly/util/hexc_to_hex.py assembly/character_count_in_string/code_with_labels.hexc
xxd -r -p assembly/character_count_in_string/code_with_labels.hex > assembly/character_count_in_string/code_with_labels.obj
```

6. Run `lc_3` on with `make lc_3 && ./lc_3 assembly/character_count_in_string/code_with_labels.obj`
