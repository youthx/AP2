/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Tilemap.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct AP_TileAnim {
  int first_id;
  int count;
  int index;
  float fps;
  float elapsed;
} AP_TileAnim;

struct AP_Tileset {
  AP_Texture *texture;
  int tile_width;
  int tile_height;
  int margin;
  int spacing;
  int columns;
  int rows;
  int count;
  unsigned char *solid;
  AP_TileAnim *anims;
};

struct AP_Tilemap {
  AP_Tileset *tileset;
  int *tiles;
  int width;
  int height;
  float origin_x;
  float origin_y;
  float scale;
  AP_FColor color;
};

static AP_FColor AP_TilemapWhite(void) {
  AP_FColor color;
  color.r = 1.0f;
  color.g = 1.0f;
  color.b = 1.0f;
  color.a = 1.0f;
  return color;
}

static int AP_TileId(int cell) {
  return (int)((unsigned int)cell & AP_TILE_GID_MASK);
}

static void AP_TilemapFlipFromCell(int cell, float *angle, AP_FlipMode *flip);

static int AP_TilesetIndex(int tile_id) { return tile_id - 1; }

static bool AP_TilesetIdOk(const AP_Tileset *tileset, int tile_id) {
  return tileset != NULL && tile_id >= 1 && tile_id <= tileset->count;
}

static void AP_TilesetDestroy(AP_Tileset *tileset) {
  if (tileset == NULL) {
    return;
  }

  free(tileset->solid);
  free(tileset->anims);
  free(tileset);
}

static AP_Tileset *AP_TilesetCreate(AP_Texture *texture, int tile_width,
                                    int tile_height, int margin, int spacing) {
  AP_Tileset *tileset;
  int tex_w = 0;
  int tex_h = 0;
  int inner_w;
  int inner_h;
  int stride_w;
  int stride_h;

  if (!AP_TextureIsValid(texture) || tile_width <= 0 || tile_height <= 0 ||
      margin < 0 || spacing < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset");
    return NULL;
  }

  if (!AP_GetTextureSize(texture, &tex_w, &tex_h)) {
    return NULL;
  }

  inner_w = tex_w - margin;
  inner_h = tex_h - margin;
  stride_w = tile_width + spacing;
  stride_h = tile_height + spacing;
  if (inner_w < tile_width || inner_h < tile_height || stride_w <= 0 ||
      stride_h <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tiles do not fit in the texture");
    return NULL;
  }

  tileset = (AP_Tileset *)calloc(1, sizeof(AP_Tileset));
  if (tileset == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate tileset");
    return NULL;
  }

  tileset->texture = texture;
  tileset->tile_width = tile_width;
  tileset->tile_height = tile_height;
  tileset->margin = margin;
  tileset->spacing = spacing;
  tileset->columns = (inner_w + spacing) / stride_w;
  tileset->rows = (inner_h + spacing) / stride_h;
  tileset->count = tileset->columns * tileset->rows;
  if (tileset->columns <= 0 || tileset->rows <= 0 || tileset->count <= 0) {
    AP_TilesetDestroy(tileset);
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tileset has no tiles");
    return NULL;
  }

  tileset->solid =
      (unsigned char *)calloc((size_t)tileset->count, sizeof(unsigned char));
  tileset->anims =
      (AP_TileAnim *)calloc((size_t)tileset->count, sizeof(AP_TileAnim));
  if (tileset->solid == NULL || tileset->anims == NULL) {
    AP_TilesetDestroy(tileset);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate tileset data");
    return NULL;
  }

  return tileset;
}

AP_Tileset *AP_CreateTileset(AP_Texture *texture, int tile_width,
                             int tile_height) {
  return AP_CreateTilesetEx(texture, tile_width, tile_height, 0, 0);
}

AP_Tileset *AP_CreateTilesetEx(AP_Texture *texture, int tile_width,
                               int tile_height, int margin, int spacing) {
  return AP_TilesetCreate(texture, tile_width, tile_height, margin, spacing);
}

AP_Tileset *AP_CreateTilesetGrid(AP_Texture *texture, int columns, int rows) {
  int tex_w = 0;
  int tex_h = 0;

  if (!AP_TextureIsValid(texture) || columns <= 0 || rows <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset grid");
    return NULL;
  }

  if (!AP_GetTextureSize(texture, &tex_w, &tex_h)) {
    return NULL;
  }

  if (tex_w % columns != 0 || tex_h % rows != 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Texture size is not divisible by the grid");
    return NULL;
  }

  return AP_CreateTileset(texture, tex_w / columns, tex_h / rows);
}

void AP_DestroyTileset(AP_Tileset *tileset) { AP_TilesetDestroy(tileset); }

bool AP_TilesetIsValid(const AP_Tileset *tileset) {
  return tileset != NULL && AP_TextureIsValid(tileset->texture) &&
         tileset->count > 0 && tileset->solid != NULL && tileset->anims != NULL;
}

AP_Texture *AP_TilesetGetTexture(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->texture : NULL;
}

int AP_TilesetGetTileWidth(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->tile_width : 0;
}

int AP_TilesetGetTileHeight(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->tile_height : 0;
}

int AP_TilesetGetColumns(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->columns : 0;
}

int AP_TilesetGetRows(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->rows : 0;
}

int AP_TilesetGetCount(const AP_Tileset *tileset) {
  return tileset != NULL ? tileset->count : 0;
}

bool AP_TilesetGetSource(const AP_Tileset *tileset, int tile_id,
                         AP_FRect *src) {
  int index;
  int col;
  int row;

  if (!AP_TilesetIsValid(tileset) || src == NULL ||
      !AP_TilesetIdOk(tileset, tile_id)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset source");
    return false;
  }

  index = AP_TilesetIndex(tile_id);
  col = index % tileset->columns;
  row = index / tileset->columns;
  src->x =
      (float)(tileset->margin + col * (tileset->tile_width + tileset->spacing));
  src->y = (float)(tileset->margin +
                   row * (tileset->tile_height + tileset->spacing));
  src->w = (float)tileset->tile_width;
  src->h = (float)tileset->tile_height;
  return true;
}

bool AP_TilesetSetSolid(AP_Tileset *tileset, int tile_id, bool solid) {
  if (!AP_TilesetIsValid(tileset) || !AP_TilesetIdOk(tileset, tile_id)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset solid flag");
    return false;
  }

  tileset->solid[AP_TilesetIndex(tile_id)] = solid ? 1u : 0u;
  return true;
}

bool AP_TilesetIsSolid(const AP_Tileset *tileset, int tile_id) {
  int id = AP_TileId(tile_id);
  if (!AP_TilesetIsValid(tileset) || !AP_TilesetIdOk(tileset, id)) {
    return false;
  }

  return tileset->solid[AP_TilesetIndex(id)] != 0u;
}

bool AP_TilesetSetAnim(AP_Tileset *tileset, int tile_id, int first_id,
                       int frame_count, float fps) {
  AP_TileAnim *anim;

  if (!AP_TilesetIsValid(tileset) || !AP_TilesetIdOk(tileset, tile_id) ||
      !AP_TilesetIdOk(tileset, first_id) || frame_count <= 0 || fps < 0.0f ||
      first_id + frame_count - 1 > tileset->count) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset animation");
    return false;
  }

  anim = &tileset->anims[AP_TilesetIndex(tile_id)];
  anim->first_id = first_id;
  anim->count = frame_count;
  anim->index = 0;
  anim->fps = fps;
  anim->elapsed = 0.0f;
  return true;
}

bool AP_TilesetClearAnim(AP_Tileset *tileset, int tile_id) {
  if (!AP_TilesetIsValid(tileset) || !AP_TilesetIdOk(tileset, tile_id)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset animation");
    return false;
  }

  memset(&tileset->anims[AP_TilesetIndex(tile_id)], 0, sizeof(AP_TileAnim));
  return true;
}

bool AP_TilesetUpdate(AP_Tileset *tileset, float delta_seconds) {
  int i;

  if (!AP_TilesetIsValid(tileset)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tileset");
    return false;
  }

  if (delta_seconds <= 0.0f) {
    return true;
  }

  for (i = 0; i < tileset->count; ++i) {
    AP_TileAnim *anim = &tileset->anims[i];
    int advanced;

    if (anim->count <= 0 || anim->fps <= 0.0f) {
      continue;
    }

    anim->elapsed += delta_seconds;
    advanced = (int)(anim->elapsed * anim->fps);
    if (advanced <= 0) {
      continue;
    }

    anim->elapsed -= (float)advanced / anim->fps;
    anim->index = (anim->index + advanced) % anim->count;
  }

  return true;
}

int AP_TilesetResolve(const AP_Tileset *tileset, int tile_id) {
  int id = AP_TileId(tile_id);
  const AP_TileAnim *anim;

  if (!AP_TilesetIsValid(tileset) || !AP_TilesetIdOk(tileset, id)) {
    return id;
  }

  anim = &tileset->anims[AP_TilesetIndex(id)];
  if (anim->count <= 0) {
    return id;
  }

  return anim->first_id + anim->index;
}

bool AP_RenderTile(const AP_Tileset *tileset, int tile_id, float x, float y,
                   float scale) {
  AP_FRect src;
  AP_FRect dst;
  float angle = 0.0f;
  AP_FlipMode flip = AP_FLIP_NONE;
  int id = AP_TilesetResolve(tileset, tile_id);

  if (scale <= 0.0f) {
    scale = 1.0f;
  }

  if (id <= 0) {
    return true;
  }

  if (!AP_TilesetGetSource(tileset, id, &src)) {
    return false;
  }

  dst.x = x;
  dst.y = y;
  dst.w = src.w * scale;
  dst.h = src.h * scale;
  AP_TilemapFlipFromCell(tile_id, &angle, &flip);
  return AP_RenderTextureRotated(tileset->texture, &src, &dst, angle, NULL,
                                 flip);
}

static bool AP_TilemapInBounds(const AP_Tilemap *map, int x, int y) {
  return map != NULL && x >= 0 && y >= 0 && x < map->width && y < map->height;
}

static int AP_TilemapCell(const AP_Tilemap *map, int x, int y) {
  if (!AP_TilemapInBounds(map, x, y) || map->tiles == NULL) {
    return AP_TILE_EMPTY;
  }

  return map->tiles[y * map->width + x];
}

static float AP_TilemapTilePixelWidth(const AP_Tilemap *map) {
  if (map == NULL || map->tileset == NULL) {
    return 0.0f;
  }

  return (float)map->tileset->tile_width * map->scale;
}

static float AP_TilemapTilePixelHeight(const AP_Tilemap *map) {
  if (map == NULL || map->tileset == NULL) {
    return 0.0f;
  }

  return (float)map->tileset->tile_height * map->scale;
}

static AP_Tilemap *AP_TilemapAlloc(AP_Tileset *tileset, int width, int height) {
  AP_Tilemap *map;

  if (!AP_TilesetIsValid(tileset) || width <= 0 || height <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return NULL;
  }

  map = (AP_Tilemap *)calloc(1, sizeof(AP_Tilemap));
  if (map == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate tilemap");
    return NULL;
  }

  map->tiles = (int *)calloc((size_t)width * (size_t)height, sizeof(int));
  if (map->tiles == NULL) {
    free(map);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate tilemap cells");
    return NULL;
  }

  map->tileset = tileset;
  map->width = width;
  map->height = height;
  map->scale = 1.0f;
  map->color = AP_TilemapWhite();
  return map;
}

AP_Tilemap *AP_CreateTilemap(AP_Tileset *tileset, int width, int height) {
  return AP_TilemapAlloc(tileset, width, height);
}

AP_Tilemap *AP_CreateTilemapFrom(AP_Tileset *tileset, int width, int height,
                                 const int *tiles) {
  AP_Tilemap *map;

  if (tiles == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tile data cannot be NULL");
    return NULL;
  }

  map = AP_TilemapAlloc(tileset, width, height);
  if (map == NULL) {
    return NULL;
  }

  memcpy(map->tiles, tiles, (size_t)width * (size_t)height * sizeof(int));
  return map;
}

static bool AP_TilemapParseCSV(const char *csv, int **out_tiles, int *out_width,
                               int *out_height) {
  int *tiles = NULL;
  int capacity = 0;
  int count = 0;
  int width = -1;
  int col = 0;
  int rows = 0;
  const char *p;

  if (csv == NULL || out_tiles == NULL || out_width == NULL ||
      out_height == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap CSV");
    return false;
  }

  p = csv;
  while (*p != '\0') {
    char *end = NULL;
    unsigned long value;
    int *grown;

    while (*p == ' ' || *p == '\t') {
      p++;
    }

    if (*p == '\0') {
      break;
    }

    if (*p == '\r') {
      p++;
      continue;
    }

    if (*p == '\n') {
      if (col > 0) {
        if (width < 0) {
          width = col;
        } else if (col != width) {
          free(tiles);
          AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                       "Tilemap CSV rows differ in length");
          return false;
        }
        rows++;
        col = 0;
      }
      p++;
      continue;
    }

    if (*p == ',') {
      p++;
      continue;
    }

    value = strtoul(p, &end, 10);
    if (end == p) {
      free(tiles);
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                   "Tilemap CSV has a non-integer cell");
      return false;
    }

    if (count >= capacity) {
      capacity = capacity == 0 ? 64 : capacity * 2;
      grown = (int *)realloc(tiles, (size_t)capacity * sizeof(int));
      if (grown == NULL) {
        free(tiles);
        AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to grow tilemap CSV");
        return false;
      }
      tiles = grown;
    }

    tiles[count++] = (int)value;
    col++;
    p = end;
  }

  if (col > 0) {
    if (width < 0) {
      width = col;
    } else if (col != width) {
      free(tiles);
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                   "Tilemap CSV rows differ in length");
      return false;
    }
    rows++;
  }

  if (width <= 0 || rows <= 0 || count != width * rows) {
    free(tiles);
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tilemap CSV is empty");
    return false;
  }

  *out_tiles = tiles;
  *out_width = width;
  *out_height = rows;
  return true;
}

AP_Tilemap *AP_CreateTilemapFromCSV(AP_Tileset *tileset, const char *csv) {
  int *tiles = NULL;
  int width = 0;
  int height = 0;
  AP_Tilemap *map;

  if (!AP_TilemapParseCSV(csv, &tiles, &width, &height)) {
    return NULL;
  }

  map = AP_CreateTilemapFrom(tileset, width, height, tiles);
  free(tiles);
  return map;
}

AP_Tilemap *AP_LoadTilemapCSV(const char *path, AP_Tileset *tileset) {
  FILE *file;
  long length;
  char *data;
  AP_Tilemap *map;

  if (path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tilemap path cannot be NULL");
    return NULL;
  }

  file = fopen(path, "rb");
  if (file == NULL) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Tilemap CSV could not be opened");
    return NULL;
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Tilemap CSV could not be read");
    return NULL;
  }

  length = ftell(file);
  if (length <= 0) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Tilemap CSV is empty");
    return NULL;
  }

  rewind(file);
  data = (char *)malloc((size_t)length + 1u);
  if (data == NULL) {
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate tilemap CSV");
    return NULL;
  }

  if (fread(data, 1, (size_t)length, file) != (size_t)length) {
    free(data);
    fclose(file);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Tilemap CSV could not be read");
    return NULL;
  }

  fclose(file);
  data[length] = '\0';
  map = AP_CreateTilemapFromCSV(tileset, data);
  free(data);
  return map;
}

void AP_DestroyTilemap(AP_Tilemap *map) {
  if (map == NULL) {
    return;
  }

  free(map->tiles);
  free(map);
}

bool AP_TilemapIsValid(const AP_Tilemap *map) {
  return map != NULL && map->tiles != NULL && map->width > 0 && map->height > 0;
}

bool AP_TilemapSetTileset(AP_Tilemap *map, AP_Tileset *tileset) {
  if (!AP_TilemapIsValid(map) || !AP_TilesetIsValid(tileset)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap tileset");
    return false;
  }

  map->tileset = tileset;
  return true;
}

AP_Tileset *AP_TilemapGetTileset(const AP_Tilemap *map) {
  return map != NULL ? map->tileset : NULL;
}

int AP_TilemapGetWidth(const AP_Tilemap *map) {
  return map != NULL ? map->width : 0;
}

int AP_TilemapGetHeight(const AP_Tilemap *map) {
  return map != NULL ? map->height : 0;
}

float AP_TilemapGetPixelWidth(const AP_Tilemap *map) {
  return AP_TilemapIsValid(map)
             ? (float)map->width * AP_TilemapTilePixelWidth(map)
             : 0.0f;
}

float AP_TilemapGetPixelHeight(const AP_Tilemap *map) {
  return AP_TilemapIsValid(map)
             ? (float)map->height * AP_TilemapTilePixelHeight(map)
             : 0.0f;
}

bool AP_TilemapSetOrigin(AP_Tilemap *map, float x, float y) {
  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  map->origin_x = x;
  map->origin_y = y;
  return true;
}

bool AP_TilemapGetOrigin(const AP_Tilemap *map, float *x, float *y) {
  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  if (x != NULL) {
    *x = map->origin_x;
  }
  if (y != NULL) {
    *y = map->origin_y;
  }
  return true;
}

bool AP_TilemapSetScale(AP_Tilemap *map, float scale) {
  if (!AP_TilemapIsValid(map) || scale <= 0.0f) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap scale");
    return false;
  }

  map->scale = scale;
  return true;
}

float AP_TilemapGetScale(const AP_Tilemap *map) {
  return map != NULL ? map->scale : 1.0f;
}

bool AP_TilemapSetColor(AP_Tilemap *map, AP_FColor color) {
  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  map->color = color;
  return true;
}

AP_FColor AP_TilemapGetColor(const AP_Tilemap *map) {
  return map != NULL ? map->color : AP_TilemapWhite();
}

bool AP_TilemapSet(AP_Tilemap *map, int x, int y, int tile) {
  if (!AP_TilemapInBounds(map, x, y)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT,
                 "Tile coordinates are out of range");
    return false;
  }

  map->tiles[y * map->width + x] = tile;
  return true;
}

int AP_TilemapGet(const AP_Tilemap *map, int x, int y) {
  return AP_TilemapCell(map, x, y);
}

int AP_TilemapGetId(const AP_Tilemap *map, int x, int y) {
  return AP_TileId(AP_TilemapCell(map, x, y));
}

bool AP_TilemapFill(AP_Tilemap *map, int tile) {
  int i;
  int count;

  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  count = map->width * map->height;
  for (i = 0; i < count; ++i) {
    map->tiles[i] = tile;
  }
  return true;
}

bool AP_TilemapFillRect(AP_Tilemap *map, int x, int y, int w, int h, int tile) {
  int row;
  int col;
  int x1;
  int y1;

  if (!AP_TilemapIsValid(map) || w < 0 || h < 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap fill");
    return false;
  }

  x1 = x + w;
  y1 = y + h;
  if (x < 0) {
    x = 0;
  }
  if (y < 0) {
    y = 0;
  }
  if (x1 > map->width) {
    x1 = map->width;
  }
  if (y1 > map->height) {
    y1 = map->height;
  }

  for (row = y; row < y1; ++row) {
    for (col = x; col < x1; ++col) {
      map->tiles[row * map->width + col] = tile;
    }
  }

  return true;
}

int *AP_TilemapGetData(AP_Tilemap *map) {
  return AP_TilemapIsValid(map) ? map->tiles : NULL;
}

bool AP_TilemapUpdate(AP_Tilemap *map, float delta_seconds) {
  if (!AP_TilemapIsValid(map) || !AP_TilesetIsValid(map->tileset)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  return AP_TilesetUpdate(map->tileset, delta_seconds);
}

bool AP_TilemapWorldToTile(const AP_Tilemap *map, float world_x, float world_y,
                           int *tile_x, int *tile_y) {
  float tw;
  float th;
  int x;
  int y;

  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  tw = AP_TilemapTilePixelWidth(map);
  th = AP_TilemapTilePixelHeight(map);
  if (tw <= 0.0f || th <= 0.0f) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Tilemap has no tileset size");
    return false;
  }

  x = (int)floorf((world_x - map->origin_x) / tw);
  y = (int)floorf((world_y - map->origin_y) / th);
  if (tile_x != NULL) {
    *tile_x = x;
  }
  if (tile_y != NULL) {
    *tile_y = y;
  }
  return true;
}

bool AP_TilemapTileToWorld(const AP_Tilemap *map, int tile_x, int tile_y,
                           float *world_x, float *world_y) {
  if (!AP_TilemapIsValid(map)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  if (world_x != NULL) {
    *world_x = map->origin_x + (float)tile_x * AP_TilemapTilePixelWidth(map);
  }
  if (world_y != NULL) {
    *world_y = map->origin_y + (float)tile_y * AP_TilemapTilePixelHeight(map);
  }
  return true;
}

bool AP_TilemapGetTileRect(const AP_Tilemap *map, int tile_x, int tile_y,
                           AP_FRect *rect) {
  if (rect == NULL ||
      !AP_TilemapTileToWorld(map, tile_x, tile_y, &rect->x, &rect->y)) {
    if (rect == NULL) {
      AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Tile rectangle cannot be NULL");
    }
    return false;
  }

  rect->w = AP_TilemapTilePixelWidth(map);
  rect->h = AP_TilemapTilePixelHeight(map);
  return true;
}

int AP_TilemapGetAt(const AP_Tilemap *map, float world_x, float world_y) {
  int x = 0;
  int y = 0;

  if (!AP_TilemapWorldToTile(map, world_x, world_y, &x, &y)) {
    return AP_TILE_EMPTY;
  }

  return AP_TilemapGetId(map, x, y);
}

bool AP_TilemapIsSolid(const AP_Tilemap *map, int tile_x, int tile_y) {
  int id;

  if (!AP_TilemapIsValid(map) || !AP_TilesetIsValid(map->tileset)) {
    return false;
  }

  id = AP_TilemapGetId(map, tile_x, tile_y);
  return AP_TilesetIsSolid(map->tileset, id);
}

bool AP_TilemapSolidAt(const AP_Tilemap *map, float world_x, float world_y) {
  int x = 0;
  int y = 0;

  if (!AP_TilemapWorldToTile(map, world_x, world_y, &x, &y)) {
    return false;
  }

  return AP_TilemapIsSolid(map, x, y);
}

bool AP_TilemapRectSolid(const AP_Tilemap *map, const AP_FRect *rect) {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  int x;
  int y;
  float tw;
  float th;

  if (!AP_TilemapIsValid(map) || rect == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap collision rect");
    return false;
  }

  tw = AP_TilemapTilePixelWidth(map);
  th = AP_TilemapTilePixelHeight(map);
  if (tw <= 0.0f || th <= 0.0f) {
    return false;
  }

  AP_TilemapWorldToTile(map, rect->x, rect->y, &x0, &y0);
  AP_TilemapWorldToTile(map, rect->x + rect->w - 0.001f,
                        rect->y + rect->h - 0.001f, &x1, &y1);

  if (x0 < 0) {
    x0 = 0;
  }
  if (y0 < 0) {
    y0 = 0;
  }
  if (x1 >= map->width) {
    x1 = map->width - 1;
  }
  if (y1 >= map->height) {
    y1 = map->height - 1;
  }

  for (y = y0; y <= y1; ++y) {
    for (x = x0; x <= x1; ++x) {
      if (AP_TilemapIsSolid(map, x, y)) {
        return true;
      }
    }
  }

  return false;
}

static void AP_TilemapFlipFromCell(int cell, float *angle, AP_FlipMode *flip) {
  unsigned int bits = (unsigned int)cell;

  *angle = 0.0f;
  *flip = AP_FLIP_NONE;

  if ((bits & AP_TILE_FLIP_D) != 0u) {
    *angle = 90.0f;
    *flip = AP_FLIP_HORIZONTAL;
  }
  if ((bits & AP_TILE_FLIP_H) != 0u) {
    *flip =
        (AP_FlipMode)((unsigned int)*flip | (unsigned int)AP_FLIP_HORIZONTAL);
  }
  if ((bits & AP_TILE_FLIP_V) != 0u) {
    *flip = (AP_FlipMode)((unsigned int)*flip | (unsigned int)AP_FLIP_VERTICAL);
  }
}

static bool AP_TilemapVisibleRange(const AP_Tilemap *map, float origin_x,
                                   float origin_y, const AP_FRect *view,
                                   int *x0, int *y0, int *x1, int *y1) {
  float tw = AP_TilemapTilePixelWidth(map);
  float th = AP_TilemapTilePixelHeight(map);
  AP_FRect area;
  float tx = 0.0f;
  float ty = 0.0f;
  float sx = 1.0f;
  float sy = 1.0f;
  float rotation = AP_GetRenderRotation();
  AP_Rect viewport;

  if (tw <= 0.0f || th <= 0.0f) {
    return false;
  }

  if (view != NULL) {
    area = *view;
  } else if (fabsf(rotation) > 0.01f) {
    area.x = origin_x;
    area.y = origin_y;
    area.w = AP_TilemapGetPixelWidth(map);
    area.h = AP_TilemapGetPixelHeight(map);
  } else {
    if (!AP_GetRenderViewport(&viewport) || viewport.w <= 0 ||
        viewport.h <= 0) {
      area.x = origin_x;
      area.y = origin_y;
      area.w = AP_TilemapGetPixelWidth(map);
      area.h = AP_TilemapGetPixelHeight(map);
    } else {
      AP_GetRenderTranslation(&tx, &ty);
      AP_GetRenderScale(&sx, &sy);
      if (sx == 0.0f) {
        sx = 1.0f;
      }
      if (sy == 0.0f) {
        sy = 1.0f;
      }
      area.x = (0.0f - tx) / sx;
      area.y = (0.0f - ty) / sy;
      area.w = (float)viewport.w / sx;
      area.h = (float)viewport.h / sy;
    }
  }

  *x0 = (int)floorf((area.x - origin_x) / tw) - 1;
  *y0 = (int)floorf((area.y - origin_y) / th) - 1;
  *x1 = (int)floorf((area.x + area.w - origin_x) / tw) + 1;
  *y1 = (int)floorf((area.y + area.h - origin_y) / th) + 1;

  if (*x0 < 0) {
    *x0 = 0;
  }
  if (*y0 < 0) {
    *y0 = 0;
  }
  if (*x1 >= map->width) {
    *x1 = map->width - 1;
  }
  if (*y1 >= map->height) {
    *y1 = map->height - 1;
  }

  return *x0 <= *x1 && *y0 <= *y1;
}

static AP_Color AP_TilemapDrawTint(const AP_Tilemap *map) {
  AP_Color tint = map->color;
  float red = 1.0f;
  float green = 1.0f;
  float blue = 1.0f;
  float alpha = 1.0f;

  if (map->tileset != NULL && map->tileset->texture != NULL) {
    AP_GetTextureColorModFloat(map->tileset->texture, &red, &green, &blue);
    AP_GetTextureAlphaModFloat(map->tileset->texture, &alpha);
  }

  tint.r *= red;
  tint.g *= green;
  tint.b *= blue;
  tint.a *= alpha;
  return tint;
}

bool AP_RenderTilemapEx(AP_Tilemap *map, float x, float y,
                        const AP_FRect *view) {
  int x0 = 0;
  int y0 = 0;
  int x1 = 0;
  int y1 = 0;
  int col;
  int row;
  float tw;
  float th;
  AP_Color tint;

  if (!AP_TilemapIsValid(map) || !AP_TilesetIsValid(map->tileset)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid tilemap");
    return false;
  }

  map->origin_x = x;
  map->origin_y = y;
  tw = AP_TilemapTilePixelWidth(map);
  th = AP_TilemapTilePixelHeight(map);
  if (tw <= 0.0f || th <= 0.0f) {
    return true;
  }

  if (!AP_TilemapVisibleRange(map, x, y, view, &x0, &y0, &x1, &y1)) {
    return true;
  }

  tint = AP_TilemapDrawTint(map);

  for (row = y0; row <= y1; ++row) {
    for (col = x0; col <= x1; ++col) {
      int cell = map->tiles[row * map->width + col];
      int id = AP_TilesetResolve(map->tileset, AP_TileId(cell));
      AP_FRect src;
      AP_FRect dst;
      float angle = 0.0f;
      AP_FlipMode flip = AP_FLIP_NONE;

      if (id <= 0 || !AP_TilesetIdOk(map->tileset, id)) {
        continue;
      }

      if (!AP_TilesetGetSource(map->tileset, id, &src)) {
        return false;
      }

      dst.x = x + (float)col * tw;
      dst.y = y + (float)row * th;
      dst.w = tw;
      dst.h = th;
      AP_TilemapFlipFromCell(cell, &angle, &flip);
      if (!AP_TextureRenderRotatedTinted(map->tileset->texture, &src, &dst,
                                         angle, NULL, flip, tint)) {
        return false;
      }
    }
  }

  return true;
}

bool AP_RenderTilemap(AP_Tilemap *map, float x, float y) {
  return AP_RenderTilemapEx(map, x, y, NULL);
}
