#include <AP2/AP2.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>


enum DSP_StateType {
  DSP_STATE_BOOT,
  DSP_STATE_MAIN,
  DSP_STATE_SHUTDOWN,
};

typedef struct DSP_State {
  enum DSP_StateType state;

  bool initialized;
  int framerate;
  double time;

  AP_Window *window;

  bool (*is_running)(void);
  void (*update)(float dt);
  void (*render)(float dt);
  void (*shutdown)(void);
} DSP_State;

static DSP_State dsp;

/* ---------------------------------------------------------
   Running Check
--------------------------------------------------------- */
static bool dsp_is_running(void) { return AP_IsRunning(); }

/* ---------------------------------------------------------
   Debug Overlay
--------------------------------------------------------- */
static void dsp_draw_debug_info(void) {
  AP_String *fps = AP_CreateString();
  AP_StringFormat(fps, "FPS: %.2f", AP_GetFPS());
  AP_SetRenderDrawColor(255, 255, 255, 255);
  AP_DrawText(10, 10, AP_StringCStr(fps));
  AP_DestroyString(fps);
}

/* ---------------------------------------------------------
   Update
--------------------------------------------------------- */
static void dsp_update(float dt) {
  // Game logic will go here
}

/* ---------------------------------------------------------
   Render
--------------------------------------------------------- */
static void dsp_render(float dt) {
	
  AP_SetRenderDrawColor(0, 0, 0, 255);
  AP_Clear();

  AP_SetRenderDrawColor(255, 100, 150, 255);
  AP_FillRect(&(AP_RectF){50, 50, 100, 100});
  dsp_draw_debug_info();
  AP_Present();
}

/* ---------------------------------------------------------
   Shutdown
--------------------------------------------------------- */
static void dsp_shutdown(void) {
  if (dsp.window) {
    AP_DestroyWindow(dsp.window);
  }
  AP_Quit();
}

/* ---------------------------------------------------------
   Initialization
--------------------------------------------------------- */
static bool dsp_init(void) {
  dsp.state = DSP_STATE_BOOT;
  dsp.framerate = 60;
  dsp.time = 0.0;
  dsp.initialized = false;
  dsp.window = NULL;

  if (!AP_Init(AP_INIT_ALL)) {
    printf("Failed to initialize AP2: %s\n", AP_GetErrorMessage());
    return false;
  }

  AP_WindowConfig cfg = AP_WindowDefaultConfig();
  cfg.title = "Dying Sun Procession";
  cfg.width = 800;
  cfg.height = 600;

  dsp.window = AP_CreateWindowEx(&cfg);
  if (!dsp.window) {
    printf("Failed to create window: %s\n", AP_GetErrorMessage());
    return false;
  }

  dsp.initialized = true;

  dsp.is_running = dsp_is_running;
  dsp.update = dsp_update;
  dsp.render = dsp_render;
  dsp.shutdown = dsp_shutdown;

  dsp.state = DSP_STATE_MAIN;
  return true;
}

/* ---------------------------------------------------------
   Main Loop
--------------------------------------------------------- */
int main(void) {
  if (!dsp_init()) {
    return EXIT_FAILURE;
  }

  while (dsp.is_running()) {
    float dt = AP_GetDeltaTime();
    dsp.time = AP_GetTime();

    AP_PumpEvents();
    dsp.update(dt);
    dsp.render(dt);

    AP_TickFPS(dsp.framerate);
  }

  dsp.shutdown();
  return EXIT_SUCCESS;
}
