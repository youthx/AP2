# Tutorial 03 — Textures and sprites

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

A texture is a GPU image. A sprite is a slice of that image with origin, scale, rotation, and animation.

## Load and draw a texture

```c
AP_Texture *tex = AP_LoadTexture("player.png");
if (!tex) {
  AP_ERROR("texture: %s", AP_GetErrorMessage());
}

AP_SetTextureScaleMode(tex, AP_SCALEMODE_NEAREST);

AP_DrawTexture(tex, NULL, &(AP_FRect){64.0f, 64.0f, 128.0f, 128.0f});
```

`NULL` source uses the whole image. `NULL` destination fills the viewport (usually not what you want for a sprite).

Destroy what you create:

```c
AP_DestroyTexture(tex);
```

Supported image formats come from stb_image (PNG, JPEG, and others).

## Source rectangles and rotation

```c
AP_FRect src = {0.0f, 0.0f, 32.0f, 32.0f};
AP_FRect dst = {200.0f, 200.0f, 64.0f, 64.0f};
AP_DrawTextureRotated(tex, &src, &dst, 45.0f, NULL, AP_FLIP_NONE);
```

Angle is degrees, clockwise. `NULL` center rotates about the destination rectangle.

Tiled and nine-slice:

```c
AP_DrawTextureTiled(panel, NULL, 1.0f, &(AP_FRect){0.0f, 0.0f, 400.0f, 240.0f});
AP_DrawTexture9Grid(panel, NULL, 12.0f, 12.0f, 12.0f, 12.0f, 1.0f, &dst);
```

## Sprites

```c
AP_Sprite player = AP_CreateSprite(tex);
AP_SpriteSetOriginNormalized(&player, 0.5f, 0.5f);
AP_SpriteSetScale(&player, 3.0f);
AP_DrawSprite(&player, 640.0f, 360.0f);
```

`(x, y)` is where the **origin** lands in window pixels.

Atlas frame (columns × rows, then index):

```c
AP_SpriteSetFrame(&player, 8, 4, 3);
```

## Animation

```c
AP_SpritePlay(&player, 8, 1, 0, 8, 12.0f, true);

/* each frame */
float dt = (float)(AP_GetTime() - last_time);
last_time = AP_GetTime();
AP_SpriteUpdate(&player, dt);
AP_DrawSprite(&player, x, y);
```

Tint:

```c
AP_SpriteSetColor(&player, AP_C4(1.0f, 0.7f, 0.7f, 1.0f));
```

## Pixel look

Nearest filtering plus integer scale keeps pixel art crisp:

```c
AP_SetTextureScaleMode(tex, AP_SCALEMODE_NEAREST);
AP_SpriteSetScale(&player, 4.0f);
```

## Next

[Input](04-input.md)
