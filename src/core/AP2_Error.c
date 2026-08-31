/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 */

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------
 * Thread-local error state
 * --------------------------------------------------------- */

static _Thread_local AP_Error g_error = {.code = AP_ERROR_NONE,
                                         .message = NULL,
                                         .file = NULL,
                                         .function = NULL,
                                         .line = 0};

/* Static buffer for formatted messages (thread-local) */
static _Thread_local char g_error_buffer[1024];

/* ---------------------------------------------------------
 * Error code names
 * --------------------------------------------------------- */

const char *AP_ErrorCodeName(AP_ErrorCode code) {
  switch (code) {
  case AP_ERROR_NONE:
    return "NONE";
  case AP_ERROR_UNKNOWN:
    return "UNKNOWN";
  case AP_ERROR_INVALID_ARGUMENT:
    return "INVALID_ARGUMENT";
  case AP_ERROR_INVALID_STATE:
    return "INVALID_STATE";
  case AP_ERROR_OUT_OF_MEMORY:
    return "OUT_OF_MEMORY";
  case AP_ERROR_NOT_INITIALIZED:
    return "NOT_INITIALIZED";
  case AP_ERROR_ALREADY_INITIALIZED:
    return "ALREADY_INITIALIZED";
  case AP_ERROR_NOT_FOUND:
    return "NOT_FOUND";
  case AP_ERROR_UNSUPPORTED:
    return "UNSUPPORTED";
  case AP_ERROR_INITIALIZATION_FAILED:
    return "INITIALIZATION_FAILED";
  case AP_ERROR_OPERATION_FAILED:
    return "OPERATION_FAILED";
  default:
    return "UNKNOWN";
  }
}

/* ---------------------------------------------------------
 * Error state
 * --------------------------------------------------------- */

void AP_ClearError(void) {
  g_error.code = AP_ERROR_NONE;
  g_error.message = NULL;
  g_error.file = NULL;
  g_error.function = NULL;
  g_error.line = 0;
}

bool AP_HasError(void) { return g_error.code != AP_ERROR_NONE; }

AP_ErrorCode AP_GetErrorCode(void) { return g_error.code; }

const AP_Error *AP_GetError(void) { return AP_HasError() ? &g_error : NULL; }

const char *AP_GetErrorMessage(void) {
  return AP_HasError() ? g_error.message : NULL;
}

/* ---------------------------------------------------------
 * Simple error (no formatting)
 * --------------------------------------------------------- */

void AP_SetError(AP_ErrorCode code, const char *message, const char *file,
                 const char *function, int line) {
  if (code == AP_ERROR_NONE) {
    AP_ClearError();
    return;
  }

  g_error.code = code;
  g_error.message = message;
  g_error.file = file;
  g_error.function = function;
  g_error.line = line;

  AP_Log(AP_LOG_ERROR, "[%s] %s (%s:%d, %s)", AP_ErrorCodeName(code),
         message ? message : "No error message", file ? file : "unknown", line,
         function ? function : "unknown");
}

/* ---------------------------------------------------------
 * Formatted error (printf-style)
 * --------------------------------------------------------- */

void AP_SetErrorF(AP_ErrorCode code, const char *file, const char *function,
                  int line, const char *fmt, ...) {
  if (code == AP_ERROR_NONE) {
    AP_ClearError();
    return;
  }

  va_list args;
  va_start(args, fmt);
  vsnprintf(g_error_buffer, sizeof(g_error_buffer), fmt, args);
  va_end(args);

  g_error.code = code;
  g_error.message = g_error_buffer;
  g_error.file = file;
  g_error.function = function;
  g_error.line = line;

  AP_Log(AP_LOG_ERROR, "[%s] %s (%s:%d, %s)", AP_ErrorCodeName(code),
         g_error_buffer, file ? file : "unknown", line,
         function ? function : "unknown");
}
