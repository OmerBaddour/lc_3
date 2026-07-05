#include <stdlib.h>
#include <string.h>
#include "assembler/symbol_table.h"

void symbol_table_insert(SymbolTable *table, const char *label, uint16_t address) {
  if (table->length == table->capacity) {
    table->capacity = table->capacity ? table->capacity * 2 : 8;
    table->data = realloc(table->data, (size_t) table->capacity * sizeof *table->data);
  }
  table->data[table->length].label = strdup(label);
  table->data[table->length].address = address;
  table->length++;
}

int symbol_table_get(const SymbolTable *table, const char *label, uint16_t *out) {
  for (int i = 0; i < table->length; i++) {
    if (strcmp(table->data[i].label, label) == 0) {
      *out = table->data[i].address;
      return 1;
    }
  }
  return 0;
}

void symbol_table_free(SymbolTable *table) {
  for (int i = 0; i < table->length; i++) {
    free(table->data[i].label);
  }
  free(table->data);
}
