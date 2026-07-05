#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "core/util.h"

static void test_trim(void) {
  char *string;
  char *trimmed;
  
  string = "  hi ";
  trimmed = trim(string);
  assert(strcmp(trimmed, "hi") == 0);
  free(trimmed);

  string = "";
  trimmed = trim(string);
  assert(strcmp(trimmed, "") == 0);
  free(trimmed);
}

static void test_split(void) {
  char *string;
  char delimiter;
  
  string = "0,1,2";
  delimiter = ',';
  StringList *list = split(string, delimiter);
  assert(list->length == 3);
  assert(strcmp(list->data[0], "0") == 0);
  assert(strcmp(list->data[1], "1") == 0);
  assert(strcmp(list->data[2], "2") == 0);
  free_string_list(list);

  string = "0 , 1 , 2";
  delimiter = ',';
  list = split(string, delimiter);
  assert(list->length == 3);
  assert(strcmp(list->data[0], "0") == 0);
  assert(strcmp(list->data[1], "1") == 0);
  assert(strcmp(list->data[2], "2") == 0);
  free_string_list(list);
  
  string = "";
  delimiter = ',';
  list = split(string, delimiter);
  assert(list->length == 1);
  free_string_list(list);
  
  string = ",";
  delimiter = ',';
  list = split(string, delimiter);
  assert(list->length == 2);
  free_string_list(list);
  
  string = "0,,1,2";
  delimiter = ',';
  list = split(string, delimiter);
  assert(list->length == 4);
  assert(strcmp(list->data[0], "0") == 0);
  assert(strcmp(list->data[1], "") == 0);
  assert(strcmp(list->data[2], "1") == 0);
  assert(strcmp(list->data[3], "2") == 0);
  free_string_list(list);
}

int main(void) {
  test_trim();
  test_split();
  printf("test_util passed\n");
  return 0;
}