#include "AP2/AP2_Gui.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Input.h"
#include "AP2/AP2_Renderer.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define AP_GUI_MAX_PANELS 8
#define AP_GUI_MAX_WINDOWS 32
#define AP_GUI_INPUT_MAX 256

typedef struct AP_GuiWindowState {
  AP_U32 id;
  AP_FRect rect;
  bool used;
  bool dragging;
  float drag_dx;
  float drag_dy;
} AP_GuiWindowState;

typedef struct AP_GuiPanel {
  AP_U32 id;
  AP_FRect rect;
  AP_FRect content;
  float cursor_x;
  float cursor_y;
  float line_x;
  float line_max_y;
  bool visible;
} AP_GuiPanel;

static AP_GuiStyle g_style;
static bool g_style_ready = false;
static AP_Font *g_gui_font = NULL;
static AP_GuiPanel g_panels[AP_GUI_MAX_PANELS];
static int g_panel_count = 0;
static AP_GuiWindowState g_windows[AP_GUI_MAX_WINDOWS];
static bool g_same_line = false;
static float g_indent = 0.0f;
static bool g_has_next_pos = false;
static bool g_has_next_size = false;
static float g_next_x = 0.0f;
static float g_next_y = 0.0f;
static float g_next_w = 0.0f;
static float g_next_h = 0.0f;
static AP_U32 g_hot = 0;
static AP_U32 g_active = 0;
static AP_U32 g_focus = 0;
static bool g_want_mouse = false;
static bool g_want_keyboard = false;
static int g_widget_index = 0;

static AP_FColor AP_GuiColor(float r, float g, float b, float a) {
  AP_FColor color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = a;
  return color;
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

static AP_U32 AP_GuiId(const char *label) {
  AP_U32 hash = AP_GuiHash(label);
  hash ^= (AP_U32)g_widget_index * 2654435761u;
  if (g_panel_count > 0) {
    hash ^= g_panels[g_panel_count - 1].id;
  }
  g_widget_index += 1;
  return hash;
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

static void AP_GuiDrawRect(const AP_FRect *rect, AP_FColor color, bool fill) {
  AP_SetRenderDrawColorFloat(color.r, color.g, color.b, color.a);
  if (fill) {
    AP_RenderFillRoundedRect(rect, g_style.rounding);
  } else {
    AP_SetRenderLineWidth(g_style.border_width > 0.0f ? g_style.border_width
                                                      : 1.0f);
    AP_RenderRoundedRect(rect, g_style.rounding);
  }
}

static bool AP_GuiContains(const AP_FRect *rect, float x, float y) {
  return rect != NULL && x >= rect->x && y >= rect->y &&
         x < rect->x + rect->w && y < rect->y + rect->h;
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

  g_windows[empty].id = id;
  g_windows[empty].used = true;
  g_windows[empty].dragging = false;
  g_windows[empty].rect = *fallback;
  return &g_windows[empty];
}

static AP_GuiPanel *AP_GuiCurrentPanel(void) {
  if (g_panel_count <= 0) {
    return NULL;
  }
  return &g_panels[g_panel_count - 1];
}

static AP_FRect AP_GuiNextRect(float height, float width) {
  AP_GuiPanel *panel = AP_GuiCurrentPanel();
  AP_FRect rect;
  float spacing;

  AP_GuiEnsureStyle();
  spacing = g_style.spacing;

  if (panel == NULL) {
    rect.x = 0.0f;
    rect.y = 0.0f;
    rect.w = width;
    rect.h = height;
    return rect;
  }

  if (g_same_line) {
    rect.x = panel->cursor_x + spacing;
    rect.y = panel->cursor_y;
    g_same_line = false;
  } else {
    rect.x = panel->line_x + g_indent;
    rect.y = panel->cursor_y + panel->line_max_y;
    if (panel->line_max_y > 0.0f) {
      rect.y += spacing;
    }
    panel->cursor_y = rect.y;
    panel->line_max_y = 0.0f;
  }

  if (width <= 0.0f) {
    width = panel->content.x + panel->content.w - rect.x;
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

  return rect;
}

static AP_FColor AP_GuiWidgetColor(AP_U32 id, const AP_FRect *rect) {
  float mx = (float)AP_GetMouseX();
  float my = (float)AP_GetMouseY();
  bool hovered = AP_GuiContains(rect, mx, my);

  if (hovered) {
    g_hot = id;
    g_want_mouse = true;
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
  float mx = (float)AP_GetMouseX();
  float my = (float)AP_GetMouseY();
  bool hovered = AP_GuiContains(rect, mx, my);

  if (hovered) {
    g_hot = id;
    g_want_mouse = true;
    if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
      g_active = id;
    }
  }

  if (g_active == id && AP_IsMouseReleased(AP_MOUSE_LEFT)) {
    g_active = 0;
    return hovered;
  }

  return false;
}

AP_GuiStyle AP_GuiDarkStyle(void) {
  AP_GuiStyle style;
  memset(&style, 0, sizeof(style));
  style.window_bg = AP_GuiColor(0.11f, 0.12f, 0.15f, 0.96f);
  style.title_bg = AP_GuiColor(0.16f, 0.18f, 0.24f, 1.0f);
  style.title_text = AP_GuiColor(0.94f, 0.95f, 0.97f, 1.0f);
  style.border = AP_GuiColor(1.0f, 1.0f, 1.0f, 0.08f);
  style.text = AP_GuiColor(0.90f, 0.91f, 0.94f, 1.0f);
  style.text_disabled = AP_GuiColor(0.55f, 0.57f, 0.62f, 1.0f);
  style.widget_bg = AP_GuiColor(0.18f, 0.20f, 0.25f, 1.0f);
  style.widget_hovered = AP_GuiColor(0.24f, 0.28f, 0.36f, 1.0f);
  style.widget_active = AP_GuiColor(0.28f, 0.42f, 0.82f, 1.0f);
  style.accent = AP_GuiColor(0.35f, 0.55f, 1.00f, 1.0f);
  style.check_mark = AP_GuiColor(0.95f, 0.96f, 1.00f, 1.0f);
  style.slider_grab = AP_GuiColor(0.45f, 0.62f, 1.00f, 1.0f);
  style.separator = AP_GuiColor(1.0f, 1.0f, 1.0f, 0.10f);
  style.rounding = 8.0f;
  style.padding = 12.0f;
  style.spacing = 8.0f;
  style.title_height = 32.0f;
  style.widget_height = 28.0f;
  style.border_width = 1.0f;
  style.font_size = 16.0f;
  style.grab_width = 12.0f;
  return style;
}

AP_GuiStyle AP_GuiLightStyle(void) {
  AP_GuiStyle style = AP_GuiDarkStyle();
  style.window_bg = AP_GuiColor(0.94f, 0.94f, 0.96f, 0.98f);
  style.title_bg = AP_GuiColor(0.35f, 0.50f, 0.90f, 1.0f);
  style.title_text = AP_GuiColor(1.0f, 1.0f, 1.0f, 1.0f);
  style.border = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.12f);
  style.text = AP_GuiColor(0.12f, 0.13f, 0.16f, 1.0f);
  style.text_disabled = AP_GuiColor(0.45f, 0.47f, 0.52f, 1.0f);
  style.widget_bg = AP_GuiColor(1.0f, 1.0f, 1.0f, 1.0f);
  style.widget_hovered = AP_GuiColor(0.88f, 0.91f, 0.98f, 1.0f);
  style.widget_active = AP_GuiColor(0.70f, 0.80f, 0.98f, 1.0f);
  style.accent = AP_GuiColor(0.25f, 0.45f, 0.90f, 1.0f);
  style.check_mark = AP_GuiColor(0.20f, 0.35f, 0.85f, 1.0f);
  style.slider_grab = AP_GuiColor(0.25f, 0.45f, 0.90f, 1.0f);
  style.separator = AP_GuiColor(0.0f, 0.0f, 0.0f, 0.12f);
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

bool AP_GuiWantCaptureMouse(void) { return g_want_mouse || g_active != 0; }

bool AP_GuiWantCaptureKeyboard(void) { return g_want_keyboard; }

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

void AP_GuiShutdown(void) {
  memset(g_windows, 0, sizeof(g_windows));
  g_panel_count = 0;
  g_gui_font = NULL;
  g_hot = 0;
  g_active = 0;
  g_focus = 0;
  g_want_mouse = false;
  g_want_keyboard = false;
}

bool AP_GuiBeginPanelEx(const char *title, AP_FRect *rect, bool *open) {
  AP_GuiPanel *panel;
  AP_GuiWindowState *state;
  AP_FRect area;
  AP_FRect title_bar;
  AP_FRect close_rect;
  AP_U32 id;
  float mx;
  float my;
  bool hovered;

  AP_GuiEnsureStyle();
  AP_FontInit();

  if (title == NULL) {
    title = "Panel";
  }

  if (g_panel_count == 0) {
    g_want_mouse = false;
    g_want_keyboard = false;
    g_widget_index = 0;
    g_hot = 0;
  }

  if (g_panel_count >= AP_GUI_MAX_PANELS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "GUI panel stack overflow");
    return false;
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
  title_bar = state->rect;
  title_bar.h = g_style.title_height;
  hovered = AP_GuiContains(&state->rect, mx, my);
  if (hovered) {
    g_want_mouse = true;
  }

  if (AP_GuiContains(&title_bar, mx, my) && AP_IsMousePressed(AP_MOUSE_LEFT)) {
    close_rect.x = state->rect.x + state->rect.w - g_style.title_height;
    close_rect.y = state->rect.y;
    close_rect.w = g_style.title_height;
    close_rect.h = g_style.title_height;
    if (!(open != NULL && AP_GuiContains(&close_rect, mx, my))) {
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

  AP_GuiDrawRect(&state->rect, g_style.window_bg, true);
  AP_GuiDrawRect(&title_bar, g_style.title_bg, true);
  AP_GuiDrawRect(&state->rect, g_style.border, false);

  {
    AP_FRect title_text = title_bar;
    title_text.x += g_style.padding;
    title_text.w -= g_style.padding * 2.0f;
    if (open != NULL) {
      title_text.w -= g_style.title_height;
    }
    AP_RenderTextAligned(AP_GuiFont(), &title_text, title, g_style.title_text,
                         AP_GuiFontSize(), AP_TEXT_ALIGN_LEFT);
  }

  if (open != NULL) {
    close_rect.x = state->rect.x + state->rect.w - g_style.title_height;
    close_rect.y = state->rect.y;
    close_rect.w = g_style.title_height;
    close_rect.h = g_style.title_height;
    AP_RenderTextAligned(AP_GuiFont(), &close_rect, "x", g_style.title_text,
                         AP_GuiFontSize(), AP_TEXT_ALIGN_CENTER);
    if (AP_GuiClicked(id ^ 0xC10CEu, &close_rect)) {
      *open = false;
      return false;
    }
  }

  panel = &g_panels[g_panel_count++];
  memset(panel, 0, sizeof(*panel));
  panel->id = id;
  panel->rect = state->rect;
  panel->content.x = state->rect.x + g_style.padding;
  panel->content.y = state->rect.y + g_style.title_height + g_style.padding;
  panel->content.w = state->rect.w - g_style.padding * 2.0f;
  panel->content.h = state->rect.h - g_style.title_height - g_style.padding * 2.0f;
  panel->cursor_x = panel->content.x;
  panel->cursor_y = panel->content.y;
  panel->line_x = panel->content.x;
  panel->line_max_y = 0.0f;
  panel->visible = true;
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
  AP_GuiSetNextPanelPos(x, y);
  AP_GuiSetNextPanelSize(width, height);
  return AP_GuiBeginPanelEx(title, &rect, NULL);
}

void AP_GuiEndPanel(void) {
  if (g_panel_count > 0) {
    g_panel_count -= 1;
  }
  g_same_line = false;
  g_indent = 0.0f;
}

void AP_GuiSameLine(void) { g_same_line = true; }

void AP_GuiDummy(float width, float height) {
  (void)AP_GuiNextRect(height, width);
}

void AP_GuiSeparator(void) {
  AP_FRect rect = AP_GuiNextRect(1.0f, 0.0f);
  AP_SetRenderDrawColorFloat(g_style.separator.r, g_style.separator.g,
                             g_style.separator.b, g_style.separator.a);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderLine(rect.x, rect.y, rect.x + rect.w, rect.y);
}

void AP_GuiIndent(float width) { g_indent = width; }

AP_FRect AP_GuiLayoutRect(float height) {
  AP_GuiEnsureStyle();
  if (height <= 0.0f) {
    height = g_style.widget_height;
  }
  return AP_GuiNextRect(height, 0.0f);
}

void AP_GuiLabel(const char *text) {
  AP_FPoint size;
  AP_FRect rect;

  AP_GuiEnsureStyle();
  size = AP_MeasureTextEx(AP_GuiFont(), text != NULL ? text : "", AP_GuiFontSize());
  rect = AP_GuiNextRect(size.y > g_style.widget_height * 0.6f
                            ? size.y
                            : g_style.widget_height * 0.75f,
                        0.0f);
  AP_RenderTextAligned(AP_GuiFont(), &rect, text, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);
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
  bool clicked;

  AP_GuiEnsureStyle();
  id = AP_GuiId(label);
  rect = AP_GuiNextRect(g_style.widget_height, width);
  clicked = AP_GuiClicked(id, &rect);
  AP_GuiDrawRect(&rect, AP_GuiWidgetColor(id, &rect), true);
  AP_RenderTextAligned(AP_GuiFont(), &rect, label, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_CENTER);
  return clicked;
}

bool AP_GuiButton(const char *label) { return AP_GuiButtonEx(label, 0.0f); }

bool AP_GuiCheckbox(const char *label, bool *checked) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect box;
  bool clicked;

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
  AP_RenderTextAligned(AP_GuiFont(), &row, label, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);
  return clicked;
}

bool AP_GuiRadio(const char *label, int *value, int item) {
  bool selected = value != NULL && *value == item;
  bool clicked = AP_GuiCheckbox(label, &selected);
  if (clicked && value != NULL) {
    *value = item;
  }
  return clicked;
}

bool AP_GuiSliderF(const char *label, float *value, float min_value,
                   float max_value) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect track;
  AP_FRect grab;
  AP_FRect text;
  float mx;
  float t = 0.0f;
  bool changed = false;
  char value_text[32];

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
  AP_RenderTextAligned(AP_GuiFont(), &text, label, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);

  track = row;
  track.x += text.w + g_style.spacing;
  track.w = row.w - text.w - g_style.spacing;
  track.y += (row.h - 6.0f) * 0.5f;
  track.h = 6.0f;

  if (AP_GuiContains(&row, mx, (float)AP_GetMouseY())) {
    g_hot = id;
    g_want_mouse = true;
    if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
      g_active = id;
    }
  }

  if (g_active == id) {
    if (AP_IsMouseDown(AP_MOUSE_LEFT)) {
      t = (mx - track.x) / track.w;
      if (t < 0.0f) {
        t = 0.0f;
      }
      if (t > 1.0f) {
        t = 1.0f;
      }
      *value = min_value + t * (max_value - min_value);
      changed = true;
      g_want_mouse = true;
    } else {
      g_active = 0;
    }
  }

  t = (*value - min_value) / (max_value - min_value);
  if (t < 0.0f) {
    t = 0.0f;
  }
  if (t > 1.0f) {
    t = 1.0f;
  }

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

bool AP_GuiToggle(const char *label, bool *on) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect track;
  AP_FRect knob;
  bool clicked;

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
  AP_RenderTextAligned(AP_GuiFont(), &row, label, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);
  return clicked;
}

bool AP_GuiProgress(float fraction) {
  AP_FRect row;
  AP_FRect fill;

  AP_GuiEnsureStyle();
  if (fraction < 0.0f) {
    fraction = 0.0f;
  }
  if (fraction > 1.0f) {
    fraction = 1.0f;
  }

  row = AP_GuiNextRect(g_style.widget_height * 0.45f, 0.0f);
  AP_GuiDrawRect(&row, g_style.widget_bg, true);
  fill = row;
  fill.w *= fraction;
  if (fill.w > 0.0f) {
    AP_GuiDrawRect(&fill, g_style.accent, true);
  }
  return true;
}

bool AP_GuiInputText(const char *label, char *buffer, int capacity) {
  AP_U32 id;
  AP_FRect row;
  AP_FRect field;
  AP_FRect text;
  const char *typed;
  int length;
  bool hovered;
  bool active;

  AP_GuiEnsureStyle();
  if (buffer == NULL || capacity <= 1) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Input buffer is invalid");
    return false;
  }

  id = AP_GuiId(label);
  row = AP_GuiNextRect(g_style.widget_height, 0.0f);
  text = row;
  text.w = row.w * 0.38f;
  field = row;
  field.x += text.w + g_style.spacing;
  field.w = row.w - text.w - g_style.spacing;

  hovered = AP_GuiContains(&field, (float)AP_GetMouseX(), (float)AP_GetMouseY());
  if (hovered) {
    g_want_mouse = true;
    if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
      g_focus = id;
    }
  } else if (AP_IsMousePressed(AP_MOUSE_LEFT) && g_focus == id) {
    g_focus = 0;
  }

  active = g_focus == id;
  if (active) {
    g_want_keyboard = true;
    typed = AP_GetText();
    length = (int)strlen(buffer);
    if (typed != NULL) {
      while (*typed != '\0' && length < capacity - 1) {
        if ((unsigned char)*typed >= 32) {
          buffer[length++] = *typed;
          buffer[length] = '\0';
        }
        typed += 1;
      }
    }

    if (AP_IsKeyPressed(AP_KEY_BACKSPACE) && length > 0) {
      buffer[length - 1] = '\0';
    }

    if (AP_IsKeyPressed(AP_KEY_ENTER) || AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      g_focus = 0;
    }

    if (AP_IsCtrlDown() && AP_IsKeyPressed(AP_KEY_V)) {
      const char *clip = AP_GetClipboardText();
      if (clip != NULL) {
        int remaining = capacity - 1 - (int)strlen(buffer);
        strncat(buffer, clip, remaining > 0 ? (size_t)remaining : 0);
      }
    }
  }

  AP_RenderTextAligned(AP_GuiFont(), &text, label, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);
  AP_GuiDrawRect(&field, active ? g_style.widget_hovered : g_style.widget_bg, true);
  AP_RenderTextAligned(AP_GuiFont(), &field, buffer, g_style.text, AP_GuiFontSize(),
                       AP_TEXT_ALIGN_LEFT);
  return active;
}

bool AP_GuiColorEdit(const char *label, AP_FColor *color) {
  AP_FRect preview;
  bool changed = false;

  if (color == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Color cannot be NULL");
    return false;
  }

  AP_GuiLabel(label);
  preview = AP_GuiNextRect(g_style.widget_height, 48.0f);
  AP_SetRenderDrawColorFloat(color->r, color->g, color->b, 1.0f);
  AP_RenderFillRoundedRect(&preview, g_style.rounding);

  AP_GuiSameLine();
  changed |= AP_GuiSliderF("R", &color->r, 0.0f, 1.0f);
  changed |= AP_GuiSliderF("G", &color->g, 0.0f, 1.0f);
  changed |= AP_GuiSliderF("B", &color->b, 0.0f, 1.0f);
  return changed;
}
