# Tutorial 04 — Input

Copyright (c) 2026 Jack Waechter. MIT licensed.

Input is sampled after `AP_PumpEvents()`. Each call is “this frame” relative to the last pump.

## Keyboard

```c
if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
  AP_RequestClose();
}

if (AP_IsKeyDown(AP_KEY_W)) {
  y -= speed * dt;
}

if (AP_IsKeyReleased(AP_KEY_SPACE)) {
  /* just let go */
}
```

| Query | Meaning |
|---|---|
| `AP_IsKeyDown` | Held |
| `AP_IsKeyPressed` | Went down this frame |
| `AP_IsKeyReleased` | Went up this frame |
| `AP_IsKeyRepeat` | OS key-repeat |

Modifiers: `AP_IsShiftDown()`, `AP_IsCtrlDown()`, `AP_IsAltDown()`.

Key names match GLFW (`AP_KEY_A` … `AP_KEY_Z`, `AP_KEY_LEFT`, `AP_KEY_SPACE`, function keys).

## Mouse

Coordinates are window pixels, top-left origin.

```c
float mx = (float)AP_GetMouseX();
float my = (float)AP_GetMouseY();

if (AP_IsMousePressed(AP_MOUSE_LEFT)) {
  /* click */
}

if (AP_IsMouseDown(AP_MOUSE_RIGHT)) {
  /* drag */
}
```

`AP_GetMouseDeltaX()` / `AP_GetMouseDeltaY()` are motion since last pump. Useful for look / camera.

If you are using the immediate GUI, skip game clicks when the UI wants the pointer:

```c
if (!AP_GuiWantCaptureMouse() && AP_IsMousePressed(AP_MOUSE_LEFT)) {
  /* world click */
}
```

## Gamepad

Indices are `0 .. AP_GAMEPAD_MAX-1`. Stick Y is **up = negative**, same as GLFW.

```c
int pad = AP_GetFirstGamepad();
if (pad >= 0 && AP_IsGamepadConnected(pad)) {
  AP_Vec2 stick = AP_GetGamepadStick(pad, AP_GAMEPAD_STICK_LEFT);
  x += stick.x * speed * dt;
  y += stick.y * speed * dt; /* up is negative */

  if (AP_IsGamepadButtonPressed(pad, AP_GAMEPAD_A)) {
    jump = true;
  }

  float trigger = AP_GetGamepadTrigger(pad, AP_GAMEPAD_AXIS_RIGHT_TRIGGER);
}
```

`AP_SetGamepadDeadzone(0.15f)` if the stick chatters at rest.

## Frame time

There is no `AP_GetDeltaTime`. Measure it from `AP_GetTime()` (seconds since init):

```c
static double last = 0.0;
double now = AP_GetTime();
float dt = (last == 0.0) ? 0.0f : (float)(now - last);
last = now;
if (dt > 0.05f) {
  dt = 0.05f; /* clamp after a hitch */
}
```

## Next

[Audio](05-audio.md)
