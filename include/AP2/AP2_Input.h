/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_INPUT_H
#define AP2_INPUT_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Input
 *
 * Immediate, frame-based input. Call AP_PumpEvents() once per
 * frame, then query state:
 *
 *     if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
 *         AP_RequestClose();
 *     }
 *
 *     if (AP_IsKeyDown(AP_KEY_W)) {
 *         move_forward();
 *     }
 *
 * Down     = held this frame
 * Pressed  = went down this frame
 * Released = went up this frame
 *
 * Keyboard, mouse, and gamepad queries operate on the focused
 * window. GLFW is never exposed.
 */

#define AP_GAMEPAD_MAX 16
#define AP_MOUSE_BUTTON_COUNT 8

/* =========================================================
 * Keyboard
 * ========================================================= */

typedef enum AP_Key {
  AP_KEY_UNKNOWN = -1,

  AP_KEY_SPACE = 32,
  AP_KEY_APOSTROPHE = 39,
  AP_KEY_COMMA = 44,
  AP_KEY_MINUS = 45,
  AP_KEY_PERIOD = 46,
  AP_KEY_SLASH = 47,
  AP_KEY_0 = 48,
  AP_KEY_1 = 49,
  AP_KEY_2 = 50,
  AP_KEY_3 = 51,
  AP_KEY_4 = 52,
  AP_KEY_5 = 53,
  AP_KEY_6 = 54,
  AP_KEY_7 = 55,
  AP_KEY_8 = 56,
  AP_KEY_9 = 57,
  AP_KEY_SEMICOLON = 59,
  AP_KEY_EQUAL = 61,
  AP_KEY_A = 65,
  AP_KEY_B = 66,
  AP_KEY_C = 67,
  AP_KEY_D = 68,
  AP_KEY_E = 69,
  AP_KEY_F = 70,
  AP_KEY_G = 71,
  AP_KEY_H = 72,
  AP_KEY_I = 73,
  AP_KEY_J = 74,
  AP_KEY_K = 75,
  AP_KEY_L = 76,
  AP_KEY_M = 77,
  AP_KEY_N = 78,
  AP_KEY_O = 79,
  AP_KEY_P = 80,
  AP_KEY_Q = 81,
  AP_KEY_R = 82,
  AP_KEY_S = 83,
  AP_KEY_T = 84,
  AP_KEY_U = 85,
  AP_KEY_V = 86,
  AP_KEY_W = 87,
  AP_KEY_X = 88,
  AP_KEY_Y = 89,
  AP_KEY_Z = 90,
  AP_KEY_LEFT_BRACKET = 91,
  AP_KEY_BACKSLASH = 92,
  AP_KEY_RIGHT_BRACKET = 93,
  AP_KEY_GRAVE = 96,
  AP_KEY_WORLD_1 = 161,
  AP_KEY_WORLD_2 = 162,

  AP_KEY_ESCAPE = 256,
  AP_KEY_ENTER = 257,
  AP_KEY_TAB = 258,
  AP_KEY_BACKSPACE = 259,
  AP_KEY_INSERT = 260,
  AP_KEY_DELETE = 261,
  AP_KEY_RIGHT = 262,
  AP_KEY_LEFT = 263,
  AP_KEY_DOWN = 264,
  AP_KEY_UP = 265,
  AP_KEY_PAGE_UP = 266,
  AP_KEY_PAGE_DOWN = 267,
  AP_KEY_HOME = 268,
  AP_KEY_END = 269,
  AP_KEY_CAPS_LOCK = 280,
  AP_KEY_SCROLL_LOCK = 281,
  AP_KEY_NUM_LOCK = 282,
  AP_KEY_PRINT_SCREEN = 283,
  AP_KEY_PAUSE = 284,
  AP_KEY_F1 = 290,
  AP_KEY_F2 = 291,
  AP_KEY_F3 = 292,
  AP_KEY_F4 = 293,
  AP_KEY_F5 = 294,
  AP_KEY_F6 = 295,
  AP_KEY_F7 = 296,
  AP_KEY_F8 = 297,
  AP_KEY_F9 = 298,
  AP_KEY_F10 = 299,
  AP_KEY_F11 = 300,
  AP_KEY_F12 = 301,
  AP_KEY_F13 = 302,
  AP_KEY_F14 = 303,
  AP_KEY_F15 = 304,
  AP_KEY_F16 = 305,
  AP_KEY_F17 = 306,
  AP_KEY_F18 = 307,
  AP_KEY_F19 = 308,
  AP_KEY_F20 = 309,
  AP_KEY_F21 = 310,
  AP_KEY_F22 = 311,
  AP_KEY_F23 = 312,
  AP_KEY_F24 = 313,
  AP_KEY_F25 = 314,
  AP_KEY_KP_0 = 320,
  AP_KEY_KP_1 = 321,
  AP_KEY_KP_2 = 322,
  AP_KEY_KP_3 = 323,
  AP_KEY_KP_4 = 324,
  AP_KEY_KP_5 = 325,
  AP_KEY_KP_6 = 326,
  AP_KEY_KP_7 = 327,
  AP_KEY_KP_8 = 328,
  AP_KEY_KP_9 = 329,
  AP_KEY_KP_DECIMAL = 330,
  AP_KEY_KP_DIVIDE = 331,
  AP_KEY_KP_MULTIPLY = 332,
  AP_KEY_KP_SUBTRACT = 333,
  AP_KEY_KP_ADD = 334,
  AP_KEY_KP_ENTER = 335,
  AP_KEY_KP_EQUAL = 336,
  AP_KEY_LEFT_SHIFT = 340,
  AP_KEY_LEFT_CTRL = 341,
  AP_KEY_LEFT_ALT = 342,
  AP_KEY_LEFT_SUPER = 343,
  AP_KEY_RIGHT_SHIFT = 344,
  AP_KEY_RIGHT_CTRL = 345,
  AP_KEY_RIGHT_ALT = 346,
  AP_KEY_RIGHT_SUPER = 347,
  AP_KEY_MENU = 348,

  AP_KEY_COUNT = 512
} AP_Key;

#define AP_KEY_RETURN AP_KEY_ENTER
#define AP_KEY_ESC AP_KEY_ESCAPE
#define AP_KEY_LEFT_CONTROL AP_KEY_LEFT_CTRL
#define AP_KEY_RIGHT_CONTROL AP_KEY_RIGHT_CTRL

/* =========================================================
 * Modifiers
 * ========================================================= */

typedef enum AP_KeyMod {
  AP_MOD_NONE = 0,
  AP_MOD_SHIFT = 1 << 0,
  AP_MOD_CTRL = 1 << 1,
  AP_MOD_ALT = 1 << 2,
  AP_MOD_SUPER = 1 << 3,
  AP_MOD_CAPS = 1 << 4,
  AP_MOD_NUM = 1 << 5
} AP_KeyMod;

/* =========================================================
 * Mouse
 * ========================================================= */

typedef enum AP_MouseButton {
  AP_MOUSE_LEFT = 0,
  AP_MOUSE_RIGHT = 1,
  AP_MOUSE_MIDDLE = 2,
  AP_MOUSE_X1 = 3,
  AP_MOUSE_X2 = 4,
  AP_MOUSE_4 = 3,
  AP_MOUSE_5 = 4,
  AP_MOUSE_6 = 5,
  AP_MOUSE_7 = 6,
  AP_MOUSE_8 = 7
} AP_MouseButton;

#define AP_BUTTON_LEFT 1
#define AP_BUTTON_MIDDLE 2
#define AP_BUTTON_RIGHT 3
#define AP_BUTTON_X1 4
#define AP_BUTTON_X2 5
#define AP_BUTTON_MASK(X) (1u << ((X) - 1))
#define AP_BUTTON_LMASK AP_BUTTON_MASK(AP_BUTTON_LEFT)
#define AP_BUTTON_MMASK AP_BUTTON_MASK(AP_BUTTON_MIDDLE)
#define AP_BUTTON_RMASK AP_BUTTON_MASK(AP_BUTTON_RIGHT)
#define AP_BUTTON_X1MASK AP_BUTTON_MASK(AP_BUTTON_X1)
#define AP_BUTTON_X2MASK AP_BUTTON_MASK(AP_BUTTON_X2)

/* =========================================================
 * Gamepad
 * ========================================================= */

typedef enum AP_GamepadButton {
  AP_GAMEPAD_A = 0,
  AP_GAMEPAD_B,
  AP_GAMEPAD_X,
  AP_GAMEPAD_Y,
  AP_GAMEPAD_LEFT_BUMPER,
  AP_GAMEPAD_RIGHT_BUMPER,
  AP_GAMEPAD_BACK,
  AP_GAMEPAD_START,
  AP_GAMEPAD_GUIDE,
  AP_GAMEPAD_LEFT_THUMB,
  AP_GAMEPAD_RIGHT_THUMB,
  AP_GAMEPAD_DPAD_UP,
  AP_GAMEPAD_DPAD_RIGHT,
  AP_GAMEPAD_DPAD_DOWN,
  AP_GAMEPAD_DPAD_LEFT,
  AP_GAMEPAD_BUTTON_COUNT
} AP_GamepadButton;

#define AP_GAMEPAD_SOUTH AP_GAMEPAD_A
#define AP_GAMEPAD_EAST AP_GAMEPAD_B
#define AP_GAMEPAD_WEST AP_GAMEPAD_X
#define AP_GAMEPAD_NORTH AP_GAMEPAD_Y
#define AP_GAMEPAD_CROSS AP_GAMEPAD_A
#define AP_GAMEPAD_CIRCLE AP_GAMEPAD_B
#define AP_GAMEPAD_SQUARE AP_GAMEPAD_X
#define AP_GAMEPAD_TRIANGLE AP_GAMEPAD_Y
#define AP_GAMEPAD_SELECT AP_GAMEPAD_BACK
#define AP_GAMEPAD_LEFT_SHOULDER AP_GAMEPAD_LEFT_BUMPER
#define AP_GAMEPAD_RIGHT_SHOULDER AP_GAMEPAD_RIGHT_BUMPER

typedef enum AP_GamepadAxis {
  AP_GAMEPAD_AXIS_LEFT_X = 0,
  AP_GAMEPAD_AXIS_LEFT_Y,
  AP_GAMEPAD_AXIS_RIGHT_X,
  AP_GAMEPAD_AXIS_RIGHT_Y,
  AP_GAMEPAD_AXIS_LEFT_TRIGGER,
  AP_GAMEPAD_AXIS_RIGHT_TRIGGER,
  AP_GAMEPAD_AXIS_COUNT
} AP_GamepadAxis;

typedef enum AP_GamepadStick {
  AP_GAMEPAD_STICK_LEFT = 0,
  AP_GAMEPAD_STICK_RIGHT
} AP_GamepadStick;

/* =========================================================
 * Keyboard queries
 *
 * AP_GetKeyboardState returns an array indexed by AP_Key.
 * ========================================================= */

const bool *AP_GetKeyboardState(int *numkeys);

bool AP_IsKeyDown(AP_Key key);

bool AP_IsKeyPressed(AP_Key key);

bool AP_IsKeyReleased(AP_Key key);

bool AP_IsKeyRepeat(AP_Key key);

bool AP_IsAnyKeyDown(void);

bool AP_IsAnyKeyPressed(void);

AP_Key AP_GetLastKeyPressed(void);

AP_Key AP_GetLastKeyReleased(void);

const char *AP_GetKeyName(AP_Key key);

/* =========================================================
 * Modifiers
 * ========================================================= */

uint32_t AP_GetKeyMods(void);

bool AP_IsModDown(AP_KeyMod mod);

bool AP_IsShiftDown(void);

bool AP_IsCtrlDown(void);

bool AP_IsAltDown(void);

bool AP_IsSuperDown(void);

bool AP_IsCapsLockOn(void);

bool AP_IsNumLockOn(void);

/* =========================================================
 * Mouse buttons
 * ========================================================= */

bool AP_IsMouseDown(AP_MouseButton button);

bool AP_IsMousePressed(AP_MouseButton button);

bool AP_IsMouseReleased(AP_MouseButton button);

bool AP_IsAnyMouseDown(void);

AP_MouseButton AP_GetLastMousePressed(void);

/* =========================================================
 * Mouse position / motion
 *
 * Coordinates are in window pixels, origin at the top-left.
 * AP_GetMouseState writes the cursor position and returns a
 * mask of AP_BUTTON_* bits, matching SDL_GetMouseState.
 * ========================================================= */

uint32_t AP_GetMouseState(float *x, float *y);

double AP_GetMouseX(void);

double AP_GetMouseY(void);

bool AP_GetMousePosition(double *x, double *y);

double AP_GetMouseDeltaX(void);

double AP_GetMouseDeltaY(void);

bool AP_GetMouseDelta(double *x, double *y);

bool AP_IsMouseOnWindow(void);

/* =========================================================
 * Mouse wheel
 *
 * Typical vertical notch is +1.0 or -1.0.
 * ========================================================= */

double AP_GetMouseWheelX(void);

double AP_GetMouseWheelY(void);

bool AP_GetMouseWheel(double *x, double *y);

/* =========================================================
 * Text
 *
 * UTF-8 characters generated this frame. Empty string when
 * nothing was typed.
 * ========================================================= */

const char *AP_GetText(void);

bool AP_HasText(void);

/* =========================================================
 * Clipboard
 * ========================================================= */

bool AP_SetClipboardText(const char *text);

const char *AP_GetClipboardText(void);

/* =========================================================
 * File drop
 *
 * Paths dropped onto the window this frame.
 * ========================================================= */

int AP_GetDropCount(void);

const char *AP_GetDropFile(int index);

/* =========================================================
 * Gamepad
 *
 * Indices are 0 .. AP_GAMEPAD_MAX-1.
 * Stick axes are -1 .. 1. Up is negative Y.
 * Triggers are 0 .. 1 after remapping.
 * ========================================================= */

bool AP_IsGamepadConnected(int gamepad);

int AP_GetGamepadCount(void);

int AP_GetFirstGamepad(void);

const char *AP_GetGamepadName(int gamepad);

bool AP_IsGamepadButtonDown(int gamepad, AP_GamepadButton button);

bool AP_IsGamepadButtonPressed(int gamepad, AP_GamepadButton button);

bool AP_IsGamepadButtonReleased(int gamepad, AP_GamepadButton button);

float AP_GetGamepadAxis(int gamepad, AP_GamepadAxis axis);

AP_Vec2 AP_GetGamepadStick(int gamepad, AP_GamepadStick stick);

float AP_GetGamepadTrigger(int gamepad, AP_GamepadAxis trigger);

void AP_SetGamepadDeadzone(float deadzone);

float AP_GetGamepadDeadzone(void);

/* =========================================================
 * Reset
 *
 * Clears held keys/buttons and frame transients. Gamepad
 * connections are preserved.
 * ========================================================= */

void AP_ResetInput(void);

#ifdef __cplusplus
}
#endif

#endif /* AP2_INPUT_H */
