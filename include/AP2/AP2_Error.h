/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_ERROR_H
#define AP2_ERROR_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Error codes
 * --------------------------------------------------------- */

typedef enum AP_ErrorCode {
  AP_ERROR_NONE = 0,

  AP_ERROR_UNKNOWN,
  AP_ERROR_INVALID_ARGUMENT,
  AP_ERROR_INVALID_STATE,
  AP_ERROR_OUT_OF_MEMORY,
  AP_ERROR_NOT_INITIALIZED,
  AP_ERROR_ALREADY_INITIALIZED,
  AP_ERROR_NOT_FOUND,
  AP_ERROR_UNSUPPORTED,
  AP_ERROR_INITIALIZATION_FAILED,
  AP_ERROR_OPERATION_FAILED,

  AP_ERROR_COUNT
} AP_ErrorCode;

/* ---------------------------------------------------------
 * Error information
 * --------------------------------------------------------- */

typedef struct AP_Error {
  AP_ErrorCode code;

  const char *message;

  const char *file;
  const char *function;

  int line;
} AP_Error;

/* ---------------------------------------------------------
 * Error state
 * --------------------------------------------------------- */

void AP_ClearError(void);

bool AP_HasError(void);

AP_ErrorCode AP_GetErrorCode(void);

const AP_Error *AP_GetError(void);

const char *AP_GetErrorMessage(void);

/* ---------------------------------------------------------
 * Error creation
 * --------------------------------------------------------- */

void AP_SetError(AP_ErrorCode code, const char *message, const char *file,
                 const char *function, int line);

/* ---------------------------------------------------------
 * Convenience macros
 * --------------------------------------------------------- */

#define AP_SET_ERROR(code, message)                                            \
  AP_SetError((code), (message), __FILE__, __func__, __LINE__)

#define AP_RETURN_ERROR(code, message)                                         \
  do {                                                                         \
    AP_SET_ERROR((code), (message));                                           \
    return false;                                                              \
  } while (0)

#define AP_RETURN_NULL_ERROR(code, message)                                    \
  do {                                                                         \
    AP_SET_ERROR((code), (message));                                           \
    return NULL;                                                               \
  } while (0)

#define AP_RETURN_VOID_ERROR(code, message)                                    \
  do {                                                                         \
    AP_SET_ERROR((code), (message));                                           \
    return;                                                                    \
  } while (0)

/* ---------------------------------------------------------
 * Error strings
 * --------------------------------------------------------- */

const char *AP_ErrorCodeName(AP_ErrorCode code);

#ifdef __cplusplus
}
#endif

#endif /* AP2_ERROR_H */
