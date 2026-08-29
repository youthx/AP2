# Tutorial 13 — Tilemaps

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

A **tileset** is a texture cut into a grid. A **tilemap** is a 2D array of tile IDs drawn from that set.

Tile `0` is empty (not drawn). `1` is the first tile in the atlas, `2` the second, matching Tiled CSV export.

## Load an atlas and build a map

```c
AP_Texture *atlas = AP_LoadTexture("tiles.png");
AP_SetTextureScaleMode(atlas, AP_SCALEMODE_NEAREST);

AP_Tileset *tileset = AP_CreateTileset(atlas, 16, 16);
AP_TilesetSetSolid(tileset, 1, true); /* walls */

static const int level[] = {
    1, 1, 1, 1, 1, 1, 1, 1,
    1, 0, 0, 0, 0, 0, 0, 1,
    1, 0, 0, 2, 2, 0, 0, 1,
    1, 1, 1, 1, 1, 1, 1, 1,
};

AP_Tilemap *map = AP_CreateTilemapFrom(tileset, 8, 4, level);
AP_TilemapSetScale(map, 3.0f);
```

`AP_CreateTilesetGrid(atlas, 8, 4)` is the same idea when you know columns × rows instead of pixel size.

## Draw

`(x, y)` is where tile `(0, 0)` lands. Camera is the usual render translation:

```c
AP_SetTranslation(-cam_x, -cam_y);
AP_DrawTilemap(map, 0.0f, 0.0f);
```

Only tiles that overlap the viewport are submitted. The draw also stores the origin, so collision queries use the same space.

Draw one tile from the set without a map:

```c
AP_DrawTile(tileset, 2, 64.0f, 64.0f, 3.0f);
```

## Collision

World pixels → tile cell, using the map origin and scale:

```c
if (AP_TilemapSolidAt(map, player.x, player.y)) {
    /* center is inside a solid tile */
}

AP_FRect body = {player.x - 8.0f, player.y - 8.0f, 16.0f, 16.0f};
if (AP_TilemapRectSolid(map, &body)) {
    /* AABB overlaps a solid tile */
}

int tx, ty;
AP_TilemapWorldToTile(map, player.x, player.y, &tx, &ty);
AP_FRect cell;
AP_TilemapGetTileRect(map, tx, ty, &cell);
```

`AP_TilemapGetAt` returns the tile ID at a world point (`0` off the map). `AP_TilemapSet` / `AP_TilemapFillRect` edit cells at runtime.

## CSV (Tiled)

Export a layer as CSV. `0` is empty; GIDs match the tileset. Flip flags in the high bits are honored when drawing:

```c
AP_Tilemap *map = AP_LoadTilemapCSV("level.csv", tileset);
```

Or embed the same text:

```c
AP_Tilemap *map = AP_CreateTilemapFromCSV(tileset,
    "1,1,1,1\n"
    "1,0,0,1\n"
    "1,1,1,1\n");
```

## Animation

```c
AP_TilesetSetAnim(tileset, 2, 2, 4, 8.0f); /* id 2 cycles through 2,3,4,5 at 8 fps */

/* each frame */
AP_TilemapUpdate(map, dt);
AP_DrawTilemap(map, 0.0f, 0.0f);
```

## Ownership

The tileset does not own the texture. The map does not own the tileset:

```c
AP_DestroyTilemap(map);
AP_DestroyTileset(tileset);
AP_DestroyTexture(atlas);
```

Exclude the module with `#define AP2_NO_TILEMAP` before `<AP2/AP2.h>`.

## Next

[Breakout](10-breakout.md) · [Top-down walker](11-top-down.md)
