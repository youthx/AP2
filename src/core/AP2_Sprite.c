/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Sprite.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"

#include <string.h>

static AP_FColor AP_SpriteWhite(void) {
  AP_FColor color;
  color.r = 1.0f;
  color.g = 1.0f;
  color.b = 1.0f;
  color.a = 1.0f;
  return color;
}

AP_Sprite AP_CreateSprite(AP_Texture *texture) {
  AP_Sprite sprite;

  memset(&sprite, 0, sizeof(sprite));
  sprite.scale_x = 1.0f;
  sprite.scale_y = 1.0f;
  sprite.color = AP_SpriteWhite();
  AP_SpriteSetTexture(&sprite, texture);
  return sprite;
}

bool AP_SpriteSetTexture(AP_Sprite *sprite, AP_Texture *texture) {
  int width = 0;
  int height = 0;

  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->texture = texture;
  sprite->src.x = 0.0f;
  sprite->src.y = 0.0f;
  sprite->src.w = 0.0f;
  sprite->src.h = 0.0f;

  if (texture == NULL) {
    return true;
  }

  if (!AP_GetTextureSize(texture, &width, &height)) {
    return false;
  }

  sprite->src.w = (float)width;
  sprite->src.h = (float)height;
  return true;
}

bool AP_SpriteSetSource(AP_Sprite *sprite, const AP_FRect *src) {
  if (sprite == NULL || src == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite source cannot be NULL");
    return false;
  }

  sprite->src = *src;
  return true;
}

bool AP_SpriteSetOrigin(AP_Sprite *sprite, float x, float y) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->origin.x = x;
  sprite->origin.y = y;
  return true;
}

bool AP_SpriteSetOriginNormalized(AP_Sprite *sprite, float x, float y) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->origin.x = sprite->src.w * x;
  sprite->origin.y = sprite->src.h * y;
  return true;
}

bool AP_SpriteSetRotation(AP_Sprite *sprite, float degrees) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->rotation = degrees;
  return true;
}

bool AP_SpriteSetScale(AP_Sprite *sprite, float scale) {
  return AP_SpriteSetScaleXY(sprite, scale, scale);
}

bool AP_SpriteSetScaleXY(AP_Sprite *sprite, float scale_x, float scale_y) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->scale_x = scale_x;
  sprite->scale_y = scale_y;
  return true;
}

bool AP_SpriteSetFlip(AP_Sprite *sprite, AP_FlipMode flip) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->flip = flip;
  return true;
}

bool AP_SpriteSetColor(AP_Sprite *sprite, AP_FColor color) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->color = color;
  return true;
}

bool AP_SpriteSetFrame(AP_Sprite *sprite, int columns, int rows, int frame) {
  float frame_w;
  float frame_h;

  if (sprite == NULL || columns <= 0 || rows <= 0 || frame < 0 ||
      frame >= columns * rows) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid sprite atlas frame");
    return false;
  }

  frame_w = (float)AP_GetTextureWidth(sprite->texture) / (float)columns;
  frame_h = (float)AP_GetTextureHeight(sprite->texture) / (float)rows;
  sprite->src.x = (float)(frame % columns) * frame_w;
  sprite->src.y = (float)(frame / columns) * frame_h;
  sprite->src.w = frame_w;
  sprite->src.h = frame_h;
  return true;
}

static bool AP_SpriteApplyAnimFrame(AP_Sprite *sprite) {
  return AP_SpriteSetFrame(sprite, sprite->anim_columns, sprite->anim_rows,
                           sprite->anim_start + sprite->anim_index);
}

bool AP_SpritePlay(AP_Sprite *sprite, int columns, int rows, int start_frame,
                   int frame_count, float fps, bool loop) {
  int total;

  if (sprite == NULL || columns <= 0 || rows <= 0 || start_frame < 0 ||
      fps < 0.0f) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid sprite animation");
    return false;
  }

  total = columns * rows;
  if (frame_count < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid sprite animation");
    return false;
  }

  if (frame_count == 0) {
    frame_count = total - start_frame;
  }

  if (frame_count <= 0 || start_frame + frame_count > total) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite animation is out of range");
    return false;
  }

  sprite->anim_columns = columns;
  sprite->anim_rows = rows;
  sprite->anim_start = start_frame;
  sprite->anim_count = frame_count;
  sprite->anim_index = 0;
  sprite->anim_fps = fps;
  sprite->anim_elapsed = 0.0f;
  sprite->anim_loop = loop;
  sprite->anim_playing = fps > 0.0f;
  return AP_SpriteApplyAnimFrame(sprite);
}

bool AP_SpriteStop(AP_Sprite *sprite) {
  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  sprite->anim_playing = false;
  return true;
}

bool AP_SpriteUpdate(AP_Sprite *sprite, float delta_seconds) {
  int advanced;

  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  if (!sprite->anim_playing || sprite->anim_fps <= 0.0f ||
      sprite->anim_count <= 0 || delta_seconds <= 0.0f) {
    return true;
  }

  sprite->anim_elapsed += delta_seconds;
  advanced = (int)(sprite->anim_elapsed * sprite->anim_fps);
  if (advanced <= 0) {
    return true;
  }

  sprite->anim_elapsed -= (float)advanced / sprite->anim_fps;
  if (sprite->anim_loop) {
    sprite->anim_index =
        (sprite->anim_index + advanced) % sprite->anim_count;
  } else {
    sprite->anim_index += advanced;
    if (sprite->anim_index >= sprite->anim_count) {
      sprite->anim_index = sprite->anim_count - 1;
      sprite->anim_playing = false;
    }
  }

  return AP_SpriteApplyAnimFrame(sprite);
}

int AP_SpriteGetFrame(const AP_Sprite *sprite) {
  if (sprite == NULL || sprite->anim_count <= 0) {
    return 0;
  }

  return sprite->anim_start + sprite->anim_index;
}

bool AP_RenderSpriteEx(const AP_Sprite *sprite, const AP_FRect *dst) {
  AP_FPoint center;
  AP_Color tint;
  float red;
  float green;
  float blue;
  float alpha;

  if (sprite == NULL || dst == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite draw arguments cannot be NULL");
    return false;
  }

  center.x = dst->x;
  center.y = dst->y;
  if (sprite->src.w > 0.0f) {
    center.x += sprite->origin.x / sprite->src.w * dst->w;
  }
  if (sprite->src.h > 0.0f) {
    center.y += sprite->origin.y / sprite->src.h * dst->h;
  }

  if (!AP_GetTextureColorModFloat(sprite->texture, &red, &green, &blue) ||
      !AP_GetTextureAlphaModFloat(sprite->texture, &alpha)) {
    return false;
  }

  tint.r = sprite->color.r * red;
  tint.g = sprite->color.g * green;
  tint.b = sprite->color.b * blue;
  tint.a = sprite->color.a * alpha;
  return AP_TextureRenderRotatedTinted(sprite->texture, &sprite->src, dst,
                                       sprite->rotation, &center, sprite->flip,
                                       tint);
}

bool AP_RenderSprite(const AP_Sprite *sprite, float x, float y) {
  AP_FRect dest;

  if (sprite == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sprite cannot be NULL");
    return false;
  }

  dest.w = sprite->src.w * sprite->scale_x;
  dest.h = sprite->src.h * sprite->scale_y;
  dest.x = x - sprite->origin.x * sprite->scale_x;
  dest.y = y - sprite->origin.y * sprite->scale_y;
  return AP_RenderSpriteEx(sprite, &dest);
}
