#include "AP2/AP2_String.h"

#include <stdlib.h>
#include <string.h>

#define ASSERT_STRING_EXISTS(s)                                                \
  if (!s)                                                                      \
  return false

#define ASSERT_STRING_EXISTS_ELSE_VOID(s)                                      \
  if (!s)                                                                      \
  return

static size_t AP_String_NextCapacity(size_t current, size_t required) {
  size_t capacity = current;

  if (capacity == 0)
    capacity = AP_STRING_INITIAL_CAPACITY;

  while (capacity < required)
    capacity *= 2;

  return capacity;
}

bool AP_InitString(AP_String *str) {
  ASSERT_STRING_EXISTS(str);

  str->data = malloc(AP_STRING_INITIAL_CAPACITY);
  if (!str->data)
    return false;

  str->data[0] = '\0';
  str->length = 0;
  str->capacity = AP_STRING_INITIAL_CAPACITY;

  return true;
}

void AP_DestroyString(AP_String *str) {
  ASSERT_STRING_EXISTS_ELSE_VOID(str);

  free(str->data);

  str->data = NULL;
  str->length = 0;
  str->capacity = 0;
}

AP_String *AP_CreateString() {
  AP_String *str = (AP_String *)malloc(sizeof(struct AP_String));
  AP_InitString(str);
  return str;
}

AP_String *AP_EmptyString() { return AP_CreateStringFrom("\0"); }

AP_String *AP_CreateStringFrom(const char *init) {
  AP_String *str = AP_CreateString();

  if (!init)
    return str;

  size_t length = strlen(init);
  str->data = malloc(length + 1);
  if (!str->data)
    return str;

  memcpy(str->data, init, length + 1);

  str->length = length;
  str->capacity = length + 1;
  return str;
}

bool AP_String_Reserve(AP_String *str, size_t capacity) {
  ASSERT_STRING_EXISTS(str);

  if (capacity <= str->capacity)
    return true;

  char *new_data = realloc(str->data, capacity);

  if (!new_data)
    return false;

  str->data = new_data;
  str->capacity = capacity;

  return true;
}

bool AP_String_AppendFixed(AP_String *str, const char *text, size_t length) {
  ASSERT_STRING_EXISTS(str);

  if (!text)
    return false;

  size_t required = str->length + length + 1;

  if (required > str->capacity) {
    size_t new_capacity = AP_String_NextCapacity(str->capacity, required);

    if (!AP_String_Reserve(str, new_capacity))
      return false;
  }

  memcpy(str->data + str->length, text, length);

  str->length += length;
  str->data[str->length] = '\0';

  return true;
}

bool AP_String_Append(AP_String *str, const char *text) {
  return AP_String_AppendFixed(str, text, strlen(text));
}

bool AP_String_AppendChar(AP_String *str, char c) {
  ASSERT_STRING_EXISTS(str);

  size_t required = str->length + 2;

  if (required > str->capacity) {
    size_t new_capacity = AP_String_NextCapacity(str->capacity, required);

    if (!AP_String_Reserve(str, new_capacity))
      return false;
  }

  str->data[str->length++] = c;
  str->data[str->length] = '\0';

  return true;
}

void AP_String_Clear(AP_String *str) {
  if (!str || !str->data)
    return;

  str->length = 0;
  str->data[0] = '\0';
}

bool AP_String_Insert(AP_String *str, size_t index, const char *text) {
  if (!str || !text)
    return false;

  if (index > str->length)
    return false;

  size_t text_length = strlen(text);
  size_t required = str->length + text_length + 1;

  if (required > str->capacity) {
    size_t new_capacity = AP_String_NextCapacity(str->capacity, required);

    if (!AP_String_Reserve(str, new_capacity))
      return false;
  }

  memmove(str->data + index + text_length, str->data + index,
          str->length - index + 1);

  memcpy(str->data + index, text, text_length);

  str->length += text_length;

  return true;
}

bool AP_String_Erase(AP_String *str, size_t index, size_t count) {
  ASSERT_STRING_EXISTS(str);

  if (index > str->length)
    return false;

  if (count > str->length - index)
    count = str->length - index;

  memmove(str->data + index, str->data + index + count,
          str->length - index - count + 1);

  str->length -= count;

  return true;
}

const char *AP_StringCStr(const AP_String *str) {
  if (!str)
    return NULL;

  return str->data;
}

char AP_String_CharAt(AP_String *str, uint32_t index) {
  if (!str || !str->data || index >= str->length)
    return '\0';

  return str->data[index];
}
