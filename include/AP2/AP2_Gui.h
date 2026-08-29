/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_GUI_H
#define AP2_GUI_H

#include "AP2/AP2_Font.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AP_Texture AP_Texture;

/*
 * AP2 Immediate GUI
 *
 * Immediate-mode widgets for building applications. Call them
 * every frame. Layout is vertical inside a window unless
 * AP_GuiSameLine() is used.
 *
 *     bool open = true;
 *     if (AP_GuiBeginWindow("Inspector", &open, AP_GUI_WINDOW_MENU_BAR)) {
 *         if (AP_GuiBeginMenuBar()) {
 *             if (AP_GuiBeginMenu("File")) {
 *                 if (AP_GuiMenuItem("Quit", "Esc")) {
 *                     AP_Quit();
 *                 }
 *                 AP_GuiEndMenu();
 *             }
 *             AP_GuiEndMenuBar();
 *         }
 *         AP_GuiCheckbox("Grid", &show_grid);
 *         AP_GuiEndWindow();
 *     }
 *
 * Labels may use "##id" to keep a unique ID while showing a
 * shorter name: AP_GuiButton("OK##save").
 *
 * Style: AP_GuiDarkStyle() is the default. Copy, edit, and pass
 * to AP_GuiSetStyle(), or mutate AP_GuiGetStyle() in place.
 */

typedef enum AP_GuiWindowFlags {
  AP_GUI_WINDOW_NONE = 0,
  AP_GUI_WINDOW_NO_TITLE = 1 << 0,
  AP_GUI_WINDOW_NO_MOVE = 1 << 1,
  AP_GUI_WINDOW_NO_RESIZE = 1 << 2,
  AP_GUI_WINDOW_NO_COLLAPSE = 1 << 3,
  AP_GUI_WINDOW_NO_SCROLL = 1 << 4,
  AP_GUI_WINDOW_MENU_BAR = 1 << 5,
  AP_GUI_WINDOW_AUTO_RESIZE = 1 << 6
} AP_GuiWindowFlags;

typedef enum AP_GuiInputFlags {
  AP_GUI_INPUT_NONE = 0,
  AP_GUI_INPUT_PASSWORD = 1 << 0,
  AP_GUI_INPUT_READ_ONLY = 1 << 1,
  AP_GUI_INPUT_ENTER_RETURNS = 1 << 2
} AP_GuiInputFlags;

typedef struct AP_GuiStyle {
  AP_FColor window_bg;
  AP_FColor title_bg;
  AP_FColor title_text;
  AP_FColor menubar_bg;
  AP_FColor popup_bg;
  AP_FColor border;
  AP_FColor text;
  AP_FColor text_disabled;
  AP_FColor widget_bg;
  AP_FColor widget_hovered;
  AP_FColor widget_active;
  AP_FColor header;
  AP_FColor header_hovered;
  AP_FColor accent;
  AP_FColor check_mark;
  AP_FColor slider_grab;
  AP_FColor separator;
  AP_FColor dim;
  AP_FColor scrollbar;
  float rounding;
  float padding;
  float spacing;
  float title_height;
  float widget_height;
  float border_width;
  float font_size;
  float grab_width;
  float scrollbar_size;
  float indent_size;
  float min_window_w;
  float min_window_h;
} AP_GuiStyle;

/* =========================================================
 * Style / font
 * ========================================================= */

AP_GuiStyle AP_GuiDarkStyle(void);

AP_GuiStyle AP_GuiLightStyle(void);

AP_GuiStyle AP_GuiDefaultStyle(void);

void AP_GuiSetStyle(const AP_GuiStyle *style);

AP_GuiStyle *AP_GuiGetStyle(void);

bool AP_GuiSetFont(AP_Font *font);

AP_Font *AP_GuiGetFont(void);

/* =========================================================
 * Layer
 *
 * AP_GUI_LAYER_OFF      no draw, no input capture
 * AP_GUI_LAYER_SCENE    drawn with the scene (inside post)
 * AP_GUI_LAYER_OVERLAY  drawn after post (default)
 * ========================================================= */

typedef enum AP_GuiLayer {
  AP_GUI_LAYER_OFF = 0,
  AP_GUI_LAYER_SCENE,
  AP_GUI_LAYER_OVERLAY
} AP_GuiLayer;

void AP_SetGuiLayer(AP_GuiLayer layer);

AP_GuiLayer AP_GetGuiLayer(void);

bool AP_GuiLayerEnabled(void);

/* =========================================================
 * Input capture
 *
 * True while the cursor is over a window or a widget is active.
 * Skip camera / game input when these return true.
 * ========================================================= */

bool AP_GuiWantCaptureMouse(void);

bool AP_GuiWantCaptureKeyboard(void);

/* =========================================================
 * Windows
 *
 * First call uses x, y, width, height. Later frames keep the
 * dragged / resized rect unless AP_GuiSetNextWindowPos() is used.
 *
 *     if (AP_GuiBeginWindow("Inspector", &open, 0)) {
 *         AP_GuiCheckbox("Grid", &show_grid);
 *         AP_GuiEndWindow();
 *     }
 *
 * Begin returns false when *open is already false (do not call End).
 * Collapsed windows still return true; widgets inside are skipped.
 * ========================================================= */

void AP_GuiSetNextWindowPos(float x, float y);

void AP_GuiSetNextWindowSize(float width, float height);

void AP_GuiSetNextWindowFlags(AP_U32 flags);

bool AP_GuiBeginWindow(const char *name, bool *open, AP_U32 flags);

void AP_GuiEndWindow(void);

bool AP_GuiBeginPanel(const char *title, float x, float y, float width,
                      float height);

bool AP_GuiBeginPanelEx(const char *title, AP_FRect *rect, bool *open);

void AP_GuiEndPanel(void);

void AP_GuiSetNextPanelPos(float x, float y);

void AP_GuiSetNextPanelSize(float width, float height);

/* =========================================================
 * IDs / width / disabled
 * ========================================================= */

void AP_GuiPushId(const char *id);

void AP_GuiPushIdInt(int id);

void AP_GuiPopId(void);

void AP_GuiPushItemWidth(float width);

void AP_GuiPopItemWidth(void);

void AP_GuiBeginDisabled(bool disabled);

void AP_GuiEndDisabled(void);

/* =========================================================
 * Layout
 * ========================================================= */

void AP_GuiSameLine(void);

void AP_GuiSameLineEx(float spacing, float offset_y);

void AP_GuiDummy(float width, float height);

void AP_GuiSpacing(void);

void AP_GuiSeparator(void);

void AP_GuiSeparatorText(const char *text);

void AP_GuiIndent(float width);

void AP_GuiUnindent(float width);

void AP_GuiColumns(int count);

void AP_GuiNextColumn(void);

void AP_GuiSetCursor(float x, float y);

AP_FPoint AP_GuiGetCursor(void);

AP_FPoint AP_GuiGetContentAvail(void);

AP_FRect AP_GuiLayoutRect(float height);

bool AP_GuiBeginChild(const char *id, float width, float height, bool border);

void AP_GuiEndChild(void);

bool AP_GuiBeginGroup(void);

void AP_GuiEndGroup(void);

/* =========================================================
 * Item query  (last submitted widget)
 * ========================================================= */

bool AP_GuiIsItemHovered(void);

bool AP_GuiIsItemActive(void);

bool AP_GuiIsItemClicked(void);

bool AP_GuiIsWindowHovered(void);

AP_FRect AP_GuiGetItemRect(void);

AP_FRect AP_GuiGetWindowRect(void);

void AP_GuiSetTooltip(const char *text);

/* =========================================================
 * Menus
 * ========================================================= */

bool AP_GuiBeginMenuBar(void);

void AP_GuiEndMenuBar(void);

bool AP_GuiBeginMenu(const char *label);

void AP_GuiEndMenu(void);

bool AP_GuiMenuItem(const char *label, const char *shortcut);

/* =========================================================
 * Popups / modal
 * ========================================================= */

void AP_GuiOpenPopup(const char *id);

bool AP_GuiBeginPopup(const char *id);

bool AP_GuiBeginPopupContextItem(const char *id);

bool AP_GuiBeginPopupModal(const char *title, bool *open);

void AP_GuiEndPopup(void);

void AP_GuiCloseCurrentPopup(void);

bool AP_GuiIsPopupOpen(const char *id);

/* =========================================================
 * Tabs / trees
 * ========================================================= */

bool AP_GuiBeginTabBar(const char *id);

bool AP_GuiTab(const char *label);

void AP_GuiEndTabBar(void);

bool AP_GuiCollapsingHeader(const char *label, bool default_open);

bool AP_GuiTreeNode(const char *label);

void AP_GuiTreePop(void);

/* =========================================================
 * Widgets
 * ========================================================= */

void AP_GuiLabel(const char *text);

void AP_GuiLabelF(const char *format, ...);

bool AP_GuiButton(const char *label);

bool AP_GuiButtonEx(const char *label, float width);

bool AP_GuiInvisibleButton(const char *id, float width, float height);

bool AP_GuiCheckbox(const char *label, bool *checked);

bool AP_GuiRadio(const char *label, int *value, int item);

bool AP_GuiSelectable(const char *label, bool selected);

bool AP_GuiSliderF(const char *label, float *value, float min_value,
                   float max_value);

bool AP_GuiSliderI(const char *label, int *value, int min_value, int max_value);

bool AP_GuiDragF(const char *label, float *value, float speed, float min_value,
                 float max_value);

bool AP_GuiDragI(const char *label, int *value, float speed, int min_value,
                 int max_value);

bool AP_GuiToggle(const char *label, bool *on);

bool AP_GuiProgress(float fraction);

bool AP_GuiInputText(const char *label, char *buffer, int capacity);

bool AP_GuiInputTextEx(const char *label, char *buffer, int capacity,
                       AP_U32 flags);

bool AP_GuiColorEdit(const char *label, AP_FColor *color);

bool AP_GuiCombo(const char *label, int *current, const char **items,
                 int count);

bool AP_GuiListBox(const char *label, int *current, const char **items,
                   int count, int visible_rows);

void AP_GuiImage(AP_Texture *texture, float width, float height);

bool AP_GuiImageButton(AP_Texture *texture, float width, float height);

#ifdef __cplusplus
}
#endif

#endif /* AP2_GUI_H */
