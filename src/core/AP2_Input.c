/*
 * AP2 â€” Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Input.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"
#include "AP2/AP2_Window.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <math.h>
#include <string.h>

#define AP_INPUT_KEY_MAX AP_KEY_COUNT
#define AP_INPUT_TEXT_MAX 1024
#define AP_INPUT_CLIPBOARD_MAX 4096
#define AP_INPUT_DROP_MAX 32
#define AP_INPUT_DROP_PATH 1024
#define AP_INPUT_GAMEPAD_NAME 128
#define AP_INPUT_DEFAULT_DEADZONE 0.15f /* sticks rest a little dirty */

typedef struct AP_InputGamepad {
  bool connected;
  bool buttons[AP_GAMEPAD_BUTTON_COUNT];
  bool prev_buttons[AP_GAMEPAD_BUTTON_COUNT];
  float axes[AP_GAMEPAD_AXIS_COUNT];
  char name[AP_INPUT_GAMEPAD_NAME];
} AP_InputGamepad;

typedef struct AP_InputState {
  bool initialized;

  bool keys[AP_INPUT_KEY_MAX];
  bool prev_keys[AP_INPUT_KEY_MAX];
  bool repeat[AP_INPUT_KEY_MAX];

  bool mouse[AP_MOUSE_BUTTON_COUNT];
  bool prev_mouse[AP_MOUSE_BUTTON_COUNT];

  uint32_t mods;
  uint32_t lock_mods;

  double mouse_x;
  double mouse_y;
  double frame_mouse_x;
  double frame_mouse_y;
  double mouse_delta_x;
  double mouse_delta_y;
  double wheel_x;
  double wheel_y;
  bool mouse_on_window;
  bool mouse_pos_valid;

  AP_Key last_pressed;
  AP_Key last_released;
  AP_MouseButton last_mouse_pressed;

  char text[AP_INPUT_TEXT_MAX];
  int text_len;

  char clipboard[AP_INPUT_CLIPBOARD_MAX];

  int drop_count;
  char drop_files[AP_INPUT_DROP_MAX][AP_INPUT_DROP_PATH];

  AP_InputGamepad gamepads[AP_GAMEPAD_MAX];
  float deadzone;
} AP_InputState;

static AP_InputState g_input;

/* =========================================================
 * Helpers
 * ========================================================= */

static bool AP_InputKeyValid(AP_Key key) {
  return (int)key >= 0 && (int)key < AP_INPUT_KEY_MAX;
}

static bool AP_InputMouseValid(AP_MouseButton button) {
  return (int)button >= 0 && (int)button < AP_MOUSE_BUTTON_COUNT;
}

static bool AP_InputGamepadValid(int gamepad) {
  return gamepad >= 0 && gamepad < AP_GAMEPAD_MAX;
}

static bool AP_InputWindowFocused(GLFWwindow *handle) {
  return handle != NULL && glfwGetWindowAttrib(handle, GLFW_FOCUSED) != 0;
}

static void AP_InputAppendCodepoint(unsigned int codepoint) {
  unsigned char bytes[4];
  int count = 0;
  int i;

  if (codepoint < 0x80u) {
    bytes[0] = (unsigned char)codepoint;
    count = 1;
  } else if (codepoint < 0x800u) {
    bytes[0] = (unsigned char)(0xC0u | (codepoint >> 6));
    bytes[1] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
    count = 2;
  } else if (codepoint < 0x10000u) {
    bytes[0] = (unsigned char)(0xE0u | (codepoint >> 12));
    bytes[1] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    bytes[2] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
    count = 3;
  } else if (codepoint < 0x110000u) {
    bytes[0] = (unsigned char)(0xF0u | (codepoint >> 18));
    bytes[1] = (unsigned char)(0x80u | ((codepoint >> 12) & 0x3Fu));
    bytes[2] = (unsigned char)(0x80u | ((codepoint >> 6) & 0x3Fu));
    bytes[3] = (unsigned char)(0x80u | (codepoint & 0x3Fu));
    count = 4;
  } else {
    return;
  }

  if (g_input.text_len + count >= AP_INPUT_TEXT_MAX) {
    return;
  }

  for (i = 0; i < count; ++i) {
    g_input.text[g_input.text_len++] = (char)bytes[i];
  }

  g_input.text[g_input.text_len] = '\0';
}

static float AP_InputApplyDeadzone1(float value, float deadzone) {
  float mag;

  if (deadzone <= 0.0f) {
    return value;
  }

  mag = value < 0.0f ? -value : value;
  if (mag < deadzone) {
    return 0.0f;
  }

  mag = (mag - deadzone) / (1.0f - deadzone);
  return value < 0.0f ? -mag : mag;
}

static AP_Vec2 AP_InputApplyDeadzone2(float x, float y, float deadzone) {
  AP_Vec2 result;
  float length;
  float scaled;

  result.x = x;
  result.y = y;

  if (deadzone <= 0.0f) {
    return result;
  }

  length = sqrtf(x * x + y * y);
  if (length < deadzone || length <= 0.0001f) {
    result.x = 0.0f;
    result.y = 0.0f;
    return result;
  }

  scaled = (length - deadzone) / (1.0f - deadzone);
  if (scaled > 1.0f) {
    scaled = 1.0f;
  }

  result.x = (x / length) * scaled;
  result.y = (y / length) * scaled;
  return result;
}

static float AP_InputTrigger01(float glfw_axis) {
  float value = (glfw_axis + 1.0f) * 0.5f;

  if (value < 0.0f) {
    return 0.0f;
  }

  if (value > 1.0f) {
    return 1.0f;
  }

  return value;
}

static void AP_InputReleaseAll(void) {
  memset(g_input.keys, 0, sizeof(g_input.keys));
  memset(g_input.repeat, 0, sizeof(g_input.repeat));
  memset(g_input.mouse, 0, sizeof(g_input.mouse));
  g_input.mods = 0;
}

static void AP_InputCopyGamepadName(AP_InputGamepad *pad, const char *name) {
  memset(pad->name, 0, sizeof(pad->name));
  if (name != NULL) {
    strncpy(pad->name, name, sizeof(pad->name) - 1);
  }
}

static void AP_InputPollGamepads(void) {
  int i;
  int button;

  for (i = 0; i < AP_GAMEPAD_MAX; ++i) {
    AP_InputGamepad *pad = &g_input.gamepads[i];
    GLFWgamepadstate state;
    bool present = glfwJoystickPresent(i) == GLFW_TRUE;
    bool is_gamepad = present && glfwJoystickIsGamepad(i) == GLFW_TRUE;

    memcpy(pad->prev_buttons, pad->buttons, sizeof(pad->buttons));

    if (!is_gamepad) {
      if (pad->connected) {
        AP_INFO("Gamepad disconnected: %d", i);
      }

      memset(pad, 0, sizeof(*pad));
      continue;
    }

    if (!glfwGetGamepadState(i, &state)) {
      if (pad->connected) {
        AP_INFO("Gamepad disconnected: %d", i);
      }

      memset(pad, 0, sizeof(*pad));
      continue;
    }

    if (!pad->connected) {
      AP_InputCopyGamepadName(pad, glfwGetGamepadName(i));
      AP_INFO("Gamepad connected: %d (%s)", i,
              pad->name[0] != '\0' ? pad->name : "Unknown");
    } else if (pad->name[0] == '\0') {
      AP_InputCopyGamepadName(pad, glfwGetGamepadName(i));
    }

    pad->connected = true;

    for (button = 0; button < AP_GAMEPAD_BUTTON_COUNT; ++button) {
      pad->buttons[button] = state.buttons[button] == GLFW_PRESS;
    }

    pad->axes[AP_GAMEPAD_AXIS_LEFT_X] = state.axes[GLFW_GAMEPAD_AXIS_LEFT_X];
    pad->axes[AP_GAMEPAD_AXIS_LEFT_Y] = state.axes[GLFW_GAMEPAD_AXIS_LEFT_Y];
    pad->axes[AP_GAMEPAD_AXIS_RIGHT_X] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_X];
    pad->axes[AP_GAMEPAD_AXIS_RIGHT_Y] = state.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y];
    pad->axes[AP_GAMEPAD_AXIS_LEFT_TRIGGER] =
        AP_InputTrigger01(state.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER]);
    pad->axes[AP_GAMEPAD_AXIS_RIGHT_TRIGGER] =
        AP_InputTrigger01(state.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER]);
  }
}

static void AP_InputUpdateModsFromKeys(void) {
  uint32_t mods = g_input.lock_mods;

  if (g_input.keys[AP_KEY_LEFT_SHIFT] || g_input.keys[AP_KEY_RIGHT_SHIFT]) {
    mods |= AP_MOD_SHIFT;
  }

  if (g_input.keys[AP_KEY_LEFT_CTRL] || g_input.keys[AP_KEY_RIGHT_CTRL]) {
    mods |= AP_MOD_CTRL;
  }

  if (g_input.keys[AP_KEY_LEFT_ALT] || g_input.keys[AP_KEY_RIGHT_ALT]) {
    mods |= AP_MOD_ALT;
  }

  if (g_input.keys[AP_KEY_LEFT_SUPER] || g_input.keys[AP_KEY_RIGHT_SUPER]) {
    mods |= AP_MOD_SUPER;
  }

  g_input.mods = mods;
}

static uint32_t AP_InputTranslateMods(int glfw_mods) {
  uint32_t mods = 0;

  if (glfw_mods & GLFW_MOD_SHIFT) {
    mods |= AP_MOD_SHIFT;
  }

  if (glfw_mods & GLFW_MOD_CONTROL) {
    mods |= AP_MOD_CTRL;
  }

  if (glfw_mods & GLFW_MOD_ALT) {
    mods |= AP_MOD_ALT;
  }

  if (glfw_mods & GLFW_MOD_SUPER) {
    mods |= AP_MOD_SUPER;
  }

  if (glfw_mods & GLFW_MOD_CAPS_LOCK) {
    mods |= AP_MOD_CAPS;
  }

  if (glfw_mods & GLFW_MOD_NUM_LOCK) {
    mods |= AP_MOD_NUM;
  }

  return mods;
}

/* =========================================================
 * GLFW callbacks
 * ========================================================= */

static void AP_InputKeyCallback(GLFWwindow *handle, int key, int scancode,
                                int action, int mods) {
  uint32_t translated;

  (void)scancode;

  if (!g_input.initialized || !AP_InputWindowFocused(handle)) {
    return;
  }

  if (key < 0 || key >= AP_INPUT_KEY_MAX) {
    return;
  }

  translated = AP_InputTranslateMods(mods);
  g_input.lock_mods = translated & (AP_MOD_CAPS | AP_MOD_NUM);

  if (action == GLFW_PRESS) {
    g_input.keys[key] = true;
    g_input.last_pressed = (AP_Key)key;
  } else if (action == GLFW_RELEASE) {
    g_input.keys[key] = false;
    g_input.repeat[key] = false;
    g_input.last_released = (AP_Key)key;
  } else if (action == GLFW_REPEAT) {
    g_input.keys[key] = true;
    g_input.repeat[key] = true;
  }
}

static void AP_InputMouseButtonCallback(GLFWwindow *handle, int button,
                                        int action, int mods) {
  uint32_t translated;

  if (!g_input.initialized || !AP_InputWindowFocused(handle)) {
    return;
  }

  if (button < 0 || button >= AP_MOUSE_BUTTON_COUNT) {
    return;
  }

  translated = AP_InputTranslateMods(mods);
  g_input.lock_mods = translated & (AP_MOD_CAPS | AP_MOD_NUM);

  if (action == GLFW_PRESS) {
    g_input.mouse[button] = true;
    g_input.last_mouse_pressed = (AP_MouseButton)button;
  } else if (action == GLFW_RELEASE) {
    g_input.mouse[button] = false;
  }
}

static void AP_InputScrollCallback(GLFWwindow *handle, double x, double y) {
  if (!g_input.initialized || !AP_InputWindowFocused(handle)) {
    return;
  }

  g_input.wheel_x += x;
  g_input.wheel_y += y;
}

static void AP_InputCharCallback(GLFWwindow *handle, unsigned int codepoint) {
  if (!g_input.initialized || !AP_InputWindowFocused(handle)) {
    return;
  }

  AP_InputAppendCodepoint(codepoint);
}

static void AP_InputDropCallback(GLFWwindow *handle, int count,
                                 const char **paths) {
  int i;
  int stored;

  if (!g_input.initialized || handle == NULL || paths == NULL || count <= 0) {
    return;
  }

  stored = count;
  if (stored > AP_INPUT_DROP_MAX) {
    stored = AP_INPUT_DROP_MAX;
  }

  g_input.drop_count = stored;

  for (i = 0; i < stored; ++i) {
    memset(g_input.drop_files[i], 0, AP_INPUT_DROP_PATH);
    if (paths[i] != NULL) {
      strncpy(g_input.drop_files[i], paths[i], AP_INPUT_DROP_PATH - 1);
    }
  }
}

static void AP_InputCursorEnterCallback(GLFWwindow *handle, int entered) {
  if (!g_input.initialized) {
    return;
  }

  if (AP_InputWindowFocused(handle) || entered == 0) {
    g_input.mouse_on_window = entered != 0;
  }
}

/* =========================================================
 * Internal lifecycle
 * ========================================================= */

void AP_InputInit(void) {
  if (g_input.initialized) {
    return;
  }

  memset(&g_input, 0, sizeof(g_input));
  g_input.deadzone = AP_INPUT_DEFAULT_DEADZONE;
  g_input.last_pressed = AP_KEY_UNKNOWN;
  g_input.last_released = AP_KEY_UNKNOWN;
  g_input.last_mouse_pressed = AP_MOUSE_LEFT;
  g_input.initialized = true;

  AP_INFO("Input initialized");
}

void AP_InputShutdown(void) { memset(&g_input, 0, sizeof(g_input)); }

void AP_InputAttachWindow(GLFWwindow *handle) {
  if (!g_input.initialized || handle == NULL) {
    return;
  }

  glfwSetInputMode(handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);
  glfwSetKeyCallback(handle, AP_InputKeyCallback);
  glfwSetMouseButtonCallback(handle, AP_InputMouseButtonCallback);
  glfwSetScrollCallback(handle, AP_InputScrollCallback);
  glfwSetCharCallback(handle, AP_InputCharCallback);
  glfwSetDropCallback(handle, AP_InputDropCallback);
  glfwSetCursorEnterCallback(handle, AP_InputCursorEnterCallback);

  glfwGetCursorPos(handle, &g_input.mouse_x, &g_input.mouse_y);
  g_input.frame_mouse_x = g_input.mouse_x;
  g_input.frame_mouse_y = g_input.mouse_y;
  g_input.mouse_pos_valid = true;
  g_input.mouse_on_window = glfwGetWindowAttrib(handle, GLFW_HOVERED) != 0;
}

void AP_InputDetachWindow(GLFWwindow *handle) {
  if (handle == NULL) {
    return;
  }

  glfwSetKeyCallback(handle, NULL);
  glfwSetMouseButtonCallback(handle, NULL);
  glfwSetScrollCallback(handle, NULL);
  glfwSetCharCallback(handle, NULL);
  glfwSetDropCallback(handle, NULL);
  glfwSetCursorEnterCallback(handle, NULL);
  AP_InputReleaseAll();
}

void AP_InputBeginFrame(void) {
  if (!g_input.initialized) {
    return;
  }

  memcpy(g_input.prev_keys, g_input.keys, sizeof(g_input.keys));
  memcpy(g_input.prev_mouse, g_input.mouse, sizeof(g_input.mouse));
  memset(g_input.repeat, 0, sizeof(g_input.repeat));

  g_input.frame_mouse_x = g_input.mouse_x;
  g_input.frame_mouse_y = g_input.mouse_y;
  g_input.mouse_delta_x = 0.0;
  g_input.mouse_delta_y = 0.0;
  g_input.wheel_x = 0.0;
  g_input.wheel_y = 0.0;
  g_input.text_len = 0;
  g_input.text[0] = '\0';
  g_input.drop_count = 0;
}

void AP_InputEndFrame(GLFWwindow *active_handle) {
  if (!g_input.initialized) {
    return;
  }

  if (active_handle != NULL) {
    double x = 0.0;
    double y = 0.0;

    glfwGetCursorPos(active_handle, &x, &y);

    if (g_input.mouse_pos_valid) {
      /* Cursor callback already accumulated motion during poll. Only
         fill delta here if nothing was reported (some platforms skip
         the callback when the cursor is grabbed / unchanged). */
      if (g_input.mouse_delta_x == 0.0 && g_input.mouse_delta_y == 0.0) {
        g_input.mouse_delta_x = x - g_input.frame_mouse_x;
        g_input.mouse_delta_y = y - g_input.frame_mouse_y;
      }
    }

    g_input.mouse_x = x;
    g_input.mouse_y = y;

    g_input.mouse_pos_valid = true;
    g_input.mouse_on_window =
        glfwGetWindowAttrib(active_handle, GLFW_HOVERED) != 0;
  }

  AP_InputUpdateModsFromKeys();
  AP_InputPollGamepads();
}

void AP_InputOnCursorMove(double x, double y) {
  if (!g_input.initialized) {
    return;
  }

  if (g_input.mouse_pos_valid) {
    g_input.mouse_delta_x += x - g_input.mouse_x;
    g_input.mouse_delta_y += y - g_input.mouse_y;
  }

  g_input.mouse_x = x;
  g_input.mouse_y = y;
  g_input.mouse_pos_valid = true;
}

void AP_InputOnCursorWarp(double x, double y) {
  if (!g_input.initialized) {
    return;
  }

  g_input.mouse_x = x;
  g_input.mouse_y = y;
  g_input.frame_mouse_x = x;
  g_input.frame_mouse_y = y;
  g_input.mouse_pos_valid = true;
}

void AP_InputOnFocusChanged(bool focused) {
  if (!g_input.initialized) {
    return;
  }

  if (!focused) {
    AP_InputReleaseAll();
  }
}

/* =========================================================
 * Keyboard
 * ========================================================= */

const bool *AP_GetKeyboardState(int *numkeys) {
  if (numkeys != NULL) {
    *numkeys = AP_INPUT_KEY_MAX;
  }

  return g_input.keys;
}

bool AP_IsKeyDown(AP_Key key) {
  return g_input.initialized && AP_InputKeyValid(key) && g_input.keys[key];
}

bool AP_IsKeyPressed(AP_Key key) {
  return g_input.initialized && AP_InputKeyValid(key) && g_input.keys[key] &&
         !g_input.prev_keys[key];
}

bool AP_IsKeyReleased(AP_Key key) {
  return g_input.initialized && AP_InputKeyValid(key) && !g_input.keys[key] &&
         g_input.prev_keys[key];
}

bool AP_IsKeyRepeat(AP_Key key) {
  return g_input.initialized && AP_InputKeyValid(key) && g_input.repeat[key];
}

bool AP_IsAnyKeyDown(void) {
  int i;

  if (!g_input.initialized) {
    return false;
  }

  for (i = 0; i < AP_INPUT_KEY_MAX; ++i) {
    if (g_input.keys[i]) {
      return true;
    }
  }

  return false;
}

bool AP_IsAnyKeyPressed(void) {
  int i;

  if (!g_input.initialized) {
    return false;
  }

  for (i = 0; i < AP_INPUT_KEY_MAX; ++i) {
    if (g_input.keys[i] && !g_input.prev_keys[i]) {
      return true;
    }
  }

  return false;
}

AP_Key AP_GetLastKeyPressed(void) {
  return g_input.initialized ? g_input.last_pressed : AP_KEY_UNKNOWN;
}

AP_Key AP_GetLastKeyReleased(void) {
  return g_input.initialized ? g_input.last_released : AP_KEY_UNKNOWN;
}

const char *AP_GetKeyName(AP_Key key) {
  const char *name;

  if (!AP_InputKeyValid(key)) {
    return "Unknown";
  }

  if (g_input.initialized) {
    name = glfwGetKeyName((int)key, 0);
    if (name != NULL && name[0] != '\0') {
      return name;
    }
  }

  switch (key) {
  case AP_KEY_SPACE:
    return "Space";
  case AP_KEY_ESCAPE:
    return "Escape";
  case AP_KEY_ENTER:
    return "Enter";
  case AP_KEY_TAB:
    return "Tab";
  case AP_KEY_BACKSPACE:
    return "Backspace";
  case AP_KEY_INSERT:
    return "Insert";
  case AP_KEY_DELETE:
    return "Delete";
  case AP_KEY_RIGHT:
    return "Right";
  case AP_KEY_LEFT:
    return "Left";
  case AP_KEY_DOWN:
    return "Down";
  case AP_KEY_UP:
    return "Up";
  case AP_KEY_PAGE_UP:
    return "Page Up";
  case AP_KEY_PAGE_DOWN:
    return "Page Down";
  case AP_KEY_HOME:
    return "Home";
  case AP_KEY_END:
    return "End";
  case AP_KEY_CAPS_LOCK:
    return "Caps Lock";
  case AP_KEY_SCROLL_LOCK:
    return "Scroll Lock";
  case AP_KEY_NUM_LOCK:
    return "Num Lock";
  case AP_KEY_PRINT_SCREEN:
    return "Print Screen";
  case AP_KEY_PAUSE:
    return "Pause";
  case AP_KEY_F1:
    return "F1";
  case AP_KEY_F2:
    return "F2";
  case AP_KEY_F3:
    return "F3";
  case AP_KEY_F4:
    return "F4";
  case AP_KEY_F5:
    return "F5";
  case AP_KEY_F6:
    return "F6";
  case AP_KEY_F7:
    return "F7";
  case AP_KEY_F8:
    return "F8";
  case AP_KEY_F9:
    return "F9";
  case AP_KEY_F10:
    return "F10";
  case AP_KEY_F11:
    return "F11";
  case AP_KEY_F12:
    return "F12";
  case AP_KEY_KP_0:
    return "Keypad 0";
  case AP_KEY_KP_1:
    return "Keypad 1";
  case AP_KEY_KP_2:
    return "Keypad 2";
  case AP_KEY_KP_3:
    return "Keypad 3";
  case AP_KEY_KP_4:
    return "Keypad 4";
  case AP_KEY_KP_5:
    return "Keypad 5";
  case AP_KEY_KP_6:
    return "Keypad 6";
  case AP_KEY_KP_7:
    return "Keypad 7";
  case AP_KEY_KP_8:
    return "Keypad 8";
  case AP_KEY_KP_9:
    return "Keypad 9";
  case AP_KEY_KP_DECIMAL:
    return "Keypad Decimal";
  case AP_KEY_KP_DIVIDE:
    return "Keypad Divide";
  case AP_KEY_KP_MULTIPLY:
    return "Keypad Multiply";
  case AP_KEY_KP_SUBTRACT:
    return "Keypad Subtract";
  case AP_KEY_KP_ADD:
    return "Keypad Add";
  case AP_KEY_KP_ENTER:
    return "Keypad Enter";
  case AP_KEY_KP_EQUAL:
    return "Keypad Equal";
  case AP_KEY_LEFT_SHIFT:
    return "Left Shift";
  case AP_KEY_LEFT_CTRL:
    return "Left Ctrl";
  case AP_KEY_LEFT_ALT:
    return "Left Alt";
  case AP_KEY_LEFT_SUPER:
    return "Left Super";
  case AP_KEY_RIGHT_SHIFT:
    return "Right Shift";
  case AP_KEY_RIGHT_CTRL:
    return "Right Ctrl";
  case AP_KEY_RIGHT_ALT:
    return "Right Alt";
  case AP_KEY_RIGHT_SUPER:
    return "Right Super";
  case AP_KEY_MENU:
    return "Menu";
  default:
    return "Unknown";
  }
}

/* =========================================================
 * Modifiers
 * ========================================================= */

uint32_t AP_GetKeyMods(void) { return g_input.mods; }

bool AP_IsModDown(AP_KeyMod mod) { return (g_input.mods & (uint32_t)mod) != 0; }

bool AP_IsShiftDown(void) { return AP_IsModDown(AP_MOD_SHIFT); }

bool AP_IsCtrlDown(void) { return AP_IsModDown(AP_MOD_CTRL); }

bool AP_IsAltDown(void) { return AP_IsModDown(AP_MOD_ALT); }

bool AP_IsSuperDown(void) { return AP_IsModDown(AP_MOD_SUPER); }

bool AP_IsCapsLockOn(void) { return AP_IsModDown(AP_MOD_CAPS); }

bool AP_IsNumLockOn(void) { return AP_IsModDown(AP_MOD_NUM); }

/* =========================================================
 * Mouse
 * ========================================================= */

bool AP_IsMouseDown(AP_MouseButton button) {
  return g_input.initialized && AP_InputMouseValid(button) &&
         g_input.mouse[button];
}

bool AP_IsMousePressed(AP_MouseButton button) {
  return g_input.initialized && AP_InputMouseValid(button) &&
         g_input.mouse[button] && !g_input.prev_mouse[button];
}

bool AP_IsMouseReleased(AP_MouseButton button) {
  return g_input.initialized && AP_InputMouseValid(button) &&
         !g_input.mouse[button] && g_input.prev_mouse[button];
}

bool AP_IsAnyMouseDown(void) {
  int i;

  if (!g_input.initialized) {
    return false;
  }

  for (i = 0; i < AP_MOUSE_BUTTON_COUNT; ++i) {
    if (g_input.mouse[i]) {
      return true;
    }
  }

  return false;
}

AP_MouseButton AP_GetLastMousePressed(void) {
  return g_input.last_mouse_pressed;
}

uint32_t AP_GetMouseState(float *x, float *y) {
  uint32_t buttons = 0;

  if (x != NULL) {
    *x = (float)g_input.mouse_x;
  }

  if (y != NULL) {
    *y = (float)g_input.mouse_y;
  }

  if (g_input.mouse[AP_MOUSE_LEFT]) {
    buttons |= AP_BUTTON_LMASK;
  }

  if (g_input.mouse[AP_MOUSE_MIDDLE]) {
    buttons |= AP_BUTTON_MMASK;
  }

  if (g_input.mouse[AP_MOUSE_RIGHT]) {
    buttons |= AP_BUTTON_RMASK;
  }

  if (g_input.mouse[AP_MOUSE_X1]) {
    buttons |= AP_BUTTON_X1MASK;
  }

  if (g_input.mouse[AP_MOUSE_X2]) {
    buttons |= AP_BUTTON_X2MASK;
  }

  return buttons;
}

double AP_GetMouseX(void) { return g_input.mouse_x; }

double AP_GetMouseY(void) { return g_input.mouse_y; }

bool AP_GetMousePosition(double *x, double *y) {
  if (x == NULL || y == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Mouse position out pointers are NULL");
    return false;
  }

  *x = g_input.mouse_x;
  *y = g_input.mouse_y;
  return true;
}

double AP_GetMouseDeltaX(void) { return g_input.mouse_delta_x; }

double AP_GetMouseDeltaY(void) { return g_input.mouse_delta_y; }

bool AP_GetMouseDelta(double *x, double *y) {
  if (x == NULL || y == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Mouse delta out pointers are NULL");
    return false;
  }

  *x = g_input.mouse_delta_x;
  *y = g_input.mouse_delta_y;
  return true;
}

bool AP_IsMouseOnWindow(void) { return g_input.mouse_on_window; }

double AP_GetMouseWheelX(void) { return g_input.wheel_x; }

double AP_GetMouseWheelY(void) { return g_input.wheel_y; }

bool AP_GetMouseWheel(double *x, double *y) {
  if (x == NULL || y == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Mouse wheel out pointers are NULL");
    return false;
  }

  *x = g_input.wheel_x;
  *y = g_input.wheel_y;
  return true;
}

/* =========================================================
 * Text / clipboard / drop
 * ========================================================= */

const char *AP_GetText(void) { return g_input.text; }

bool AP_HasText(void) { return g_input.text_len > 0; }

bool AP_SetClipboardText(const char *text) {
  GLFWwindow *handle;

  if (!g_input.initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "Input is not initialized");
    return false;
  }

  if (text == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Clipboard text cannot be NULL");
    return false;
  }

  handle = AP_WindowGetGLFW(AP_GetWindow());
  glfwSetClipboardString(handle, text);
  return true;
}

const char *AP_GetClipboardText(void) {
  GLFWwindow *handle;
  const char *text;

  memset(g_input.clipboard, 0, sizeof(g_input.clipboard));

  if (!g_input.initialized) {
    return g_input.clipboard;
  }

  handle = AP_WindowGetGLFW(AP_GetWindow());
  text = glfwGetClipboardString(handle);

  if (text != NULL) {
    strncpy(g_input.clipboard, text, sizeof(g_input.clipboard) - 1);
  }

  return g_input.clipboard;
}

int AP_GetDropCount(void) { return g_input.drop_count; }

const char *AP_GetDropFile(int index) {
  if (index < 0 || index >= g_input.drop_count) {
    return NULL;
  }

  return g_input.drop_files[index];
}

/* =========================================================
 * Gamepad
 * ========================================================= */

bool AP_IsGamepadConnected(int gamepad) {
  return AP_InputGamepadValid(gamepad) && g_input.gamepads[gamepad].connected;
}

int AP_GetGamepadCount(void) {
  int i;
  int count = 0;

  for (i = 0; i < AP_GAMEPAD_MAX; ++i) {
    if (g_input.gamepads[i].connected) {
      count += 1;
    }
  }

  return count;
}

int AP_GetFirstGamepad(void) {
  int i;

  for (i = 0; i < AP_GAMEPAD_MAX; ++i) {
    if (g_input.gamepads[i].connected) {
      return i;
    }
  }

  return -1;
}

const char *AP_GetGamepadName(int gamepad) {
  if (!AP_IsGamepadConnected(gamepad)) {
    return NULL;
  }

  return g_input.gamepads[gamepad].name;
}

bool AP_IsGamepadButtonDown(int gamepad, AP_GamepadButton button) {
  if (!AP_IsGamepadConnected(gamepad) || button < 0 ||
      button >= AP_GAMEPAD_BUTTON_COUNT) {
    return false;
  }

  return g_input.gamepads[gamepad].buttons[button];
}

bool AP_IsGamepadButtonPressed(int gamepad, AP_GamepadButton button) {
  if (!AP_IsGamepadConnected(gamepad) || button < 0 ||
      button >= AP_GAMEPAD_BUTTON_COUNT) {
    return false;
  }

  return g_input.gamepads[gamepad].buttons[button] &&
         !g_input.gamepads[gamepad].prev_buttons[button];
}

bool AP_IsGamepadButtonReleased(int gamepad, AP_GamepadButton button) {
  if (!AP_IsGamepadConnected(gamepad) || button < 0 ||
      button >= AP_GAMEPAD_BUTTON_COUNT) {
    return false;
  }

  return !g_input.gamepads[gamepad].buttons[button] &&
         g_input.gamepads[gamepad].prev_buttons[button];
}

float AP_GetGamepadAxis(int gamepad, AP_GamepadAxis axis) {
  float value;

  if (!AP_IsGamepadConnected(gamepad) || axis < 0 ||
      axis >= AP_GAMEPAD_AXIS_COUNT) {
    return 0.0f;
  }

  value = g_input.gamepads[gamepad].axes[axis];

  if (axis == AP_GAMEPAD_AXIS_LEFT_TRIGGER ||
      axis == AP_GAMEPAD_AXIS_RIGHT_TRIGGER) {
    if (value < g_input.deadzone) {
      return 0.0f;
    }

    return (value - g_input.deadzone) / (1.0f - g_input.deadzone);
  }

  return AP_InputApplyDeadzone1(value, g_input.deadzone);
}

AP_Vec2 AP_GetGamepadStick(int gamepad, AP_GamepadStick stick) {
  AP_Vec2 zero = {0.0f, 0.0f};
  AP_GamepadAxis axis_x;
  AP_GamepadAxis axis_y;

  if (!AP_IsGamepadConnected(gamepad)) {
    return zero;
  }

  if (stick == AP_GAMEPAD_STICK_RIGHT) {
    axis_x = AP_GAMEPAD_AXIS_RIGHT_X;
    axis_y = AP_GAMEPAD_AXIS_RIGHT_Y;
  } else {
    axis_x = AP_GAMEPAD_AXIS_LEFT_X;
    axis_y = AP_GAMEPAD_AXIS_LEFT_Y;
  }

  return AP_InputApplyDeadzone2(g_input.gamepads[gamepad].axes[axis_x],
                                g_input.gamepads[gamepad].axes[axis_y],
                                g_input.deadzone);
}

float AP_GetGamepadTrigger(int gamepad, AP_GamepadAxis trigger) {
  if (trigger != AP_GAMEPAD_AXIS_LEFT_TRIGGER &&
      trigger != AP_GAMEPAD_AXIS_RIGHT_TRIGGER) {
    return 0.0f;
  }

  return AP_GetGamepadAxis(gamepad, trigger);
}

void AP_SetGamepadDeadzone(float deadzone) {
  if (deadzone < 0.0f) {
    deadzone = 0.0f;
  }

  if (deadzone > 0.9f) {
    deadzone = 0.9f;
  }

  g_input.deadzone = deadzone;
}

float AP_GetGamepadDeadzone(void) { return g_input.deadzone; }

void AP_ResetInput(void) {
  int i;

  memset(g_input.keys, 0, sizeof(g_input.keys));
  memset(g_input.prev_keys, 0, sizeof(g_input.prev_keys));
  memset(g_input.repeat, 0, sizeof(g_input.repeat));
  memset(g_input.mouse, 0, sizeof(g_input.mouse));
  memset(g_input.prev_mouse, 0, sizeof(g_input.prev_mouse));

  g_input.mods = 0;
  g_input.mouse_delta_x = 0.0;
  g_input.mouse_delta_y = 0.0;
  g_input.wheel_x = 0.0;
  g_input.wheel_y = 0.0;
  g_input.text_len = 0;
  g_input.text[0] = '\0';
  g_input.drop_count = 0;
  g_input.last_pressed = AP_KEY_UNKNOWN;
  g_input.last_released = AP_KEY_UNKNOWN;

  for (i = 0; i < AP_GAMEPAD_MAX; ++i) {
    memset(g_input.gamepads[i].buttons, 0, sizeof(g_input.gamepads[i].buttons));
    memset(g_input.gamepads[i].prev_buttons, 0,
           sizeof(g_input.gamepads[i].prev_buttons));
    memset(g_input.gamepads[i].axes, 0, sizeof(g_input.gamepads[i].axes));
  }
}