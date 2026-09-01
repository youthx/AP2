/*
 * Example: GUI App
 *
 * A small desktop-style app built on the retained-mode AP2_GuiAdvanced API:
 * a window, box layouts, themed widgets, CSS-like style strings, signals,
 * and a graphics canvas element that renders live 2D content.
 */

#include <AP2/AP2.h>

#include <math.h>
#include <stdio.h>

#include "gui_app.h"

typedef struct AppState AppState;

typedef struct PopupSlot
{
    AP_Window *window;
    AP_GuiWidget *panel; /* the popup's own GUI root */
} PopupSlot;

typedef struct PopupCloseRef
{
    AppState *app;
    int slot;
} PopupCloseRef;

struct AppState
{
    AP_Window *main_window;
    PopupSlot popups[5];
    PopupCloseRef close_refs[5];
    AP_GuiWidget *click_button;
    AP_GuiWidget *counter_label;
    AP_GuiWidget *volume_label;
    AP_GuiWidget *volume_slider;
    AP_GuiWidget *progress_bar;
    AP_GuiWidget *animate_checkbox;
    AP_GuiWidget *font_label;
    AP_GuiTheme *theme;
    AP_Font *loaded_font;
    int font_index;
    int click_count;
};

static bool OnClickButtonClicked(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;
    char buf[64];

    app->click_count++;
    snprintf(buf, sizeof(buf), "Click Me (%d)", app->click_count);
    AP_GuiWidgetSetText(app->click_button, buf);

    return true;
}

static bool OnResetClicked(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;

    app->click_count = 0;
    AP_GuiWidgetSetText(app->click_button, "Click Me (0)");
    AP_GuiWidgetSetValue(app->volume_slider, 50.0f);

    return true;
}

/* ------------------------------------------------------------------
 * Popup windows
 *
 * A popup is a real secondary window that shares the main window's GL
 * context and renderer. Each popup owns its own widget tree and is
 * laid out / updated / rendered every frame from the main loop, exactly
 * like the main window — so it can do anything the main window can.
 *
 * Two styles:
 *   - decorated:  normal OS window with a draggable title bar (and an
 *                 optional taskbar entry)
 *   - borderless: floating inlay popup (tooltip / context-menu style)
 * ------------------------------------------------------------------ */

static void ClosePopupSlot(AppState *app, int slot)
{
    PopupSlot *p = &app->popups[slot];

    if (p->window == NULL)
    {
        return;
    }

    if (p->panel != NULL)
    {
        AP_GuiWidgetDestroy(p->panel);
        p->panel = NULL;
    }
    AP_DestroyWindow(p->window);
    p->window = NULL;
}

static bool OnPopupCloseClicked(AP_GuiEvent *event)
{
    PopupCloseRef *ref = (PopupCloseRef *)event->user_data;

    ClosePopupSlot(ref->app, ref->slot);
    return true;
}

/* Builds the popup's GUI into its slot. Each popup gets a different
 * little UI to show that popups are full-featured windows. */
static void BuildPopupGUI(AppState *app, int slot, const char *heading)
{
    PopupSlot *p = &app->popups[slot];
    AP_GuiWidget *root;
    AP_GuiWidget *label;
    AP_GuiWidget *close_button;
    char title[96];

    p->panel = AP_GuiPanelNew();
    AP_GuiWidgetSetGeometry(p->panel, 0.0f, 0.0f,
                            (float)AP_GetWindowWidth(),
                            (float)AP_GetWindowHeight());

    root = AP_GuiVBoxNew();
    AP_GuiWidgetSetPaddingAll(root, 12.0f);
    AP_GuiWidgetAddChild(p->panel, root);

    snprintf(title, sizeof(title), "%s\nDrag me by the title bar.", heading);
    label = AP_GuiLabelNew(title);
    AP_GuiWidgetSetSize(label, 260.0f, 60.0f);
    AP_GuiWidgetAddChild(root, label);

    close_button = AP_GuiButtonNew("Close");
    AP_GuiWidgetSetSize(close_button, 120.0f, 30.0f);
    app->close_refs[slot].app = app;
    app->close_refs[slot].slot = slot;
    AP_GuiWidgetConnect(close_button, "clicked", OnPopupCloseClicked,
                        &app->close_refs[slot]);
    AP_GuiWidgetAddChild(root, close_button);
}

static bool OpenWindowPopup(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;
    AP_PopupConfig cfg = AP_PopupDefaultConfig();
    int slot;

    for (slot = 0; slot < 5; ++slot)
    {
        if (app->popups[slot].window == NULL)
        {
            break;
        }
    }
    if (slot >= 5)
    {
        return true; /* too many popups open */
    }

    /* Decorated: a normal OS window with a title bar you can drag, and a
     * taskbar entry like any other window. */
    cfg.decorated = true;
    cfg.title = "Tool window";
    cfg.show_in_taskbar = true;
    cfg.offset_x = 60 + slot * 40;
    cfg.offset_y = 120 + slot * 30;
    cfg.width = 300;
    cfg.height = 190;
    cfg.close_on_focus_loss = false;

    app->popups[slot].window = AP_CreatePopupWindow(&cfg);
    if (app->popups[slot].window == NULL)
    {
        return true;
    }

    BuildPopupGUI(app, slot, "Decorated popup");
    AP_SetActiveWindow(app->main_window);
    return true;
}

static bool OpenInlayPopup(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;
    AP_PopupConfig cfg = AP_PopupDefaultConfig();
    int slot;

    for (slot = 0; slot < 5; ++slot)
    {
        if (app->popups[slot].window == NULL)
        {
            break;
        }
    }
    if (slot >= 5)
    {
        return true;
    }

    /* Borderless inlay: floating, no decorations, no taskbar entry.
     * dismisses itself when it loses focus, like a tooltip. */
    cfg.decorated = false;
    cfg.close_on_focus_loss = true;
    cfg.offset_x = 640;
    cfg.offset_y = 200;
    cfg.width = 260;
    cfg.height = 120;

    app->popups[slot].window = AP_CreatePopupWindow(&cfg);
    if (app->popups[slot].window == NULL)
    {
        return true;
    }

    BuildPopupGUI(app, slot, "Borderless inlay popup\n(click away to dismiss)");
    AP_SetActiveWindow(app->main_window);
    return true;
}

/* Canvas render callback reused inside a popup window. */
static void PaintPopupCanvas(AP_GuiWidget *canvas, AP_FRect bounds,
                             void *user_data)
{
    float t = (float)AP_GetTime();
    float cx = bounds.w * 0.5f;
    float cy = bounds.h * 0.5f;

    (void)user_data;

    AP_GuiCanvasClear(canvas, AP_C4(0.06f, 0.09f, 0.14f, 1.0f));
    AP_GuiCanvasDrawCircle(canvas,
                           cx + cosf(t * 2.0f) * bounds.w * 0.3f,
                           cy + sinf(t * 3.0f) * bounds.h * 0.3f, 16.0f,
                           AP_C4(0.95f, 0.55f, 0.25f, 1.0f), true);
    AP_GuiCanvasDrawText(canvas, "live canvas in a popup", 8.0f, 8.0f,
                         AP_C4(0.8f, 0.85f, 0.9f, 1.0f));
}

static bool OpenCanvasPopup(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;
    AP_PopupConfig cfg = AP_PopupDefaultConfig();
    PopupSlot *p;
    AP_GuiWidget *canvas;
    int slot;

    for (slot = 0; slot < 5; ++slot)
    {
        if (app->popups[slot].window == NULL)
        {
            break;
        }
    }
    if (slot >= 5)
    {
        return true;
    }
    p = &app->popups[slot];

    cfg.decorated = true;
    cfg.title = "Canvas popup";
    cfg.show_in_taskbar = false; /* tool window: no taskbar entry */
    cfg.offset_x = 380;
    cfg.offset_y = 320;
    cfg.width = 340;
    cfg.height = 240;
    cfg.close_on_focus_loss = false;

    p->window = AP_CreatePopupWindow(&cfg);
    if (p->window == NULL)
    {
        return true;
    }

    BuildPopupGUI(app, slot, "Canvas in a popup");

    canvas = AP_GuiCanvasNew(300.0f, 140.0f);
    AP_GuiWidgetSetSize(canvas, 300.0f, 140.0f);
    AP_GuiCanvasSetRenderMode(canvas, AP_GUI_CANVAS_2D);
    AP_GuiCanvasSetRenderCallback(canvas, PaintPopupCanvas, app);
    AP_GuiWidgetAddChild(
        AP_GuiWidgetChildAt(p->panel, 0), canvas);

    AP_SetActiveWindow(app->main_window);
    return true;
}

/* Cycles through every system font found on this machine, applying each
   pick to the shared theme so the whole UI re-renders with the new font. */
static bool OnNextFontClicked(AP_GuiEvent *event)
{
    AppState *app = (AppState *)event->user_data;
    int count = AP_GetSystemFontCount();
    const char *name;
    AP_Font *font;
    char buf[128];

    if (count <= 0)
    {
        AP_GuiWidgetSetText(app->font_label, "Font: (no system fonts found)");
        return true;
    }

    app->font_index = (app->font_index + 1) % count;
    name = AP_GetSystemFontName(app->font_index);

    font = AP_LoadSystemFont(name, 16.0f);
    if (font == NULL)
    {
        return true;
    }

    if (app->loaded_font != NULL)
    {
        AP_DestroyFont(app->loaded_font);
    }
    app->loaded_font = font;
    app->theme->font_default = font;

    snprintf(buf, sizeof(buf), "Font: %s (%d/%d)", name, app->font_index + 1,
             count);
    AP_GuiWidgetSetText(app->font_label, buf);

    return true;
}

/* Canvas render callback: draws a bouncing ball using canvas-local coordinates
 */
static void PaintCanvas(AP_GuiWidget *canvas, AP_FRect bounds,
                        void *user_data)
{
    AppState *app = (AppState *)user_data;
    float t = (float)AP_GetTime();
    float bx, by;

    AP_GuiCanvasClear(canvas, AP_C4(0.06f, 0.07f, 0.10f, 1.0f));

    AP_GuiCanvasDrawRect(canvas, (AP_FRect){0.0f, 0.0f, bounds.w, bounds.h},
                         AP_C4(0.2f, 0.2f, 0.25f, 1.0f), false);

    if (AP_GuiWidgetChecked(app->animate_checkbox))
    {
        bx = bounds.w * 0.5f + (bounds.w * 0.35f) * cosf(t * 1.5f);
        by = bounds.h * 0.5f + (bounds.h * 0.35f) * sinf(t * 2.0f);
    }
    else
    {
        bx = bounds.w * 0.5f;
        by = bounds.h * 0.5f;
    }
    AP_GuiCanvasDrawCircle(canvas, bx, by, 14.0f,
                           AP_C4(0.95f, 0.55f, 0.25f, 1.0f), true);
    AP_GuiCanvasDrawText(canvas, "2D Canvas", 8.0f, 8.0f,
                         AP_C4(0.8f, 0.8f, 0.85f, 1.0f));
}

int Example_GuiApp(void)
{
    AppState app = {0};
    AP_GuiWidget *root_panel;
    AP_GuiWidget *root;
    AP_GuiWidget *title;
    AP_GuiWidget *row;
    AP_GuiWidget *reset_button;
    AP_GuiWidget *popup_button;
    AP_GuiWidget *font_row;
    AP_GuiWidget *font_button;
    AP_GuiWidget *checkbox;
    AP_GuiWidget *separator;
    AP_GuiWidget *canvas;

    AP_Init(AP_INIT_VIDEO);
    AP_CreateWindow("AP2 - GUI App", 960, 700,
                    AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE | AP_WINDOW_MSAA);
    app.main_window = AP_GetWindow();

    app.theme = AP_GuiThemeDarkNew();
    app.font_index = -1;
    AP_GuiSetGlobalTheme(app.theme);

    /* Root panel fills the GLFW window directly — no inner decorative window */
    root_panel = AP_GuiPanelNew();
    AP_GuiWidgetSetGeometry(root_panel, 0.0f, 0.0f, (float)AP_GetWindowWidth(),
                            (float)AP_GetWindowHeight());

    root = AP_GuiVBoxNew();
    AP_GuiWidgetSetPaddingAll(root, 16.0f);
    AP_GuiWidgetAddChild(root_panel, root);

    title = AP_GuiLabelNew("AP2 Advanced GUI Demo");
    AP_GuiWidgetSetSize(title, 400.0f, 24.0f);
    AP_GuiWidgetAddChild(root, title);

    /* Button row: click counter + reset, styled with CSS-like utility classes */
    row = AP_GuiHBoxNew();
    AP_GuiWidgetSetSize(row, 400.0f, 40.0f);
    AP_GuiWidgetAddChild(root, row);

    app.click_button = AP_GuiButtonNew("Click Me (0)");
    AP_GuiWidgetSetSize(app.click_button, 180.0f, 36.0f);
    AP_GuiWidgetSetStyle(app.click_button, "rounded-lg shadow");
    AP_GuiWidgetConnect(app.click_button, "clicked", OnClickButtonClicked, &app);
    AP_GuiWidgetAddChild(row, app.click_button);

    reset_button = AP_GuiButtonNew("Reset");
    AP_GuiWidgetSetSize(reset_button, 120.0f, 36.0f);
    AP_GuiWidgetSetBorderRadiusCorners(reset_button, 4.0f, 4.0f, 4.0f, 4.0f);
    AP_GuiWidgetConnect(reset_button, "clicked", OnResetClicked, &app);
    AP_GuiWidgetAddChild(row, reset_button);

    popup_button = AP_GuiButtonNew("Window popup");
    AP_GuiWidgetSetSize(popup_button, 130.0f, 36.0f);
    AP_GuiWidgetConnect(popup_button, "clicked", OpenWindowPopup, &app);
    AP_GuiWidgetAddChild(row, popup_button);

    popup_button = AP_GuiButtonNew("Inlay popup");
    AP_GuiWidgetSetSize(popup_button, 120.0f, 36.0f);
    AP_GuiWidgetConnect(popup_button, "clicked", OpenInlayPopup, &app);
    AP_GuiWidgetAddChild(row, popup_button);

    popup_button = AP_GuiButtonNew("Canvas popup");
    AP_GuiWidgetSetSize(popup_button, 130.0f, 36.0f);
    AP_GuiWidgetConnect(popup_button, "clicked", OpenCanvasPopup, &app);
    AP_GuiWidgetAddChild(row, popup_button);

    /* System font picker: cycles AP_GetSystemFontCount() fonts found on this
     * machine */
    font_row = AP_GuiHBoxNew();
    AP_GuiWidgetSetSize(font_row, 400.0f, 36.0f);
    AP_GuiWidgetAddChild(root, font_row);

    font_button = AP_GuiButtonNew("Next Font");
    AP_GuiWidgetSetSize(font_button, 140.0f, 32.0f);
    AP_GuiWidgetConnect(font_button, "clicked", OnNextFontClicked, &app);
    AP_GuiWidgetAddChild(font_row, font_button);

    app.font_label = AP_GuiLabelNew("Font: (default)");
    AP_GuiWidgetSetSize(app.font_label, 260.0f, 32.0f);
    AP_GuiWidgetAddChild(font_row, app.font_label);

    /* Checkbox toggles the canvas animation below */
    checkbox = AP_GuiCheckboxNew("Animate canvas");
    AP_GuiWidgetSetSize(checkbox, 220.0f, 28.0f);
    AP_GuiWidgetSetChecked(checkbox, true);
    app.animate_checkbox = checkbox;
    AP_GuiWidgetAddChild(root, checkbox);

    separator = AP_GuiSeparatorNew(false);
    AP_GuiWidgetSetSize(separator, 400.0f, 2.0f);
    AP_GuiWidgetAddChild(root, separator);

    /* Volume label + slider + progress bar, kept in sync each frame */
    app.volume_label = AP_GuiLabelNew("Volume: 50");
    AP_GuiWidgetSetSize(app.volume_label, 300.0f, 20.0f);
    AP_GuiWidgetAddChild(root, app.volume_label);

    app.volume_slider = AP_GuiSliderNew(0.0f, 100.0f, 1.0f);
    AP_GuiWidgetSetSize(app.volume_slider, 300.0f, 24.0f);
    AP_GuiWidgetSetValue(app.volume_slider, 50.0f);
    AP_GuiWidgetAddChild(root, app.volume_slider);

    app.progress_bar = AP_GuiProgressBarNew();
    AP_GuiWidgetSetSize(app.progress_bar, 300.0f, 18.0f);
    AP_GuiWidgetAddChild(root, app.progress_bar);

    /* Graphics canvas: a widget that renders live 2D content */
    canvas = AP_GuiCanvasNew(400.0f, 180.0f);
    AP_GuiWidgetSetSize(canvas, 400.0f, 180.0f);
    AP_GuiWidgetSetSizePolicy(canvas, AP_GUI_SIZE_EXPANDING, AP_GUI_SIZE_FIXED);
    AP_GuiCanvasSetRenderMode(canvas, AP_GUI_CANVAS_2D);
    AP_GuiCanvasSetRenderCallback(canvas, PaintCanvas, &app);
    AP_GuiWidgetAddChild(root, canvas);

    while (AP_IsRunning())
    {
        double dt;
        char buf[64];

        AP_PumpEvents();

        if (AP_IsKeyPressed(AP_KEY_ESCAPE))
        {
            AP_RequestClose();
        }

        dt = AP_Tick();

        /* Track the real GLFW window size so the GUI always fills it exactly */
        AP_GuiWidgetSetGeometry(root_panel, 0.0f, 0.0f, (float)AP_GetWindowWidth(),
                                (float)AP_GetWindowHeight());

        /* Keep dependent widgets in sync with the slider each frame */
        snprintf(buf, sizeof(buf), "Volume: %.0f",
                 AP_GuiWidgetValue(app.volume_slider));
        AP_GuiWidgetSetText(app.volume_label, buf);
        AP_GuiWidgetSetValue(app.progress_bar,
                             AP_GuiWidgetValue(app.volume_slider) / 100.0f);

        AP_GuiWidgetRecalculateLayout(root_panel);
        AP_GuiWidgetUpdate(root_panel, (float)dt);

        AP_SetDrawColor(0.04f, 0.05f, 0.07f, 1.0f);
        AP_Clear();

        AP_GuiWidgetRender(root_panel);

        AP_Present();

        /* Render every open popup each frame, same pipeline as the main
         * window: layout, update, clear, draw, present. */
        for (int slot = 0; slot < 5; ++slot)
        {
            PopupSlot *p = &app.popups[slot];

            if (p->window == NULL)
            {
                continue;
            }

            if (!AP_WindowIsValid(p->window))
            {
                /* Closed via focus loss or the OS close button. */
                if (p->panel != NULL)
                {
                    AP_GuiWidgetDestroy(p->panel);
                    p->panel = NULL;
                }
                p->window = NULL;
                continue;
            }

            AP_SetActiveWindow(p->window);

            AP_GuiWidgetSetGeometry(p->panel, 0.0f, 0.0f,
                                    (float)AP_GetWindowWidth(),
                                    (float)AP_GetWindowHeight());
            AP_GuiWidgetRecalculateLayout(p->panel);
            AP_GuiWidgetUpdate(p->panel, (float)dt);

            AP_SetDrawColor(0.10f, 0.12f, 0.18f, 1.0f);
            AP_Clear();
            AP_GuiWidgetRender(p->panel);
            AP_Present();
        }

        AP_SetActiveWindow(app.main_window);
    }

    AP_GuiWidgetDestroy(root_panel);
    if (app.loaded_font != NULL)
    {
        AP_DestroyFont(app.loaded_font);
    }
    AP_GuiThemeDestroy(app.theme);
    AP_DestroyWindow(NULL);
    AP_Quit();
    return 0;
}
