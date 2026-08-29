/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_STRING_H
#define AP2_STRING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AP_String {
  char *data;
  size_t length;
  size_t capacity;
} AP_String;

bool AP_InitString(AP_String *str);

void AP_DestroyString(AP_String *str);

AP_String *AP_CreateString(void);

void AP_FreeString(AP_String *str);

bool AP_StringReserve(AP_String *str, size_t capacity);

bool AP_StringAppendFixed(AP_String *str, const char *text, size_t length);

bool AP_StringAppend(AP_String *str, const char *text);

bool AP_StringAppendChar(AP_String *str, char c);

void AP_StringClear(AP_String *str);

bool AP_StringInsert(AP_String *str, size_t index, const char *text);

bool AP_StringErase(AP_String *str, size_t index, size_t count);

const char *AP_StringCStr(const AP_String *str);

char AP_StringCharAt(const AP_String *str, size_t index);

#define AP_String_Reserve AP_StringReserve
#define AP_String_AppendFixed AP_StringAppendFixed
#define AP_String_Append AP_StringAppend
#define AP_String_AppendChar AP_StringAppendChar
#define AP_String_Clear AP_StringClear
#define AP_String_Insert AP_StringInsert
#define AP_String_Erase AP_StringErase
#define AP_String_CharAt AP_StringCharAt

#ifdef __cplusplus
}
#endif

#endif /* AP2_STRING_H */
