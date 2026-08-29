/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_LOGGER_H
#define AP2_LOGGER_H

#include <stdarg.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---------------------------------------------------------
 * Log levels
 * --------------------------------------------------------- */

typedef enum AP_LogLevel {
  AP_LOG_TRACE = 0,
  AP_LOG_DEBUG,
  AP_LOG_INFO,
  AP_LOG_WARN,
  AP_LOG_ERROR,
  AP_LOG_FATAL,
  AP_LOG_LEVEL_COUNT
} AP_LogLevel;

/* ---------------------------------------------------------
 * Logger configuration
 * --------------------------------------------------------- */

typedef struct AP_LoggerConfig {
  AP_LogLevel minimum_level;
  bool include_timestamp;
  bool include_level;
} AP_LoggerConfig;

/* ---------------------------------------------------------
 * Logger lifecycle
 * --------------------------------------------------------- */

bool AP_LoggerInit(const AP_LoggerConfig *config);
void AP_LoggerClose(void);

/* ---------------------------------------------------------
 * Logger configuration
 * --------------------------------------------------------- */

void AP_LoggerSetLevel(AP_LogLevel level);
AP_LogLevel AP_LoggerGetLevel(void);

/* ---------------------------------------------------------
 * Logging
 * --------------------------------------------------------- */

void AP_Log(AP_LogLevel level, const char *format, ...);

void AP_LogV(AP_LogLevel level, const char *format, va_list args);

/* ---------------------------------------------------------
 * Convenience functions
 * --------------------------------------------------------- */

void AP_LogTrace(const char *format, ...);
void AP_LogDebug(const char *format, ...);
void AP_LogInfo(const char *format, ...);
void AP_LogWarn(const char *format, ...);
void AP_LogError(const char *format, ...);
void AP_LogFatal(const char *format, ...); /* logs. does not abort. */

/* ---------------------------------------------------------
 * Macros
 * --------------------------------------------------------- */

#define AP_TRACE(...) AP_LogTrace(__VA_ARGS__)
#define AP_DEBUG(...) AP_LogDebug(__VA_ARGS__)
#define AP_INFO(...) AP_LogInfo(__VA_ARGS__)
#define AP_WARN(...) AP_LogWarn(__VA_ARGS__)
#define AP_ERROR(...) AP_LogError(__VA_ARGS__)
#define AP_FATAL(...) AP_LogFatal(__VA_ARGS__)

#ifdef __cplusplus
}
#endif

#endif
