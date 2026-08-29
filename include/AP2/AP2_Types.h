/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_TYPES_H
#define AP2_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* ============================================================
 * Integer types
 * ============================================================ */

typedef uint8_t AP_U8;
typedef uint16_t AP_U16;
typedef uint32_t AP_U32;
typedef uint64_t AP_U64;

typedef int8_t AP_I8;
typedef int16_t AP_I16;
typedef int32_t AP_I32;
typedef int64_t AP_I64;

/* ============================================================
 * Floating point
 * ============================================================ */

typedef float AP_F32;
typedef double AP_F64;

/* ============================================================
 * Common integer aliases
 * ============================================================ */

typedef AP_I32 AP_Int;
typedef AP_U32 AP_UInt;

/* ============================================================
 * Size / difference types
 * ============================================================ */

typedef size_t AP_Size;
typedef ptrdiff_t AP_SSize;

/* ============================================================
 * Boolean
 * ============================================================ */

typedef bool AP_Bool;

#ifndef AP_TRUE
#define AP_TRUE true
#endif

#ifndef AP_FALSE
#define AP_FALSE false
#endif

/* ============================================================
 * Handles
 * ============================================================ */

typedef AP_U32 AP_Handle;

#define AP_NULL_HANDLE ((AP_Handle)0)

/* ============================================================
 * Generic result
 * ============================================================ */

typedef enum AP_Result { AP_SUCCESS = 0, AP_FAILURE = -1 } AP_Result;

/* ============================================================
 * Version
 * ============================================================ */

typedef struct AP_Version {
  AP_U32 major;
  AP_U32 minor;
  AP_U32 patch;
} AP_Version;

/* ============================================================
 * 2D integer vector / point
 * ============================================================ */

typedef struct AP_Vec2I {
  AP_I32 x;
  AP_I32 y;
} AP_Vec2I;

typedef AP_Vec2I AP_Point;

/* ============================================================
 * 2D float vector / point
 * ============================================================ */

typedef struct AP_Vec2 {
  AP_F32 x;
  AP_F32 y;
} AP_Vec2;

typedef AP_Vec2 AP_FPoint;

/* ============================================================
 * 3D float vector
 * ============================================================ */

typedef struct AP_Vec3 {
  AP_F32 x;
  AP_F32 y;
  AP_F32 z;
} AP_Vec3;

/* ============================================================
 * 4D float vector
 * ============================================================ */

typedef struct AP_Vec4 {
  AP_F32 x;
  AP_F32 y;
  AP_F32 z;
  AP_F32 w;
} AP_Vec4;

/* ============================================================
 * Color
 * ============================================================ */

typedef struct AP_Color {
  AP_F32 r;
  AP_F32 g;
  AP_F32 b;
  AP_F32 a;
} AP_Color;

typedef AP_Color AP_FColor;

typedef struct AP_Color8 {
  AP_U8 r;
  AP_U8 g;
  AP_U8 b;
  AP_U8 a;
} AP_Color8;

/* ============================================================
 * Rectangle (SDL_Rect layout: x, y, w, h)
 * ============================================================ */

typedef struct AP_Rect {
  AP_I32 x;
  AP_I32 y;
  AP_I32 w;
  AP_I32 h;
} AP_Rect;

/* ============================================================
 * Floating-point rectangle (SDL_FRect layout: x, y, w, h)
 * ============================================================ */

typedef struct AP_FRect {
  AP_F32 x;
  AP_F32 y;
  AP_F32 w;
  AP_F32 h;
} AP_FRect;

typedef AP_FRect AP_RectF;

/* ============================================================
 * Colored 2D vertex (SDL_Vertex layout)
 * ============================================================ */

typedef struct AP_Vertex {
  AP_FPoint position;
  AP_FColor color;
  AP_FPoint tex_coord;
} AP_Vertex;

/* ============================================================
 * 2D affine transform (scale, rotation, translation)
 *
 * Rotation is in degrees. Positive angles rotate clockwise
 * in window space (Y-down).
 * ============================================================ */

typedef struct AP_Transform {
  AP_F32 translate_x;
  AP_F32 translate_y;
  AP_F32 scale_x;
  AP_F32 scale_y;
  AP_F32 rotation;
  AP_F32 origin_x;
  AP_F32 origin_y;
} AP_Transform;

/* =========================================================
 * Window Geometry
 * ========================================================= */

typedef struct AP_WindowPosition {
  int x;
  int y;
} AP_WindowPosition;

typedef struct AP_WindowSize {
  int width;
  int height;
} AP_WindowSize;

typedef void (*AP_Callback)(void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* AP2_TYPES_H */
