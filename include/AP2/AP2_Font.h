/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_FONT_H
#define AP2_FONT_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Font
 *
 * Text is drawn in window pixels, top-left origin. A built-in
 * 8x8 Latin font is always available after the window exists
 * (not pretty :( — it's there so you can print a HUD before you have a TTF!! ):
 *
 *     AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
 *     AP_DrawText(32.0f, 32.0f, "Hello");
 *
 * Custom TrueType fonts:
 *
 *     AP_Font *font = AP_LoadFont("Inter.ttf", 18.0f);
 *     AP_SetFont(font);
 *     AP_DrawText(32.0f, 64.0f, "Custom");
 *     AP_DestroyFont(font);
 *
 * AP_DrawText uses the current font and the current draw color.
 * Size 0 in *Ex helpers means the font's baked size.
 */

typedef struct AP_Font AP_Font;

typedef enum AP_TextAlign {
  AP_TEXT_ALIGN_LEFT = 0,
  AP_TEXT_ALIGN_CENTER,
  AP_TEXT_ALIGN_RIGHT
} AP_TextAlign;

AP_Font *AP_GetDefaultFont(void);

AP_Font *AP_LoadFont(const char *path, float pixel_size);

AP_Font *AP_CreateFontFromMemory(const void *data, int data_size,
                                 float pixel_size);

void AP_DestroyFont(AP_Font *font);

bool AP_FontIsValid(const AP_Font *font);

float AP_GetFontSize(const AP_Font *font);

float AP_GetFontLineHeight(const AP_Font *font);

bool AP_SetFont(AP_Font *font);

AP_Font *AP_GetFont(void);

AP_FPoint AP_MeasureText(const char *text);

AP_FPoint AP_MeasureTextEx(AP_Font *font, const char *text, float size);

bool AP_RenderText(float x, float y, const char *text);

bool AP_RenderTextEx(AP_Font *font, float x, float y, const char *text,
                     AP_FColor color, float size);

bool AP_RenderTextAligned(AP_Font *font, const AP_FRect *bounds,
                          const char *text, AP_FColor color, float size,
                          AP_TextAlign align);

#define AP_DrawText AP_RenderText
#define AP_DrawTextEx AP_RenderTextEx
#define AP_DrawTextAligned AP_RenderTextAligned

/*
 * System fonts installed on this machine, discovered from the OS font
 * registry/directories (e.g. "Segoe UI", "Arial", "Consolas" on Windows).
 *
 *     AP_RefreshSystemFonts();
 *     for (int i = 0; i < AP_GetSystemFontCount(); ++i) {
 *         printf("%s\n", AP_GetSystemFontName(i));
 *     }
 *     AP_Font *font = AP_LoadSystemFont("Segoe UI", 16.0f);
 *
 * The list is lazily populated on first use; call AP_RefreshSystemFonts()
 * to force a rescan. AP_LoadSystemFont matches names case-insensitively
 * and falls back to a short list of common fonts if the exact name isn't
 * found (so "Arial" still resolves something reasonable on non-Windows).
 */

void AP_RefreshSystemFonts(void);

int AP_GetSystemFontCount(void);

const char *AP_GetSystemFontName(int index);

const char *AP_GetSystemFontPath(int index);

AP_Font *AP_LoadSystemFont(const char *name, float pixel_size);

/* Best-effort default UI font for the current platform (e.g. Segoe UI on
   Windows), falling back to the built-in bitmap font if none is found. */
AP_Font *AP_LoadSystemDefaultFont(float pixel_size);

#ifdef __cplusplus
}
#endif

#endif /* AP2_FONT_H */
