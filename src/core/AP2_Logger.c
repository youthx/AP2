/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Logger.h"

#include <stdio.h>
#include <time.h>

/* ---------------------------------------------------------
 * Internal state
 * --------------------------------------------------------- */

typedef struct AP_LoggerState {
  AP_LogLevel minimum_level;

  bool include_timestamp;
  bool include_level;

  bool initialized;
} AP_LoggerState;

static AP_LoggerState g_logger = {.minimum_level = AP_LOG_INFO,
                                  .include_timestamp = true,
                                  .include_level = true,
                                  .initialized = false};

/* ---------------------------------------------------------
 * Internal helpers
 * --------------------------------------------------------- */

static const char *AP_LogLevelName(AP_LogLevel level) {
  switch (level) {
  case AP_LOG_TRACE:
    return "TRACE";

  case AP_LOG_DEBUG:
    return "DEBUG";

  case AP_LOG_INFO:
    return "INFO";

  case AP_LOG_WARN:
    return "WARN";

  case AP_LOG_ERROR:
    return "ERROR";

  case AP_LOG_FATAL:
    return "FATAL";

  default:
    return "UNKNOWN";
  }
}

static FILE *AP_LogOutput(AP_LogLevel level) {
  switch (level) {
  case AP_LOG_ERROR:
  case AP_LOG_FATAL:
    return stderr;

  default:
    return stdout;
  }
}

/* ---------------------------------------------------------
 * Logger lifecycle
 * --------------------------------------------------------- */

bool AP_LoggerInit(const AP_LoggerConfig *config) {
  if (g_logger.initialized) {
    return true;
  }

  if (config) {
    if (config->minimum_level >= AP_LOG_LEVEL_COUNT) {
      return false;
    }

    g_logger.minimum_level = config->minimum_level;

    g_logger.include_timestamp = config->include_timestamp;

    g_logger.include_level = config->include_level;
  }

  g_logger.initialized = true;

  return true;
}

void AP_LoggerClose(void) {
  if (!g_logger.initialized) {
    return;
  }

  fflush(stdout);
  fflush(stderr);

  g_logger.initialized = false;
}

/* ---------------------------------------------------------
 * Configuration
 * --------------------------------------------------------- */

void AP_LoggerSetLevel(AP_LogLevel level) {
  if (level >= AP_LOG_LEVEL_COUNT) {
    return;
  }

  g_logger.minimum_level = level;
}

AP_LogLevel AP_LoggerGetLevel(void) { return g_logger.minimum_level; }

/* ---------------------------------------------------------
 * Logging
 * --------------------------------------------------------- */

void AP_LogV(AP_LogLevel level, const char *format, va_list args) {
  if (!format) {
    return;
  }

  if (level >= AP_LOG_LEVEL_COUNT) {
    return;
  }

  /*
   * Messages below the configured level are ignored.
   */
  if (level < g_logger.minimum_level) {
    return;
  }

  FILE *output = AP_LogOutput(level);

  /* Timestamp */

  if (g_logger.include_timestamp) {
    time_t current_time = time(NULL);
    struct tm local_buf;
    struct tm *local_time = NULL;

#if defined(_WIN32)
    if (localtime_s(&local_buf, &current_time) == 0) {
      local_time = &local_buf;
    }
#else
    local_time = localtime_r(&current_time, &local_buf);
#endif

    if (local_time) {
      fprintf(output, "[%02d:%02d:%02d] ", local_time->tm_hour,
              local_time->tm_min, local_time->tm_sec);
    }
  }

  /* Log level */

  if (g_logger.include_level) {
    fprintf(output, "[%s] ", AP_LogLevelName(level));
  }

  /* Message */

  vfprintf(output, format, args);

  fputc('\n', output);

  fflush(output);

  /*
   * Fatal messages terminate the process.
   */
  if (level == AP_LOG_FATAL) {
    fflush(output);
  }
}

void AP_Log(AP_LogLevel level, const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(level, format, args);

  va_end(args);
}

/* ---------------------------------------------------------
 * Convenience functions
 * --------------------------------------------------------- */

void AP_LogTrace(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_TRACE, format, args);

  va_end(args);
}

void AP_LogDebug(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_DEBUG, format, args);

  va_end(args);
}

void AP_LogInfo(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_INFO, format, args);

  va_end(args);
}

void AP_LogWarn(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_WARN, format, args);

  va_end(args);
}

void AP_LogError(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_ERROR, format, args);

  va_end(args);
}

void AP_LogFatal(const char *format, ...) {
  va_list args;

  va_start(args, format);

  AP_LogV(AP_LOG_FATAL, format, args);

  va_end(args);
}
