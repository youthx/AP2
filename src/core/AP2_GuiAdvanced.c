/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_GuiAdvanced.h"

#ifndef AP2_NO_GUI_ADVANCED

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Input.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Font.h"
#include "AP2/AP2_Math.h"
#include "AP2/AP2_Logger.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#define AP_GUI_MAX_WIDGETS 2048
#define AP_GUI_MAX_SIGNALS 4096
#define AP_GUI_MAX_CHILDREN 128
#define AP_GUI_SIGNAL_NAME_MAX 64

/* =========================================================
 * Internal Structures
 * ========================================================= */

typedef struct AP_GuiSignalConnection
{
    char signal_name[AP_GUI_SIGNAL_NAME_MAX];
    AP_GuiEventCallback callback;
    void *user_data;
    bool connected;
} AP_GuiSignalConnection;

typedef struct AP_GuiWidgetData
{
    AP_GuiWidgetType type;
    char *name;
    AP_GuiWidget *parent;
    AP_GuiWidget **children;
    int child_count;
    int child_capacity;

    /* Geometry */
    AP_FRect rect;
    float min_width;
    float min_height;
    float max_width;
    float max_height;
    AP_GuiSizePolicy size_policy_x;
    AP_GuiSizePolicy size_policy_y;

    /* Content */
    char *text;
    float value;
    float slider_min;
    float slider_max;
    float slider_step;
    bool checked;
    AP_Texture *image;

    /* State */
    bool visible;
    bool enabled;
    bool focused;
    bool hovered;
    bool pressed;

    /* Style */
    AP_FColor bg_color;
    AP_FColor text_color;
    AP_FColor border_color;
    float border_width;
    AP_GuiMargins padding;
    AP_GuiMargins margins;
    float corner_radius;
    AP_GuiTheme *theme;

    /* CSS-like Properties */
    AP_GuiDisplay display;
    AP_GuiPosition position;
    float top, bottom, left, right;
    AP_GuiFlexDirection flex_direction;
    AP_GuiJustifyContent justify_content;
    AP_GuiAlignItems align_items;
    float flex_gap;
    bool flex_wrap;
    float flex_grow;
    float flex_shrink;
    float flex_basis;
    AP_GuiBorder border;
    AP_GuiShadow shadow;
    AP_GuiOverflow overflow;
    bool clipped;
    float opacity;
    bool pointer_events;
    char *style_string;
    char **classes;
    int class_count;
    int class_capacity;

    /* Canvas-specific data */
    AP_GuiCanvasRenderMode canvas_mode;
    AP_GuiCanvasRenderCallback canvas_callback;
    void *canvas_user_data;
    AP_Texture *canvas_texture;

    /* Layout */
    AP_GuiLayout *layout;

    /* Tooltips */
    char *tooltip;
    char *status_tip;

    /* Signals */
    AP_GuiSignalConnection *signals;
    int signal_count;
    int signal_capacity;
} AP_GuiWidgetData;

typedef struct AP_GuiLayout
{
    AP_GuiLayoutType type;
    AP_GuiWidget *owner;
    float spacing;
    AP_GuiMargins margins;
    AP_GuiAlignment alignment;
    int rows;
    int cols;
    AP_GuiWidget **widgets;
    int *row_spans;
    int *col_spans;
    int widget_count;
    int widget_capacity;
    bool needs_layout;
} AP_GuiLayout;

static AP_GuiWidgetData g_widgets[AP_GUI_MAX_WIDGETS];
static bool g_widgets_used[AP_GUI_MAX_WIDGETS];
static int g_widget_count = 0;

static AP_GuiTheme *g_global_theme = NULL;
static AP_Font *g_global_font = NULL;
static AP_GuiWidget *g_focused_widget = NULL;
static AP_GuiWidget *g_hovered_widget = NULL;

/* =========================================================
 * Utility Functions
 * ========================================================= */

static AP_FColor AP_GuiColorFromFloat(float r, float g, float b, float a)
{
    AP_FColor c;
    c.r = r;
    c.g = g;
    c.b = b;
    c.a = a;
    return c;
}

static float AP_GuiMaxf(float a, float b)
{
    return a > b ? a : b;
}

static float AP_GuiMinf(float a, float b)
{
    return a < b ? a : b;
}

static float AP_GuiClampf(float x, float lo, float hi)
{
    return AP_GuiMaxf(lo, AP_GuiMinf(x, hi));
}

static bool AP_GuiRectContains(AP_FRect rect, float x, float y)
{
    return x >= rect.x && x < rect.x + rect.w &&
           y >= rect.y && y < rect.y + rect.h;
}

/* =========================================================
 * Widget Allocation
 * ========================================================= */

static AP_GuiWidget *AP_GuiAllocateWidget(void)
{
    int i;
    for (i = 0; i < AP_GUI_MAX_WIDGETS; ++i)
    {
        if (!g_widgets_used[i])
        {
            AP_GuiWidgetData *data = &g_widgets[i];
            g_widgets_used[i] = true;
            memset(data, 0, sizeof(AP_GuiWidgetData));
            g_widget_count++;

            /* Sane CSS-like defaults for every widget */
            data->opacity = 1.0f;
            data->pointer_events = true;
            data->flex_shrink = 1.0f;
            data->display = AP_GUI_DISPLAY_BLOCK;
            data->position = AP_GUI_POSITION_STATIC;

            return (AP_GuiWidget *)data;
        }
    }
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "GUI widget pool exhausted");
    return NULL;
}

static void AP_GuiFreeWidget(AP_GuiWidget *widget)
{
    int i;
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return;
    }

    data = (AP_GuiWidgetData *)widget;

    /* Find and free from pool */
    for (i = 0; i < AP_GUI_MAX_WIDGETS; ++i)
    {
        if ((AP_GuiWidget *)&g_widgets[i] == widget)
        {
            g_widgets_used[i] = false;
            g_widget_count--;
            break;
        }
    }
}

/* =========================================================
 * Theme Functions
 * ========================================================= */

AP_GuiTheme *AP_GuiThemeDarkNew(void)
{
    AP_GuiTheme *theme = (AP_GuiTheme *)malloc(sizeof(AP_GuiTheme));
    if (theme == NULL)
    {
        return NULL;
    }

    memset(theme, 0, sizeof(AP_GuiTheme));

    /* Dark theme colors */
    theme->colors.window_bg = AP_GuiColorFromFloat(0.1f, 0.1f, 0.1f, 1.0f);
    theme->colors.panel_bg = AP_GuiColorFromFloat(0.15f, 0.15f, 0.15f, 1.0f);
    theme->colors.text = AP_GuiColorFromFloat(0.9f, 0.9f, 0.9f, 1.0f);
    theme->colors.text_disabled = AP_GuiColorFromFloat(0.5f, 0.5f, 0.5f, 1.0f);
    theme->colors.button_bg = AP_GuiColorFromFloat(0.2f, 0.2f, 0.2f, 1.0f);
    theme->colors.button_bg_hovered = AP_GuiColorFromFloat(0.3f, 0.3f, 0.3f, 1.0f);
    theme->colors.button_bg_pressed = AP_GuiColorFromFloat(0.1f, 0.1f, 0.1f, 1.0f);
    theme->colors.button_text = AP_GuiColorFromFloat(0.9f, 0.9f, 0.9f, 1.0f);
    theme->colors.widget_bg = AP_GuiColorFromFloat(0.2f, 0.2f, 0.2f, 1.0f);
    theme->colors.widget_bg_hovered = AP_GuiColorFromFloat(0.25f, 0.25f, 0.25f, 1.0f);
    theme->colors.widget_bg_focused = AP_GuiColorFromFloat(0.3f, 0.3f, 0.35f, 1.0f);
    theme->colors.border = AP_GuiColorFromFloat(0.3f, 0.3f, 0.3f, 1.0f);
    theme->colors.border_hover = AP_GuiColorFromFloat(0.5f, 0.5f, 0.5f, 1.0f);
    theme->colors.accent = AP_GuiColorFromFloat(0.2f, 0.5f, 1.0f, 1.0f);
    theme->colors.accent_light = AP_GuiColorFromFloat(0.4f, 0.7f, 1.0f, 1.0f);
    theme->colors.error = AP_GuiColorFromFloat(1.0f, 0.2f, 0.2f, 1.0f);
    theme->colors.warning = AP_GuiColorFromFloat(1.0f, 0.8f, 0.2f, 1.0f);
    theme->colors.success = AP_GuiColorFromFloat(0.2f, 1.0f, 0.2f, 1.0f);
    theme->colors.info = AP_GuiColorFromFloat(0.2f, 0.8f, 1.0f, 1.0f);

    /* Default sizes */
    theme->font_size = 14.0f;
    theme->font_size_title = 18.0f;
    theme->font_size_small = 11.0f;
    theme->button_height = 32.0f;
    theme->edit_height = 28.0f;
    theme->spacing = 4.0f;
    theme->padding = 8.0f;
    theme->border_width = 1.0f;
    theme->rounding = 4.0f;
    theme->shadow_blur = 4.0f;
    theme->shadow_offset = 2.0f;
    theme->shadow_color = AP_GuiColorFromFloat(0.0f, 0.0f, 0.0f, 0.3f);

    return theme;
}

AP_GuiTheme *AP_GuiThemeLightNew(void)
{
    AP_GuiTheme *theme = (AP_GuiTheme *)malloc(sizeof(AP_GuiTheme));
    if (theme == NULL)
    {
        return NULL;
    }

    memset(theme, 0, sizeof(AP_GuiTheme));

    /* Light theme colors */
    theme->colors.window_bg = AP_GuiColorFromFloat(0.95f, 0.95f, 0.95f, 1.0f);
    theme->colors.panel_bg = AP_GuiColorFromFloat(0.9f, 0.9f, 0.9f, 1.0f);
    theme->colors.text = AP_GuiColorFromFloat(0.1f, 0.1f, 0.1f, 1.0f);
    theme->colors.text_disabled = AP_GuiColorFromFloat(0.5f, 0.5f, 0.5f, 1.0f);
    theme->colors.button_bg = AP_GuiColorFromFloat(0.8f, 0.8f, 0.8f, 1.0f);
    theme->colors.button_bg_hovered = AP_GuiColorFromFloat(0.7f, 0.7f, 0.7f, 1.0f);
    theme->colors.button_bg_pressed = AP_GuiColorFromFloat(0.6f, 0.6f, 0.6f, 1.0f);
    theme->colors.button_text = AP_GuiColorFromFloat(0.1f, 0.1f, 0.1f, 1.0f);
    theme->colors.widget_bg = AP_GuiColorFromFloat(0.85f, 0.85f, 0.85f, 1.0f);
    theme->colors.widget_bg_hovered = AP_GuiColorFromFloat(0.8f, 0.8f, 0.8f, 1.0f);
    theme->colors.widget_bg_focused = AP_GuiColorFromFloat(0.75f, 0.8f, 0.9f, 1.0f);
    theme->colors.border = AP_GuiColorFromFloat(0.7f, 0.7f, 0.7f, 1.0f);
    theme->colors.border_hover = AP_GuiColorFromFloat(0.5f, 0.5f, 0.5f, 1.0f);
    theme->colors.accent = AP_GuiColorFromFloat(0.2f, 0.5f, 1.0f, 1.0f);
    theme->colors.accent_light = AP_GuiColorFromFloat(0.4f, 0.7f, 1.0f, 1.0f);
    theme->colors.error = AP_GuiColorFromFloat(1.0f, 0.2f, 0.2f, 1.0f);
    theme->colors.warning = AP_GuiColorFromFloat(1.0f, 0.8f, 0.2f, 1.0f);
    theme->colors.success = AP_GuiColorFromFloat(0.2f, 1.0f, 0.2f, 1.0f);
    theme->colors.info = AP_GuiColorFromFloat(0.2f, 0.8f, 1.0f, 1.0f);

    /* Default sizes */
    theme->font_size = 14.0f;
    theme->font_size_title = 18.0f;
    theme->font_size_small = 11.0f;
    theme->button_height = 32.0f;
    theme->edit_height = 28.0f;
    theme->spacing = 4.0f;
    theme->padding = 8.0f;
    theme->border_width = 1.0f;
    theme->rounding = 4.0f;
    theme->shadow_blur = 4.0f;
    theme->shadow_offset = 2.0f;
    theme->shadow_color = AP_GuiColorFromFloat(0.0f, 0.0f, 0.0f, 0.1f);

    return theme;
}

AP_GuiTheme *AP_GuiThemeHighContrastNew(void)
{
    AP_GuiTheme *theme = AP_GuiThemeDarkNew();
    if (theme == NULL)
    {
        return NULL;
    }

    /* High contrast adjustments */
    theme->colors.text = AP_GuiColorFromFloat(1.0f, 1.0f, 1.0f, 1.0f);
    theme->colors.window_bg = AP_GuiColorFromFloat(0.0f, 0.0f, 0.0f, 1.0f);
    theme->colors.accent = AP_GuiColorFromFloat(1.0f, 1.0f, 0.0f, 1.0f);
    theme->border_width = 2.0f;

    return theme;
}

AP_GuiTheme *AP_GuiThemeCustomNew(void)
{
    return (AP_GuiTheme *)malloc(sizeof(AP_GuiTheme));
}

void AP_GuiThemeDestroy(AP_GuiTheme *theme)
{
    if (theme != NULL)
    {
        free(theme);
    }
}

AP_GuiTheme *AP_GuiThemeClone(AP_GuiTheme *theme)
{
    AP_GuiTheme *clone;

    if (theme == NULL)
    {
        return NULL;
    }

    clone = (AP_GuiTheme *)malloc(sizeof(AP_GuiTheme));
    if (clone != NULL)
    {
        memcpy(clone, theme, sizeof(AP_GuiTheme));
    }

    return clone;
}

void AP_GuiSetGlobalTheme(AP_GuiTheme *theme)
{
    if (g_global_theme != NULL && g_global_theme != theme)
    {
        AP_GuiThemeDestroy(g_global_theme);
    }
    g_global_theme = theme;
}

AP_GuiTheme *AP_GuiGlobalTheme(void)
{
    if (g_global_theme == NULL)
    {
        g_global_theme = AP_GuiThemeDarkNew();
    }
    return g_global_theme;
}

void AP_GuiSetGlobalFont(AP_Font *font)
{
    g_global_font = font;
}

AP_Font *AP_GuiGlobalFont(void)
{
    return g_global_font;
}

/* =========================================================
 * Widget Creation
 * ========================================================= */

AP_GuiWidget *AP_GuiWindowNew(const char *title, float width, float height)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_WINDOW;
    data->name = title != NULL ? (char *)malloc(strlen(title) + 1) : NULL;
    if (data->name != NULL)
    {
        strcpy(data->name, title);
    }
    data->rect.w = width;
    data->rect.h = height;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_PREFERRED;
    data->theme = AP_GuiGlobalTheme();
    data->display = AP_GUI_DISPLAY_BLOCK;
    data->position = AP_GUI_POSITION_STATIC;
    data->opacity = 1.0f;
    data->pointer_events = true;
    data->flex_grow = 0.0f;
    data->flex_shrink = 1.0f;
    data->flex_basis = 0.0f;

    return widget;
}

AP_GuiWidget *AP_GuiPanelNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_PANEL;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_PREFERRED;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiLabelNew(const char *text)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_LABEL;
    data->text = text != NULL ? (char *)malloc(strlen(text) + 1) : NULL;
    if (data->text != NULL)
    {
        strcpy(data->text, text);
    }
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiButtonNew(const char *text)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_BUTTON;
    data->text = text != NULL ? (char *)malloc(strlen(text) + 1) : NULL;
    if (data->text != NULL)
    {
        strcpy(data->text, text);
    }
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();
    data->rect.h = data->theme->button_height;

    return widget;
}

AP_GuiWidget *AP_GuiTextEditNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_TEXT_EDIT;
    data->text = (char *)malloc(256);
    if (data->text != NULL)
    {
        data->text[0] = '\0';
    }
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();
    data->rect.h = data->theme->edit_height;

    return widget;
}

AP_GuiWidget *AP_GuiCheckboxNew(const char *text)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_CHECKBOX;
    data->text = text != NULL ? (char *)malloc(strlen(text) + 1) : NULL;
    if (data->text != NULL)
    {
        strcpy(data->text, text);
    }
    data->visible = true;
    data->enabled = true;
    data->checked = false;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiRadioNew(const char *text)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_RADIO;
    data->text = text != NULL ? (char *)malloc(strlen(text) + 1) : NULL;
    if (data->text != NULL)
    {
        strcpy(data->text, text);
    }
    data->visible = true;
    data->enabled = true;
    data->checked = false;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiSliderNew(float min_val, float max_val, float step)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_SLIDER;
    data->value = min_val;
    data->slider_min = min_val;
    data->slider_max = max_val;
    data->slider_step = step;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiSpinnerNew(int min_val, int max_val)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_SPINNER;
    data->value = (float)min_val;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiProgressBarNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_PROGRESS_BAR;
    data->value = 0.0f;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiComboBoxNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_COMBO_BOX;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_MINIMUM;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiListBoxNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_LIST_BOX;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiImageNew(AP_Texture *texture)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_IMAGE;
    data->image = texture;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_PREFERRED;
    data->size_policy_y = AP_GUI_SIZE_PREFERRED;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiScrollAreaNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_SCROLL_AREA;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiTabWidgetNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_TAB_WIDGET;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiTreeWidgetNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_TREE_WIDGET;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiVBoxNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_VBOX;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiHBoxNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_HBOX;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiGridNew(int rows, int cols)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_GRID;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiFlexBoxNew(void)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_FLEX;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiSpacerNew(float width, float height)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_SPACER;
    data->rect.w = width;
    data->rect.h = height;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_FIXED;
    data->size_policy_y = AP_GUI_SIZE_FIXED;
    data->theme = AP_GuiGlobalTheme();

    return widget;
}

AP_GuiWidget *AP_GuiSeparatorNew(bool vertical)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_SEPARATOR;
    data->visible = true;
    data->enabled = true;
    data->theme = AP_GuiGlobalTheme();

    if (vertical)
    {
        data->size_policy_x = AP_GUI_SIZE_MINIMUM;
        data->size_policy_y = AP_GUI_SIZE_EXPANDING;
        data->rect.w = 2.0f;
    }
    else
    {
        data->size_policy_x = AP_GUI_SIZE_EXPANDING;
        data->size_policy_y = AP_GUI_SIZE_MINIMUM;
        data->rect.h = 2.0f;
    }

    return widget;
}

void AP_GuiWidgetDestroy(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data;
    int i;

    if (widget == NULL)
    {
        return;
    }

    data = (AP_GuiWidgetData *)widget;

    /* Remove from parent */
    if (data->parent != NULL)
    {
        AP_GuiWidgetRemoveChild(data->parent, widget);
    }

    /* Destroy all children */
    if (data->children != NULL)
    {
        for (i = 0; i < data->child_count; ++i)
        {
            AP_GuiWidgetDestroy(data->children[i]);
        }
        free(data->children);
    }

    /* Free strings */
    if (data->name != NULL)
    {
        free(data->name);
    }
    if (data->text != NULL)
    {
        free(data->text);
    }
    if (data->tooltip != NULL)
    {
        free(data->tooltip);
    }
    if (data->status_tip != NULL)
    {
        free(data->status_tip);
    }
    if (data->style_string != NULL)
    {
        free(data->style_string);
    }
    if (data->classes != NULL)
    {
        for (i = 0; i < data->class_count; ++i)
        {
            free(data->classes[i]);
        }
        free(data->classes);
    }
    if (data->canvas_texture != NULL)
    {
        AP_DestroyTexture(data->canvas_texture);
    }

    /* Free signals */
    if (data->signals != NULL)
    {
        free(data->signals);
    }

    /* Free layout */
    if (data->layout != NULL)
    {
        free(data->layout);
    }

    /* Update focus */
    if (g_focused_widget == widget)
    {
        g_focused_widget = NULL;
    }
    if (g_hovered_widget == widget)
    {
        g_hovered_widget = NULL;
    }

    AP_GuiFreeWidget(widget);
}

/* =========================================================
 * Widget Hierarchy
 * ========================================================= */

void AP_GuiWidgetAddChild(AP_GuiWidget *parent, AP_GuiWidget *child)
{
    AP_GuiWidgetData *pdata;
    AP_GuiWidgetData *cdata;

    if (parent == NULL || child == NULL)
    {
        return;
    }

    pdata = (AP_GuiWidgetData *)parent;
    cdata = (AP_GuiWidgetData *)child;

    /* Remove from old parent */
    if (cdata->parent != NULL)
    {
        AP_GuiWidgetRemoveChild(cdata->parent, child);
    }

    cdata->parent = parent;

    /* Grow array if needed */
    if (pdata->child_count >= pdata->child_capacity)
    {
        int new_capacity = AP_GuiMaxf(4, pdata->child_capacity * 2);
        AP_GuiWidget **new_children = (AP_GuiWidget **)realloc(
            pdata->children, (size_t)new_capacity * sizeof(AP_GuiWidget *));
        if (new_children == NULL)
        {
            return;
        }
        pdata->children = new_children;
        pdata->child_capacity = new_capacity;
    }

    pdata->children[pdata->child_count++] = child;
}

void AP_GuiWidgetRemoveChild(AP_GuiWidget *parent, AP_GuiWidget *child)
{
    AP_GuiWidgetData *pdata;
    AP_GuiWidgetData *cdata;
    int i;

    if (parent == NULL || child == NULL)
    {
        return;
    }

    pdata = (AP_GuiWidgetData *)parent;
    cdata = (AP_GuiWidgetData *)child;

    for (i = 0; i < pdata->child_count; ++i)
    {
        if (pdata->children[i] == child)
        {
            pdata->children[i] = pdata->children[pdata->child_count - 1];
            pdata->child_count--;
            cdata->parent = NULL;
            break;
        }
    }
}

void AP_GuiWidgetRemoveAllChildren(AP_GuiWidget *parent)
{
    AP_GuiWidgetData *pdata;

    if (parent == NULL)
    {
        return;
    }

    pdata = (AP_GuiWidgetData *)parent;
    pdata->child_count = 0;
}

AP_GuiWidget *AP_GuiWidgetParent(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->parent : NULL;
}

AP_GuiWidget *AP_GuiWidgetChildAt(AP_GuiWidget *parent, int index)
{
    AP_GuiWidgetData *pdata = (AP_GuiWidgetData *)parent;
    if (pdata != NULL && index >= 0 && index < pdata->child_count)
    {
        return pdata->children[index];
    }
    return NULL;
}

int AP_GuiWidgetChildCount(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->child_count : 0;
}

/* =========================================================
 * Widget Properties
 * ========================================================= */

AP_GuiWidgetType AP_GuiWidgetGetType(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->type : AP_GUI_WIDGET_NONE;
}

const char *AP_GuiWidgetName(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->name != NULL ? data->name : "";
}

void AP_GuiWidgetSetName(AP_GuiWidget *widget, const char *name)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data == NULL)
    {
        return;
    }

    if (data->name != NULL)
    {
        free(data->name);
    }

    if (name != NULL)
    {
        data->name = (char *)malloc(strlen(name) + 1);
        if (data->name != NULL)
        {
            strcpy(data->name, name);
        }
    }
    else
    {
        data->name = NULL;
    }
}

bool AP_GuiWidgetVisible(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->visible;
}

void AP_GuiWidgetSetVisible(AP_GuiWidget *widget, bool visible)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->visible = visible;
    }
}

bool AP_GuiWidgetEnabled(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->enabled;
}

void AP_GuiWidgetSetEnabled(AP_GuiWidget *widget, bool enabled)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->enabled = enabled;
    }
}

AP_FRect AP_GuiWidgetGeometry(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        return data->rect;
    }
    return (AP_FRect){0, 0, 0, 0};
}

float AP_GuiWidgetX(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->rect.x : 0.0f;
}

float AP_GuiWidgetY(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->rect.y : 0.0f;
}

float AP_GuiWidgetWidth(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->rect.w : 0.0f;
}

float AP_GuiWidgetHeight(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->rect.h : 0.0f;
}

void AP_GuiWidgetSetGeometry(AP_GuiWidget *widget, float x, float y, float w, float h)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->rect.x = x;
        data->rect.y = y;
        data->rect.w = w;
        data->rect.h = h;
    }
}

void AP_GuiWidgetSetPos(AP_GuiWidget *widget, float x, float y)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->rect.x = x;
        data->rect.y = y;
    }
}

void AP_GuiWidgetSetSize(AP_GuiWidget *widget, float width, float height)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->rect.w = width;
        data->rect.h = height;
    }
}

float AP_GuiWidgetMinWidth(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->min_width : 0.0f;
}

float AP_GuiWidgetMinHeight(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->min_height : 0.0f;
}

float AP_GuiWidgetMaxWidth(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->max_width : 0.0f;
}

float AP_GuiWidgetMaxHeight(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->max_height : 0.0f;
}

void AP_GuiWidgetSetMinSize(AP_GuiWidget *widget, float width, float height)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->min_width = width;
        data->min_height = height;
    }
}

void AP_GuiWidgetSetMaxSize(AP_GuiWidget *widget, float width, float height)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->max_width = width;
        data->max_height = height;
    }
}

AP_GuiSizePolicy AP_GuiWidgetSizePolicyX(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->size_policy_x : AP_GUI_SIZE_FIXED;
}

AP_GuiSizePolicy AP_GuiWidgetSizePolicyY(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->size_policy_y : AP_GUI_SIZE_FIXED;
}

void AP_GuiWidgetSetSizePolicy(AP_GuiWidget *widget, AP_GuiSizePolicy x, AP_GuiSizePolicy y)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->size_policy_x = x;
        data->size_policy_y = y;
    }
}

const char *AP_GuiWidgetText(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->text != NULL ? data->text : "";
}

void AP_GuiWidgetSetText(AP_GuiWidget *widget, const char *text)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data == NULL)
    {
        return;
    }

    if (data->text != NULL)
    {
        free(data->text);
    }

    if (text != NULL)
    {
        data->text = (char *)malloc(strlen(text) + 1);
        if (data->text != NULL)
        {
            strcpy(data->text, text);
        }
    }
    else
    {
        data->text = NULL;
    }
}

float AP_GuiWidgetValue(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->value : 0.0f;
}

void AP_GuiWidgetSetValue(AP_GuiWidget *widget, float value)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->value = value;
    }
}

bool AP_GuiWidgetChecked(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->checked;
}

void AP_GuiWidgetSetChecked(AP_GuiWidget *widget, bool checked)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->checked = checked;
    }
}

AP_Texture *AP_GuiWidgetImage(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->image : NULL;
}

void AP_GuiWidgetSetImage(AP_GuiWidget *widget, AP_Texture *texture)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->image = texture;
    }
}

const char *AP_GuiWidgetTooltip(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->tooltip != NULL ? data->tooltip : "";
}

void AP_GuiWidgetSetTooltip(AP_GuiWidget *widget, const char *text)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data == NULL)
    {
        return;
    }

    if (data->tooltip != NULL)
    {
        free(data->tooltip);
    }

    if (text != NULL)
    {
        data->tooltip = (char *)malloc(strlen(text) + 1);
        if (data->tooltip != NULL)
        {
            strcpy(data->tooltip, text);
        }
    }
    else
    {
        data->tooltip = NULL;
    }
}

const char *AP_GuiWidgetStatusTip(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->status_tip != NULL ? data->status_tip : "";
}

void AP_GuiWidgetSetStatusTip(AP_GuiWidget *widget, const char *text)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data == NULL)
    {
        return;
    }

    if (data->status_tip != NULL)
    {
        free(data->status_tip);
    }

    if (text != NULL)
    {
        data->status_tip = (char *)malloc(strlen(text) + 1);
        if (data->status_tip != NULL)
        {
            strcpy(data->status_tip, text);
        }
    }
    else
    {
        data->status_tip = NULL;
    }
}

/* =========================================================
 * Styling
 * ========================================================= */

AP_GuiTheme *AP_GuiWidgetTheme(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->theme : NULL;
}

void AP_GuiWidgetSetTheme(AP_GuiWidget *widget, AP_GuiTheme *theme)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->theme = theme;
    }
}

void AP_GuiWidgetSetBackgroundColor(AP_GuiWidget *widget, AP_FColor color)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->bg_color = color;
    }
}

void AP_GuiWidgetSetTextColor(AP_GuiWidget *widget, AP_FColor color)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->text_color = color;
    }
}

void AP_GuiWidgetSetBorderColor(AP_GuiWidget *widget, AP_FColor color)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->border_color = color;
    }
}

void AP_GuiWidgetSetBorderWidth(AP_GuiWidget *widget, float width)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->border_width = width;
    }
}

void AP_GuiWidgetSetPadding(AP_GuiWidget *widget, AP_GuiMargins margins)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->padding = margins;
    }
}

void AP_GuiWidgetSetMargins(AP_GuiWidget *widget, AP_GuiMargins margins)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->margins = margins;
    }
}

void AP_GuiWidgetSetCornerRadius(AP_GuiWidget *widget, float radius)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->corner_radius = radius;
    }
}

AP_FColor AP_GuiWidgetBackgroundColor(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        return data->bg_color;
    }
    return AP_GuiColorFromFloat(0, 0, 0, 0);
}

AP_FColor AP_GuiWidgetTextColor(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        return data->text_color;
    }
    return AP_GuiColorFromFloat(1, 1, 1, 1);
}

/* =========================================================
 * Layout Management (stubs for now)
 * ========================================================= */

AP_GuiLayout *AP_GuiWidgetLayout(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->layout : NULL;
}

void AP_GuiWidgetSetLayout(AP_GuiWidget *widget, AP_GuiLayout *layout)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->layout = layout;
    }
}

void AP_GuiLayoutSetSpacing(AP_GuiLayout *layout, float spacing)
{
    if (layout != NULL)
    {
        layout->spacing = spacing;
    }
}

void AP_GuiLayoutSetMargins(AP_GuiLayout *layout, AP_GuiMargins margins)
{
    if (layout != NULL)
    {
        layout->margins = margins;
    }
}

void AP_GuiLayoutSetAlignment(AP_GuiLayout *layout, AP_GuiAlignment align)
{
    if (layout != NULL)
    {
        layout->alignment = align;
    }
}

void AP_GuiLayoutAddWidget(AP_GuiLayout *layout, AP_GuiWidget *widget)
{
    /* Stub */
    (void)layout;
    (void)widget;
}

void AP_GuiLayoutAddWidgetAt(AP_GuiLayout *layout, AP_GuiWidget *widget, int row, int col)
{
    /* Stub */
    (void)layout;
    (void)widget;
    (void)row;
    (void)col;
}

void AP_GuiLayoutSetRowSpan(AP_GuiLayout *layout, AP_GuiWidget *widget, int span)
{
    /* Stub */
    (void)layout;
    (void)widget;
    (void)span;
}

void AP_GuiLayoutSetColSpan(AP_GuiLayout *layout, AP_GuiWidget *widget, int span)
{
    /* Stub */
    (void)layout;
    (void)widget;
    (void)span;
}

/* =========================================================
 * Focus & Input Management
 * ========================================================= */

AP_GuiWidget *AP_GuiFocusedWidget(void)
{
    return g_focused_widget;
}

void AP_GuiSetFocusedWidget(AP_GuiWidget *widget)
{
    g_focused_widget = widget;
}

bool AP_GuiWidgetHasFocus(AP_GuiWidget *widget)
{
    return g_focused_widget == widget;
}

bool AP_GuiWidgetUnderMouse(AP_GuiWidget *widget)
{
    return g_hovered_widget == widget;
}

bool AP_GuiWidgetAcceptsKeyboardInput(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data == NULL)
    {
        return false;
    }
    return data->type == AP_GUI_WIDGET_TEXT_EDIT ||
           data->type == AP_GUI_WIDGET_COMBO_BOX ||
           data->type == AP_GUI_WIDGET_SPINNER;
}

bool AP_GuiWidgetAcceptsMouseInput(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->enabled;
}

/* =========================================================
 * Events & Signals
 * ========================================================= */

void AP_GuiWidgetConnect(AP_GuiWidget *widget, const char *signal,
                         AP_GuiEventCallback callback, void *user_data)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    AP_GuiSignalConnection *new_signals;
    int i;

    if (data == NULL || signal == NULL)
    {
        return;
    }

    /* Look for existing connection */
    for (i = 0; i < data->signal_count; ++i)
    {
        if (data->signals[i].connected &&
            strcmp(data->signals[i].signal_name, signal) == 0)
        {
            data->signals[i].callback = callback;
            data->signals[i].user_data = user_data;
            return;
        }
    }

    /* Grow array if needed */
    if (data->signal_count >= data->signal_capacity)
    {
        int new_capacity = AP_GuiMaxf(4, data->signal_capacity * 2);
        new_signals = (AP_GuiSignalConnection *)realloc(
            data->signals, (size_t)new_capacity * sizeof(AP_GuiSignalConnection));
        if (new_signals == NULL)
        {
            return;
        }
        data->signals = new_signals;
        data->signal_capacity = new_capacity;
    }

    data->signals[data->signal_count].callback = callback;
    data->signals[data->signal_count].user_data = user_data;
    data->signals[data->signal_count].connected = true;
    strncpy(data->signals[data->signal_count].signal_name, signal,
            AP_GUI_SIGNAL_NAME_MAX - 1);
    data->signal_count++;
}

void AP_GuiWidgetDisconnect(AP_GuiWidget *widget, const char *signal,
                            AP_GuiEventCallback callback)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;

    if (data == NULL || signal == NULL)
    {
        return;
    }

    for (i = 0; i < data->signal_count; ++i)
    {
        if (data->signals[i].connected &&
            strcmp(data->signals[i].signal_name, signal) == 0 &&
            data->signals[i].callback == callback)
        {
            data->signals[i].connected = false;
            break;
        }
    }
}

void AP_GuiWidgetEmit(AP_GuiWidget *widget, const char *signal)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    AP_GuiEvent event;
    int i;

    if (data == NULL || signal == NULL)
    {
        return;
    }

    event.type = AP_GUI_EVENT_NONE;
    event.target = widget;
    event.key = 0;
    event.text = NULL;
    event.value = 0.0f;
    event.index = 0;
    event.user_data = NULL;

    for (i = 0; i < data->signal_count; ++i)
    {
        if (data->signals[i].connected &&
            strcmp(data->signals[i].signal_name, signal) == 0)
        {
            if (data->signals[i].callback != NULL)
            {
                event.user_data = data->signals[i].user_data;
                data->signals[i].callback(&event);
            }
        }
    }
}

void AP_GuiWidgetEmitWithValue(AP_GuiWidget *widget, const char *signal, float value)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    AP_GuiEvent event;
    int i;

    if (data == NULL || signal == NULL)
    {
        return;
    }

    event.type = AP_GUI_EVENT_VALUE_CHANGED;
    event.target = widget;
    event.value = value;
    event.key = 0;
    event.text = NULL;
    event.index = 0;
    event.user_data = NULL;

    for (i = 0; i < data->signal_count; ++i)
    {
        if (data->signals[i].connected &&
            strcmp(data->signals[i].signal_name, signal) == 0)
        {
            if (data->signals[i].callback != NULL)
            {
                event.user_data = data->signals[i].user_data;
                data->signals[i].callback(&event);
            }
        }
    }
}

void AP_GuiWidgetEmitWithText(AP_GuiWidget *widget, const char *signal, const char *text)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    AP_GuiEvent event;
    int i;

    if (data == NULL || signal == NULL)
    {
        return;
    }

    event.type = AP_GUI_EVENT_TEXT_CHANGED;
    event.target = widget;
    event.text = (char *)text;
    event.key = 0;
    event.value = 0.0f;
    event.index = 0;
    event.user_data = NULL;

    for (i = 0; i < data->signal_count; ++i)
    {
        if (data->signals[i].connected &&
            strcmp(data->signals[i].signal_name, signal) == 0)
        {
            if (data->signals[i].callback != NULL)
            {
                event.user_data = data->signals[i].user_data;
                data->signals[i].callback(&event);
            }
        }
    }
}

void AP_GuiProcessEvents(void)
{
    /* Process any queued events - to be called each frame */
}

/* =========================================================
 * Rendering & Updates
 * ========================================================= */

static void AP_GuiWidgetUpdateInternal(AP_GuiWidget *widget, float dt)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;
    double mx = 0.0, my = 0.0;
    bool inside;

    if (data == NULL || !data->visible || !data->enabled)
    {
        return;
    }

    AP_GetMousePosition(&mx, &my);
    inside = data->pointer_events && AP_GuiRectContains(data->rect, (float)mx, (float)my);
    data->hovered = inside;
    if (inside)
    {
        g_hovered_widget = widget;
    }

    switch (data->type)
    {
    case AP_GUI_WIDGET_BUTTON:
        if (inside && AP_IsMouseDown(AP_MOUSE_LEFT))
        {
            data->pressed = true;
        }
        else
        {
            if (data->pressed && inside && AP_IsMouseReleased(AP_MOUSE_LEFT))
            {
                AP_GuiWidgetEmit(widget, "clicked");
            }
            data->pressed = false;
        }
        break;

    case AP_GUI_WIDGET_CHECKBOX:
    case AP_GUI_WIDGET_RADIO:
        if (inside && AP_IsMousePressed(AP_MOUSE_LEFT))
        {
            data->checked = !data->checked;
            AP_GuiWidgetEmitWithValue(widget, "toggled", data->checked ? 1.0f : 0.0f);
        }
        break;

    case AP_GUI_WIDGET_SLIDER:
        /* A drag only starts when the press itself lands on the slider. */
        if (inside && AP_IsMousePressed(AP_MOUSE_LEFT))
        {
            data->pressed = true;
            g_focused_widget = widget;
        }

        /* Once started, the drag continues wherever the mouse goes, but a
         * click that starts elsewhere on screen must never affect a slider
         * that merely happens to still hold keyboard focus. */
        if (data->pressed)
        {
            if (AP_IsMouseDown(AP_MOUSE_LEFT))
            {
                float t = ((float)mx - data->rect.x) / AP_GuiMaxf(data->rect.w, 1.0f);
                float range = data->slider_max - data->slider_min;
                t = AP_GuiClampf(t, 0.0f, 1.0f);
                data->value = data->slider_min + t * range;
                if (data->slider_step > 0.0f)
                {
                    data->value = data->slider_min +
                                  AP_GuiClampf((float)((int)((data->value - data->slider_min) / data->slider_step + 0.5f)) * data->slider_step,
                                               0.0f, range);
                }
                AP_GuiWidgetEmitWithValue(widget, "valueChanged", data->value);
            }
            else
            {
                data->pressed = false;
            }
        }
        break;

    default:
        break;
    }

    /* Update all children */
    for (i = 0; i < data->child_count; ++i)
    {
        AP_GuiWidgetUpdateInternal(data->children[i], dt);
    }
}

void AP_GuiWidgetUpdate(AP_GuiWidget *widget, float dt)
{
    /* g_hovered_widget is recomputed fresh every frame starting from the
     * root; otherwise a widget the mouse has since left would stay
     * "hovered" forever (AP_GuiWidgetUnderMouse would never go false). */
    g_hovered_widget = NULL;
    AP_GuiWidgetUpdateInternal(widget, dt);
}

static AP_FColor AP_GuiResolveBgColor(AP_GuiWidgetData *data, AP_GuiTheme *theme)
{
    if (data->bg_color.a > 0.0f)
    {
        return data->bg_color;
    }

    switch (data->type)
    {
    case AP_GUI_WIDGET_WINDOW:
        return theme->colors.window_bg;
    case AP_GUI_WIDGET_PANEL:
        return theme->colors.panel_bg;
    case AP_GUI_WIDGET_BUTTON:
        return data->pressed   ? theme->colors.button_bg_pressed
               : data->hovered ? theme->colors.button_bg_hovered
                               : theme->colors.button_bg;
    default:
        return data->hovered ? theme->colors.widget_bg_hovered : theme->colors.widget_bg;
    }
}

void AP_GuiWidgetRender(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    AP_GuiTheme *theme;
    AP_FColor bg;
    AP_FColor text_color;
    float radius;
    int i;

    if (data == NULL || !data->visible || data->opacity <= 0.0f)
    {
        return;
    }

    theme = data->theme != NULL ? data->theme : AP_GuiGlobalTheme();
    radius = data->corner_radius > 0.0f ? data->corner_radius : theme->rounding;
    text_color = data->text_color.a > 0.0f ? data->text_color : theme->colors.text;

    switch (data->type)
    {
    case AP_GUI_WIDGET_WINDOW:
        bg = AP_GuiResolveBgColor(data, theme);
        AP_SetDrawColor(bg.r, bg.g, bg.b, bg.a * data->opacity);
        AP_FillRoundedRect(&data->rect, radius);
        if (data->name != NULL && data->name[0] != '\0')
        {
            AP_FRect title_bar = {data->rect.x, data->rect.y, data->rect.w, 28.0f};
            AP_SetDrawColor(theme->colors.accent.r, theme->colors.accent.g,
                            theme->colors.accent.b, data->opacity);
            AP_FillRoundedRect(&title_bar, radius);
            AP_RenderTextEx(theme->font_default, data->rect.x + 8.0f, data->rect.y + 6.0f,
                            data->name, theme->colors.text, theme->font_size);
        }
        break;

    case AP_GUI_WIDGET_PANEL:
        bg = AP_GuiResolveBgColor(data, theme);
        AP_SetDrawColor(bg.r, bg.g, bg.b, bg.a * data->opacity);
        AP_FillRoundedRect(&data->rect, radius);
        break;

    case AP_GUI_WIDGET_LABEL:
        AP_RenderTextEx(theme->font_default, data->rect.x, data->rect.y,
                        data->text != NULL ? data->text : "", text_color, theme->font_size);
        break;

    case AP_GUI_WIDGET_BUTTON:
        bg = AP_GuiResolveBgColor(data, theme);
        AP_SetDrawColor(bg.r, bg.g, bg.b, bg.a * data->opacity);
        AP_FillRoundedRect(&data->rect, radius);
        AP_RenderTextAligned(theme->font_default, &data->rect, data->text != NULL ? data->text : "",
                             theme->colors.button_text, theme->font_size, AP_TEXT_ALIGN_CENTER);
        break;

    case AP_GUI_WIDGET_CHECKBOX:
    case AP_GUI_WIDGET_RADIO:
    {
        float box_size = 18.0f;
        AP_FRect box = {data->rect.x, data->rect.y + (data->rect.h - box_size) * 0.5f, box_size, box_size};
        AP_SetDrawColor(theme->colors.widget_bg.r, theme->colors.widget_bg.g,
                        theme->colors.widget_bg.b, data->opacity);
        AP_FillRect(&box);
        AP_SetDrawColor(theme->colors.border.r, theme->colors.border.g, theme->colors.border.b, data->opacity);
        AP_DrawRect(&box);
        if (data->checked)
        {
            AP_FRect inner = {box.x + 3.0f, box.y + 3.0f, box_size - 6.0f, box_size - 6.0f};
            AP_SetDrawColor(theme->colors.accent.r, theme->colors.accent.g,
                            theme->colors.accent.b, data->opacity);
            AP_FillRect(&inner);
        }
        AP_RenderTextEx(theme->font_default, box.x + box_size + 8.0f,
                        data->rect.y + (data->rect.h - theme->font_size) * 0.5f,
                        data->text != NULL ? data->text : "", text_color, theme->font_size);
    }
    break;

    case AP_GUI_WIDGET_SLIDER:
    {
        float range = data->slider_max - data->slider_min;
        float t = range > 0.0f ? (data->value - data->slider_min) / range : 0.0f;
        float track_h = 4.0f;
        float handle_r = 8.0f;
        AP_FRect track = {data->rect.x, data->rect.y + (data->rect.h - track_h) * 0.5f, data->rect.w, track_h};
        AP_SetDrawColor(theme->colors.widget_bg.r, theme->colors.widget_bg.g,
                        theme->colors.widget_bg.b, data->opacity);
        AP_FillRoundedRect(&track, track_h * 0.5f);
        AP_SetDrawColor(theme->colors.accent.r, theme->colors.accent.g,
                        theme->colors.accent.b, data->opacity);
        AP_FillCircleF(data->rect.x + t * data->rect.w, data->rect.y + data->rect.h * 0.5f, handle_r);
    }
    break;

    case AP_GUI_WIDGET_PROGRESS_BAR:
    {
        float range = 1.0f;
        float t = AP_GuiClampf(data->value, 0.0f, range);
        AP_SetDrawColor(theme->colors.widget_bg.r, theme->colors.widget_bg.g,
                        theme->colors.widget_bg.b, data->opacity);
        AP_FillRoundedRect(&data->rect, radius);
        AP_FRect fill = {data->rect.x, data->rect.y, data->rect.w * t, data->rect.h};
        AP_SetDrawColor(theme->colors.accent.r, theme->colors.accent.g,
                        theme->colors.accent.b, data->opacity);
        AP_FillRoundedRect(&fill, radius);
    }
    break;

    case AP_GUI_WIDGET_SEPARATOR:
        AP_SetDrawColor(theme->colors.border.r, theme->colors.border.g,
                        theme->colors.border.b, data->opacity);
        AP_FillRect(&data->rect);
        break;

    case AP_GUI_WIDGET_IMAGE:
        if (data->canvas_callback != NULL)
        {
            data->canvas_callback(widget, data->rect, data->canvas_user_data);
        }
        else if (data->image != NULL)
        {
            AP_DrawTexture(data->image, NULL, &data->rect);
        }
        break;

    default:
        break;
    }

    /* Render all children */
    for (i = 0; i < data->child_count; ++i)
    {
        AP_GuiWidgetRender(data->children[i]);
    }
}

void AP_GuiWidgetRecalculateLayout(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    float spacing;
    float cursor_x, cursor_y;
    int i;

    if (data == NULL)
    {
        return;
    }

    spacing = data->theme != NULL ? data->theme->spacing : 4.0f;

    if (data->type == AP_GUI_WIDGET_WINDOW || data->type == AP_GUI_WIDGET_PANEL)
    {
        float top_offset = data->type == AP_GUI_WIDGET_WINDOW ? 28.0f : 0.0f;

        if (data->child_count == 1)
        {
            AP_GuiWidgetData *child = (AP_GuiWidgetData *)data->children[0];
            child->rect.x = data->rect.x + data->padding.left;
            child->rect.y = data->rect.y + data->padding.top + top_offset;
            child->rect.w = data->rect.w - data->padding.left - data->padding.right;
            child->rect.h = data->rect.h - data->padding.top - data->padding.bottom - top_offset;
            AP_GuiWidgetRecalculateLayout(data->children[0]);
        }
        else
        {
            cursor_x = data->rect.x + data->padding.left;
            cursor_y = data->rect.y + data->padding.top + top_offset;
            for (i = 0; i < data->child_count; ++i)
            {
                AP_GuiWidgetData *child = (AP_GuiWidgetData *)data->children[i];
                float avail_w = data->rect.w - data->padding.left - data->padding.right;

                child->rect.x = cursor_x;
                child->rect.y = cursor_y;
                if (child->size_policy_x == AP_GUI_SIZE_EXPANDING && avail_w > 0.0f)
                {
                    child->rect.w = avail_w;
                }

                AP_GuiWidgetRecalculateLayout(data->children[i]);
                cursor_y += child->rect.h + spacing;
            }
        }
    }
    else if (data->type == AP_GUI_WIDGET_VBOX)
    {
        cursor_x = data->rect.x + data->padding.left;
        cursor_y = data->rect.y + data->padding.top;

        for (i = 0; i < data->child_count; ++i)
        {
            AP_GuiWidgetData *child = (AP_GuiWidgetData *)data->children[i];
            float avail_w = data->rect.w - data->padding.left - data->padding.right;

            child->rect.x = cursor_x;
            child->rect.y = cursor_y;
            if (child->size_policy_x == AP_GUI_SIZE_EXPANDING && avail_w > 0.0f)
            {
                child->rect.w = avail_w;
            }

            AP_GuiWidgetRecalculateLayout(data->children[i]);
            cursor_y += child->rect.h + spacing;
        }
    }
    else if (data->type == AP_GUI_WIDGET_HBOX)
    {
        cursor_x = data->rect.x + data->padding.left;
        cursor_y = data->rect.y + data->padding.top;

        for (i = 0; i < data->child_count; ++i)
        {
            AP_GuiWidgetData *child = (AP_GuiWidgetData *)data->children[i];
            float avail_h = data->rect.h - data->padding.top - data->padding.bottom;

            child->rect.x = cursor_x;
            child->rect.y = cursor_y;
            if (child->size_policy_y == AP_GUI_SIZE_EXPANDING && avail_h > 0.0f)
            {
                child->rect.h = avail_h;
            }

            AP_GuiWidgetRecalculateLayout(data->children[i]);
            cursor_x += child->rect.w + spacing;
        }
    }
    else
    {
        for (i = 0; i < data->child_count; ++i)
        {
            AP_GuiWidgetRecalculateLayout(data->children[i]);
        }
    }
}

/* =========================================================
 * Utility Functions
 * ========================================================= */

AP_GuiWidget *AP_GuiFindWidget(AP_GuiWidget *root, const char *name)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)root;
    int i;

    if (root == NULL || name == NULL)
    {
        return NULL;
    }

    if (data->name != NULL && strcmp(data->name, name) == 0)
    {
        return root;
    }

    for (i = 0; i < data->child_count; ++i)
    {
        AP_GuiWidget *found = AP_GuiFindWidget(data->children[i], name);
        if (found != NULL)
        {
            return found;
        }
    }

    return NULL;
}

AP_GuiWidget *AP_GuiWidgetAt(AP_GuiWidget *root, float x, float y)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)root;
    int i;

    if (root == NULL)
    {
        return NULL;
    }

    if (!data->visible)
    {
        return NULL;
    }

    /* Check children first (top to bottom) */
    for (i = data->child_count - 1; i >= 0; --i)
    {
        AP_GuiWidget *found = AP_GuiWidgetAt(data->children[i], x, y);
        if (found != NULL)
        {
            return found;
        }
    }

    /* Check if point is in this widget */
    if (AP_GuiRectContains(data->rect, x, y))
    {
        return root;
    }

    return NULL;
}

void AP_GuiWidgetPrintHierarchy(AP_GuiWidget *widget, int indent)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;

    if (widget == NULL)
    {
        return;
    }

    printf("%*s%s (%s)\n", indent, "",
           data->name != NULL ? data->name : "(unnamed)",
           data->type == AP_GUI_WIDGET_WINDOW ? "Window" : data->type == AP_GUI_WIDGET_BUTTON ? "Button"
                                                       : data->type == AP_GUI_WIDGET_LABEL    ? "Label"
                                                                                              : "Widget");

    for (i = 0; i < data->child_count; ++i)
    {
        AP_GuiWidgetPrintHierarchy(data->children[i], indent + 2);
    }
}

/* =========================================================
 * CSS-like Styling Functions
 * ========================================================= */

void AP_GuiWidgetSetDisplay(AP_GuiWidget *widget, AP_GuiDisplay display)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->display = display;
    }
}

AP_GuiDisplay AP_GuiWidgetDisplay(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->display : AP_GUI_DISPLAY_BLOCK;
}

void AP_GuiWidgetSetPosition(AP_GuiWidget *widget, AP_GuiPosition position)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->position = position;
    }
}

AP_GuiPosition AP_GuiWidgetPosition(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->position : AP_GUI_POSITION_STATIC;
}

void AP_GuiWidgetSetTop(AP_GuiWidget *widget, float top)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->top = top;
}

void AP_GuiWidgetSetBottom(AP_GuiWidget *widget, float bottom)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->bottom = bottom;
}

void AP_GuiWidgetSetLeft(AP_GuiWidget *widget, float left)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->left = left;
}

void AP_GuiWidgetSetRight(AP_GuiWidget *widget, float right)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->right = right;
}

float AP_GuiWidgetTop(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->top : 0.0f;
}

float AP_GuiWidgetBottom(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->bottom : 0.0f;
}

float AP_GuiWidgetLeft(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->left : 0.0f;
}

float AP_GuiWidgetRight(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->right : 0.0f;
}

void AP_GuiWidgetSetFlexDirection(AP_GuiWidget *widget, AP_GuiFlexDirection direction)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_direction = direction;
}

void AP_GuiWidgetSetJustifyContent(AP_GuiWidget *widget, AP_GuiJustifyContent justify)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->justify_content = justify;
}

void AP_GuiWidgetSetAlignItems(AP_GuiWidget *widget, AP_GuiAlignItems align)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->align_items = align;
}

void AP_GuiWidgetSetFlexGap(AP_GuiWidget *widget, float gap)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_gap = gap;
}

void AP_GuiWidgetSetFlexWrap(AP_GuiWidget *widget, bool wrap)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_wrap = wrap;
}

void AP_GuiWidgetSetFlexGrow(AP_GuiWidget *widget, float grow)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_grow = grow;
}

void AP_GuiWidgetSetFlexShrink(AP_GuiWidget *widget, float shrink)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_shrink = shrink;
}

void AP_GuiWidgetSetFlexBasis(AP_GuiWidget *widget, float basis)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
        data->flex_basis = basis;
}

float AP_GuiWidgetFlexGrow(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->flex_grow : 0.0f;
}

float AP_GuiWidgetFlexShrink(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->flex_shrink : 1.0f;
}

float AP_GuiWidgetFlexBasis(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->flex_basis : 0.0f;
}

void AP_GuiWidgetSetMarginAll(AP_GuiWidget *widget, float margin)
{
    AP_GuiMargins m = {margin, margin, margin, margin};
    AP_GuiWidgetSetMargins(widget, m);
}

void AP_GuiWidgetSetMarginVertical(AP_GuiWidget *widget, float margin)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->margins.top = margin;
        data->margins.bottom = margin;
    }
}

void AP_GuiWidgetSetMarginHorizontal(AP_GuiWidget *widget, float margin)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->margins.left = margin;
        data->margins.right = margin;
    }
}

void AP_GuiWidgetSetPaddingAll(AP_GuiWidget *widget, float padding)
{
    AP_GuiMargins p = {padding, padding, padding, padding};
    AP_GuiWidgetSetPadding(widget, p);
}

void AP_GuiWidgetSetPaddingVertical(AP_GuiWidget *widget, float padding)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->padding.top = padding;
        data->padding.bottom = padding;
    }
}

void AP_GuiWidgetSetPaddingHorizontal(AP_GuiWidget *widget, float padding)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->padding.left = padding;
        data->padding.right = padding;
    }
}

void AP_GuiWidgetSetBorder(AP_GuiWidget *widget, AP_GuiBorder border)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->border = border;
    }
}

AP_GuiBorder AP_GuiWidgetBorder(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        return data->border;
    }
    return (AP_GuiBorder){0};
}

void AP_GuiWidgetSetBorderRadius(AP_GuiWidget *widget, float radius)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->border.radius = radius;
        data->border.radius_top_left = radius;
        data->border.radius_top_right = radius;
        data->border.radius_bottom_left = radius;
        data->border.radius_bottom_right = radius;
    }
}

void AP_GuiWidgetSetBorderRadiusCorners(AP_GuiWidget *widget,
                                        float top_left, float top_right,
                                        float bottom_right, float bottom_left)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->border.radius_top_left = top_left;
        data->border.radius_top_right = top_right;
        data->border.radius_bottom_right = bottom_right;
        data->border.radius_bottom_left = bottom_left;
    }
}

void AP_GuiWidgetSetShadow(AP_GuiWidget *widget, AP_GuiShadow shadow)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->shadow = shadow;
    }
}

AP_GuiShadow AP_GuiWidgetShadow(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        return data->shadow;
    }
    return (AP_GuiShadow){0};
}

void AP_GuiWidgetSetOverflow(AP_GuiWidget *widget, AP_GuiOverflow overflow)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->overflow = overflow;
    }
}

AP_GuiOverflow AP_GuiWidgetOverflow(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->overflow : AP_GUI_OVERFLOW_VISIBLE;
}

void AP_GuiWidgetSetClipped(AP_GuiWidget *widget, bool clipped)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->clipped = clipped;
    }
}

bool AP_GuiWidgetClipped(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->clipped;
}

void AP_GuiWidgetSetOpacity(AP_GuiWidget *widget, float opacity)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->opacity = AP_GuiClampf(opacity, 0.0f, 1.0f);
    }
}

float AP_GuiWidgetOpacity(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->opacity : 1.0f;
}

void AP_GuiWidgetSetPointerEvents(AP_GuiWidget *widget, bool accepts_events)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    if (data != NULL)
    {
        data->pointer_events = accepts_events;
    }
}

bool AP_GuiWidgetPointerEvents(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL ? data->pointer_events : true;
}

void AP_GuiWidgetAddClass(AP_GuiWidget *widget, const char *class_name)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    char **new_classes;

    if (data == NULL || class_name == NULL)
        return;

    /* Check if class already exists */
    int i;
    for (i = 0; i < data->class_count; ++i)
    {
        if (strcmp(data->classes[i], class_name) == 0)
        {
            return;
        }
    }

    /* Grow array if needed */
    if (data->class_count >= data->class_capacity)
    {
        int new_capacity = AP_GuiMaxf(4, data->class_capacity * 2);
        new_classes = (char **)realloc(data->classes, (size_t)new_capacity * sizeof(char *));
        if (new_classes == NULL)
            return;
        data->classes = new_classes;
        data->class_capacity = new_capacity;
    }

    data->classes[data->class_count] = (char *)malloc(strlen(class_name) + 1);
    if (data->classes[data->class_count] != NULL)
    {
        strcpy(data->classes[data->class_count], class_name);
        data->class_count++;
    }
}

void AP_GuiWidgetRemoveClass(AP_GuiWidget *widget, const char *class_name)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;

    if (data == NULL || class_name == NULL)
        return;

    for (i = 0; i < data->class_count; ++i)
    {
        if (strcmp(data->classes[i], class_name) == 0)
        {
            free(data->classes[i]);
            data->classes[i] = data->classes[data->class_count - 1];
            data->class_count--;
            break;
        }
    }
}

bool AP_GuiWidgetHasClass(AP_GuiWidget *widget, const char *class_name)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;

    if (data == NULL || class_name == NULL)
        return false;

    for (i = 0; i < data->class_count; ++i)
    {
        if (strcmp(data->classes[i], class_name) == 0)
        {
            return true;
        }
    }
    return false;
}

void AP_GuiWidgetClearClasses(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    int i;

    if (data == NULL)
        return;

    for (i = 0; i < data->class_count; ++i)
    {
        free(data->classes[i]);
    }
    data->class_count = 0;
}

void AP_GuiWidgetSetStyle(AP_GuiWidget *widget, const char *style)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;

    if (data == NULL)
        return;

    if (data->style_string != NULL)
    {
        free(data->style_string);
    }

    if (style != NULL)
    {
        data->style_string = (char *)malloc(strlen(style) + 1);
        if (data->style_string != NULL)
        {
            strcpy(data->style_string, style);
            AP_GuiApplyStyleString(widget, style);
        }
    }
    else
    {
        data->style_string = NULL;
    }
}

const char *AP_GuiWidgetStyle(AP_GuiWidget *widget)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    return data != NULL && data->style_string != NULL ? data->style_string : "";
}

/* =========================================================
 * Canvas Widget Implementation
 * ========================================================= */

AP_GuiWidget *AP_GuiCanvasNew(float width, float height)
{
    AP_GuiWidget *widget = AP_GuiAllocateWidget();
    AP_GuiWidgetData *data;

    if (widget == NULL)
    {
        return NULL;
    }

    data = (AP_GuiWidgetData *)widget;
    data->type = AP_GUI_WIDGET_IMAGE; /* Use IMAGE as base type for now */
    data->rect.w = width;
    data->rect.h = height;
    data->visible = true;
    data->enabled = true;
    data->size_policy_x = AP_GUI_SIZE_EXPANDING;
    data->size_policy_y = AP_GUI_SIZE_EXPANDING;
    data->theme = AP_GuiGlobalTheme();
    data->canvas_mode = AP_GUI_CANVAS_2D;
    data->canvas_callback = NULL;
    data->canvas_user_data = NULL;

    /* Create render target texture */
    data->canvas_texture = AP_CreateTexture((int)width, (int)height);

    return widget;
}

void AP_GuiCanvasSetRenderMode(AP_GuiWidget *canvas, AP_GuiCanvasRenderMode mode)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data != NULL)
    {
        data->canvas_mode = mode;
    }
}

AP_GuiCanvasRenderMode AP_GuiCanvasGetRenderMode(AP_GuiWidget *canvas)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    return data != NULL ? data->canvas_mode : AP_GUI_CANVAS_2D;
}

void AP_GuiCanvasSetRenderCallback(AP_GuiWidget *canvas,
                                   AP_GuiCanvasRenderCallback callback,
                                   void *user_data)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data != NULL)
    {
        data->canvas_callback = callback;
        data->canvas_user_data = user_data;
    }
}

void AP_GuiCanvasClear(AP_GuiWidget *canvas, AP_FColor color)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data == NULL)
        return;

    AP_SetDrawColor(color.r, color.g, color.b, color.a);
    AP_FillRect(&data->rect);
}

AP_Texture *AP_GuiCanvasTexture(AP_GuiWidget *canvas)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    return data != NULL ? data->canvas_texture : NULL;
}

/* Canvas draw primitives operate in canvas-local coordinates (0,0 = top-left of the widget) */
void AP_GuiCanvasDrawRect(AP_GuiWidget *canvas, AP_FRect rect, AP_FColor color, bool filled)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    AP_FRect r;

    if (data == NULL)
        return;

    r.x = data->rect.x + rect.x;
    r.y = data->rect.y + rect.y;
    r.w = rect.w;
    r.h = rect.h;

    AP_SetDrawColor(color.r, color.g, color.b, color.a);
    if (filled)
    {
        AP_FillRect(&r);
    }
    else
    {
        AP_DrawRect(&r);
    }
}

void AP_GuiCanvasDrawCircle(AP_GuiWidget *canvas, float x, float y, float radius, AP_FColor color, bool filled)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;

    if (data == NULL)
        return;

    AP_SetDrawColor(color.r, color.g, color.b, color.a);
    if (filled)
    {
        AP_FillCircleF(data->rect.x + x, data->rect.y + y, radius);
    }
    else
    {
        /* Approximate an outlined circle with a filled ring is unsupported; fall back to filled */
        AP_FillCircleF(data->rect.x + x, data->rect.y + y, radius);
    }
}

void AP_GuiCanvasDrawLine(AP_GuiWidget *canvas, float x1, float y1, float x2, float y2, AP_FColor color, float width)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;

    if (data == NULL)
        return;

    (void)width;
    AP_SetDrawColor(color.r, color.g, color.b, color.a);
    AP_RenderLine(data->rect.x + x1, data->rect.y + y1, data->rect.x + x2, data->rect.y + y2);
}

void AP_GuiCanvasDrawText(AP_GuiWidget *canvas, const char *text, float x, float y, AP_FColor color)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;

    if (data == NULL || text == NULL)
        return;

    AP_RenderTextEx(NULL, data->rect.x + x, data->rect.y + y, text, color, 0.0f);
}

void AP_GuiCanvasDrawImage(AP_GuiWidget *canvas, AP_Texture *texture, AP_FRect src, AP_FRect dst)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    AP_FRect d;

    if (data == NULL || texture == NULL)
        return;

    d.x = data->rect.x + dst.x;
    d.y = data->rect.y + dst.y;
    d.w = dst.w;
    d.h = dst.h;

    AP_DrawTexture(texture, &src, &d);
}

void AP_GuiCanvasSetup3D(AP_GuiWidget *canvas)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data != NULL && data->canvas_texture != NULL)
    {
        AP_SetRenderTarget(data->canvas_texture);
        AP_SetDrawColor(0.0f, 0.0f, 0.0f, 1.0f);
        AP_Clear();
    }
}

void AP_GuiCanvasEnd3D(AP_GuiWidget *canvas)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data != NULL && data->canvas_texture != NULL)
    {
        AP_SetRenderTarget(NULL);
        AP_DrawTexture(data->canvas_texture, NULL, &data->rect);
    }
}

bool AP_GuiCanvasGetMousePos(AP_GuiWidget *canvas, float *x, float *y)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)canvas;
    if (data == NULL || x == NULL || y == NULL)
        return false;

    /* Get global mouse position and convert to canvas-local coordinates */
    float mouse_x = (float)AP_GetMouseX();
    float mouse_y = (float)AP_GetMouseY();

    if (AP_GuiRectContains(data->rect, mouse_x, mouse_y))
    {
        *x = mouse_x - data->rect.x;
        *y = mouse_y - data->rect.y;
        return true;
    }
    return false;
}

bool AP_GuiCanvasIsMouseInside(AP_GuiWidget *canvas)
{
    float x, y;
    return AP_GuiCanvasGetMousePos(canvas, &x, &y);
}

/* =========================================================
 * Style String Parsing (Tailwind-like)
 *
 * Utility classes are grouped below by category. Colors are computed
 * procedurally from a {hue, saturation} table + a shade->lightness curve
 * (HSL), giving every hue the full 50-950 shade range instead of a fixed
 * set of hand-picked literal colors.
 * ========================================================= */

typedef struct AP_GuiStyleHue
{
    const char *name;
    float hue;
    float saturation;
} AP_GuiStyleHue;

static const AP_GuiStyleHue AP_GUI_STYLE_HUES[] = {
    {"slate", 215.0f, 0.25f},
    {"gray", 220.0f, 0.13f},
    {"zinc", 240.0f, 0.05f},
    {"neutral", 0.0f, 0.0f},
    {"stone", 30.0f, 0.08f},
    {"red", 0.0f, 0.84f},
    {"orange", 25.0f, 0.95f},
    {"amber", 38.0f, 0.92f},
    {"yellow", 48.0f, 0.94f},
    {"lime", 84.0f, 0.81f},
    {"green", 142.0f, 0.71f},
    {"emerald", 160.0f, 0.84f},
    {"teal", 173.0f, 0.80f},
    {"cyan", 189.0f, 0.94f},
    {"sky", 199.0f, 0.89f},
    {"blue", 217.0f, 0.91f},
    {"indigo", 239.0f, 0.84f},
    {"violet", 258.0f, 0.90f},
    {"purple", 271.0f, 0.81f},
    {"fuchsia", 292.0f, 0.84f},
    {"pink", 330.0f, 0.81f},
    {"rose", 350.0f, 0.89f}};

typedef struct AP_GuiStyleShade
{
    int shade;
    float lightness;
} AP_GuiStyleShade;

static const AP_GuiStyleShade AP_GUI_STYLE_SHADES[] = {
    {50, 0.97f}, {100, 0.94f}, {200, 0.87f}, {300, 0.77f}, {400, 0.65f}, {500, 0.55f}, {600, 0.46f}, {700, 0.38f}, {800, 0.30f}, {900, 0.24f}, {950, 0.15f}};

static float AP_GuiStyleHueToRgb(float p, float q, float t)
{
    if (t < 0.0f)
        t += 1.0f;
    if (t > 1.0f)
        t -= 1.0f;
    if (t < 1.0f / 6.0f)
        return p + (q - p) * 6.0f * t;
    if (t < 0.5f)
        return q;
    if (t < 2.0f / 3.0f)
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
    return p;
}

static AP_FColor AP_GuiStyleHSLToColor(float h, float s, float l, float a)
{
    float r, g, b;

    if (s <= 0.0001f)
    {
        r = g = b = l;
    }
    else
    {
        float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
        float p = 2.0f * l - q;
        float hh = h / 360.0f;

        r = AP_GuiStyleHueToRgb(p, q, hh + 1.0f / 3.0f);
        g = AP_GuiStyleHueToRgb(p, q, hh);
        b = AP_GuiStyleHueToRgb(p, q, hh - 1.0f / 3.0f);
    }

    return AP_GuiColorFromFloat(r, g, b, a);
}

static const AP_GuiStyleHue *AP_GuiStyleFindHue(const char *name)
{
    size_t i;

    for (i = 0; i < sizeof(AP_GUI_STYLE_HUES) / sizeof(AP_GUI_STYLE_HUES[0]); ++i)
    {
        if (strcmp(AP_GUI_STYLE_HUES[i].name, name) == 0)
        {
            return &AP_GUI_STYLE_HUES[i];
        }
    }
    return NULL;
}

static float AP_GuiStyleFindShadeLightness(int shade)
{
    size_t i;
    size_t count = sizeof(AP_GUI_STYLE_SHADES) / sizeof(AP_GUI_STYLE_SHADES[0]);

    for (i = 0; i < count; ++i)
    {
        if (AP_GUI_STYLE_SHADES[i].shade == shade)
        {
            return AP_GUI_STYLE_SHADES[i].lightness;
        }
    }

    /* Nearest match for non-standard shade numbers */
    for (i = 0; i + 1 < count; ++i)
    {
        if (shade < AP_GUI_STYLE_SHADES[i + 1].shade)
        {
            return AP_GUI_STYLE_SHADES[i].lightness;
        }
    }
    return AP_GUI_STYLE_SHADES[count - 1].lightness;
}

/* Parses tokens like "red-500", "slate-200", "white", "black", "transparent" */
static bool AP_GuiStyleParseColor(const char *token, float alpha, AP_FColor *out)
{
    char name[32];
    const char *dash;
    int shade = 500;
    const AP_GuiStyleHue *hue;

    if (strcmp(token, "transparent") == 0)
    {
        *out = AP_GuiColorFromFloat(0.0f, 0.0f, 0.0f, 0.0f);
        return true;
    }
    if (strcmp(token, "white") == 0)
    {
        *out = AP_GuiColorFromFloat(1.0f, 1.0f, 1.0f, alpha);
        return true;
    }
    if (strcmp(token, "black") == 0)
    {
        *out = AP_GuiColorFromFloat(0.0f, 0.0f, 0.0f, alpha);
        return true;
    }

    dash = strrchr(token, '-');
    if (dash != NULL && dash[1] >= '0' && dash[1] <= '9')
    {
        size_t name_len = (size_t)(dash - token);
        if (name_len == 0 || name_len >= sizeof(name))
        {
            return false;
        }
        memcpy(name, token, name_len);
        name[name_len] = '\0';
        shade = atoi(dash + 1);
    }
    else
    {
        strncpy(name, token, sizeof(name) - 1);
        name[sizeof(name) - 1] = '\0';
    }

    hue = AP_GuiStyleFindHue(name);
    if (hue == NULL)
    {
        return false;
    }

    *out = AP_GuiStyleHSLToColor(hue->hue, hue->saturation,
                                 AP_GuiStyleFindShadeLightness(shade), alpha);
    return true;
}

/* Tailwind's 0.25rem (4px) spacing scale */
static float AP_GuiStyleSpacing(const char *number)
{
    return (float)atof(number) * 4.0f;
}

static bool AP_GuiStyleTokenIs(const char *token, const char *literal)
{
    return strcmp(token, literal) == 0;
}

static bool AP_GuiStyleTokenStarts(const char *token, const char *prefix, const char **rest)
{
    size_t len = strlen(prefix);
    if (strncmp(token, prefix, len) != 0)
    {
        return false;
    }
    *rest = token + len;
    return true;
}

static void AP_GuiStyleApplyToken(AP_GuiWidget *widget, const char *token)
{
    AP_GuiWidgetData *data = (AP_GuiWidgetData *)widget;
    const char *rest;
    AP_FColor color;

    /* --- Display / visibility --- */
    if (AP_GuiStyleTokenIs(token, "flex"))
    {
        AP_GuiWidgetSetDisplay(widget, AP_GUI_DISPLAY_FLEX);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "block"))
    {
        AP_GuiWidgetSetDisplay(widget, AP_GUI_DISPLAY_BLOCK);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "inline-block"))
    {
        AP_GuiWidgetSetDisplay(widget, AP_GUI_DISPLAY_INLINE_BLOCK);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "hidden"))
    {
        AP_GuiWidgetSetDisplay(widget, AP_GUI_DISPLAY_NONE);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "invisible"))
    {
        AP_GuiWidgetSetOpacity(widget, 0.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "visible"))
    {
        AP_GuiWidgetSetOpacity(widget, 1.0f);
        return;
    }

    /* --- Position --- */
    if (AP_GuiStyleTokenIs(token, "static"))
    {
        AP_GuiWidgetSetPosition(widget, AP_GUI_POSITION_STATIC);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "relative"))
    {
        AP_GuiWidgetSetPosition(widget, AP_GUI_POSITION_RELATIVE);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "absolute"))
    {
        AP_GuiWidgetSetPosition(widget, AP_GUI_POSITION_ABSOLUTE);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "fixed"))
    {
        AP_GuiWidgetSetPosition(widget, AP_GUI_POSITION_FIXED);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "top-", &rest))
    {
        AP_GuiWidgetSetTop(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "bottom-", &rest))
    {
        AP_GuiWidgetSetBottom(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "left-", &rest))
    {
        AP_GuiWidgetSetLeft(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "right-", &rest))
    {
        AP_GuiWidgetSetRight(widget, AP_GuiStyleSpacing(rest));
        return;
    }

    /* --- Flexbox --- */
    if (AP_GuiStyleTokenIs(token, "flex-row"))
    {
        AP_GuiWidgetSetFlexDirection(widget, AP_GUI_FLEX_ROW);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "flex-row-reverse"))
    {
        AP_GuiWidgetSetFlexDirection(widget, AP_GUI_FLEX_ROW_REVERSE);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "flex-col"))
    {
        AP_GuiWidgetSetFlexDirection(widget, AP_GUI_FLEX_COLUMN);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "flex-col-reverse"))
    {
        AP_GuiWidgetSetFlexDirection(widget, AP_GUI_FLEX_COLUMN_REVERSE);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "flex-wrap"))
    {
        AP_GuiWidgetSetFlexWrap(widget, true);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "flex-nowrap"))
    {
        AP_GuiWidgetSetFlexWrap(widget, false);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "grow"))
    {
        AP_GuiWidgetSetFlexGrow(widget, 1.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "grow-0"))
    {
        AP_GuiWidgetSetFlexGrow(widget, 0.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shrink"))
    {
        AP_GuiWidgetSetFlexShrink(widget, 1.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shrink-0"))
    {
        AP_GuiWidgetSetFlexShrink(widget, 0.0f);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "basis-", &rest))
    {
        AP_GuiWidgetSetFlexBasis(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "gap-", &rest))
    {
        AP_GuiWidgetSetFlexGap(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-start"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_START);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-center"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_CENTER);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-end"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_END);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-between"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_SPACE_BETWEEN);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-around"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_SPACE_AROUND);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "justify-evenly"))
    {
        AP_GuiWidgetSetJustifyContent(widget, AP_GUI_JUSTIFY_SPACE_EVENLY);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "items-start"))
    {
        AP_GuiWidgetSetAlignItems(widget, AP_GUI_ALIGN_ITEMS_START);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "items-center"))
    {
        AP_GuiWidgetSetAlignItems(widget, AP_GUI_ALIGN_ITEMS_CENTER);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "items-end"))
    {
        AP_GuiWidgetSetAlignItems(widget, AP_GUI_ALIGN_ITEMS_END);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "items-stretch"))
    {
        AP_GuiWidgetSetAlignItems(widget, AP_GUI_ALIGN_ITEMS_STRETCH);
        return;
    }

    /* --- Sizing --- */
    if (AP_GuiStyleTokenIs(token, "w-full"))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GUI_SIZE_EXPANDING, AP_GuiWidgetSizePolicyY(widget));
        return;
    }
    if (AP_GuiStyleTokenIs(token, "w-auto"))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GUI_SIZE_PREFERRED, AP_GuiWidgetSizePolicyY(widget));
        return;
    }
    if (AP_GuiStyleTokenIs(token, "h-full"))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GuiWidgetSizePolicyX(widget), AP_GUI_SIZE_EXPANDING);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "h-auto"))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GuiWidgetSizePolicyX(widget), AP_GUI_SIZE_PREFERRED);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "w-", &rest))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GUI_SIZE_FIXED, AP_GuiWidgetSizePolicyY(widget));
        AP_GuiWidgetSetSize(widget, AP_GuiStyleSpacing(rest), AP_GuiWidgetHeight(widget));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "h-", &rest))
    {
        AP_GuiWidgetSetSizePolicy(widget, AP_GuiWidgetSizePolicyX(widget), AP_GUI_SIZE_FIXED);
        AP_GuiWidgetSetSize(widget, AP_GuiWidgetWidth(widget), AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "min-w-", &rest))
    {
        AP_GuiWidgetSetMinSize(widget, AP_GuiStyleSpacing(rest), AP_GuiWidgetMinHeight(widget));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "min-h-", &rest))
    {
        AP_GuiWidgetSetMinSize(widget, AP_GuiWidgetMinWidth(widget), AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "max-w-", &rest))
    {
        AP_GuiWidgetSetMaxSize(widget, AP_GuiStyleSpacing(rest), AP_GuiWidgetMaxHeight(widget));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "max-h-", &rest))
    {
        AP_GuiWidgetSetMaxSize(widget, AP_GuiWidgetMaxWidth(widget), AP_GuiStyleSpacing(rest));
        return;
    }

    /* --- Spacing: margin / padding (all-sides, axis, and per-side) --- */
    if (AP_GuiStyleTokenStarts(token, "mx-", &rest))
    {
        AP_GuiWidgetSetMarginHorizontal(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "my-", &rest))
    {
        AP_GuiWidgetSetMarginVertical(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "mt-", &rest))
    {
        data->margins.top = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "mb-", &rest))
    {
        data->margins.bottom = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "ml-", &rest))
    {
        data->margins.left = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "mr-", &rest))
    {
        data->margins.right = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "m-", &rest))
    {
        AP_GuiWidgetSetMarginAll(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "px-", &rest))
    {
        AP_GuiWidgetSetPaddingHorizontal(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "py-", &rest))
    {
        AP_GuiWidgetSetPaddingVertical(widget, AP_GuiStyleSpacing(rest));
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "pt-", &rest))
    {
        data->padding.top = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "pb-", &rest))
    {
        data->padding.bottom = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "pl-", &rest))
    {
        data->padding.left = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "pr-", &rest))
    {
        data->padding.right = AP_GuiStyleSpacing(rest);
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "p-", &rest))
    {
        AP_GuiWidgetSetPaddingAll(widget, AP_GuiStyleSpacing(rest));
        return;
    }

    /* --- Colors: background / text / border --- */
    if (AP_GuiStyleTokenStarts(token, "bg-", &rest))
    {
        if (AP_GuiStyleParseColor(rest, 1.0f, &color))
        {
            AP_GuiWidgetSetBackgroundColor(widget, color);
        }
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "text-", &rest))
    {
        if (AP_GuiStyleParseColor(rest, 1.0f, &color))
        {
            AP_GuiWidgetSetTextColor(widget, color);
        }
        return;
    }
    if (AP_GuiStyleTokenStarts(token, "border-", &rest))
    {
        if (rest[0] >= '0' && rest[0] <= '9')
        {
            AP_GuiWidgetSetBorderWidth(widget, (float)atof(rest));
        }
        else if (AP_GuiStyleParseColor(rest, 1.0f, &color))
        {
            AP_GuiWidgetSetBorderColor(widget, color);
        }
        return;
    }
    if (AP_GuiStyleTokenIs(token, "border"))
    {
        AP_GuiWidgetSetBorderWidth(widget, 1.0f);
        return;
    }

    /* --- Border radius --- */
    if (AP_GuiStyleTokenIs(token, "rounded-none"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 0.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-sm"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 2.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-md"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 6.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-lg"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 8.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-xl"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 12.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-2xl"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 16.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-3xl"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 24.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded-full"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 9999.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "rounded"))
    {
        AP_GuiWidgetSetBorderRadius(widget, 4.0f);
        return;
    }

    /* --- Shadows --- */
    if (AP_GuiStyleTokenIs(token, "shadow-none"))
    {
        AP_GuiShadow shadow = {0.0f, 0.0f, 0.0f, {0, 0, 0, 0}, false};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shadow-sm"))
    {
        AP_GuiShadow shadow = {0.0f, 1.0f, 2.0f, {0, 0, 0, 0.15f}, true};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shadow-lg"))
    {
        AP_GuiShadow shadow = {0.0f, 6.0f, 12.0f, {0, 0, 0, 0.35f}, true};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shadow-xl"))
    {
        AP_GuiShadow shadow = {0.0f, 10.0f, 20.0f, {0, 0, 0, 0.4f}, true};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shadow-2xl"))
    {
        AP_GuiShadow shadow = {0.0f, 16.0f, 32.0f, {0, 0, 0, 0.5f}, true};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "shadow"))
    {
        AP_GuiShadow shadow = {2.0f, 2.0f, 4.0f, {0, 0, 0, 0.3f}, true};
        AP_GuiWidgetSetShadow(widget, shadow);
        return;
    }

    /* --- Overflow / clipping --- */
    if (AP_GuiStyleTokenIs(token, "overflow-hidden"))
    {
        AP_GuiWidgetSetOverflow(widget, AP_GUI_OVERFLOW_HIDDEN);
        AP_GuiWidgetSetClipped(widget, true);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "overflow-visible"))
    {
        AP_GuiWidgetSetOverflow(widget, AP_GUI_OVERFLOW_VISIBLE);
        AP_GuiWidgetSetClipped(widget, false);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "overflow-scroll"))
    {
        AP_GuiWidgetSetOverflow(widget, AP_GUI_OVERFLOW_SCROLL);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "overflow-auto"))
    {
        AP_GuiWidgetSetOverflow(widget, AP_GUI_OVERFLOW_AUTO);
        return;
    }

    /* --- Opacity / pointer events --- */
    if (AP_GuiStyleTokenStarts(token, "opacity-", &rest))
    {
        AP_GuiWidgetSetOpacity(widget, (float)atof(rest) / 100.0f);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "pointer-events-none"))
    {
        AP_GuiWidgetSetPointerEvents(widget, false);
        return;
    }
    if (AP_GuiStyleTokenIs(token, "pointer-events-auto"))
    {
        AP_GuiWidgetSetPointerEvents(widget, true);
        return;
    }

    AP_WARN("Unrecognized GUI style token: '%s'", token);
}

void AP_GuiApplyStyleString(AP_GuiWidget *widget, const char *style_string)
{
    char *copy;
    char *token;

    if (widget == NULL || style_string == NULL)
        return;

    copy = (char *)malloc(strlen(style_string) + 1);
    if (copy == NULL)
        return;

    strcpy(copy, style_string);
    token = strtok(copy, " ");

    while (token != NULL)
    {
        AP_GuiStyleApplyToken(widget, token);
        token = strtok(NULL, " ");
    }

    free(copy);
}

#endif /* AP2_NO_GUI_ADVANCED */
