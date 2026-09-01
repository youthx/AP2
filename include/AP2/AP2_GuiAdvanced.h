/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_GUI_ADVANCED_H
#define AP2_GUI_ADVANCED_H

#include "AP2/AP2_Font.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef AP2_NO_GUI_ADVANCED

/* Stubs for when GUI is disabled */
typedef struct AP_GuiWidget AP_GuiWidget;

#else

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AP_Texture AP_Texture;

/*
 * AP2 Advanced GUI System (Retained Mode)
 *
 * A complete GUI framework with:
 *   - Retained-mode widget hierarchy (like Qt)
 *   - Flexible layout system (box, grid, flex)
 *   - Comprehensive theming and styling
 *   - Event/signal system
 *   - Keyboard/mouse focus management
 *   - Desktop application builders in mind
 *
 * Complements the existing immediate-mode AP2_Gui API.
 *
 *     // Create main window
 *     AP_GuiWidget *window = AP_GuiWindowNew("My App", 800, 600);
 *     AP_GuiWidgetSetSize(window, 800, 600);
 *
 *     // Create layout
 *     AP_GuiWidget *vbox = AP_GuiVBoxNew();
 *     AP_GuiWidgetAddChild(window, vbox);
 *
 *     // Add widgets
 *     AP_GuiWidget *btn = AP_GuiButtonNew("Click Me");
 *     AP_GuiWidgetConnect(btn, "clicked", on_click, user_data);
 *     AP_GuiWidgetAddChild(vbox, btn);
 *
 *     // Render
 *     while (AP_IsRunning()) {
 *         AP_GuiWidgetUpdate(window, dt);
 *         AP_GuiWidgetRender(window);
 *     }
 *
 * Exclude with AP2_NO_GUI_ADVANCED.
 */

/* =========================================================
 * Forward declarations
 * ========================================================= */

typedef struct AP_GuiWidget AP_GuiWidget;
typedef struct AP_GuiTheme AP_GuiTheme;
typedef struct AP_GuiLayout AP_GuiLayout;
typedef struct AP_GuiEvent AP_GuiEvent;

/* =========================================================
 * Enums & Types
 * ========================================================= */

typedef enum AP_GuiWidgetType {
  AP_GUI_WIDGET_NONE = 0,
  AP_GUI_WIDGET_WINDOW,
  AP_GUI_WIDGET_PANEL,
  AP_GUI_WIDGET_LABEL,
  AP_GUI_WIDGET_BUTTON,
  AP_GUI_WIDGET_TEXT_EDIT,
  AP_GUI_WIDGET_CHECKBOX,
  AP_GUI_WIDGET_RADIO,
  AP_GUI_WIDGET_SLIDER,
  AP_GUI_WIDGET_COMBO_BOX,
  AP_GUI_WIDGET_LIST_BOX,
  AP_GUI_WIDGET_SPINNER,
  AP_GUI_WIDGET_PROGRESS_BAR,
  AP_GUI_WIDGET_IMAGE,
  AP_GUI_WIDGET_SCROLL_AREA,
  AP_GUI_WIDGET_TAB_WIDGET,
  AP_GUI_WIDGET_TREE_WIDGET,
  AP_GUI_WIDGET_VBOX,
  AP_GUI_WIDGET_HBOX,
  AP_GUI_WIDGET_GRID,
  AP_GUI_WIDGET_FLEX,
  AP_GUI_WIDGET_SPACER,
  AP_GUI_WIDGET_SEPARATOR,
  AP_GUI_WIDGET_POPUP
} AP_GuiWidgetType;

/* Where a popup opens relative to its anchor widget. AP_GUI_POPUP_CURSOR
 * ignores the anchor and opens at the current mouse position instead. */
typedef enum AP_GuiPopupPlacement {
  AP_GUI_POPUP_BELOW = 0,
  AP_GUI_POPUP_ABOVE,
  AP_GUI_POPUP_LEFT,
  AP_GUI_POPUP_RIGHT,
  AP_GUI_POPUP_CURSOR
} AP_GuiPopupPlacement;

typedef enum AP_GuiLayoutType {
  AP_GUI_LAYOUT_NONE = 0,
  AP_GUI_LAYOUT_VERTICAL,
  AP_GUI_LAYOUT_HORIZONTAL,
  AP_GUI_LAYOUT_GRID,
  AP_GUI_LAYOUT_FLEX
} AP_GuiLayoutType;

typedef enum AP_GuiSizePolicy {
  AP_GUI_SIZE_FIXED = 0,
  AP_GUI_SIZE_MINIMUM,
  AP_GUI_SIZE_PREFERRED,
  AP_GUI_SIZE_EXPANDING,
  AP_GUI_SIZE_IGNORED
} AP_GuiSizePolicy;

typedef enum AP_GuiAlignment {
  AP_GUI_ALIGN_LEFT = 1 << 0,
  AP_GUI_ALIGN_CENTER = 1 << 1,
  AP_GUI_ALIGN_RIGHT = 1 << 2,
  AP_GUI_ALIGN_TOP = 1 << 3,
  AP_GUI_ALIGN_VCENTER = 1 << 4,
  AP_GUI_ALIGN_BOTTOM = 1 << 5,
  AP_GUI_ALIGN_JUSTIFY = 1 << 6
} AP_GuiAlignment;

typedef enum AP_GuiEventType {
  AP_GUI_EVENT_NONE = 0,
  AP_GUI_EVENT_CLICKED,
  AP_GUI_EVENT_DOUBLE_CLICKED,
  AP_GUI_EVENT_MOUSE_ENTER,
  AP_GUI_EVENT_MOUSE_LEAVE,
  AP_GUI_EVENT_FOCUS_IN,
  AP_GUI_EVENT_FOCUS_OUT,
  AP_GUI_EVENT_KEY_PRESS,
  AP_GUI_EVENT_KEY_RELEASE,
  AP_GUI_EVENT_TEXT_CHANGED,
  AP_GUI_EVENT_VALUE_CHANGED,
  AP_GUI_EVENT_STATE_CHANGED
} AP_GuiEventType;

typedef struct AP_GuiEvent {
  AP_GuiEventType type;
  AP_GuiWidget *target;
  int key;
  char *text;
  float value;
  int index;
  void *user_data;
} AP_GuiEvent;

/* Event callback: return true to accept, false to reject */
typedef bool (*AP_GuiEventCallback)(AP_GuiEvent *event);

typedef struct AP_GuiMargins {
  float left;
  float top;
  float right;
  float bottom;
} AP_GuiMargins;

typedef struct AP_GuiThemeColors {
  AP_FColor window_bg;
  AP_FColor panel_bg;
  AP_FColor text;
  AP_FColor text_disabled;
  AP_FColor button_bg;
  AP_FColor button_bg_hovered;
  AP_FColor button_bg_pressed;
  AP_FColor button_text;
  AP_FColor widget_bg;
  AP_FColor widget_bg_hovered;
  AP_FColor widget_bg_focused;
  AP_FColor border;
  AP_FColor border_hover;
  AP_FColor accent;
  AP_FColor accent_light;
  AP_FColor error;
  AP_FColor warning;
  AP_FColor success;
  AP_FColor info;
} AP_GuiThemeColors;

typedef struct AP_GuiTheme {
  AP_GuiThemeColors colors;
  AP_Font *font_default;
  AP_Font *font_title;
  AP_Font *font_mono;
  float font_size;
  float font_size_title;
  float font_size_small;
  float button_height;
  float edit_height;
  float spacing;
  float padding;
  float border_width;
  float rounding;
  float shadow_blur;
  float shadow_offset;
  AP_FColor shadow_color;
} AP_GuiTheme;

/* =========================================================
 * Core Widget API
 * ========================================================= */

/* Factory methods for widget creation */
AP_GuiWidget *AP_GuiWindowNew(const char *title, float width, float height);
AP_GuiWidget *AP_GuiPanelNew(void);
AP_GuiWidget *AP_GuiLabelNew(const char *text);
AP_GuiWidget *AP_GuiButtonNew(const char *text);
AP_GuiWidget *AP_GuiTextEditNew(void);
AP_GuiWidget *AP_GuiCheckboxNew(const char *text);
AP_GuiWidget *AP_GuiRadioNew(const char *text);
AP_GuiWidget *AP_GuiSliderNew(float min_val, float max_val, float step);
AP_GuiWidget *AP_GuiSpinnerNew(int min_val, int max_val);
AP_GuiWidget *AP_GuiProgressBarNew(void);
AP_GuiWidget *AP_GuiComboBoxNew(void);
AP_GuiWidget *AP_GuiListBoxNew(void);
AP_GuiWidget *AP_GuiImageNew(AP_Texture *texture);
AP_GuiWidget *AP_GuiScrollAreaNew(void);
AP_GuiWidget *AP_GuiTabWidgetNew(void);
AP_GuiWidget *AP_GuiTreeWidgetNew(void);
AP_GuiWidget *AP_GuiVBoxNew(void);
AP_GuiWidget *AP_GuiHBoxNew(void);
AP_GuiWidget *AP_GuiGridNew(int rows, int cols);
AP_GuiWidget *AP_GuiFlexBoxNew(void);
AP_GuiWidget *AP_GuiSpacerNew(float width, float height);
AP_GuiWidget *AP_GuiSeparatorNew(bool vertical);

void AP_GuiWidgetDestroy(AP_GuiWidget *widget);

/* =========================================================
 * Widget Hierarchy
 * ========================================================= */

void AP_GuiWidgetAddChild(AP_GuiWidget *parent, AP_GuiWidget *child);
void AP_GuiWidgetRemoveChild(AP_GuiWidget *parent, AP_GuiWidget *child);
void AP_GuiWidgetRemoveAllChildren(AP_GuiWidget *parent);

AP_GuiWidget *AP_GuiWidgetParent(AP_GuiWidget *widget);
AP_GuiWidget *AP_GuiWidgetChildAt(AP_GuiWidget *parent, int index);
int AP_GuiWidgetChildCount(AP_GuiWidget *widget);

/* =========================================================
 * Widget Properties
 * ========================================================= */

AP_GuiWidgetType AP_GuiWidgetGetType(AP_GuiWidget *widget);
const char *AP_GuiWidgetName(AP_GuiWidget *widget);
void AP_GuiWidgetSetName(AP_GuiWidget *widget, const char *name);

/* Visibility & Enabled */
bool AP_GuiWidgetVisible(AP_GuiWidget *widget);
void AP_GuiWidgetSetVisible(AP_GuiWidget *widget, bool visible);
bool AP_GuiWidgetEnabled(AP_GuiWidget *widget);
void AP_GuiWidgetSetEnabled(AP_GuiWidget *widget, bool enabled);

/* Geometry */
AP_FRect AP_GuiWidgetGeometry(AP_GuiWidget *widget);
float AP_GuiWidgetX(AP_GuiWidget *widget);
float AP_GuiWidgetY(AP_GuiWidget *widget);
float AP_GuiWidgetWidth(AP_GuiWidget *widget);
float AP_GuiWidgetHeight(AP_GuiWidget *widget);

void AP_GuiWidgetSetGeometry(AP_GuiWidget *widget, float x, float y, float w,
                             float h);
void AP_GuiWidgetSetPos(AP_GuiWidget *widget, float x, float y);
void AP_GuiWidgetSetSize(AP_GuiWidget *widget, float width, float height);

float AP_GuiWidgetMinWidth(AP_GuiWidget *widget);
float AP_GuiWidgetMinHeight(AP_GuiWidget *widget);
float AP_GuiWidgetMaxWidth(AP_GuiWidget *widget);
float AP_GuiWidgetMaxHeight(AP_GuiWidget *widget);

void AP_GuiWidgetSetMinSize(AP_GuiWidget *widget, float width, float height);
void AP_GuiWidgetSetMaxSize(AP_GuiWidget *widget, float width, float height);

AP_GuiSizePolicy AP_GuiWidgetSizePolicyX(AP_GuiWidget *widget);
AP_GuiSizePolicy AP_GuiWidgetSizePolicyY(AP_GuiWidget *widget);
void AP_GuiWidgetSetSizePolicy(AP_GuiWidget *widget, AP_GuiSizePolicy x,
                               AP_GuiSizePolicy y);

/* Text & Content */
const char *AP_GuiWidgetText(AP_GuiWidget *widget);
void AP_GuiWidgetSetText(AP_GuiWidget *widget, const char *text);

float AP_GuiWidgetValue(AP_GuiWidget *widget);
void AP_GuiWidgetSetValue(AP_GuiWidget *widget, float value);

bool AP_GuiWidgetChecked(AP_GuiWidget *widget);
void AP_GuiWidgetSetChecked(AP_GuiWidget *widget, bool checked);

AP_Texture *AP_GuiWidgetImage(AP_GuiWidget *widget);
void AP_GuiWidgetSetImage(AP_GuiWidget *widget, AP_Texture *texture);

/* Tooltips & Hints */
const char *AP_GuiWidgetTooltip(AP_GuiWidget *widget);
void AP_GuiWidgetSetTooltip(AP_GuiWidget *widget, const char *text);

const char *AP_GuiWidgetStatusTip(AP_GuiWidget *widget);
void AP_GuiWidgetSetStatusTip(AP_GuiWidget *widget, const char *text);

/* =========================================================
 * Styling
 * ========================================================= */

AP_GuiTheme *AP_GuiWidgetTheme(AP_GuiWidget *widget);
void AP_GuiWidgetSetTheme(AP_GuiWidget *widget, AP_GuiTheme *theme);

/* Property-based styling (overrides theme) */
void AP_GuiWidgetSetBackgroundColor(AP_GuiWidget *widget, AP_FColor color);
void AP_GuiWidgetSetTextColor(AP_GuiWidget *widget, AP_FColor color);
void AP_GuiWidgetSetBorderColor(AP_GuiWidget *widget, AP_FColor color);
void AP_GuiWidgetSetBorderWidth(AP_GuiWidget *widget, float width);
void AP_GuiWidgetSetPadding(AP_GuiWidget *widget, AP_GuiMargins margins);
void AP_GuiWidgetSetMargins(AP_GuiWidget *widget, AP_GuiMargins margins);
void AP_GuiWidgetSetCornerRadius(AP_GuiWidget *widget, float radius);

AP_FColor AP_GuiWidgetBackgroundColor(AP_GuiWidget *widget);
AP_FColor AP_GuiWidgetTextColor(AP_GuiWidget *widget);

/* =========================================================
 * Layout Management
 * ========================================================= */

AP_GuiLayout *AP_GuiWidgetLayout(AP_GuiWidget *widget);
void AP_GuiWidgetSetLayout(AP_GuiWidget *widget, AP_GuiLayout *layout);

/* Layout properties */
void AP_GuiLayoutSetSpacing(AP_GuiLayout *layout, float spacing);
void AP_GuiLayoutSetMargins(AP_GuiLayout *layout, AP_GuiMargins margins);
void AP_GuiLayoutSetAlignment(AP_GuiLayout *layout, AP_GuiAlignment align);

/* Add to grid layout */
void AP_GuiLayoutAddWidget(AP_GuiLayout *layout, AP_GuiWidget *widget);
void AP_GuiLayoutAddWidgetAt(AP_GuiLayout *layout, AP_GuiWidget *widget,
                             int row, int col);

/* Grid spans */
void AP_GuiLayoutSetRowSpan(AP_GuiLayout *layout, AP_GuiWidget *widget,
                            int span);
void AP_GuiLayoutSetColSpan(AP_GuiLayout *layout, AP_GuiWidget *widget,
                            int span);

/* =========================================================
 * Focus & Input Management
 * ========================================================= */

AP_GuiWidget *AP_GuiFocusedWidget(void);
void AP_GuiSetFocusedWidget(AP_GuiWidget *widget);

bool AP_GuiWidgetHasFocus(AP_GuiWidget *widget);
bool AP_GuiWidgetUnderMouse(AP_GuiWidget *widget);

bool AP_GuiWidgetAcceptsKeyboardInput(AP_GuiWidget *widget);
bool AP_GuiWidgetAcceptsMouseInput(AP_GuiWidget *widget);

/* =========================================================
 * Events & Signals
 * ========================================================= */

/* Connect a callback to a signal (e.g., "clicked", "valueChanged",
 * "textChanged") */
void AP_GuiWidgetConnect(AP_GuiWidget *widget, const char *signal,
                         AP_GuiEventCallback callback, void *user_data);
void AP_GuiWidgetDisconnect(AP_GuiWidget *widget, const char *signal,
                            AP_GuiEventCallback callback);

/* Emit a signal/event */
void AP_GuiWidgetEmit(AP_GuiWidget *widget, const char *signal);
void AP_GuiWidgetEmitWithValue(AP_GuiWidget *widget, const char *signal,
                               float value);
void AP_GuiWidgetEmitWithText(AP_GuiWidget *widget, const char *signal,
                              const char *text);

/* Process pending events */
void AP_GuiProcessEvents(void);

/* =========================================================
 * Rendering & Updates
 * ========================================================= */

/* Update layout and state (call once per frame for root widget) */
void AP_GuiWidgetUpdate(AP_GuiWidget *widget, float dt);

/* Render widget and all children (call in render loop) */
void AP_GuiWidgetRender(AP_GuiWidget *widget);

/* Manually trigger layout recalculation */
void AP_GuiWidgetRecalculateLayout(AP_GuiWidget *widget);

/* =========================================================
 * Theme Factory
 * ========================================================= */

AP_GuiTheme *AP_GuiThemeDarkNew(void);
AP_GuiTheme *AP_GuiThemeLightNew(void);
AP_GuiTheme *AP_GuiThemeHighContrastNew(void);
AP_GuiTheme *AP_GuiThemeCustomNew(void);

void AP_GuiThemeDestroy(AP_GuiTheme *theme);

/* Clone and modify a theme */
AP_GuiTheme *AP_GuiThemeClone(AP_GuiTheme *theme);

/* =========================================================
 * Global Theme Management
 * ========================================================= */

void AP_GuiSetGlobalTheme(AP_GuiTheme *theme);
AP_GuiTheme *AP_GuiGlobalTheme(void);

void AP_GuiSetGlobalFont(AP_Font *font);
AP_Font *AP_GuiGlobalFont(void);

/* =========================================================
 * CSS-like Styling (Tailwind/CSS Capabilities)
 * ========================================================= */

typedef enum AP_GuiDisplay {
  AP_GUI_DISPLAY_BLOCK = 0,
  AP_GUI_DISPLAY_INLINE,
  AP_GUI_DISPLAY_INLINE_BLOCK,
  AP_GUI_DISPLAY_FLEX,
  AP_GUI_DISPLAY_GRID,
  AP_GUI_DISPLAY_NONE
} AP_GuiDisplay;

typedef enum AP_GuiPosition {
  AP_GUI_POSITION_STATIC = 0,
  AP_GUI_POSITION_RELATIVE,
  AP_GUI_POSITION_ABSOLUTE,
  AP_GUI_POSITION_FIXED
} AP_GuiPosition;

typedef enum AP_GuiFlexDirection {
  AP_GUI_FLEX_ROW = 0,
  AP_GUI_FLEX_COLUMN,
  AP_GUI_FLEX_ROW_REVERSE,
  AP_GUI_FLEX_COLUMN_REVERSE
} AP_GuiFlexDirection;

typedef enum AP_GuiJustifyContent {
  AP_GUI_JUSTIFY_START = 0,
  AP_GUI_JUSTIFY_CENTER,
  AP_GUI_JUSTIFY_END,
  AP_GUI_JUSTIFY_SPACE_BETWEEN,
  AP_GUI_JUSTIFY_SPACE_AROUND,
  AP_GUI_JUSTIFY_SPACE_EVENLY
} AP_GuiJustifyContent;

typedef enum AP_GuiAlignItems {
  AP_GUI_ALIGN_ITEMS_START = 0,
  AP_GUI_ALIGN_ITEMS_CENTER,
  AP_GUI_ALIGN_ITEMS_END,
  AP_GUI_ALIGN_ITEMS_STRETCH
} AP_GuiAlignItems;

typedef enum AP_GuiOverflow {
  AP_GUI_OVERFLOW_VISIBLE = 0,
  AP_GUI_OVERFLOW_HIDDEN,
  AP_GUI_OVERFLOW_SCROLL,
  AP_GUI_OVERFLOW_AUTO
} AP_GuiOverflow;

typedef struct AP_GuiShadow {
  float offset_x;
  float offset_y;
  float blur;
  AP_FColor color;
  bool enabled;
} AP_GuiShadow;

typedef struct AP_GuiBorder {
  float width;
  float width_top;
  float width_right;
  float width_bottom;
  float width_left;
  AP_FColor color;
  AP_FColor color_top;
  AP_FColor color_right;
  AP_FColor color_bottom;
  AP_FColor color_left;
  float radius;
  float radius_top_left;
  float radius_top_right;
  float radius_bottom_right;
  float radius_bottom_left;
} AP_GuiBorder;

/* Display & Layout CSS Properties */
void AP_GuiWidgetSetDisplay(AP_GuiWidget *widget, AP_GuiDisplay display);
AP_GuiDisplay AP_GuiWidgetDisplay(AP_GuiWidget *widget);

void AP_GuiWidgetSetPosition(AP_GuiWidget *widget, AP_GuiPosition position);
AP_GuiPosition AP_GuiWidgetPosition(AP_GuiWidget *widget);

void AP_GuiWidgetSetTop(AP_GuiWidget *widget, float top);
void AP_GuiWidgetSetBottom(AP_GuiWidget *widget, float bottom);
void AP_GuiWidgetSetLeft(AP_GuiWidget *widget, float left);
void AP_GuiWidgetSetRight(AP_GuiWidget *widget, float right);

float AP_GuiWidgetTop(AP_GuiWidget *widget);
float AP_GuiWidgetBottom(AP_GuiWidget *widget);
float AP_GuiWidgetLeft(AP_GuiWidget *widget);
float AP_GuiWidgetRight(AP_GuiWidget *widget);

/* Flex Layout Properties */
void AP_GuiWidgetSetFlexDirection(AP_GuiWidget *widget,
                                  AP_GuiFlexDirection direction);
void AP_GuiWidgetSetJustifyContent(AP_GuiWidget *widget,
                                   AP_GuiJustifyContent justify);
void AP_GuiWidgetSetAlignItems(AP_GuiWidget *widget, AP_GuiAlignItems align);
void AP_GuiWidgetSetFlexGap(AP_GuiWidget *widget, float gap);
void AP_GuiWidgetSetFlexWrap(AP_GuiWidget *widget, bool wrap);
void AP_GuiWidgetSetFlexGrow(AP_GuiWidget *widget, float grow);
void AP_GuiWidgetSetFlexShrink(AP_GuiWidget *widget, float shrink);
void AP_GuiWidgetSetFlexBasis(AP_GuiWidget *widget, float basis);

float AP_GuiWidgetFlexGrow(AP_GuiWidget *widget);
float AP_GuiWidgetFlexShrink(AP_GuiWidget *widget);
float AP_GuiWidgetFlexBasis(AP_GuiWidget *widget);

/* Spacing Properties */
void AP_GuiWidgetSetMarginAll(AP_GuiWidget *widget, float margin);
void AP_GuiWidgetSetMarginVertical(AP_GuiWidget *widget, float margin);
void AP_GuiWidgetSetMarginHorizontal(AP_GuiWidget *widget, float margin);

void AP_GuiWidgetSetPaddingAll(AP_GuiWidget *widget, float padding);
void AP_GuiWidgetSetPaddingVertical(AP_GuiWidget *widget, float padding);
void AP_GuiWidgetSetPaddingHorizontal(AP_GuiWidget *widget, float padding);

/* Border Properties */
void AP_GuiWidgetSetBorder(AP_GuiWidget *widget, AP_GuiBorder border);
AP_GuiBorder AP_GuiWidgetBorder(AP_GuiWidget *widget);

void AP_GuiWidgetSetBorderRadius(AP_GuiWidget *widget, float radius);
void AP_GuiWidgetSetBorderRadiusCorners(AP_GuiWidget *widget, float top_left,
                                        float top_right, float bottom_right,
                                        float bottom_left);

/* Shadow Properties */
void AP_GuiWidgetSetShadow(AP_GuiWidget *widget, AP_GuiShadow shadow);
AP_GuiShadow AP_GuiWidgetShadow(AP_GuiWidget *widget);

/* Overflow & Clipping */
void AP_GuiWidgetSetOverflow(AP_GuiWidget *widget, AP_GuiOverflow overflow);
AP_GuiOverflow AP_GuiWidgetOverflow(AP_GuiWidget *widget);

void AP_GuiWidgetSetClipped(AP_GuiWidget *widget, bool clipped);
bool AP_GuiWidgetClipped(AP_GuiWidget *widget);

/* Opacity & Visibility */
void AP_GuiWidgetSetOpacity(AP_GuiWidget *widget, float opacity);
float AP_GuiWidgetOpacity(AP_GuiWidget *widget);

void AP_GuiWidgetSetPointerEvents(AP_GuiWidget *widget, bool accepts_events);
bool AP_GuiWidgetPointerEvents(AP_GuiWidget *widget);

/* Style Class System (CSS-like class names) */
void AP_GuiWidgetAddClass(AP_GuiWidget *widget, const char *class_name);
void AP_GuiWidgetRemoveClass(AP_GuiWidget *widget, const char *class_name);
bool AP_GuiWidgetHasClass(AP_GuiWidget *widget, const char *class_name);
void AP_GuiWidgetClearClasses(AP_GuiWidget *widget);

/* Inline Style Strings (Tailwind-like utility class support) */
void AP_GuiWidgetSetStyle(AP_GuiWidget *widget, const char *style);
const char *AP_GuiWidgetStyle(AP_GuiWidget *widget);

/* =========================================================
 * Graphics Canvas Widget (2D/3D Rendering)
 * ========================================================= */

typedef enum AP_GuiCanvasRenderMode {
  AP_GUI_CANVAS_2D = 0,
  AP_GUI_CANVAS_3D,
  AP_GUI_CANVAS_CUSTOM
} AP_GuiCanvasRenderMode;

/* Canvas render callback: called every frame to render custom content */
typedef void (*AP_GuiCanvasRenderCallback)(AP_GuiWidget *canvas,
                                           AP_FRect bounds, void *user_data);

/* Canvas creation and setup */
AP_GuiWidget *AP_GuiCanvasNew(float width, float height);

void AP_GuiCanvasSetRenderMode(AP_GuiWidget *canvas,
                               AP_GuiCanvasRenderMode mode);
AP_GuiCanvasRenderMode AP_GuiCanvasGetRenderMode(AP_GuiWidget *canvas);

/* Set custom render callback for the canvas */
void AP_GuiCanvasSetRenderCallback(AP_GuiWidget *canvas,
                                   AP_GuiCanvasRenderCallback callback,
                                   void *user_data);

/* Clear the canvas (useful for 2D drawing) */
void AP_GuiCanvasClear(AP_GuiWidget *canvas, AP_FColor color);

/* Get the render target texture (for advanced users) */
AP_Texture *AP_GuiCanvasTexture(AP_GuiWidget *canvas);

/* Draw primitives directly to canvas (2D mode) */
void AP_GuiCanvasDrawRect(AP_GuiWidget *canvas, AP_FRect rect, AP_FColor color,
                          bool filled);
void AP_GuiCanvasDrawCircle(AP_GuiWidget *canvas, float x, float y,
                            float radius, AP_FColor color, bool filled);
void AP_GuiCanvasDrawLine(AP_GuiWidget *canvas, float x1, float y1, float x2,
                          float y2, AP_FColor color, float width);
void AP_GuiCanvasDrawText(AP_GuiWidget *canvas, const char *text, float x,
                          float y, AP_FColor color);
void AP_GuiCanvasDrawImage(AP_GuiWidget *canvas, AP_Texture *texture,
                           AP_FRect src, AP_FRect dst);

/* 3D rendering helpers: binds a texture sized to the canvas as the render
   target, so a normal AP_Begin3D(&camera)/.../AP_End3D() scene draws into
   it; AP_GuiCanvasEnd3D() unbinds and blits the result into the widget. */
void AP_GuiCanvasSetup3D(AP_GuiWidget *canvas);
void AP_GuiCanvasEnd3D(AP_GuiWidget *canvas);

/*
 * Full-renderer 2D mode: sets the clip rect to the canvas bounds and
 * translates so (0,0) is the canvas's top-left corner, then lets you
 * call ANY normal AP2 2D function directly (AP_FillRect, AP_DrawTexture,
 * AP_DrawText, AP_FillCircleF, custom shaders, ...) using local
 * coordinates, exactly as you would in a plain window:
 *
 *     AP_GuiCanvasBegin2D(canvas);
 *     AP_SetDrawColor(1, 0, 0, 1);
 *     AP_FillRect(&(AP_FRect){10, 10, 64, 64});
 *     AP_DrawText(8, 8, "Hello");
 *     AP_GuiCanvasEnd2D(canvas);
 *
 * Safe to nest inside a render callback set via
 * AP_GuiCanvasSetRenderCallback; the canvas's own draw dispatch already
 * clips the callback to its bounds, so this is purely for local-origin
 * coordinates and is optional but recommended for direct renderer use.
 */
void AP_GuiCanvasBegin2D(AP_GuiWidget *canvas);
void AP_GuiCanvasEnd2D(AP_GuiWidget *canvas);

/* Canvas size in pixels (matches the widget's current on-screen size) */
float AP_GuiCanvasWidth(AP_GuiWidget *canvas);
float AP_GuiCanvasHeight(AP_GuiWidget *canvas);

/* Mouse/input event positions within canvas */
bool AP_GuiCanvasGetMousePos(AP_GuiWidget *canvas, float *x, float *y);
bool AP_GuiCanvasIsMouseInside(AP_GuiWidget *canvas);

/* =========================================================
 * Popups (context menus, dropdowns, modal dialogs)
 *
 * A popup is a freestanding widget (it has no parent) that floats
 * above the rest of the tree, positioned in absolute screen space.
 * Build its contents the same way as any other container:
 *
 *     AP_GuiWidget *menu = AP_GuiPopupNew();
 *     AP_GuiWidgetSetSize(menu, 160.0f, 120.0f);
 *     AP_GuiWidgetAddChild(menu, AP_GuiButtonNew("Copy"));
 *     AP_GuiWidgetAddChild(menu, AP_GuiButtonNew("Paste"));
 *
 *     // in the trigger button's "clicked" callback:
 *     AP_GuiPopupOpenNear(menu, trigger_button, AP_GUI_POPUP_BELOW);
 *
 * Non-modal popups (the default) close themselves the frame a click
 * lands outside their bounds. Modal popups block input to the rest
 * of the tree until explicitly closed with AP_GuiPopupClose.
 *
 * Call AP_GuiWidgetUpdate / AP_GuiWidgetRender on the root as usual;
 * open popups are updated and rendered on top automatically.
 * ========================================================= */

AP_GuiWidget *AP_GuiPopupNew(void);

void AP_GuiPopupOpenAt(AP_GuiWidget *popup, float x, float y);
void AP_GuiPopupOpenNear(AP_GuiWidget *popup, AP_GuiWidget *anchor,
                         AP_GuiPopupPlacement placement);
void AP_GuiPopupClose(AP_GuiWidget *popup);
bool AP_GuiPopupIsOpen(AP_GuiWidget *popup);

/* Modal popups suppress update/input on the rest of the tree while open. */
void AP_GuiPopupSetModal(AP_GuiWidget *popup, bool modal);
bool AP_GuiPopupIsModal(AP_GuiWidget *popup);

/* Default true: a click outside the popup's bounds closes it. */
void AP_GuiPopupSetCloseOnOutsideClick(AP_GuiWidget *popup,
                                       bool close_on_outside_click);
bool AP_GuiPopupCloseOnOutsideClick(AP_GuiWidget *popup);

/* =========================================================
 * Utility Functions
 * ========================================================= */

/* Find widget by name (recursive search) */
AP_GuiWidget *AP_GuiFindWidget(AP_GuiWidget *root, const char *name);

/* Get widget at screen position */
AP_GuiWidget *AP_GuiWidgetAt(AP_GuiWidget *root, float x, float y);

/* Layout debugging */
void AP_GuiWidgetPrintHierarchy(AP_GuiWidget *widget, int indent);

/* Parse and apply Tailwind-like style strings */
void AP_GuiApplyStyleString(AP_GuiWidget *widget, const char *style_string);

#ifdef __cplusplus
}
#endif

#endif /* AP2_NO_GUI_ADVANCED */

#endif /* AP2_GUI_ADVANCED_H */
