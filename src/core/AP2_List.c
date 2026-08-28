#include "AP2/AP2_List.h"

#include <stdlib.h>
#include <string.h>

#define ASSERT_LIST_EXISTS(l)                                                  \
  if (!(l))                                                                    \
  return false

#define ASSERT_LIST_EXISTS_ELSE_VOID(l)                                        \
  if (!(l))                                                                    \
  return

#define AP_LIST_INITIAL_CAPACITY 8

static size_t AP_List_NextCapacity(size_t current, size_t required) {
  size_t capacity = current;

  if (capacity == 0) {
    capacity = AP_LIST_INITIAL_CAPACITY;
  }

  while (capacity < required) {
    /*
     * Grow by 1.5x to reduce memory waste while still
     * maintaining amortized constant-time appends.
     */
    size_t growth = capacity / 2;

    if (growth == 0) {
      growth = 1;
    }

    /*
     * Prevent integer overflow.
     */
    if (capacity > SIZE_MAX - growth) {
      return required;
    }

    capacity += growth;
  }

  return capacity;
}

bool AP_InitList(AP_List *list, size_t element_size) {
  if (!list || element_size == 0) {
    return false;
  }

  list->data = NULL;
  list->element_size = element_size;
  list->size = 0;
  list->capacity = 0;

  return true;
}

void AP_DestroyList(AP_List *list) {
  if (!list) {
    return;
  }

  free(list->data);

  list->data = NULL;
  list->element_size = 0;
  list->size = 0;
  list->capacity = 0;
}

AP_List *AP_CreateList(size_t element_size) {
  AP_List *list = malloc(sizeof(AP_List));

  if (!list) {
    return NULL;
  }

  if (!AP_InitList(list, element_size)) {
    free(list);
    return NULL;
  }

  return list;
}

AP_List *AP_CreateListFrom(void **elements) {
  if (!elements) {
    return NULL;
  }

  AP_List *list = AP_CreateList(sizeof(void *));

  if (!list) {
    return NULL;
  }

  for (size_t i = 0; elements[i] != NULL; ++i) {
    if (!AP_ListAppend(list, &elements[i])) {
      AP_DestroyList(list);
      free(list);
      return NULL;
    }
  }

  return list;
}

bool AP_List_Reserve(AP_List *list, size_t capacity) {
  ASSERT_LIST_EXISTS(list);

  if (capacity <= list->capacity) {
    return true;
  }

  if (list->element_size != 0 && capacity > SIZE_MAX / list->element_size) {
    return false;
  }

  size_t bytes = capacity * list->element_size;

  void *new_data = realloc(list->data, bytes);

  if (!new_data) {
    return false;
  }

  list->data = new_data;
  list->capacity = capacity;

  return true;
}

bool AP_ListAppend(AP_List *list, const void *element) {
  ASSERT_LIST_EXISTS(list);

  if (!element) {
    return false;
  }

  if (list->size == SIZE_MAX) {
    return false;
  }

  size_t required = list->size + 1;

  if (required > list->capacity) {
    size_t new_capacity = AP_List_NextCapacity(list->capacity, required);

    if (!AP_List_Reserve(list, new_capacity)) {
      return false;
    }
  }

  unsigned char *destination =
      (unsigned char *)list->data + (list->size * list->element_size);

  memcpy(destination, element, list->element_size);

  list->size++;

  return true;
}

bool AP_ListInsert(AP_List *list, size_t index, const void *element) {
  ASSERT_LIST_EXISTS(list);

  if (!element) {
    return false;
  }

  /*
   * Insertion is allowed at list->size, which is equivalent
   * to appending.
   */
  if (index > list->size) {
    return false;
  }

  if (list->size == SIZE_MAX) {
    return false;
  }

  size_t required = list->size + 1;

  if (required > list->capacity) {
    size_t new_capacity = AP_List_NextCapacity(list->capacity, required);

    if (!AP_List_Reserve(list, new_capacity)) {
      return false;
    }
  }

  unsigned char *base = (unsigned char *)list->data;

  void *destination = base + (index * list->element_size);

  /*
   * Move existing elements one position to the right.
   */
  if (index < list->size) {
    memmove((unsigned char *)destination + list->element_size, destination,
            (list->size - index) * list->element_size);
  }

  memcpy(destination, element, list->element_size);

  list->size++;

  return true;
}

bool AP_ListRemove(AP_List *list, size_t index) {
  ASSERT_LIST_EXISTS(list);

  if (index >= list->size) {
    return false;
  }

  unsigned char *base = (unsigned char *)list->data;

  void *destination = base + (index * list->element_size);

  /*
   * Shift everything after the removed element left.
   */
  if (index < list->size - 1) {
    memmove(destination, (unsigned char *)destination + list->element_size,
            (list->size - index - 1) * list->element_size);
  }

  list->size--;

  return true;
}

bool AP_ListPop(AP_List *list, void *out) {
  ASSERT_LIST_EXISTS(list);

  if (list->size == 0) {
    return false;
  }

  size_t index = list->size - 1;

  unsigned char *base = (unsigned char *)list->data;

  void *source = base + (index * list->element_size);

  if (out) {
    memcpy(out, source, list->element_size);
  }

  list->size--;

  return true;
}

void *AP_ListGet(AP_List *list, size_t index) {
  if (!list || index >= list->size) {
    return NULL;
  }

  return (unsigned char *)list->data + (index * list->element_size);
}

const void *AP_ListGetConst(const AP_List *list, size_t index) {
  if (!list || index >= list->size) {
    return NULL;
  }

  return (const unsigned char *)list->data + (index * list->element_size);
}

void AP_ListClear(AP_List *list) {
  ASSERT_LIST_EXISTS_ELSE_VOID(list);

  list->size = 0;
}
