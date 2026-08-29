/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Gui.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Input.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Window.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define AP_GUI_MAX_PANELS 16
#define AP_GUI_MAX_WINDOWS 64
#define AP_GUI_MAX_KV 256
#define AP_GUI_ID_STACK 16
#define AP_GUI_WIDTH_STACK 8
#define AP_GUI_GROUP_STACK 4
#define AP_GUI_MAX_COLUMNS 8
#define AP_GUI_LABEL_MAX 128

typedef struct AP_GuiClip {
  AP_Rect rect;
  bool enabled;
} AP_GuiClip;

typedef struct AP_GuiKv {
  AP_U32 id;
  int i;
  float f;
  bool used;
} AP_GuiKv;

typedef struct AP_GuiWindowState {
  AP_U32 id;
  AP_FRect rect;
  bool used;
  bool dragging;
  bool resizing;
  bool collapsed;
  float drag_dx;
  float drag_dy;
  float scroll_y;
  float content_height;
} AP_GuiWindowState;

typedef struct AP_GuiPanel {
  AP_U32 id;
  AP_FRect rect;
  AP_FRect content;
  float cursor_x;
  float cursor_y;
  float line_x;
  float line_max_y;
  float content_start_y;
  float item_width;
  AP_U32 flags;
  int column_count;
  int column_index;
  float column_y[AP_GUI_MAX_COLUMNS];
  float column_max_y[AP_GUI_MAX_COLUMNS];
  bool visible;
  bool is_child;
  bool is_popup;
  bool clip;
  AP_GuiClip prev_clip;
  AP_GuiWindowState *window;
} AP_GuiPanel;

typedef struct AP_GuiGroup {
  float x;
  float y;
  float max_x;
  float max_y;
} AP_GuiGroup;

static AP_GuiStyle g_style;
static bool g_style_ready = false;
static AP_Font *g_gui_font = NULL;
static AP_GuiLayer g_gui_layer = AP_GUI_LAYER_OVERLAY;
static AP_GuiPanel g_panels[AP_GUI_MAX_PANELS];
static int g_panel_count = 0;
static AP_GuiWindowState g_windows[AP_GUI_MAX_WINDOWS];
static AP_GuiKv g_kv[AP_GUI_MAX_KV];
static AP_U32 g_id_stack[AP_GUI_ID_STACK];
static int g_id_depth = 0;
static float g_width_stack[AP_GUI_WIDTH_STACK];
static int g_width_depth = 0;
static AP_GuiGroup g_groups[AP_GUI_GROUP_STACK];
static int g_group_depth = 0;
static bool g_same_line = false;
static float g_same_spacing = -1.0f;
static float g_same_offset_y = 0.0f;
static float g_indent = 0.0f;
static bool g_has_next_pos = false;
static bool g_has_next_size = false;
static AP_U32 g_next_flags = 0;
static bool g_has_next_flags = false;
static float g_next_x = 0.0f;
static float g_next_y = 0.0f;
static float g_next_w = 0.0f;
static float g_next_h = 0.0f;
static AP_U32 g_hot = 0;
static AP_U32 g_active = 0;
static AP_U32 g_focus = 0;
static bool g_want_mouse = false;
static bool g_want_keyboard = false;
static int g_disabled_count = 0;
static bool g_disabled_stack[8];
static int g_disabled_depth = 0;
static int g_menu_bar_items = 0;
static int g_image_button_index = 0;
static AP_U32 g_last_id = 0;
static AP_FRect g_last_item;
static bool g_last_hovered = false;
static bool g_last_clicked = false;
static bool g_window_hovered = false;
static char g_tooltip[256];
static bool g_has_tooltip = false;

static AP_U32 g_popup_queued = 0;
static AP_U32 g_popup_id = 0;
static AP_U32 g_popup_owner = 0;
static AP_FRect g_popup_rect;
static AP_FRect g_popup_owner_rect;
static float g_popup_pos_x = 0.0f;
static float g_popup_pos_y = 0.0f;
static float g_popup_width = 180.0f;
static bool g_popup_closing = false;

static bool g_in_menu_bar = false;
static AP_U32 g_menu_bar_open = 0;
static AP_FRect g_menu_bar_rect;

static bool g_tab_active = false;
static AP_U32 g_tab_bar_id = 0;
static AP_U32 g_tab_selected = 0;
static float g_tab_x = 0.0f;
static float g_tab_y = 0.0f;
static float g_tab_h = 0.0f;

static AP_FColor AP_GuiColor(float r, float g, float b, float a) {
  AP_FColor color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = a;
  return color;
}

static float AP_GuiMinf(float a, float b) { return a < b ? a : b; }

static float AP_GuiMaxf(float a, float b) { return a > b ? a : b; }

static float AP_GuiClampf(float x, float lo, float hi) {
  return AP_GuiMaxf(lo, AP_GuiMinf(x, hi));
}

static AP_U32 AP_GuiHash(const char *text) {
  AP_U32 hash = 2166136261u;
  if (text == NULL) {
    return hash;
  }

  while (*text != '\0') {
    hash ^= (unsigned char)*text++;
    hash *= 16777619u;
  }

  return hash;
}

static AP_U32 AP_GuiMix(AP_U32 a, AP_U32 b) {
  return a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
}

static const char *AP_GuiVisibleLabel(const char *label, char *buffer,
                                      int capacity) {
  const char *mark;
  int length;

  if (label == NULL) {
    buffer[0] = '\0';
    return buffer;
  }

  mark = strstr(label, "##");
  length = mark != NULL ? (int)(mark - label) : (int)strlen(label);
  if (length >= capacity) {
    length = capacity - 1;
  }

  memcpy(buffer, label, (size_t)length);
  buffer[length] = '\0';
  return buffer;
}

static AP_U32 AP_GuiSeed(void) {
  if (g_id_depth > 0) {
    return g_id_stack[g_id_depth - 1];
  }
  if (g_panel_count > 0) {
    return g_panels[g_panel_count - 1].id;
  }
  return 0;
}

static AP_U32 AP_GuiId(const char *label) {
  return AP_GuiMix(AP_GuiSeed(), AP_GuiHash(label != NULL ? label : ""));
}

static void AP_GuiEnsureStyle(void) {
  if (!g_style_ready) {
    g_style = AP_GuiDarkStyle();
    g_style_ready = true;
  }
}

static AP_Font *AP_GuiFont(void) {
  if (g_gui_font != NULL) {
    return g_gui_font;
  }
  return AP_GetDefaultFont();
}

static float AP_GuiFontSize(void) {
  AP_GuiEnsureStyle();
  if (g_style.font_size > 0.0f) {
    return g_style.font_size;
  }
  return AP_GetFontSize(AP_GuiFont());
}

static float AP_GuiTextWidth(const char *text) {
  char visible[AP_GUI_LABEL_MAX];
  return AP_MeasureTextEx(AP_GuiFont(),
                          AP_GuiVisibleLabel(text, visible, sizeof(visible)),
                          AP_GuiFontSize())
      .x;
}

static AP_FColor AP_GuiTextCol(void) {
  return g_disabled_count > 0 ? g_style.text_disabled : g_style.text;
}

static void AP_GuiSaveClip(AP_GuiClip *clip) {
  clip->enabled = AP_RenderClipEnabled();
  AP_GetRenderClipRect(&clip->rect);
}

static void AP_GuiRestoreClip(const AP_GuiClip *clip) {
  if (clip->enabled) {
    AP_SetRenderClipRect(&clip->rect);
  } else {
    AP_SetRenderClipRect(NULL);
  }
}

static void AP_GuiPushClipRect(const AP_FRect *rect) {
  AP_Rect clip;
  AP_GuiClip prev;
  float x1;
  float y1;
  float x2;
  float y2;

  AP_GuiSaveClip(&prev);
  x1 = rect->x;
  y1 = rect->y;
  x2 = rect->x + rect->w;
  y2 = rect->y + rect->h;
  if (prev.enabled) {
    x1 = AP_GuiMaxf(x1, (float)prev.rect.x);
    y1 = AP_GuiMaxf(y1, (float)prev.rect.y);
    x2 = AP_GuiMinf(x2, (float)(prev.rect.x + prev.rect.w));
    y2 = AP_GuiMinf(y2, (float)(prev.rect.y + prev.rect.h));
  }

  clip.x = (int)x1;
  clip.y = (int)y1;
  clip.w = (int)(x2 - x1);
  clip.h = (int)(y2 - y1);
  if (clip.w < 0) {
    clip.w = 0;
  }
  if (clip.h < 0) {
    clip.h = 0;
  }
  AP_SetRenderClipRect(&clip);
}

static void AP_GuiDrawRect(const AP_FRect *rect, AP_FColor color, bool fill) {
  float rounding = g_style.rounding;
  if (rect == NULL) {
    return;
  }
  if (rounding > rect->h * 0.5f) {
    rounding = rect->h * 0.5f;
  }
  AP_SetRenderDrawColorFloat(color.r, color.g, color.b, color.a);
  if (fill) {
    AP_RenderFillRoundedRect(rect, rounding);
  } else {
    AP_SetRenderLineWidth(g_style.border_width > 0.0f ? g_style.border_width
                                                      : 1.0f);
    AP_RenderRoundedRect(rect, rounding);
  }
}

static bool AP_GuiContains(const AP_FRect *rect, float x, float y) {
  return rect != NULL && x >= rect->x && y >= rect->y &&
         x < rect->x + rect->w && y < rect->y + rect->h;
}

static AP_GuiKv *AP_GuiKvFind(AP_U32 id, int initial, float initial_f) {
  int index;
  int empty = -1;

  for (index = 0; index < AP_GUI_MAX_KV; ++index) {
    if (g_kv[index].used && g_kv[index].id == id) {
      return &g_kv[index];
    }
    if (!g_kv[index].used && empty < 0) {
      empty = index;
    }
  }

  if (empty < 0) {
    empty = 0;
  }

  g_kv[empty].id = id;
  g_kv[empty].used = true;
  g_kv[empty].i = initial;
  g_kv[empty].f = initial_f;
  return &g_kv[empty];
}

static AP_GuiWindowState *AP_GuiFindWindow(AP_U32 id, const AP_FRect *fallback) {
  int index;
  int empty = -1;

  for (index = 0; index < AP_GUI_MAX_WINDOWS; ++index) {
    if (g_windows[index].used && g_windows[index].id == id) {
      return &g_windows[index];
    }
    if (!g_windows[index].used && empty < 0) {
      empty = index;
    }
  }

  if (empty < 0) {
    empty = 0;
  }

  memset(&g_windows[empty], 0, sizeof(g_windows[empty]));
  g_windows[empty].id = id;
  g_windows[empty].used = true;
  g_windows[empty].rect = *fallback;
  return &g_windows[empty];
}

static AP_GuiPanel *AP_GuiCurrentPanel(void) {
  if (g_panel_count <= 0) {
    return NULL;
  }
  return &g_panels[g_panel_count - 1];
}

static bool AP_GuiSkip(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  return panel != NULL && !panel->visible;
}

static bool AP_GuiInteractable(AP_U32 id) {
  AP_GuiPanel *panel;

  if (g_disabled_count > 0) {
    return false;
  }

  if (g_popup_id == 0 || g_popup_closing) {
    return true;
  }

  panel = AP_GuiCurrentPanel();
  if (panel != NULL && panel->is_popup) {
    return true;
  }

  return id == g_popup_owner;
}

static void AP_GuiSetLast(AP_U32 id, const AP_FRect *rect, bool hovered,
                          bool clicked) {
  g_last_id = id;
  g_last_item = *rect;
  g_last_hovered = hovered;
  g_last_clicked = clicked;
  if (g_group_depth > 0) {
    AP_GuiGroup *group = &g_groups[g_group_depth - 1];
    group->max_x = AP_GuiMaxf(group->max_x, rect->x + rect->w);
    group->max_y = AP_GuiMaxf(group->max_y, rect->y + rect->h);
  }
}

static AP_FRect AP_GuiNextRect(float height, float width) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FRect rect;
  float spacing;
  float avail;

  AP_GuiEnsureStyle();
  spacing = g_same_spacing >= 0.0f ? g_same_spacing : g_style.spacing;
  g_same_spacing = -1.0f;

  if (panel == NULL) {
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = width;
    rect.h = height;
    return rect;
  }

  if (g_same_line) {
    rect.x = panel->cursor_x + spacing;
    rect.y = panel->cursor_y + g_same_offset_y;
    g_same_line = false;
    g_same_offset_y = 0.0f;
  } else {
    rect.x = panel->line_x + g_indent;
    rect.y = panel->cursor_y + panel->line_max_y;
    if (panel->line_max_y > 0.0f) {
      rect.y += g_style.spacing;
    }
    panel->cursor_y = rect.y;
    panel->line_max_y = 0.0f;
    if (panel->column_count > 1) {
      panel->column_y[panel->column_index] = rect.y;
    }
  }

  avail = panel->content.x + panel->content.w - rect.x;
  if (panel->column_count > 1) {
    float col_w = panel->content.w / (float)panel->column_count;
    avail = panel->content.x + col_w * (float)(panel->column_index + 1) -
            g_style.padding - rect.x;
  }

  if (width <= 0.0f) {
    width = panel->item_width > 0.0f ? panel->item_width : avail;
  }

  if (width < 8.0f) {
    width = 8.0f;
  }

  rect.w = width;
  rect.h = height;
  panel->cursor_x = rect.x + rect.w;
  if (rect.h > panel->line_max_y) {
    panel->line_max_y = rect.h;
  }
  if (panel->column_count > 1 && rect.h > panel->column_max_y[panel->column_index]) {
    panel->column_max_y[panel->column_index] = rect.h;
  }

  return rect;
}

static bool AP_GuiHovered(const AP_FRect *rect) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  float mx = (float)AP_GetMouseX();
  float my = (float)AP_GetMouseY();

  if (!AP_GuiContains(rect, mx, my)) {
    return false;
  }

  if (panel != NULL && panel->clip && !AP_GuiContains(&panel->content, mx, my)) {
    return false;
  }

  return true;
}

static AP_FColor AP_GuiWidgetColor(AP_U32 id, const AP_FRect *rect) {
  bool hovered = AP_GuiHovered(rect) && AP_GuiInteractable(id);

  if (hovered) {
    g_hot = id;
    g_want_mouse = true;
  }

  if (g_disabled_count > 0) {
    return g_style.widget_bg;
  }
  if (g_active == id) {
    return g_style.widget_active;
  }
  if (hovered) {
    return g_style.widget_hovered;
  }
  return g_style.widget_bg;
}

static bool AP_GuiClicked(AP_U32 id, const AP_FRect *rect) {
  bool hovered = AP_GuiHovered(rect);
  bool can = AP_GuiInteractable(id);

  if (hovered && can) {
    g_hot = id;
    g_want_mouse = true;
    if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
      g_active = id;
    }
  }

  if (g_popup_id != 0 && !can && AP_IsMousePressed(AP_MOUSE_LEFT) &&
      !AP_GuiContains(&g_popup_rect, (float)AP_GetMouseX(),
                      (float)AP_GetMouseY()) &&
      !AP_GuiContains(&g_popup_owner_rect, (float)AP_GetMouseX(),
                      (float)AP_GetMouseY())) {
    g_popup_closing = true;
    g_popup_id = 0;
  }

  if (g_active == id && AP_IsMouseReleased(AP_MOUSE_LEFT)) {
    g_active = 0;
    return hovered && can;
  }

  return false;
}

static void AP_GuiDrawTooltip(void) {
  AP_FPoint size;
  AP_FRect rect;
  float mx;
  float my;
  int pixel_w = 0;
  int pixel_h = 0;

  if (!g_has_tooltip) {
    return;
  }

  size = AP_MeasureTextEx(AP_GuiFont(), g_tooltip, AP_GuiFontSize());
  mx = (float)AP_GetMouseX();
  my = (float)AP_GetMouseY();
  AP_GetWindowSizeInPixels(&pixel_w, &pixel_h);
  rect.x = mx + 16.0f;
  rect.y = my + 16.0f;
  rect.w = size.x + 16.0f;
  rect.h = size.y + 10.0f;
  if (rect.x + rect.w > (float)pixel_w) {
    rect.x = mx - rect.w - 8.0f;
  }
  if (rect.y + rect.h > (float)pixel_h) {
    rect.y = my - rect.h - 8.0f;
  }

  AP_SetRenderClipRect(NULL);
  AP_GuiDrawRect(&rect, g_style.popup_bg, true);
  AP_GuiDrawRect(&rect, g_style.border, false);
  rect.x += 8.0f;
  rect.w -= 16.0f;
  AP_RenderTextAligned(AP_GuiFont(), &rect, g_tooltip, g_style.text,
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  g_has_tooltip = false;
}

static void AP_GuiDrawScrollbar(AP_GuiPanel *panel, AP_GuiWindowState *state) {
  AP_FRect bar;
  AP_FRect grab;
  float overflow;
  float grab_h;
  float t;
  AP_U32 id;

  if (state == NULL || (panel->flags & AP_GUI_WINDOW_NO_SCROLL) != 0) {
    return;
  }

  overflow = state->content_height - panel->content.h;
  if (overflow <= 1.0f) {
    state->scroll_y = 0.0f;
    return;
  }

  bar.x = panel->rect.x + panel->rect.w - g_style.scrollbar_size - 3.0f;
  bar.y = panel->content.y;
  bar.w = g_style.scrollbar_size;
  bar.h = panel->content.h;
  AP_GuiDrawRect(&bar, g_style.widget_bg, true);

  grab_h = bar.h * (panel->content.h / state->content_height);
  grab_h = AP_GuiClampf(grab_h, 16.0f, bar.h);
  t = AP_GuiClampf(state->scroll_y / overflow, 0.0f, 1.0f);
  grab = bar;
  grab.h = grab_h;
  grab.y = bar.y + t * (bar.h - grab_h);

  id = AP_GuiMix(panel->id, AP_GuiHash("##scroll"));
  if (AP_GuiClicked(id, &bar) || g_active == id) {
    float my = (float)AP_GetMouseY();
    float ratio = (my - bar.y - grab_h * 0.5f) / AP_GuiMaxf(1.0f, bar.h - grab_h);
    state->scroll_y = AP_GuiClampf(ratio, 0.0f, 1.0f) * overflow;
    g_active = id;
    g_want_mouse = true;
    if (!AP_IsMouseDown(AP_MOUSE_LEFT)) {
      g_active = 0;
    }
  }

  AP_GuiDrawRect(&grab, g_style.scrollbar, true);
}

/* =========================================================
 * Style
 * ========================================================= */

AP_GuiStyle AP_GuiDarkStyle(void) {
  AP_GuiStyle style;
  memset(&style, 0, sizeof(style));
  style.window_bg = AP_GuiColor(0.11f, 0.12f, 0.15f, 0.96f);
  style.title_bg = AP_GuiColor(0.16f, 0.18f, 0.24f, 1.0f);
  style.title_text = AP_GuiColor(0.94f, 0.95f, 0.97f, 1.0f);
  style.menubar_bg = AP_GuiColor(0.14f, 0.15f, 0.20f, 1.0f);
  style.popup_bg = AP_GuiColor(0.13f, 0.14f, 0.18f, 0.98f);
  style.border = AP_GuiColor(1.0f, 1.0f, 1.0f, 0.08f);
  style.text = AP_GuiColor(0.90f, 0.91f, 0.94f, 1.0f);
  style.text_disabled = AP_GuiColor(0.55f, 0.57f, 0.62f, 1.0f);
  style.widget_bg = AP_GuiColor(0.18f, 0.20f, 0.25f, 1.0f);
  style.widget_hovered = AP_GuiColor(0.24f, 0.28f, 0.36f, 1.0f);
  style.widget_active = AP_GuiColor(0.28f, 0.42f, 0.82f, 1.0f);
  style.header = AP_GuiColor(0.20f, 0.24f, 0.32f, 1.0f);
  style.header_hovered = AP_GuiColor(0.26f, 0.32f, 0.44f, 1.0f);
  style.accent = AP_GuiColor(0.35f, 0.55f, 1.00f, 1.0f);
  style.check_mark = AP_GuiColor(0.95f, 0.96f, 1.00f, 1.0f);
  style.slider_grab = AP_GuiColor(0.45f, 0.62f, 1.00f, 1.0f);
  style.separator = AP_GuiColor(1.0f, 1.0f, 1.0f, 0.10f);
  style.dim = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.45f);
  style.scrollbar = AP_GuiColor(0.40f, 0.45f, 0.55f, 1.0f);
  style.rounding = 8.0f;
  style.padding = 12.0f;
  style.spacing = 8.0f;
  style.title_height = 32.0f;
  style.widget_height = 28.0f;
  style.border_width = 1.0f;
  style.font_size = 16.0f;
  style.grab_width = 12.0f;
  style.scrollbar_size = 12.0f;
  style.indent_size = 16.0f;
  style.min_window_w = 160.0f;
  style.min_window_h = 80.0f;
  return style;
}

AP_GuiStyle AP_GuiLightStyle(void) {
  AP_GuiStyle style = AP_GuiDarkStyle();
  style.window_bg = AP_GuiColor(0.94f, 0.94f, 0.96f, 0.98f);
  style.title_bg = AP_GuiColor(0.35f, 0.50f, 0.90f, 1.0f);
  style.title_text = AP_GuiColor(1.0f, 1.0f, 1.0f, 1.0f);
  style.menubar_bg = AP_GuiColor(0.88f, 0.89f, 0.92f, 1.0f);
  style.popup_bg = AP_GuiColor(0.97f, 0.97f, 0.98f, 0.98f);
  style.border = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.12f);
  style.text = AP_GuiColor(0.12f, 0.13f, 0.16f, 1.0f);
  style.text_disabled = AP_GuiColor(0.45f, 0.47f, 0.52f, 1.0f);
  style.widget_bg = AP_GuiColor(1.0f, 1.0f, 1.0f, 1.0f);
  style.widget_hovered = AP_GuiColor(0.88f, 0.91f, 0.98f, 1.0f);
  style.widget_active = AP_GuiColor(0.70f, 0.80f, 0.98f, 1.0f);
  style.header = AP_GuiColor(0.82f, 0.86f, 0.95f, 1.0f);
  style.header_hovered = AP_GuiColor(0.74f, 0.80f, 0.94f, 1.0f);
  style.accent = AP_GuiColor(0.25f, 0.45f, 0.90f, 1.0f);
  style.check_mark = AP_GuiColor(0.20f, 0.35f, 0.85f, 1.0f);
  style.slider_grab = AP_GuiColor(0.25f, 0.45f, 0.90f, 1.0f);
  style.separator = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.12f);
  style.dim = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.35f);
  style.scrollbar = AP_GuiColor(0.55f, 0.58f, 0.65f, 1.0f);
  return style;
}

AP_GuiStyle AP_GuiDefaultStyle(void) { return AP_GuiDarkStyle(); }

void AP_GuiSetStyle(const AP_GuiStyle *style) {
  AP_GuiEnsureStyle();
  if (style != NULL) {
    g_style = *style;
    g_style_ready = true;
  }
}

AP_GuiStyle *AP_GuiGetStyle(void) {
  AP_GuiEnsureStyle();
  return &g_style;
}

bool AP_GuiSetFont(AP_Font *font) {
  if (font != NULL && !AP_FontIsValid(font)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "GUI font is not valid");
    return false;
  }

  g_gui_font = font;
  return true;
}

AP_Font *AP_GuiGetFont(void) { return AP_GuiFont(); }

void AP_SetGuiLayer(AP_GuiLayer layer) {
  if (layer > AP_GUI_LAYER_OVERLAY) {
    layer = AP_GUI_LAYER_OVERLAY;
  }

  g_gui_layer = layer;
}

AP_GuiLayer AP_GetGuiLayer(void) { return g_gui_layer; }

bool AP_GuiLayerEnabled(void) { return g_gui_layer != AP_GUI_LAYER_OFF; }

bool AP_GuiWantCaptureMouse(void) {
  return g_gui_layer != AP_GUI_LAYER_OFF && (g_want_mouse || g_active != 0);
}

bool AP_GuiWantCaptureKeyboard(void) {
  return g_gui_layer != AP_GUI_LAYER_OFF && g_want_keyboard;
}

void AP_GuiSetNextPanelPos(float x, float y) {
  g_next_x = x;
  g_next_y = y;
  g_has_next_pos = true;
}

void AP_GuiSetNextPanelSize(float width, float height) {
  g_next_w = width;
  g_next_h = height;
  g_has_next_size = true;
}

void AP_GuiSetNextWindowPos(float x, float y) { AP_GuiSetNextPanelPos(x, y); }

void AP_GuiSetNextWindowSize(float width, float height) {
  AP_GuiSetNextPanelSize(width, height);
}

void AP_GuiSetNextWindowFlags(AP_U32 flags) {
  g_next_flags = flags;
  g_has_next_flags = true;
}

void AP_GuiShutdown(void) {
  memset(g_windows, 0, sizeof(g_windows));
  memset(g_kv, 0, sizeof(g_kv));
  g_panel_count = 0;
  g_gui_font = NULL;
  g_id_depth = 0;
  g_width_depth = 0;
  g_group_depth = 0;
  g_hot = 0;
  g_active = 0;
  g_focus = 0;
  g_want_mouse = false;
  g_want_keyboard = false;
  g_popup_id = 0;
  g_popup_queued = 0;
  g_disabled_count = 0;
}

static void AP_GuiBeginFrame(void) {
  g_want_mouse = false;
  g_want_keyboard = false;
  g_window_hovered = false;
  g_hot = 0;
  g_has_tooltip = false;
  g_in_menu_bar = false;
  g_tab_active = false;
  g_popup_closing = false;
  g_image_button_index = 0;
  if (AP_IsKeyPressed(AP_KEY_ESCAPE) && g_popup_id != 0) {
    g_popup_id = 0;
  }
}

bool AP_GuiBeginPanelEx(const char *title, AP_FRect *rect, bool *open) {
  AP_GuiPanel *panel;
  AP_GuiWindowState *state;
  AP_FRect area;
  AP_FRect title_bar;
  AP_FRect close_rect;
  AP_FRect grip;
  AP_U32 id;
  AP_U32 flags;
  float mx;
  float my;
  float title_h;
  float menu_h;
  char visible[AP_GUI_LABEL_MAX];
  bool hovered;
  bool can_move;
  bool can_resize;
  bool can_collapse;

  AP_GuiEnsureStyle();
  AP_FontInit();

  if (title == NULL) {
    title = "Window";
  }

  if (g_panel_count == 0) {
    AP_GuiBeginFrame();
  }

  if (g_panel_count >= AP_GUI_MAX_PANELS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "GUI panel stack overflow");
    return false;
  }

  flags = 0;
  if (g_has_next_flags) {
    flags = g_next_flags;
    g_has_next_flags = false;
    g_next_flags = 0;
  }

  id = AP_GuiHash(title);
  area.x = 32.0f;
  area.y = 32.0f;
  area.w = 280.0f;
  area.h = 320.0f;
  if (rect != NULL) {
    area = *rect;
  }

  state = AP_GuiFindWindow(id, &area);
  if (g_has_next_pos) {
    state->rect.x = g_next_x;
    state->rect.y = g_next_y;
    g_has_next_pos = false;
  }
  if (g_has_next_size) {
    state->rect.w = g_next_w;
    state->rect.h = g_next_h;
    g_has_next_size = false;
  }

  if (open != NULL && !*open) {
    return false;
  }

  mx = (float)AP_GetMouseX();
  my = (float)AP_GetMouseY();
  can_move = (flags & AP_GUI_WINDOW_NO_MOVE) == 0;
  can_resize = (flags & AP_GUI_WINDOW_NO_RESIZE) == 0;
  can_collapse = (flags & AP_GUI_WINDOW_NO_COLLAPSE) == 0 &&
                 (flags & AP_GUI_WINDOW_NO_TITLE) == 0;
  title_h = (flags & AP_GUI_WINDOW_NO_TITLE) != 0 ? 0.0f : g_style.title_height;
  menu_h = (flags & AP_GUI_WINDOW_MENU_BAR) != 0 ? g_style.widget_height : 0.0f;

  title_bar = state->rect;
  title_bar.h = title_h;
  hovered = AP_GuiContains(&state->rect, mx, my);
  if (hovered) {
    g_want_mouse = true;
    g_window_hovered = true;
  }

  grip.w = 14.0f;
  grip.h = 14.0f;
  grip.x = state->rect.x + state->rect.w - grip.w;
  grip.y = state->rect.y + state->rect.h - grip.h;

  close_rect.x = state->rect.x + state->rect.w - g_style.title_height;
  close_rect.y = state->rect.y;
  close_rect.w = g_style.title_height;
  close_rect.h = g_style.title_height;

  if (can_resize && AP_GuiContains(&grip, mx, my) &&
      AP_IsMousePressed(AP_MOUSE_LEFT)) {
    state->resizing = true;
    g_active = AP_GuiMix(id, AP_GuiHash("##resize"));
  }

  if (state->resizing) {
    if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
      state->rect.w = AP_GuiMaxf(g_style.min_window_w, mx - state->rect.x);
      state->rect.h = AP_GuiMaxf(g_style.min_window_h, my - state->rect.y);
      g_want_mouse = true;
    } else {
      state->resizing = false;
    }
  }

  if (can_move && title_h > 0.0f && AP_GuiContains(&title_bar, mx, my) &&
      AP_IsMousePressed(AP_MOUSE_LEFT) && !state->resizing) {
    bool on_close = open != NULL && AP_GuiContains(&close_rect, mx, my);
    bool on_chevron = can_collapse && mx < state->rect.x + g_style.title_height;
    if (!on_close && !on_chevron) {
      state->dragging = true;
      state->drag_dx = mx - state->rect.x;
      state->drag_dy = my - state->rect.y;
      g_active = id;
    }
  }

  if (state->dragging) {
    if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
      state->rect.x = mx - state->drag_dx;
      state->rect.y = my - state->drag_dy;
      g_want_mouse = true;
    } else {
      state->dragging = false;
      if (g_active == id) {
        g_active = 0;
      }
    }
  }

  if (rect != NULL) {
    *rect = state->rect;
  }

  if (hovered && (flags & AP_GUI_WINDOW_NO_SCROLL) == 0 && !state->collapsed) {
    state->scroll_y -= (float)AP_GetMouseWheelY() * g_style.widget_height * 1.5f;
  }

  AP_GuiDrawRect(&state->rect, g_style.window_bg, true);

  if (title_h > 0.0f) {
    AP_FRect title_text = title_bar;
    AP_GuiDrawRect(&title_bar, g_style.title_bg, true);
    title_text.x += g_style.padding;
    title_text.w -= g_style.padding * 2.0f;
    if (can_collapse) {
      AP_FRect chevron = title_bar;
      chevron.w = g_style.title_height;
      AP_RenderTextAligned(AP_GuiFont(), &chevron,
                           state->collapsed ? ">" : "v", g_style.title_text,
                           AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
      if (AP_GuiClicked(AP_GuiMix(id, AP_GuiHash("##collapse")), &chevron)) {
        state->collapsed = !state->collapsed;
      }
      title_text.x += g_style.title_height - 8.0f;
      title_text.w -= g_style.title_height - 8.0f;
    }
    if (open != NULL) {
      title_text.w -= g_style.title_height;
    }
    AP_RenderTextAligned(AP_GuiFont(), &title_text,
                         AP_GuiVisibleLabel(title, visible, sizeof(visible)),
                         g_style.title_text, AP_GuiFontSize(),
                         AP_TEXT_ALIGN_LEFT);
    if (open != NULL) {
      AP_RenderTextAligned(AP_GuiFont(), &close_rect, "x", g_style.title_text,
                           AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
      if (AP_GuiClicked(AP_GuiMix(id, AP_GuiHash("##close")), &close_rect)) {
        *open = false;
      }
    }
  }

  AP_GuiDrawRect(&state->rect, g_style.border, false);

  if ((flags & AP_GUI_WINDOW_MENU_BAR) != 0 && !state->collapsed) {
    g_menu_bar_rect.x = state->rect.x;
    g_menu_bar_rect.y = state->rect.y + title_h;
    g_menu_bar_rect.w = state->rect.w;
    g_menu_bar_rect.h = menu_h;
    AP_GuiDrawRect(&g_menu_bar_rect, g_style.menubar_bg, true);
  }

  panel = &g_panels[g_panel_count++];
  memset(panel, 0, sizeof(*panel));
  panel->id = id;
  panel->rect = state->rect;
  panel->flags = flags;
  panel->window = state;
  panel->item_width = g_width_depth > 0 ? g_width_stack[g_width_depth - 1] : 0.0f;
  panel->visible =
      !state->collapsed && (open == NULL || *open);
  panel->content.x = state->rect.x + g_style.padding;
  panel->content.y = state->rect.y + title_h + menu_h + g_style.padding;
  panel->content.w = state->rect.w - g_style.padding * 2.0f - g_style.scrollbar_size;
  panel->content.h =
      state->rect.h - title_h - menu_h - g_style.padding * 2.0f;
  if (panel->content.h < 0.0f) {
    panel->content.h = 0.0f;
  }
  state->scroll_y = AP_GuiClampf(state->scroll_y, 0.0f,
                                 AP_GuiMaxf(0.0f, state->content_height -
                                                      panel->content.h));
  panel->content_start_y = panel->content.y - state->scroll_y;
  panel->cursor_x = panel->content.x;
  panel->cursor_y = panel->content_start_y;
  panel->line_x = panel->content.x;
  panel->line_max_y = 0.0f;
  panel->clip = panel->visible;
  AP_GuiSaveClip(&panel->prev_clip);
  if (panel->clip) {
    AP_FRect clip = panel->content;
    clip.w = state->rect.w - g_style.padding * 2.0f;
    AP_GuiPushClipRect(&clip);
  }

  g_same_line = false;
  g_indent = 0.0f;
  return true;
}

bool AP_GuiBeginPanel(const char *title, float x, float y, float width,
                      float height) {
  AP_FRect rect;
  rect.x = x;
  rect.y = y;
  rect.w = width;
  rect.h = height;
  return AP_GuiBeginPanelEx(title, &rect, NULL);
}

bool AP_GuiBeginWindow(const char *name, bool *open, AP_U32 flags) {
  AP_GuiSetNextWindowFlags(flags);
  return AP_GuiBeginPanelEx(name, NULL, open);
}

void AP_GuiEndPanel(void) {
  AP_GuiPanel *panel;
  AP_GuiWindowState *state;
  float used;

  if (g_panel_count <= 0) {
    return;
  }

  panel = &g_panels[g_panel_count - 1];
  state = panel->window;
  used = (panel->cursor_y + panel->line_max_y) - panel->content_start_y +
         g_style.padding;
  if (state != NULL) {
    state->content_height = used;
    if (panel->visible) {
      AP_GuiDrawScrollbar(panel, state);
    }
    if ((panel->flags & AP_GUI_WINDOW_AUTO_RESIZE) != 0 && panel->visible) {
      float title_h =
          (panel->flags & AP_GUI_WINDOW_NO_TITLE) != 0 ? 0.0f
                                                       : g_style.title_height;
      float menu_h =
          (panel->flags & AP_GUI_WINDOW_MENU_BAR) != 0 ? g_style.widget_height
                                                       : 0.0f;
      state->rect.h = title_h + menu_h + used + g_style.padding;
      if (state->rect.h < g_style.min_window_h) {
        state->rect.h = g_style.min_window_h;
      }
    }
    if ((panel->flags & AP_GUI_WINDOW_NO_RESIZE) == 0 && panel->visible) {
      AP_FRect grip;
      grip.w = 12.0f;
      grip.h = 12.0f;
      grip.x = state->rect.x + state->rect.w - grip.w - 2.0f;
      grip.y = state->rect.y + state->rect.h - grip.h - 2.0f;
      AP_SetRenderDrawColorFloat(g_style.border.r, g_style.border.g,
                                 g_style.border.b, 0.6f);
      AP_SetRenderLineWidth(1.5f);
      AP_RenderLine(grip.x, grip.y + grip.h, grip.x + grip.w, grip.y);
    }
    if (panel->is_popup) {
      g_popup_rect = state->rect;
    }
  }

  if (panel->clip) {
    AP_GuiRestoreClip(&panel->prev_clip);
  }

  g_panel_count -= 1;
  g_same_line = false;
  g_indent = 0.0f;
  g_in_menu_bar = false;
  g_tab_active = false;

  if (g_panel_count == 0) {
    AP_GuiDrawTooltip();
  }
}

void AP_GuiEndWindow(void) { AP_GuiEndPanel(); }

void AP_GuiPushId(const char *id) {
  if (g_id_depth < AP_GUI_ID_STACK) {
    g_id_stack[g_id_depth] = AP_GuiId(id);
    g_id_depth += 1;
  }
}

void AP_GuiPushIdInt(int id) {
  char buffer[32];
  snprintf(buffer, sizeof(buffer), "%d", id);
  AP_GuiPushId(buffer);
}

void AP_GuiPopId(void) {
  if (g_id_depth > 0) {
    g_id_depth -= 1;
  }
}

void AP_GuiPushItemWidth(float width) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  if (g_width_depth < AP_GUI_WIDTH_STACK) {
    g_width_stack[g_width_depth++] = width;
  }
  if (panel != NULL) {
    panel->item_width = width;
  }
}

void AP_GuiPopItemWidth(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  if (g_width_depth > 0) {
    g_width_depth -= 1;
  }
  if (panel != NULL) {
    panel->item_width =
        g_width_depth > 0 ? g_width_stack[g_width_depth - 1] : 0.0f;
  }
}

void AP_GuiBeginDisabled(bool disabled) {
  if (g_disabled_depth < 8) {
    g_disabled_stack[g_disabled_depth++] = disabled;
  }
  if (disabled) {
    g_disabled_count += 1;
  }
}

void AP_GuiEndDisabled(void) {
  bool was_disabled = false;
  if (g_disabled_depth > 0) {
    g_disabled_depth -= 1;
    was_disabled = g_disabled_stack[g_disabled_depth];
  }
  if (was_disabled && g_disabled_count > 0) {
    g_disabled_count -= 1;
  }
}

void AP_GuiSameLine(void) {
  g_same_line = true;
  g_same_spacing = -1.0f;
  g_same_offset_y = 0.0f;
}

void AP_GuiSameLineEx(float spacing, float offset_y) {
  g_same_line = true;
  g_same_spacing = spacing;
  g_same_offset_y = offset_y;
}

void AP_GuiDummy(float width, float height) {
  if (!AP_GuiSkip()) {
    (void)AP_GuiNextRect(height, width);
  }
}

void AP_GuiSpacing(void) {
  AP_GuiEnsureStyle();
  AP_GuiDummy(0.0f, g_style.spacing);
}

void AP_GuiSeparator(void) {
  AP_FRect rect;
  if (AP_GuiSkip()) {
    return;
  }
  rect = AP_GuiNextRect(1.0f, 0.0f);
  AP_SetRenderDrawColorFloat(g_style.separator.r, g_style.separator.g,
                             g_style.separator.b, g_style.separator.a);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderLine(rect.x, rect.y, rect.x + rect.w, rect.y);
}

void AP_GuiSeparatorText(const char *text) {
  AP_FRect rect;
  AP_FPoint size;
  char visible[AP_GUI_LABEL_MAX];

  if (AP_GuiSkip()) {
    return;
  }

  AP_GuiEnsureStyle();
  AP_GuiVisibleLabel(text, visible, sizeof(visible));
  size = AP_MeasureTextEx(AP_GuiFont(), visible, AP_GuiFontSize());
  rect = AP_GuiNextRect(AP_GuiMaxf(size.y, g_style.widget_height * 0.7f), 0.0f);
  AP_SetRenderDrawColorFloat(g_style.separator.r, g_style.separator.g,
                             g_style.separator.b, g_style.separator.a);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderLine(rect.x, rect.y + rect.h * 0.5f, rect.x + 8.0f,
                rect.y + rect.h * 0.5f);
  rect.x += 14.0f;
  AP_RenderTextAligned(AP_GuiFont(), &rect, visible, g_style.text_disabled,
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
}

void AP_GuiIndent(float width) {
  AP_GuiEnsureStyle();
  if (width == 0.0f) {
    width = g_style.indent_size;
  }
  g_indent += width;
}

void AP_GuiUnindent(float width) {
  AP_GuiEnsureStyle();
  if (width == 0.0f) {
    width = g_style.indent_size;
  }
  g_indent -= width;
  if (g_indent < 0.0f) {
    g_indent = 0.0f;
  }
}

void AP_GuiColumns(int count) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  int i;
  if (panel == NULL) {
    return;
  }
  if (count < 1) {
    count = 1;
  }
  if (count > AP_GUI_MAX_COLUMNS) {
    count = AP_GUI_MAX_COLUMNS;
  }
  panel->column_count = count;
  panel->column_index = 0;
  for (i = 0; i < count; ++i) {
    panel->column_y[i] = panel->cursor_y + panel->line_max_y;
    panel->column_max_y[i] = 0.0f;
  }
  panel->line_x = panel->content.x;
}

void AP_GuiNextColumn(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  float col_w;
  if (panel == NULL || panel->column_count <= 1) {
    return;
  }
  panel->column_index = (panel->column_index + 1) % panel->column_count;
  col_w = panel->content.w / (float)panel->column_count;
  panel->line_x = panel->content.x + col_w * (float)panel->column_index;
  panel->cursor_x = panel->line_x;
  panel->cursor_y = panel->column_y[0];
  panel->line_max_y = 0.0f;
  g_same_line = false;
}

void AP_GuiSetCursor(float x, float y) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  if (panel == NULL) {
    return;
  }
  panel->cursor_x = x;
  panel->cursor_y = y;
  panel->line_x = x;
  panel->line_max_y = 0.0f;
  g_same_line = false;
}

AP_FPoint AP_GuiGetCursor(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FPoint point;
  point.x = panel != NULL ? panel->cursor_x : 0.0f;
  point.y = panel != NULL ? panel->cursor_y + panel->line_max_y : 0.0f;
  return point;
}

AP_FPoint AP_GuiGetContentAvail(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FPoint point;
  point.x = 0.0f;
  point.y = 0.0f;
  if (panel != NULL) {
    AP_FPoint cursor = AP_GuiGetCursor();
    point.x = panel->content.x + panel->content.w - cursor.x;
    point.y = panel->content.y + panel->content.h - cursor.y;
    if (point.x < 0.0f) {
      point.x = 0.0f;
    }
    if (point.y < 0.0f) {
      point.y = 0.0f;
    }
  }
  return point;
}

AP_FRect AP_GuiLayoutRect(float height) {
  AP_GuiEnsureStyle();
  if (height <= 0.0f) {
    height = g_style.widget_height;
  }
  return AP_GuiNextRect(height, 0.0f);
}

bool AP_GuiBeginChild(const char *id, float width, float height, bool border) {
  AP_GuiPanel *parent;
  AP_GuiPanel *child;
  AP_FRect rect;
  AP_FPoint avail;
  AP_GuiKv *kv;
  AP_U32 child_id;

  if (AP_GuiSkip() || g_panel_count >= AP_GUI_MAX_PANELS) {
    return false;
  }

  parent = AP_GuiCurrentPanel();
  avail = AP_GuiGetContentAvail();
  if (width <= 0.0f) {
    width = avail.x;
  }
  if (height <= 0.0f) {
    height = avail.y;
  }
  rect = AP_GuiNextRect(height, width);
  child_id = AP_GuiId(id != NULL ? id : "##child");
  kv = AP_GuiKvFind(child_id, 0, 0.0f);

  if (border) {
    AP_GuiDrawRect(&rect, g_style.widget_bg, true);
    AP_GuiDrawRect(&rect, g_style.border, false);
  }

  if (AP_GuiContains(&rect, (float)AP_GetMouseX(), (float)AP_GetMouseY())) {
    kv->f -= (float)AP_GetMouseWheelY() * g_style.widget_height * 1.5f;
    g_want_mouse = true;
  }

  child = &g_panels[g_panel_count++];
  memset(child, 0, sizeof(*child));
  child->id = child_id;
  child->rect = rect;
  child->is_child = true;
  child->visible = true;
  child->clip = true;
  child->window = parent != NULL ? parent->window : NULL;
  child->flags = AP_GUI_WINDOW_NO_TITLE | AP_GUI_WINDOW_NO_MOVE |
                 AP_GUI_WINDOW_NO_RESIZE | AP_GUI_WINDOW_NO_COLLAPSE;
  child->content.x = rect.x + 4.0f;
  child->content.y = rect.y + 4.0f;
  child->content.w = rect.w - 8.0f - g_style.scrollbar_size;
  child->content.h = rect.h - 8.0f;
  kv->f = AP_GuiClampf(kv->f, 0.0f, AP_GuiMaxf(0.0f, kv->i - child->content.h));
  child->content_start_y = child->content.y - kv->f;
  child->cursor_x = child->content.x;
  child->cursor_y = child->content_start_y;
  child->line_x = child->content.x;
  AP_GuiSaveClip(&child->prev_clip);
  AP_GuiPushClipRect(&rect);
  g_same_line = false;
  return true;
}

void AP_GuiEndChild(void) {
  AP_GuiPanel *child;
  AP_GuiKv *kv;
  float used;

  if (g_panel_count <= 0) {
    return;
  }

  child = AP_GuiCurrentPanel();
  if (child == NULL || !child->is_child) {
    AP_GuiEndPanel();
    return;
  }

  used = (child->cursor_y + child->line_max_y) - child->content_start_y;
  kv = AP_GuiKvFind(child->id, 0, 0.0f);
  kv->i = (int)used;
  if (used > child->content.h + 1.0f) {
    AP_FRect bar;
    AP_FRect grab;
    float overflow = used - child->content.h;
    bar.x = child->rect.x + child->rect.w - g_style.scrollbar_size - 2.0f;
    bar.y = child->content.y;
    bar.w = g_style.scrollbar_size;
    bar.h = child->content.h;
    AP_GuiDrawRect(&bar, g_style.widget_bg, true);
    grab = bar;
    grab.h = AP_GuiClampf(bar.h * (child->content.h / used), 16.0f, bar.h);
    grab.y = bar.y + AP_GuiClampf(kv->f / overflow, 0.0f, 1.0f) *
                         (bar.h - grab.h);
    AP_GuiDrawRect(&grab, g_style.scrollbar, true);
  }

  AP_GuiRestoreClip(&child->prev_clip);
  g_panel_count -= 1;
  g_same_line = false;
}

bool AP_GuiBeginGroup(void) {
  AP_FPoint cursor;
  if (g_group_depth >= AP_GUI_GROUP_STACK || AP_GuiSkip()) {
    return false;
  }
  cursor = AP_GuiGetCursor();
  g_groups[g_group_depth].x = cursor.x;
  g_groups[g_group_depth].y = cursor.y;
  g_groups[g_group_depth].max_x = cursor.x;
  g_groups[g_group_depth].max_y = cursor.y;
  g_group_depth += 1;
  return true;
}

void AP_GuiEndGroup(void) {
  AP_GuiGroup *group;
  AP_FRect rect;
  if (g_group_depth <= 0) {
    return;
  }
  g_group_depth -= 1;
  group = &g_groups[g_group_depth];
  rect.x = group->x;
  rect.y = group->y;
  rect.w = AP_GuiMaxf(8.0f, group->max_x - group->x);
  rect.h = AP_GuiMaxf(8.0f, group->max_y - group->y);
  AP_GuiSetLast(0, &rect, AP_GuiHovered(&rect), false);
}

bool AP_GuiIsItemHovered(void) { return g_last_hovered; }

bool AP_GuiIsItemActive(void) { return g_active == g_last_id && g_last_id != 0; }

bool AP_GuiIsItemClicked(void) { return g_last_clicked; }

bool AP_GuiIsWindowHovered(void) { return g_window_hovered; }

AP_FRect AP_GuiGetItemRect(void) { return g_last_item; }

AP_FRect AP_GuiGetWindowRect(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FRect rect;
  memset(&rect, 0, sizeof(rect));
  if (panel != NULL) {
    rect = panel->rect;
  }
  return rect;
}

void AP_GuiSetTooltip(const char *text) {
  if (!g_last_hovered || text == NULL) {
    return;
  }
  snprintf(g_tooltip, sizeof(g_tooltip), "%s", text);
  g_has_tooltip = true;
}

bool AP_GuiBeginMenuBar(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FRect row;

  if (panel == NULL || AP_GuiSkip()) {
    return false;
  }

  if ((panel->flags & AP_GUI_WINDOW_MENU_BAR) != 0) {
    row = g_menu_bar_rect;
    row.x += g_style.padding;
    row.w -= g_style.padding * 2.0f;
    panel->cursor_x = row.x;
    panel->cursor_y = row.y;
    panel->line_x = row.x;
    panel->line_max_y = row.h;
  } else {
    row = AP_GuiNextRect(g_style.widget_height, 0.0f);
    AP_GuiDrawRect(&row, g_style.menubar_bg, true);
    panel->cursor_x = row.x;
    panel->cursor_y = row.y;
    panel->line_max_y = row.h;
  }

  g_in_menu_bar = true;
  g_menu_bar_items = 0;
  g_same_line = false;
  return true;
}

void AP_GuiEndMenuBar(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  g_in_menu_bar = false;
  g_same_line = false;
  if (panel != NULL) {
    panel->cursor_x = panel->content.x;
    panel->cursor_y = panel->content_start_y;
    panel->line_x = panel->content.x;
    panel->line_max_y = 0.0f;
  }
}

void AP_GuiOpenPopup(const char *id) {
  g_popup_queued = AP_GuiId(id);
  g_popup_owner = g_last_id;
  g_popup_owner_rect = g_last_item;
  g_popup_pos_x = g_last_item.x;
  g_popup_pos_y = g_last_item.y + g_last_item.h;
  g_popup_width = AP_GuiMaxf(180.0f, g_last_item.w);
}

bool AP_GuiIsPopupOpen(const char *id) {
  return g_popup_id != 0 && g_popup_id == AP_GuiId(id);
}

void AP_GuiCloseCurrentPopup(void) {
  g_popup_id = 0;
  g_popup_queued = 0;
  g_menu_bar_open = 0;
}

bool AP_GuiBeginPopup(const char *id) {
  AP_U32 popup_id;
  AP_FRect rect;
  AP_GuiClip clip;
  bool opened;

  popup_id = AP_GuiId(id);
  if (g_popup_queued == popup_id) {
    g_popup_id = popup_id;
    g_popup_queued = 0;
  }

  if (g_popup_id != popup_id) {
    return false;
  }

  AP_GuiSaveClip(&clip);
  AP_SetRenderClipRect(NULL);
  AP_GuiSetNextWindowFlags(AP_GUI_WINDOW_NO_TITLE | AP_GUI_WINDOW_NO_MOVE |
                           AP_GUI_WINDOW_NO_RESIZE | AP_GUI_WINDOW_NO_COLLAPSE |
                           AP_GUI_WINDOW_AUTO_RESIZE | AP_GUI_WINDOW_NO_SCROLL);
  AP_GuiSetNextWindowPos(g_popup_pos_x, g_popup_pos_y);
  AP_GuiSetNextWindowSize(g_popup_width, 32.0f);
  rect.x = g_popup_pos_x;
  rect.y = g_popup_pos_y;
  rect.w = g_popup_width;
  rect.h = 32.0f;
  opened = AP_GuiBeginPanelEx(id, &rect, NULL);
  if (opened) {
    AP_GuiPanel *panel = AP_GuiCurrentPanel();
    if (panel != NULL) {
      panel->is_popup = true;
      panel->prev_clip = clip;
      AP_GuiDrawRect(&panel->rect, g_style.popup_bg, true);
    }
  }
  return opened;
}

bool AP_GuiBeginPopupContextItem(const char *id) {
  if (g_last_hovered && AP_IsMousePressed(AP_MOUSE_RIGHT)) {
    AP_GuiOpenPopup(id);
    g_popup_pos_x = (float)AP_GetMouseX();
    g_popup_pos_y = (float)AP_GetMouseY();
  }
  return AP_GuiBeginPopup(id);
}

bool AP_GuiBeginPopupModal(const char *title, bool *open) {
  int pixel_w = 0;
  int pixel_h = 0;
  AP_FRect dim;
  AP_FRect rect;

  if (open != NULL && !*open) {
    return false;
  }

  AP_GetWindowSizeInPixels(&pixel_w, &pixel_h);
  dim.x = 0.0f;
  dim.y = 0.0f;
  dim.w = (float)pixel_w;
  dim.h = (float)pixel_h;
  AP_SetRenderClipRect(NULL);
  AP_GuiDrawRect(&dim, g_style.dim, true);
  g_want_mouse = true;
  g_want_keyboard = true;

  rect.w = 360.0f;
  rect.h = 180.0f;
  rect.x = ((float)pixel_w - rect.w) * 0.5f;
  rect.y = ((float)pixel_h - rect.h) * 0.5f;
  AP_GuiSetNextWindowFlags(AP_GUI_WINDOW_NO_MOVE | AP_GUI_WINDOW_NO_COLLAPSE |
                           AP_GUI_WINDOW_NO_SCROLL);
  return AP_GuiBeginPanelEx(title, &rect, open);
}

void AP_GuiEndPopup(void) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  float mx = (float)AP_GetMouseX();
  float my = (float)AP_GetMouseY();

  if (panel != NULL && panel->is_popup && panel->window != NULL) {
    g_popup_rect = panel->window->rect;
    if (AP_IsMousePressed(AP_MOUSE_LEFT) &&
        !AP_GuiContains(&g_popup_rect, mx, my) &&
        !AP_GuiContains(&g_popup_owner_rect, mx, my)) {
      g_popup_id = 0;
      g_menu_bar_open = 0;
    }
  }

  AP_GuiEndPanel();
}

bool AP_GuiBeginMenu(const char *label) {
  AP_U32 id;
  AP_U32 popup_id;
  AP_FRect rect;
  char visible[AP_GUI_LABEL_MAX];
  char popup_name[AP_GUI_LABEL_MAX + 16];
  bool hovered;
  bool clicked;
  float width;
  bool open;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  AP_GuiVisibleLabel(label, visible, sizeof(visible));
  id = AP_GuiId(label);
  snprintf(popup_name, sizeof(popup_name), "%s##menu_popup",
           label != NULL ? label : "menu");
  popup_id = AP_GuiId(popup_name);
  width = AP_GuiTextWidth(label) + g_style.padding * 2.0f;
  if (g_in_menu_bar) {
    if (g_menu_bar_items > 0) {
      AP_GuiSameLine();
    }
    rect = AP_GuiNextRect(g_style.widget_height - 2.0f, width);
    g_menu_bar_items += 1;
  } else {
    rect = AP_GuiNextRect(g_style.widget_height, 0.0f);
  }

  hovered = AP_GuiHovered(&rect);
  clicked = AP_GuiClicked(id, &rect);
  open = g_popup_id == popup_id;

  if (g_in_menu_bar) {
    if (clicked) {
      if (open) {
        AP_GuiCloseCurrentPopup();
        open = false;
      } else {
        g_menu_bar_open = id;
        AP_GuiSetLast(id, &rect, hovered, clicked);
        AP_GuiOpenPopup(popup_name);
        g_popup_owner = id;
        g_popup_owner_rect = rect;
      }
    } else if (hovered && g_menu_bar_open != 0 && g_menu_bar_open != id) {
      g_menu_bar_open = id;
      AP_GuiSetLast(id, &rect, hovered, clicked);
      AP_GuiOpenPopup(popup_name);
      g_popup_owner = id;
      g_popup_owner_rect = rect;
    }
  } else if (clicked) {
    AP_GuiSetLast(id, &rect, hovered, clicked);
    AP_GuiOpenPopup(popup_name);
    g_popup_owner = id;
    g_popup_owner_rect = rect;
  }

  AP_GuiDrawRect(&rect, hovered || open ? g_style.widget_hovered : g_style.menubar_bg,
                 true);
  AP_RenderTextAligned(AP_GuiFont(), &rect, visible, AP_GuiTextCol(),
                       AP_GuiFontSize(),
                       g_in_menu_bar ? AP_TEXT_ALIGN_CENTER : AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &rect, hovered, clicked);

  if (g_popup_queued == popup_id || g_popup_id == popup_id) {
    g_popup_pos_x = rect.x;
    g_popup_pos_y = rect.y + rect.h;
    g_popup_width = AP_GuiMaxf(180.0f, rect.w);
    return AP_GuiBeginPopup(popup_name);
  }

  return false;
}

void AP_GuiEndMenu(void) { AP_GuiEndPopup(); }

bool AP_GuiMenuItem(const char *label, const char *shortcut) {
  AP_U32 id;
  AP_FRect rect;
  AP_FRect shortcut_rect;
  char visible[AP_GUI_LABEL_MAX];
  bool hovered;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  rect = AP_GuiNextRect(g_style.widget_height - 2.0f, 0.0f);
  hovered = AP_GuiHovered(&rect);
  clicked = AP_GuiClicked(id, &rect);
  AP_GuiDrawRect(&rect, hovered ? g_style.widget_hovered : g_style.popup_bg, true);
  AP_GuiVisibleLabel(label, visible, sizeof(visible));
  AP_RenderTextAligned(AP_GuiFont(), &rect, visible, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  if (shortcut != NULL && shortcut[0] != '\0') {
    shortcut_rect = rect;
    shortcut_rect.x += 8.0f;
    shortcut_rect.w -= 16.0f;
    AP_RenderTextAligned(AP_GuiFont(), &shortcut_rect, shortcut,
                         g_style.text_disabled, AP_GuiFontSize(),
                         AP_TEXT_ALIGN_RIGHT);
  }
  AP_GuiSetLast(id, &rect, hovered, clicked);
  if (clicked) {
    AP_GuiCloseCurrentPopup();
  }
  return clicked;
}

bool AP_GuiBeginTabBar(const char *id) {
  AP_FRect row;
  AP_GuiKv *kv;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  g_tab_active = true;
  g_tab_bar_id = AP_GuiId(id != NULL ? id : "##tabs");
  kv = AP_GuiKvFind(g_tab_bar_id, -1, 0.0f);
  g_tab_selected = kv->i >= 0 ? (AP_U32)kv->i : 0;
  g_tab_x = row.x;
  g_tab_y = row.y;
  g_tab_h = row.h;
  AP_SetRenderDrawColorFloat(g_style.separator.r, g_style.separator.g,
                             g_style.separator.b, g_style.separator.a);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderLine(row.x, row.y + row.h, row.x + row.w, row.y + row.h);
  return true;
}

bool AP_GuiTab(const char *label) {
  AP_U32 id;
  AP_FRect rect;
  char visible[AP_GUI_LABEL_MAX];
  float width;
  bool selected;
  bool hovered;
  AP_GuiKv *kv;

  if (!g_tab_active || AP_GuiSkip()) {
    return false;
  }

  AP_GuiVisibleLabel(label, visible, sizeof(visible));
  id = AP_GuiMix(g_tab_bar_id, AP_GuiHash(label));
  width = AP_GuiTextWidth(label) + g_style.padding * 2.0f;
  rect.x = g_tab_x;
  rect.y = g_tab_y;
  rect.w = width;
  rect.h = g_tab_h;
  g_tab_x += width + 4.0f;

  kv = AP_GuiKvFind(g_tab_bar_id, -1, 0.0f);
  if (kv->i < 0) {
    kv->i = (int)id;
    g_tab_selected = id;
  }

  selected = (AP_U32)kv->i == id;
  hovered = AP_GuiHovered(&rect);
  if (hovered && AP_IsMousePressed(AP_MOUSE_LEFT) && AP_GuiInteractable(id)) {
    kv->i = (int)id;
    selected = true;
    g_want_mouse = true;
  }

  AP_GuiDrawRect(&rect, selected ? g_style.widget_active
                                 : (hovered ? g_style.header_hovered : g_style.header),
                 true);
  AP_RenderTextAligned(AP_GuiFont(), &rect, visible, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
  AP_GuiSetLast(id, &rect, hovered, false);
  return selected;
}

void AP_GuiEndTabBar(void) { g_tab_active = false; }

bool AP_GuiCollapsingHeader(const char *label, bool default_open) {
  AP_U32 id;
  AP_FRect rect;
  AP_GuiKv *kv;
  char visible[AP_GUI_LABEL_MAX];
  bool hovered;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  kv = AP_GuiKvFind(id, default_open ? 1 : 0, 0.0f);
  rect = AP_GuiNextRect(g_style.widget_height, 0.0f);
  hovered = AP_GuiHovered(&rect);
  clicked = AP_GuiClicked(id, &rect);
  if (clicked) {
    kv->i = kv->i ? 0 : 1;
  }

  AP_GuiDrawRect(&rect, hovered ? g_style.header_hovered : g_style.header, true);
  AP_GuiVisibleLabel(label, visible, sizeof(visible));
  {
    char caption[AP_GUI_LABEL_MAX + 4];
    snprintf(caption, sizeof(caption), "%s %s", kv->i ? "v" : ">", visible);
    AP_RenderTextAligned(AP_GuiFont(), &rect, caption, AP_GuiTextCol(),
                         AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  }
  AP_GuiSetLast(id, &rect, hovered, clicked);
  return kv->i != 0;
}

bool AP_GuiTreeNode(const char *label) {
  bool open = AP_GuiCollapsingHeader(label, false);
  if (open) {
    AP_GuiIndent(0.0f);
  }
  return open;
}

void AP_GuiTreePop(void) { AP_GuiUnindent(0.0f); }

void AP_GuiLabel(const char *text) {
  AP_FPoint size;
  AP_FRect rect;
  char visible[AP_GUI_LABEL_MAX];

  if (AP_GuiSkip()) {
    return;
  }

  AP_GuiEnsureStyle();
  AP_GuiVisibleLabel(text, visible, sizeof(visible));
  size = AP_MeasureTextEx(AP_GuiFont(), visible, AP_GuiFontSize());
  rect = AP_GuiNextRect(size.y > g_style.widget_height * 0.6f
                            ? size.y
                            : g_style.widget_height * 0.75f,
                        0.0f);
  AP_RenderTextAligned(AP_GuiFont(), &rect, visible, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(0, &rect, AP_GuiHovered(&rect), false);
}

void AP_GuiLabelF(const char *format, ...) {
  char buffer[256];
  va_list args;

  if (format == NULL) {
    return;
  }

  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  AP_GuiLabel(buffer);
}

bool AP_GuiButtonEx(const char *label, float width) {
  AP_U32 id;
  AP_FRect rect;
  char visible[AP_GUI_LABEL_MAX];
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  rect = AP_GuiNextRect(g_style.widget_height, width);
  clicked = AP_GuiClicked(id, &rect);
  AP_GuiDrawRect(&rect, AP_GuiWidgetColor(id, &rect), true);
  AP_RenderTextAligned(AP_GuiFont(), &rect,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
  AP_GuiSetLast(id, &rect, AP_GuiHovered(&rect), clicked);
  return clicked;
}

bool AP_GuiButton(const char *label) { return AP_GuiButtonEx(label, 0.0f); }

bool AP_GuiInvisibleButton(const char *id, float width, float height) {
  AP_U32 widget;
  AP_FRect rect;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  widget = AP_GuiId(id);
  rect = AP_GuiNextRect(height, width);
  clicked = AP_GuiClicked(widget, &rect);
  AP_GuiSetLast(widget, &rect, AP_GuiHovered(&rect), clicked);
  return clicked;
}

bool AP_GuiCheckbox(const char *label, bool *checked) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect box;
  char visible[AP_GUI_LABEL_MAX];
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  box = row;
  box.w = g_style.widget_height;
  clicked = AP_GuiClicked(id, &row);
  if (clicked && checked != NULL) {
    *checked = !*checked;
  }

  AP_GuiDrawRect(&box, AP_GuiWidgetColor(id, &box), true);
  if (checked != NULL && *checked) {
    AP_FRect mark = box;
    mark.x += 7.0f;
    mark.y += 7.0f;
    mark.w -= 14.0f;
    mark.h -= 14.0f;
    AP_GuiDrawRect(&mark, g_style.check_mark, true);
  }

  row.x += g_style.widget_height + g_style.spacing;
  row.w -= g_style.widget_height + g_style.spacing;
  AP_RenderTextAligned(AP_GuiFont(), &row,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &box, AP_GuiHovered(&box), clicked);
  return clicked;
}

bool AP_GuiRadio(const char *label, int *value, int item) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect box;
  char visible[AP_GUI_LABEL_MAX];
  bool selected;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  box = row;
  box.w = g_style.widget_height;
  selected = value != NULL && *value == item;
  clicked = AP_GuiClicked(id, &row);
  if (clicked && value != NULL) {
    *value = item;
    selected = true;
  }

  AP_SetRenderDrawColorFloat(g_style.accent.r, g_style.accent.g, g_style.accent.b,
                             g_style.accent.a);
  AP_RenderFillCircle(box.x + box.w * 0.5f, box.y + box.h * 0.5f, box.w * 0.32f);
  AP_SetRenderDrawColorFloat(g_style.widget_bg.r, g_style.widget_bg.g,
                             g_style.widget_bg.b, g_style.widget_bg.a);
  AP_RenderFillCircle(box.x + box.w * 0.5f, box.y + box.h * 0.5f, box.w * 0.26f);
  if (selected) {
    AP_SetRenderDrawColorFloat(g_style.accent.r, g_style.accent.g,
                               g_style.accent.b, g_style.accent.a);
    AP_RenderFillCircle(box.x + box.w * 0.5f, box.y + box.h * 0.5f,
                        box.w * 0.14f);
  }

  row.x += g_style.widget_height + g_style.spacing;
  row.w -= g_style.widget_height + g_style.spacing;
  AP_RenderTextAligned(AP_GuiFont(), &row,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &box, AP_GuiHovered(&box), clicked);
  return clicked;
}

bool AP_GuiSelectable(const char *label, bool selected) {
  AP_U32 id;
  AP_FRect rect;
  char visible[AP_GUI_LABEL_MAX];
  bool hovered;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  rect = AP_GuiNextRect(g_style.widget_height - 2.0f, 0.0f);
  hovered = AP_GuiHovered(&rect);
  clicked = AP_GuiClicked(id, &rect);
  AP_GuiDrawRect(&rect,
                 selected ? g_style.widget_active
                          : (hovered ? g_style.widget_hovered : g_style.widget_bg),
                 true);
  AP_RenderTextAligned(AP_GuiFont(), &rect,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &rect, hovered, clicked);
  return clicked;
}

bool AP_GuiSliderF(const char *label, float *value, float min_value,
                   float max_value) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect track;
  AP_FRect grab;
  AP_FRect text;
  char visible[AP_GUI_LABEL_MAX];
  char value_text[32];
  float mx;
  float t = 0.0f;
  bool changed = false;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  if (value == NULL || max_value <= min_value) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid slider");
    return false;
  }

  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  mx = (float)AP_GetMouseX();
  text = row;
  text.w = row.w * 0.38f;
  AP_RenderTextAligned(AP_GuiFont(), &text,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);

  track = row;
  track.x += text.w + g_style.spacing;
  track.w = row.w - text.w - g_style.spacing;
  track.y += (row.h - 6.0f) * 0.5f;
  track.h = 6.0f;

  if (AP_GuiHovered(&row) && AP_GuiInteractable(id)) {
    g_hot = id;
    g_want_mouse = true;
    if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
      g_active = id;
    }
  }

  if (g_active == id) {
    if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
      t = AP_GuiClampf((mx - track.x) / track.w, 0.0f, 1.0f);
      *value = min_value + t * (max_value - min_value);
      changed = true;
      g_want_mouse = true;
    } else {
      g_active = 0;
    }
  }

  t = AP_GuiClampf((*value - min_value) / (max_value - min_value), 0.0f, 1.0f);
  AP_GuiDrawRect(&track, g_style.widget_bg, true);
  grab = track;
  grab.w = g_style.grab_width;
  grab.h = g_style.widget_height * 0.7f;
  grab.y = row.y + (row.h - grab.h) * 0.5f;
  grab.x = track.x + t * (track.w - grab.w);
  AP_GuiDrawRect(&grab, g_active == id ? g_style.widget_active : g_style.slider_grab,
                 true);
  snprintf(value_text, sizeof(value_text), "%.2f", *value);
  AP_RenderTextAligned(AP_GuiFont(), &track, value_text, g_style.text_disabled,
                       AP_GuiFontSize() * 0.85f, AP_TEXT_ALIGN_RIGHT);
  AP_GuiSetLast(id, &row, AP_GuiHovered(&row), false);
  return changed;
}

bool AP_GuiSliderI(const char *label, int *value, int min_value, int max_value) {
  float as_float;
  bool changed;

  if (value == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Slider value cannot be NULL");
    return false;
  }

  as_float = (float)*value;
  changed = AP_GuiSliderF(label, &as_float, (float)min_value, (float)max_value);
  *value = (int)(as_float + (as_float >= 0.0f ? 0.5f : -0.5f));
  if (*value < min_value) {
    *value = min_value;
  }
  if (*value > max_value) {
    *value = max_value;
  }
  return changed;
}

bool AP_GuiDragF(const char *label, float *value, float speed, float min_value,
                 float max_value) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect field;
  AP_FRect text;
  char visible[AP_GUI_LABEL_MAX];
  char value_text[32];
  bool hovered;
  bool changed = false;

  if (AP_GuiSkip() || value == NULL) {
    return false;
  }

  AP_GuiEnsureStyle();
  if (speed <= 0.0f) {
    speed = 0.1f;
  }

  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  text = row;
  text.w = row.w * 0.38f;
  field = row;
  field.x += text.w + g_style.spacing;
  field.w = row.w - text.w - g_style.spacing;
  hovered = AP_GuiHovered(&field);

  if (hovered && AP_GuiInteractable(id) && AP_IsMousePressed(AP_MOUSE_LEFT)) {
    g_active = id;
  }

  if (g_active == id) {
    if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
      float delta = (float)AP_GetMouseDeltaX() * speed;
      if (AP_IsShiftDown()) {
        delta *= 0.1f;
      }
      *value += delta;
      if (max_value > min_value) {
        *value = AP_GuiClampf(*value, min_value, max_value);
      }
      changed = delta != 0.0f;
      g_want_mouse = true;
    } else {
      g_active = 0;
    }
  }

  AP_RenderTextAligned(AP_GuiFont(), &text,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiDrawRect(&field, AP_GuiWidgetColor(id, &field), true);
  snprintf(value_text, sizeof(value_text), "%.3f", *value);
  AP_RenderTextAligned(AP_GuiFont(), &field, value_text, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
  AP_GuiSetLast(id, &row, hovered, false);
  return changed;
}

bool AP_GuiDragI(const char *label, int *value, float speed, int min_value,
                 int max_value) {
  float as_float;
  bool changed;

  if (value == NULL) {
    return false;
  }

  as_float = (float)*value;
  changed = AP_GuiDragF(label, &as_float, speed, (float)min_value,
                        (float)max_value);
  *value = (int)(as_float + (as_float >= 0.0f ? 0.5f : -0.5f));
  if (*value < min_value) {
    *value = min_value;
  }
  if (*value > max_value) {
    *value = max_value;
  }
  return changed;
}

bool AP_GuiToggle(const char *label, bool *on) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect track;
  AP_FRect knob;
  char visible[AP_GUI_LABEL_MAX];
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  clicked = AP_GuiClicked(id, &row);
  if (clicked && on != NULL) {
    *on = !*on;
  }

  track = row;
  track.w = 48.0f;
  track.y += 4.0f;
  track.h -= 8.0f;
  AP_GuiDrawRect(&track, (on != NULL && *on) ? g_style.accent : g_style.widget_bg,
                 true);
  knob = track;
  knob.w = track.h;
  if (on != NULL && *on) {
    knob.x = track.x + track.w - knob.w;
  }
  AP_GuiDrawRect(&knob, g_style.check_mark, true);
  row.x += 48.0f + g_style.spacing;
  row.w -= 48.0f + g_style.spacing;
  AP_RenderTextAligned(AP_GuiFont(), &row,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &track, AP_GuiHovered(&track), clicked);
  return clicked;
}

bool AP_GuiProgress(float fraction) {
  AP_FRect row;
  AP_FRect fill;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  fraction = AP_GuiClampf(fraction, 0.0f, 1.0f);
  row = AP_GuiNextRect(g_style.widget_height * 0.45f, 0.0f);
  AP_GuiDrawRect(&row, g_style.widget_bg, true);
  fill = row;
  fill.w *= fraction;
  if (fill.w > 0.0f) {
    AP_GuiDrawRect(&fill, g_style.accent, true);
  }
  AP_GuiSetLast(0, &row, AP_GuiHovered(&row), false);
  return true;
}

bool AP_GuiInputTextEx(const char *label, char *buffer, int capacity,
                       AP_U32 flags) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect field;
  AP_FRect text;
  AP_FRect inner;
  AP_GuiKv *kv;
  const char *typed;
  char visible[AP_GUI_LABEL_MAX];
  char display[AP_GUI_LABEL_MAX];
  int length;
  int i;
  bool hovered;
  bool active;
  bool readonly;
  bool changed = false;

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiEnsureStyle();
  if (buffer == NULL || capacity <= 1) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Input buffer is invalid");
    return false;
  }

  id = AP_GuiId(label);
  kv = AP_GuiKvFind(id, (int)strlen(buffer), 0.0f);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  text = row;
  text.w = row.w * 0.38f;
  field = row;
  field.x += text.w + g_style.spacing;
  field.w = row.w - text.w - g_style.spacing;
  inner = field;
  inner.x += 8.0f;
  inner.w -= 16.0f;
  hovered = AP_GuiHovered(&field);
  readonly = (flags & AP_GUI_INPUT_READ_ONLY) != 0;

  if (hovered && AP_GuiInteractable(id) && AP_IsMousePressed(AP_MOUSE_LEFT)) {
    g_focus = id;
    kv->i = (int)strlen(buffer);
  } else if (AP_IsMousePressed(AP_MOUSE_LEFT) && g_focus == id && !hovered) {
    g_focus = 0;
  }

  active = g_focus == id && !readonly && AP_GuiInteractable(id);
  length = (int)strlen(buffer);
  if (kv->i < 0) {
    kv->i = 0;
  }
  if (kv->i > length) {
    kv->i = length;
  }

  if (active) {
    g_want_keyboard = true;
    typed = AP_GetText();
    if (typed != NULL) {
      while (*typed != '\0' && length < capacity - 1) {
        if ((unsigned char)*typed >= 32) {
          memmove(buffer + kv->i + 1, buffer + kv->i,
                  (size_t)(length - kv->i + 1));
          buffer[kv->i] = *typed;
          kv->i += 1;
          length += 1;
          changed = true;
        }
        typed += 1;
      }
    }

    if ((AP_IsKeyPressed(AP_KEY_BACKSPACE) || AP_IsKeyRepeat(AP_KEY_BACKSPACE)) &&
        kv->i > 0) {
      memmove(buffer + kv->i - 1, buffer + kv->i, (size_t)(length - kv->i + 1));
      kv->i -= 1;
      changed = true;
    }

    if ((AP_IsKeyPressed(AP_KEY_DELETE) || AP_IsKeyRepeat(AP_KEY_DELETE)) &&
        kv->i < length) {
      memmove(buffer + kv->i, buffer + kv->i + 1, (size_t)(length - kv->i));
      changed = true;
    }

    if (AP_IsKeyPressed(AP_KEY_LEFT) || AP_IsKeyRepeat(AP_KEY_LEFT)) {
      if (kv->i > 0) {
        kv->i -= 1;
      }
    }
    if (AP_IsKeyPressed(AP_KEY_RIGHT) || AP_IsKeyRepeat(AP_KEY_RIGHT)) {
      if (kv->i < (int)strlen(buffer)) {
        kv->i += 1;
      }
    }
    if (AP_IsKeyPressed(AP_KEY_HOME)) {
      kv->i = 0;
    }
    if (AP_IsKeyPressed(AP_KEY_END)) {
      kv->i = (int)strlen(buffer);
    }

    if (AP_IsKeyPressed(AP_KEY_ENTER)) {
      if ((flags & AP_GUI_INPUT_ENTER_RETURNS) != 0) {
        g_focus = 0;
        changed = true;
      } else {
        g_focus = 0;
      }
    }
    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      g_focus = 0;
    }

    if (AP_IsCtrlDown() && AP_IsKeyPressed(AP_KEY_V)) {
      const char *clip = AP_GetClipboardText();
      if (clip != NULL) {
        while (*clip != '\0' && (int)strlen(buffer) < capacity - 1) {
          int len = (int)strlen(buffer);
          memmove(buffer + kv->i + 1, buffer + kv->i, (size_t)(len - kv->i + 1));
          buffer[kv->i] = *clip;
          kv->i += 1;
          clip += 1;
          changed = true;
        }
      }
    }

    if (AP_IsCtrlDown() && AP_IsKeyPressed(AP_KEY_C)) {
      AP_SetClipboardText(buffer);
    }
  }

  if ((flags & AP_GUI_INPUT_PASSWORD) != 0) {
    length = (int)strlen(buffer);
    if (length >= (int)sizeof(display)) {
      length = (int)sizeof(display) - 1;
    }
    for (i = 0; i < length; ++i) {
      display[i] = '*';
    }
    display[length] = '\0';
  } else {
    snprintf(display, sizeof(display), "%s", buffer);
  }

  AP_RenderTextAligned(AP_GuiFont(), &text,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiDrawRect(&field, active ? g_style.widget_hovered : g_style.widget_bg, true);
  AP_RenderTextAligned(AP_GuiFont(), &inner, display, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);

  if (active && ((int)(AP_GetTime() * 2.0) & 1) == 0) {
    char caret_prefix[AP_GUI_LABEL_MAX];
    AP_FPoint prefix;
    float caret_x;
    int caret = kv->i;
    if (caret > (int)sizeof(caret_prefix) - 1) {
      caret = (int)sizeof(caret_prefix) - 1;
    }
    memcpy(caret_prefix, display, (size_t)caret);
    caret_prefix[caret] = '\0';
    prefix = AP_MeasureTextEx(AP_GuiFont(), caret_prefix, AP_GuiFontSize());
    caret_x = inner.x + prefix.x;
    AP_SetRenderDrawColorFloat(AP_GuiTextCol().r, AP_GuiTextCol().g,
                               AP_GuiTextCol().b, 1.0f);
    AP_SetRenderLineWidth(1.0f);
    AP_RenderLine(caret_x, inner.y + 4.0f, caret_x, inner.y + inner.h - 4.0f);
  }

  AP_GuiSetLast(id, &field, hovered, false);
  return (flags & AP_GUI_INPUT_ENTER_RETURNS) != 0 ? (active && changed)
                                                   : active;
}

bool AP_GuiInputText(const char *label, char *buffer, int capacity) {
  return AP_GuiInputTextEx(label, buffer, capacity, AP_GUI_INPUT_NONE);
}

bool AP_GuiColorEdit(const char *label, AP_FColor *color) {
  AP_FRect preview;
  bool changed = false;

  if (color == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Color cannot be NULL");
    return false;
  }

  if (AP_GuiSkip()) {
    return false;
  }

  AP_GuiLabel(label);
  preview = AP_GuiNextRect(g_style.widget_height, 48.0f);
  AP_GuiDrawRect(&preview, AP_GuiColor(color->r, color->g, color->b, 1.0f), true);
  changed |= AP_GuiSliderF("R", &color->r, 0.0f, 1.0f);
  changed |= AP_GuiSliderF("G", &color->g, 0.0f, 1.0f);
  changed |= AP_GuiSliderF("B", &color->b, 0.0f, 1.0f);
  return changed;
}

bool AP_GuiCombo(const char *label, int *current, const char **items, int count) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect field;
  AP_FRect text;
  char visible[AP_GUI_LABEL_MAX];
  const char *preview;
  bool hovered;
  bool clicked;
  bool changed = false;
  int index;

  if (AP_GuiSkip() || current == NULL || items == NULL || count <= 0) {
    return false;
  }

  AP_GuiEnsureStyle();
  if (*current < 0) {
    *current = 0;
  }
  if (*current >= count) {
    *current = count - 1;
  }

  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  text = row;
  text.w = row.w * 0.38f;
  field = row;
  field.x += text.w + g_style.spacing;
  field.w = row.w - text.w - g_style.spacing;
  hovered = AP_GuiHovered(&field);
  clicked = AP_GuiClicked(id, &field);
  preview = items[*current] != NULL ? items[*current] : "";

  AP_RenderTextAligned(AP_GuiFont(), &text,
                       AP_GuiVisibleLabel(label, visible, sizeof(visible)),
                       AP_GuiTextCol(), AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiDrawRect(&field, AP_GuiWidgetColor(id, &field), true);
  AP_RenderTextAligned(AP_GuiFont(), &field, preview, AP_GuiTextCol(),
                       AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  AP_GuiSetLast(id, &field, hovered, clicked);

  {
    char popup_name[AP_GUI_LABEL_MAX + 16];
    AP_U32 popup_id;
    snprintf(popup_name, sizeof(popup_name), "%s##combo_popup",
             label != NULL ? label : "combo");
    popup_id = AP_GuiId(popup_name);

    if (clicked) {
      if (g_popup_id == popup_id) {
        AP_GuiCloseCurrentPopup();
      } else {
        AP_GuiOpenPopup(popup_name);
        g_popup_owner = id;
        g_popup_owner_rect = field;
        g_popup_pos_x = field.x;
        g_popup_pos_y = field.y + field.h;
        g_popup_width = field.w;
      }
    }

    if (g_popup_id == popup_id || g_popup_queued == popup_id) {
      g_popup_pos_x = field.x;
      g_popup_pos_y = field.y + field.h;
      g_popup_width = field.w;
      if (AP_GuiBeginPopup(popup_name)) {
        for (index = 0; index < count; ++index) {
          AP_GuiPushIdInt(index);
          if (AP_GuiSelectable(items[index] != NULL ? items[index] : "",
                               index == *current)) {
            *current = index;
            changed = true;
            AP_GuiCloseCurrentPopup();
          }
          AP_GuiPopId();
        }
        AP_GuiEndPopup();
      }
    }
  }

  return changed;
}

bool AP_GuiListBox(const char *label, int *current, const char **items,
                   int count, int visible_rows) {
  int index;
  bool changed = false;
  float height;

  if (AP_GuiSkip() || current == NULL || items == NULL) {
    return false;
  }

  AP_GuiEnsureStyle();
  if (visible_rows < 1) {
    visible_rows = 4;
  }
  AP_GuiLabel(label);
  height = (g_style.widget_height - 2.0f) * (float)visible_rows + 8.0f;
  if (AP_GuiBeginChild("##list", 0.0f, height, true)) {
    for (index = 0; index < count; ++index) {
      AP_GuiPushIdInt(index);
      if (AP_GuiSelectable(items[index] != NULL ? items[index] : "",
                           index == *current)) {
        *current = index;
        changed = true;
      }
      AP_GuiPopId();
    }
    AP_GuiEndChild();
  }
  return changed;
}

void AP_GuiImage(AP_Texture *texture, float width, float height) {
  AP_FRect rect;
  if (AP_GuiSkip() || texture == NULL) {
    return;
  }
  rect = AP_GuiNextRect(height, width);
  AP_RenderTexture(texture, NULL, &rect);
  AP_GuiSetLast(0, &rect, AP_GuiHovered(&rect), false);
}

bool AP_GuiImageButton(AP_Texture *texture, float width, float height) {
  AP_U32 id;
  AP_FRect rect;
  bool clicked;

  if (AP_GuiSkip()) {
    return false;
  }

  id = AP_GuiId("##image");
  {
    char image_id[32];
    snprintf(image_id, sizeof(image_id), "##image%d", g_image_button_index);
    g_image_button_index += 1;
    id = AP_GuiId(image_id);
  }
  rect = AP_GuiNextRect(height, width);
  clicked = AP_GuiClicked(id, &rect);
  if (texture != NULL) {
    AP_RenderTexture(texture, NULL, &rect);
  }
  if (AP_GuiHovered(&rect)) {
    AP_GuiDrawRect(&rect, AP_GuiColor(1.0f, 1.0f, 1.0f, 0.12f), false);
  }
  AP_GuiSetLast(id, &rect, AP_GuiHovered(&rect), clicked);
  return clicked;
}
