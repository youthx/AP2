/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_String.h"

#include "AP2/AP2_Error.h"

#include <stdlib.h>
#include <string.h>

static bool AP_StringGrow(AP_String *str, size_t needed) {
  size_t capacity;
  char *data;

  if (needed <= str->capacity) {
    return true;
  }

  capacity = str->capacity == 0 ? 16 : str->capacity;
  while (capacity < needed) {
    capacity *= 2;
  }

  data = (char *)realloc(str->data, capacity);
  if (data == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to grow string");
    return false;
  }

  str->data = data;
  str->capacity = capacity;
  return true;
}

bool AP_InitString(AP_String *str) {
  if (str == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String cannot be NULL");
    return false;
  }

  str->data = NULL;
  str->length = 0;
  str->capacity = 0;
  return AP_StringGrow(str, 1);
}

void AP_DestroyString(AP_String *str) {
  if (str == NULL) {
    return;
  }

  free(str->data);
  str->data = NULL;
  str->length = 0;
  str->capacity = 0;
}

AP_String *AP_CreateString(void) {
  AP_String *str = (AP_String *)malloc(sizeof(AP_String));

  if (str == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate string");
    return NULL;
  }

  if (!AP_InitString(str)) {
    free(str);
    return NULL;
  }

  return str;
}

void AP_FreeString(AP_String *str) {
  if (str == NULL) {
    return;
  }

  AP_DestroyString(str);
  free(str);
}

bool AP_StringReserve(AP_String *str, size_t capacity) {
  if (str == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String cannot be NULL");
    return false;
  }

  return AP_StringGrow(str, capacity + 1);
}

bool AP_StringAppendFixed(AP_String *str, const char *text, size_t length) {
  if (str == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String cannot be NULL");
    return false;
  }

  if (length == 0) {
    return true;
  }

  if (text == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Append text cannot be NULL");
    return false;
  }

  if (!AP_StringGrow(str, str->length + length + 1)) {
    return false;
  }

  memcpy(str->data + str->length, text, length);
  str->length += length;
  str->data[str->length] = '\0';
  return true;
}

bool AP_StringAppend(AP_String *str, const char *text) {
  if (text == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Append text cannot be NULL");
    return false;
  }

  return AP_StringAppendFixed(str, text, strlen(text));
}

bool AP_StringAppendChar(AP_String *str, char c) {
  return AP_StringAppendFixed(str, &c, 1);
}

void AP_StringClear(AP_String *str) {
  if (str == NULL || str->data == NULL) {
    return;
  }

  str->length = 0;
  str->data[0] = '\0';
}

bool AP_StringInsert(AP_String *str, size_t index, const char *text) {
  size_t length;

  if (str == NULL || text == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String insert arguments invalid");
    return false;
  }

  if (index > str->length) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String insert index out of range");
    return false;
  }

  length = strlen(text);
  if (!AP_StringGrow(str, str->length + length + 1)) {
    return false;
  }

  memmove(str->data + index + length, str->data + index,
          str->length - index + 1);
  memcpy(str->data + index, text, length);
  str->length += length;
  return true;
}

bool AP_StringErase(AP_String *str, size_t index, size_t count) {
  if (str == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String cannot be NULL");
    return false;
  }

  if (index > str->length) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "String erase index out of range");
    return false;
  }

  if (index + count > str->length) {
    count = str->length - index;
  }

  memmove(str->data + index, str->data + index + count,
          str->length - index - count + 1);
  str->length -= count;
  return true;
}

const char *AP_StringCStr(const AP_String *str) {
  if (str == NULL || str->data == NULL) {
    return "";
  }

  return str->data;
}

char AP_StringCharAt(const AP_String *str, size_t index) {
  if (str == NULL || str->data == NULL || index >= str->length) {
    return '\0';
  }

  return str->data[index];
}
