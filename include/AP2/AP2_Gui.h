#ifndef AP2_GUI_H
#define AP2_GUI_H

#include "AP2/AP2_Font.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Immediate GUI
 *
 * Widgets draw immediately with the current style and font.
 * Layout is vertical inside a panel unless AP_GuiSameLine()
 * is called.
 *
 *     AP_GuiBeginPanel("Settings", 24.0f, 24.0f, 280.0f, 360.0f);
 *     AP_GuiLabel("Renderer");
 *     AP_GuiCheckbox("Grid", &show_grid);
 *     AP_GuiSliderF("Speed", &speed, 0.0f, 4.0f);
 *     if (AP_GuiButton("Reset")) {
 *         speed = 1.0f;
 *     }
 *     AP_GuiEndPanel();
 *
 * Style: AP_GuiDarkStyle() is the default. Copy, edit, and
 * pass to AP_GuiSetStyle(), or mutate AP_GuiGetStyle() in place.
 */

typedef struct AP_GuiStyle {
  AP_FColor window_bg;
  AP_FColor title_bg;
  AP_FColor title_text;
  AP_FColor border;
  AP_FColor text;
  AP_FColor text_disabled;
  AP_FColor widget_bg;
  AP_FColor widget_hovered;
  AP_FColor widget_active;
  AP_FColor accent;
  AP_FColor check_mark;
  AP_FColor slider_grab;
  AP_FColor separator;
  float rounding;
  float padding;
  float spacing;
  float title_height;
  float widget_height;
  float border_width;
  float font_size;
  float grab_width;
} AP_GuiStyle;

/* =========================================================
 * Style
 * ========================================================= */

AP_GuiStyle AP_GuiDarkStyle(void);

AP_GuiStyle AP_GuiLightStyle(void);

AP_GuiStyle AP_GuiDefaultStyle(void);

void AP_GuiSetStyle(const AP_GuiStyle *style);

AP_GuiStyle *AP_GuiGetStyle(void);

bool AP_GuiSetFont(AP_Font *font);

AP_Font *AP_GuiGetFont(void);

/* =========================================================
 * Input capture
 *
 * True while the cursor is over a panel or a widget is active.
 * ========================================================= */

bool AP_GuiWantCaptureMouse(void);

bool AP_GuiWantCaptureKeyboard(void);

/* =========================================================
 * Panels
 *
 * First call uses x, y, width, height. Later frames keep the
 * dragged position unless AP_GuiSetNextPanelPos() is used.
 * ========================================================= */

void AP_GuiSetNextPanelPos(float x, float y);

void AP_GuiSetNextPanelSize(float width, float height);

bool AP_GuiBeginPanel(const char *title, float x, float y, float width,
                      float height);

bool AP_GuiBeginPanelEx(const char *title, AP_FRect *rect, bool *open);

void AP_GuiEndPanel(void);

/* =========================================================
 * Layout
 * ========================================================= */

void AP_GuiSameLine(void);

void AP_GuiDummy(float width, float height);

void AP_GuiSeparator(void);

void AP_GuiIndent(float width);

AP_FRect AP_GuiLayoutRect(float height);

/* =========================================================
 * Widgets
 * ========================================================= */

void AP_GuiLabel(const char *text);

void AP_GuiLabelF(const char *format, ...);

bool AP_GuiButton(const char *label);

bool AP_GuiButtonEx(const char *label, float width);

bool AP_GuiCheckbox(const char *label, bool *checked);

bool AP_GuiRadio(const char *label, int *value, int item);

bool AP_GuiSliderF(const char *label, float *value, float min_value,
                   float max_value);

bool AP_GuiSliderI(const char *label, int *value, int min_value, int max_value);

bool AP_GuiToggle(const char *label, bool *on);

bool AP_GuiProgress(float fraction);

bool AP_GuiInputText(const char *label, char *buffer, int capacity);

bool AP_GuiColorEdit(const char *label, AP_FColor *color);

#ifdef __cplusplus
}
#endif

#endif /* AP2_GUI_H */
