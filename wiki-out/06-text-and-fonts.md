# Tutorial 06 — Text and fonts

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

Text is 2D: window pixels, top-left origin. After a window exists, an 8×8 Latin font is always available.

## Built-in font

```c
AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
AP_DrawText(32.0f, 32.0f, "Score 1200");
```

`AP_DrawText` uses the current font and the current draw color.

## TrueType

```c
AP_Font *font = AP_LoadFont("Inter.ttf", 18.0f);
if (!font) {
  AP_ERROR("font: %s", AP_GetErrorMessage());
}
AP_SetFont(font);

AP_DrawText(32.0f, 64.0f, "Custom typeface");
```

`AP_CreateFontFromMemory` is the same from a buffer. Destroy custom fonts with `AP_DestroyFont`. Do not destroy the default font.

## Color and size per call

```c
AP_DrawTextEx(font, 32.0f, 96.0f, "Hi", AP_C4(1.0f, 0.8f, 0.2f, 1.0f), 24.0f);
```

Size `0` means the baked pixel size from `AP_LoadFont`.

## Alignment

```c
AP_FRect box = {0.0f, 0.0f, 1280.0f, 48.0f};
AP_DrawTextAligned(font, &box, "Paused", AP_C4(1, 1, 1, 1), 0.0f,
                   AP_TEXT_ALIGN_CENTER);
```

## Measure

```c
AP_FPoint size = AP_MeasureText("Hello");
AP_FPoint sized = AP_MeasureTextEx(font, "Hello", 24.0f);
```

Use this to center text yourself or to size HUD panels.

## HUD sketch

```c
#include <stdio.h>

char line[64];
snprintf(line, sizeof line, "hp %d  fps %.0f", hp, 1.0f / dt);

AP_SetDrawColor(0.0f, 0.0f, 0.0f, 0.45f);
AP_FillRect(&(AP_FRect){16.0f, 16.0f, 220.0f, 36.0f});

AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
AP_DrawText(24.0f, 24.0f, line);
```

## GUI fonts

`AP_GuiSetFont(font)` makes widgets use a TrueType face. See [Immediate GUI](07-immediate-gui).

## Next

[Immediate GUI](07-immediate-gui)
