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

typedef struct AppState
{
    AP_GuiWidget *click_button;
    AP_GuiWidget *counter_label;
    AP_GuiWidget *volume_label;
    AP_GuiWidget *volume_slider;
    AP_GuiWidget *progress_bar;
    AP_GuiWidget *animate_checkbox;
    int click_count;
} AppState;

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

/* Canvas render callback: draws a bouncing ball using canvas-local coordinates */
static void PaintCanvas(AP_GuiWidget *canvas, AP_FRect bounds, void *user_data)
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

    AP_GuiCanvasDrawCircle(canvas, bx, by, 14.0f, AP_C4(0.95f, 0.55f, 0.25f, 1.0f), true);
    AP_GuiCanvasDrawText(canvas, "2D Canvas", 8.0f, 8.0f, AP_C4(0.8f, 0.8f, 0.85f, 1.0f));
}

int Example_GuiApp(void)
{
    AppState app = {0};
    AP_GuiWidget *window;
    AP_GuiWidget *root;
    AP_GuiWidget *title;
    AP_GuiWidget *row;
    AP_GuiWidget *reset_button;
    AP_GuiWidget *checkbox;
    AP_GuiWidget *separator;
    AP_GuiWidget *canvas;

    AP_Init(AP_INIT_VIDEO);
    AP_CreateWindow("AP2 - GUI App", 960, 700,
                    AP_WINDOW_CENTERED | AP_WINDOW_RESIZABLE | AP_WINDOW_MSAA);

    AP_GuiSetGlobalTheme(AP_GuiThemeDarkNew());

    /* Window + root vertical layout */
    window = AP_GuiWindowNew("Widgets Showcase", 900, 660);
    AP_GuiWidgetSetGeometry(window, 30.0f, 20.0f, 900.0f, 660.0f);

    root = AP_GuiVBoxNew();
    AP_GuiWidgetSetPaddingAll(root, 16.0f);
    AP_GuiWidgetAddChild(window, root);

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

        /* Keep dependent widgets in sync with the slider each frame */
        snprintf(buf, sizeof(buf), "Volume: %.0f", AP_GuiWidgetValue(app.volume_slider));
        AP_GuiWidgetSetText(app.volume_label, buf);
        AP_GuiWidgetSetValue(app.progress_bar, AP_GuiWidgetValue(app.volume_slider) / 100.0f);

        AP_GuiWidgetRecalculateLayout(window);
        AP_GuiWidgetUpdate(window, (float)dt);

        AP_SetDrawColor(0.04f, 0.05f, 0.07f, 1.0f);
        AP_Clear();

        AP_GuiWidgetRender(window);

        AP_Present();
    }

    AP_GuiWidgetDestroy(window);
    AP_DestroyWindow(NULL);
    AP_Quit();
    return 0;
}
