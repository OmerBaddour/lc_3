#ifndef ASSEMBLER_SYMBOL_TABLE_H
#define ASSEMBLER_SYMBOL_TABLE_H

#include <stdint.h>

/* label -> address, a growable array of records. Linear scan on lookup, exactly
   like register_by_name — at assembly scale (tens/hundreds of labels) a hash
   table would be premature.

   Public (no longer static in assembler.c) so the per-operation encode functions
   in assembler/operation.c can resolve labels through symbol_table_get. */
typedef struct {
  char *label;        /* OWNED copy of the label text */
  uint16_t address;
} Symbol;

typedef struct SymbolTable {
  Symbol *data;
  int length;
  int capacity;
} SymbolTable;

void symbol_table_insert(SymbolTable *table, const char *label, uint16_t address);

/* Returns 1 and writes *out on hit; 0 on miss. */
int symbol_table_get(const SymbolTable *table, const char *label, uint16_t *out);

void symbol_table_free(SymbolTable *table);

#endif /* ASSEMBLER_SYMBOL_TABLE_H */
