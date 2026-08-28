#ifndef AP2_STRING_H
#define AP2_STRING_H

#include <stdbool.h>
#include <stdint.h>

typedef struct AP_String {
  char *data;
  size_t length;
  size_t capacity;
} AP_String;

bool AP_InitString(AP_String *str);
void AP_DestroyString(AP_String *str);

AP_String *AP_CreateString();

bool AP_String_Reserve(AP_String *str, size_t capacity);
bool AP_String_AppendFixed(AP_String *str, const char *text, size_t length);
bool AP_String_Append(AP_String *str, const char *text);
bool AP_String_AppendChar(AP_String *str, char c);

void AP_String_Clear(AP_String *str);
bool AP_String_Insert(AP_String *str, size_t index, const char *text);
bool AP_String_Erase(AP_String *str, size_t index, size_t count);

const char *AP_StringCStr(const AP_String *str);

char AP_String_CharAt(AP_String *str, uint32_t index);

#endif
