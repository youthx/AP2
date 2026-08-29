/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_LIST_H
#define AP2_LIST_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AP_List {
  void *data;

  size_t element_size;
  size_t size;
  size_t capacity;
} AP_List;

/* Lifecycle */

bool AP_InitList(AP_List *list, size_t element_size);
void AP_DestroyList(AP_List *list);

AP_List *AP_CreateList(size_t element_size);
AP_List *AP_CreateListFrom(void **elements);

/* Capacity */

bool AP_List_Reserve(AP_List *list, size_t capacity);

/* Modification */

bool AP_ListAppend(AP_List *list, const void *element);
bool AP_ListInsert(AP_List *list, size_t index, const void *element);

bool AP_ListRemove(AP_List *list, size_t index);
bool AP_ListPop(AP_List *list, void *out);

void AP_ListClear(AP_List *list);

/* Access */

void *AP_ListGet(AP_List *list, size_t index);
const void *AP_ListGetConst(const AP_List *list, size_t index);

#ifdef __cplusplus
}
#endif

#endif /* AP2_LIST_H */
