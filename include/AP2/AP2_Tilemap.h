/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_TILEMAP_H
#define AP2_TILEMAP_H

#include "AP2/AP2_Texture.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Tilemap
 *
 * A tileset is a texture sliced into a grid. A tilemap is a row-major
 * grid of tile IDs. ID 0 is empty (not drawn). IDs 1+ are tileset
 * indices plus one — the same convention Tiled uses in CSV export.
 *
 *     AP_Texture *atlas = AP_LoadTexture("tiles.png");
 *     AP_SetTextureScaleMode(atlas, AP_SCALEMODE_NEAREST);
 *
 *     AP_Tileset *tileset = AP_CreateTileset(atlas, 16, 16);
 *     AP_TilesetSetSolid(tileset, 1, true);
 *
 *     static const int level[] = {
 *         1, 1, 1, 1, 1,
 *         1, 0, 0, 0, 1,
 *         1, 1, 1, 1, 1,
 *     };
 *     AP_Tilemap *map = AP_CreateTilemapFrom(tileset, 5, 3, level);
 *
 *     AP_SetTranslation(-cam_x, -cam_y);
 *     AP_DrawTilemap(map, 0.0f, 0.0f);
 *
 *     if (AP_TilemapSolidAt(map, player_x, player_y)) {
 *         // hit a wall
 *     }
 *
 * The tileset does not own the texture. The tilemap does not own the
 * tileset. Destroy the map, then the tileset, then the texture.
 *
 * High bits of a cell may carry Tiled flip flags (AP_TILE_FLIP_*).
 * AP_TilemapGetId() strips them; drawing honors them.
 */

#define AP_TILE_EMPTY 0
#define AP_TILE_FLIP_H 0x80000000u
#define AP_TILE_FLIP_V 0x40000000u
#define AP_TILE_FLIP_D 0x20000000u
#define AP_TILE_GID_MASK 0x1FFFFFFFu

typedef struct AP_Tileset AP_Tileset;
typedef struct AP_Tilemap AP_Tilemap;

/* =========================================================
 * Tileset
 * ========================================================= */

AP_Tileset *AP_CreateTileset(AP_Texture *texture, int tile_width,
                             int tile_height);

AP_Tileset *AP_CreateTilesetEx(AP_Texture *texture, int tile_width,
                               int tile_height, int margin, int spacing);

AP_Tileset *AP_CreateTilesetGrid(AP_Texture *texture, int columns, int rows);

void AP_DestroyTileset(AP_Tileset *tileset);

bool AP_TilesetIsValid(const AP_Tileset *tileset);

AP_Texture *AP_TilesetGetTexture(const AP_Tileset *tileset);

int AP_TilesetGetTileWidth(const AP_Tileset *tileset);

int AP_TilesetGetTileHeight(const AP_Tileset *tileset);

int AP_TilesetGetColumns(const AP_Tileset *tileset);

int AP_TilesetGetRows(const AP_Tileset *tileset);

int AP_TilesetGetCount(const AP_Tileset *tileset);

bool AP_TilesetGetSource(const AP_Tileset *tileset, int tile_id, AP_FRect *src);

bool AP_TilesetSetSolid(AP_Tileset *tileset, int tile_id, bool solid);

bool AP_TilesetIsSolid(const AP_Tileset *tileset, int tile_id);

bool AP_TilesetSetAnim(AP_Tileset *tileset, int tile_id, int first_id,
                       int frame_count, float fps);

bool AP_TilesetClearAnim(AP_Tileset *tileset, int tile_id);

bool AP_TilesetUpdate(AP_Tileset *tileset, float delta_seconds);

int AP_TilesetResolve(const AP_Tileset *tileset, int tile_id);

bool AP_RenderTile(const AP_Tileset *tileset, int tile_id, float x, float y,
                   float scale);

#define AP_DrawTile AP_RenderTile

/* =========================================================
 * Tilemap
 * ========================================================= */

AP_Tilemap *AP_CreateTilemap(AP_Tileset *tileset, int width, int height);

AP_Tilemap *AP_CreateTilemapFrom(AP_Tileset *tileset, int width, int height,
                                 const int *tiles);

AP_Tilemap *AP_CreateTilemapFromCSV(AP_Tileset *tileset, const char *csv);

AP_Tilemap *AP_LoadTilemapCSV(const char *path, AP_Tileset *tileset);

void AP_DestroyTilemap(AP_Tilemap *map);

bool AP_TilemapIsValid(const AP_Tilemap *map);

bool AP_TilemapSetTileset(AP_Tilemap *map, AP_Tileset *tileset);

AP_Tileset *AP_TilemapGetTileset(const AP_Tilemap *map);

int AP_TilemapGetWidth(const AP_Tilemap *map);

int AP_TilemapGetHeight(const AP_Tilemap *map);

float AP_TilemapGetPixelWidth(const AP_Tilemap *map);

float AP_TilemapGetPixelHeight(const AP_Tilemap *map);

bool AP_TilemapSetOrigin(AP_Tilemap *map, float x, float y);

bool AP_TilemapGetOrigin(const AP_Tilemap *map, float *x, float *y);

bool AP_TilemapSetScale(AP_Tilemap *map, float scale);

float AP_TilemapGetScale(const AP_Tilemap *map);

bool AP_TilemapSetColor(AP_Tilemap *map, AP_FColor color);

AP_FColor AP_TilemapGetColor(const AP_Tilemap *map);

bool AP_TilemapSet(AP_Tilemap *map, int x, int y, int tile);

int AP_TilemapGet(const AP_Tilemap *map, int x, int y);

int AP_TilemapGetId(const AP_Tilemap *map, int x, int y);

bool AP_TilemapFill(AP_Tilemap *map, int tile);

bool AP_TilemapFillRect(AP_Tilemap *map, int x, int y, int w, int h, int tile);

int *AP_TilemapGetData(AP_Tilemap *map);

bool AP_TilemapUpdate(AP_Tilemap *map, float delta_seconds);

bool AP_TilemapWorldToTile(const AP_Tilemap *map, float world_x, float world_y,
                           int *tile_x, int *tile_y);

bool AP_TilemapTileToWorld(const AP_Tilemap *map, int tile_x, int tile_y,
                           float *world_x, float *world_y);

bool AP_TilemapGetTileRect(const AP_Tilemap *map, int tile_x, int tile_y,
                           AP_FRect *rect);

int AP_TilemapGetAt(const AP_Tilemap *map, float world_x, float world_y);

bool AP_TilemapIsSolid(const AP_Tilemap *map, int tile_x, int tile_y);

bool AP_TilemapSolidAt(const AP_Tilemap *map, float world_x, float world_y);

bool AP_TilemapRectSolid(const AP_Tilemap *map, const AP_FRect *rect);

/*
 * (x, y) is where tile (0, 0) lands in the current transform space.
 * The origin is stored so later world queries match what was drawn.
 * Visible tiles are culled to the current viewport.
 */
bool AP_RenderTilemap(AP_Tilemap *map, float x, float y);

bool AP_RenderTilemapEx(AP_Tilemap *map, float x, float y,
                        const AP_FRect *view);

#define AP_DrawTilemap AP_RenderTilemap
#define AP_DrawTilemapEx AP_RenderTilemapEx

#ifdef __cplusplus
}
#endif

#endif /* AP2_TILEMAP_H */
