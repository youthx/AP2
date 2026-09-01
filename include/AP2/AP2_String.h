/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_STRING_H
#define AP2_STRING_H

#include <stdarg.h>
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

#ifndef AP2_PRINTF_FORMAT
#if defined(__GNUC__) || defined(__clang__)
#define AP2_PRINTF_FORMAT(fmt_index, arg_index)                                \
  __attribute__((format(printf, fmt_index, arg_index)))
#else
#define AP2_PRINTF_FORMAT(fmt_index, arg_index)
#endif
#endif

bool AP_StringFormat(AP_String *str, const char *format, ...)
    AP2_PRINTF_FORMAT(2, 3);

bool AP_StringFormatV(AP_String *str, const char *format, va_list args);

bool AP_StringAppendFormat(AP_String *str, const char *format, ...)
    AP2_PRINTF_FORMAT(2, 3);

bool AP_StringAppendFormatV(AP_String *str, const char *format, va_list args);

#define AP_String_Reserve AP_StringReserve
#define AP_String_AppendFixed AP_StringAppendFixed
#define AP_String_Append AP_StringAppend
#define AP_String_AppendChar AP_StringAppendChar
#define AP_String_Clear AP_StringClear
#define AP_String_Insert AP_StringInsert
#define AP_String_Erase AP_StringErase
#define AP_String_CharAt AP_StringCharAt
#define AP_String_Format AP_StringFormat
#define AP_String_AppendFormat AP_StringAppendFormat

#ifdef __cplusplus
}
#endif

#endif /* AP2_STRING_H */
