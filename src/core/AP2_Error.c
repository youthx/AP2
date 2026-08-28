#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include <string.h>

/* ---------------------------------------------------------
 * Internal state
 * --------------------------------------------------------- */

static _Thread_local AP_Error g_error = {.code = AP_ERROR_NONE,
                                         .message = NULL,
                                         .file = NULL,
                                         .function = NULL,
                                         .line = 0};

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

const AP_Error *AP_GetError(void) {
  if (!AP_HasError()) {
    return NULL;
  }

  return &g_error;
}

const char *AP_GetErrorMessage(void) {
  if (!AP_HasError()) {
    return NULL;
  }

  return g_error.message;
}

/* ---------------------------------------------------------
 * Set error
 * --------------------------------------------------------- */

void AP_SetError(AP_ErrorCode code, const char *message, const char *file,
                 const char *function, int line) {
  /*
   * AP_ERROR_NONE means "clear the current error."
   */
  if (code == AP_ERROR_NONE) {
    AP_ClearError();
    return;
  }

  g_error.code = code;
  g_error.message = message;
  g_error.file = file;
  g_error.function = function;
  g_error.line = line;

  /*
   * Errors are automatically sent to the logger.
   */
  AP_Log(AP_LOG_ERROR, "[%s] %s (%s:%d, %s)", AP_ErrorCodeName(code),
         message ? message : "No error message", file ? file : "unknown", line,
         function ? function : "unknown");
}
