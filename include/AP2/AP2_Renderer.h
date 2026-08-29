/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_RENDERER_H
#define AP2_RENDERER_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Immediate Renderer
 *
 * The renderer is created automatically with the window.
 * Applications never create, destroy, or hold a renderer object.
 * Calling convention matches SDL3's 2D renderer, minus the
 * SDL_Renderer * argument:
 *
 *     AP_SetDrawColor(0.1f, 0.1f, 0.1f, 1.0f);
 *     AP_Clear();
 *     AP_SetDrawColor(1.0f, 0.2f, 0.2f, 1.0f);
 *     AP_FillRect(&(AP_FRect){32.0f, 32.0f, 200.0f, 120.0f});
 *     AP_Present();
 *
 * Coordinates use a top-left origin in window pixels.
 * AP_Clear() uses the current draw color, like SDL3.
 * Prefer the short names (AP_Clear, AP_FillRect, AP_Present)
 * in application code; AP_Render* is the underlying API.
 *
 * Rotation is in degrees. Positive angles rotate clockwise in
 * window space (Y increases downward).
 *
 * 2D is Y-down. 3D is Y-up. Both are correct. Both will get you once.
 */

/* =========================================================
 * Blend modes
 * ========================================================= */

typedef enum AP_BlendMode {
  AP_BLENDMODE_NONE = 0,
  AP_BLENDMODE_BLEND,
  AP_BLENDMODE_ADD,
  AP_BLENDMODE_MOD,
  AP_BLENDMODE_MUL,
  AP_BLENDMODE_SCREEN,
  AP_BLENDMODE_PREMULTIPLIED
} AP_BlendMode;

#define AP_BLEND_NONE AP_BLENDMODE_NONE
#define AP_BLEND_ALPHA AP_BLENDMODE_BLEND
#define AP_BLEND_ADD AP_BLENDMODE_ADD
#define AP_BLEND_MOD AP_BLENDMODE_MOD
#define AP_BLEND_MUL AP_BLENDMODE_MUL
#define AP_BLEND_SCREEN AP_BLENDMODE_SCREEN
#define AP_BLEND_PREMULTIPLIED AP_BLENDMODE_PREMULTIPLIED

/* =========================================================
 * Draw flags
 * ========================================================= */

typedef enum AP_DrawFlags {
  AP_DRAW_NONE = 0,
  AP_DRAW_FILL = 1u << 0,
  AP_DRAW_STROKE = 1u << 1,
  AP_DRAW_CLOSED = 1u << 2,
  AP_DRAW_CENTERED = 1u << 3,
  AP_DRAW_AA = 1u << 4,
  AP_DRAW_ROUND_POINTS = 1u << 5
} AP_DrawFlags;

/* =========================================================
 * Line style
 * ========================================================= */

typedef enum AP_LineCap {
  AP_LINE_CAP_BUTT = 0,
  AP_LINE_CAP_ROUND,
  AP_LINE_CAP_SQUARE
} AP_LineCap;

typedef enum AP_LineJoin {
  AP_LINE_JOIN_MITER = 0,
  AP_LINE_JOIN_BEVEL,
  AP_LINE_JOIN_ROUND
} AP_LineJoin;

/* =========================================================
 * Mesh primitives
 * ========================================================= */

typedef enum AP_Primitive {
  AP_PRIM_POINTS = 0,
  AP_PRIM_LINES,
  AP_PRIM_LINE_STRIP,
  AP_PRIM_LINE_LOOP,
  AP_PRIM_TRIANGLES,
  AP_PRIM_TRIANGLE_STRIP,
  AP_PRIM_TRIANGLE_FAN
} AP_Primitive;

/* =========================================================
 * Draw color
 *
 * AP_SetRenderDrawColor uses 0-255.
 * AP_SetRenderDrawColorFloat uses 0.0-1.0.
 * AP_RenderClear uses the current draw color.
 * ========================================================= */

bool AP_SetRenderDrawColor(AP_U8 r, AP_U8 g, AP_U8 b, AP_U8 a);

bool AP_SetRenderDrawColorFloat(float r, float g, float b, float a);

bool AP_GetRenderDrawColor(AP_U8 *r, AP_U8 *g, AP_U8 *b, AP_U8 *a);

bool AP_GetRenderDrawColorFloat(float *r, float *g, float *b, float *a);

bool AP_SetRenderColorScale(float r, float g, float b);

bool AP_GetRenderColorScale(float *r, float *g, float *b);

bool AP_SetRenderDrawBlendMode(AP_BlendMode mode);

AP_BlendMode AP_GetRenderDrawBlendMode(void);

/* =========================================================
 * Frame
 * ========================================================= */

bool AP_RenderClear(void);

bool AP_FlushRenderer(void);

bool AP_RenderPresent(void);

/* =========================================================
 * Draw flags / stroke state
 * ========================================================= */

bool AP_SetRenderDrawFlags(uint32_t flags);

uint32_t AP_GetRenderDrawFlags(void);

bool AP_EnableRenderDrawFlag(AP_DrawFlags flag);

bool AP_DisableRenderDrawFlag(AP_DrawFlags flag);

bool AP_RenderDrawFlagEnabled(AP_DrawFlags flag);

bool AP_SetRenderScale(float scaleX, float scaleY);

bool AP_GetRenderScale(float *scaleX, float *scaleY);

bool AP_SetRenderLineWidth(float width);

float AP_GetRenderLineWidth(void);

bool AP_SetRenderLineCap(AP_LineCap cap);

AP_LineCap AP_GetRenderLineCap(void);

bool AP_SetRenderLineJoin(AP_LineJoin join);

AP_LineJoin AP_GetRenderLineJoin(void);

bool AP_SetRenderPointSize(float size);

float AP_GetRenderPointSize(void);

bool AP_SetRenderCircleSegments(int segments);

int AP_GetRenderCircleSegments(void);

/* =========================================================
 * Points / lines / rects  (SDL3 signatures)
 * ========================================================= */

bool AP_RenderPoint(float x, float y);

bool AP_RenderPoints(const AP_FPoint *points, int count);

bool AP_RenderLine(float x1, float y1, float x2, float y2);

bool AP_RenderLines(const AP_FPoint *points, int count);

/*
 * NULL rect fills / outlines the current viewport, matching SDL3.
 */
bool AP_RenderRect(const AP_FRect *rect);

bool AP_RenderRects(const AP_FRect *rects, int count);

bool AP_RenderFillRect(const AP_FRect *rect);

bool AP_RenderFillRects(const AP_FRect *rects, int count);

bool AP_RenderFillRectGradient(const AP_FRect *rect, AP_FColor top_left,
                               AP_FColor top_right, AP_FColor bottom_right,
                               AP_FColor bottom_left);

bool AP_RenderRoundedRect(const AP_FRect *rect, float radius);

bool AP_RenderFillRoundedRect(const AP_FRect *rect, float radius);

/* =========================================================
 * Circles / ellipses / arcs
 * ========================================================= */

bool AP_RenderCircle(float x, float y, float radius);

bool AP_RenderFillCircle(float x, float y, float radius);

bool AP_RenderEllipse(float x, float y, float radius_x, float radius_y);

bool AP_RenderFillEllipse(float x, float y, float radius_x, float radius_y);

bool AP_RenderArc(float x, float y, float radius, float start_deg,
                  float end_deg);

bool AP_RenderFillPie(float x, float y, float radius, float start_deg,
                      float end_deg);

bool AP_RenderRing(float x, float y, float inner_radius, float outer_radius);

bool AP_RenderFillRing(float x, float y, float inner_radius, float outer_radius);

/* =========================================================
 * Triangles / polygons
 * ========================================================= */

bool AP_RenderTriangle(float x1, float y1, float x2, float y2, float x3,
                       float y3);

bool AP_RenderFillTriangle(float x1, float y1, float x2, float y2, float x3,
                           float y3);

bool AP_RenderFillTriangleColor(float x1, float y1, AP_FColor c1, float x2,
                                float y2, AP_FColor c2, float x3, float y3,
                                AP_FColor c3);

bool AP_RenderPolygon(const AP_FPoint *points, int count);

bool AP_RenderFillPolygon(const AP_FPoint *points, int count);

bool AP_RenderNGon(float x, float y, float radius, int sides);

bool AP_RenderFillNGon(float x, float y, float radius, int sides);

bool AP_RenderStar(float x, float y, float outer_radius, float inner_radius,
                   int points);

bool AP_RenderFillStar(float x, float y, float outer_radius, float inner_radius,
                       int points);

/* =========================================================
 * Curves / extra primitives
 * ========================================================= */

bool AP_RenderQuadraticBezier(float x1, float y1, float cx, float cy, float x2,
                              float y2);

bool AP_RenderBezier(float x1, float y1, float cx1, float cy1, float cx2,
                     float cy2, float x2, float y2);

bool AP_RenderCapsule(float x1, float y1, float x2, float y2, float radius);

bool AP_RenderFillCapsule(float x1, float y1, float x2, float y2, float radius);

bool AP_RenderCross(float x, float y, float size);

bool AP_RenderGrid(const AP_FRect *rect, int columns, int rows);

bool AP_RenderLinesClosed(const AP_FPoint *points, int count, bool closed);

/* =========================================================
 * Geometry (SDL_RenderGeometry)
 *
 * indices may be NULL. When NULL, vertices are a triangle list
 * and num_indices is ignored.
 * ========================================================= */

bool AP_RenderGeometry(const AP_Vertex *vertices, int num_vertices,
                       const int *indices, int num_indices);

bool AP_RenderPrimitives(const AP_Vertex *vertices, int count,
                         AP_Primitive primitive);

bool AP_RenderGeometryRaw(const AP_FPoint *xy, const AP_FColor *color,
                          int num_vertices);

/* =========================================================
 * Transform
 * ========================================================= */

bool AP_SetRenderRotation(float degrees);

float AP_GetRenderRotation(void);

bool AP_SetRenderRotationOrigin(float x, float y);

bool AP_GetRenderRotationOrigin(float *x, float *y);

bool AP_SetRenderTranslation(float x, float y);

bool AP_RenderTranslate(float x, float y);

bool AP_GetRenderTranslation(float *x, float *y);

bool AP_SetRenderTransform(const AP_Transform *transform);

bool AP_GetRenderTransform(AP_Transform *transform);

bool AP_ResetRenderTransform(void);

bool AP_PushRenderTransform(void);

bool AP_PopRenderTransform(void);

/* =========================================================
 * Viewport / clip
 *
 * NULL viewport resets to the full framebuffer.
 * NULL clip disables clipping.
 * ========================================================= */

bool AP_SetRenderViewport(const AP_Rect *rect);

bool AP_GetRenderViewport(AP_Rect *rect);

bool AP_RenderViewportSet(void);

bool AP_SetRenderClipRect(const AP_Rect *rect);

bool AP_GetRenderClipRect(AP_Rect *rect);

bool AP_RenderClipEnabled(void);

/* =========================================================
 * Compatibility names
 * ========================================================= */

#ifndef AP2_RENDERER_NO_SHORT_NAMES
#define AP_SetDrawColor AP_SetRenderDrawColorFloat
#define AP_SetDrawColor8 AP_SetRenderDrawColor
#define AP_GetDrawColor AP_GetRenderDrawColorFloat
#define AP_SetColorMod AP_SetRenderColorScale
#define AP_GetColorMod AP_GetRenderColorScale
#define AP_SetBlendMode AP_SetRenderDrawBlendMode
#define AP_GetBlendMode AP_GetRenderDrawBlendMode
#define AP_Clear AP_RenderClear
#define AP_Flush AP_FlushRenderer
#define AP_Present AP_RenderPresent
#define AP_SetDrawFlags AP_SetRenderDrawFlags
#define AP_GetDrawFlags AP_GetRenderDrawFlags
#define AP_EnableDrawFlag AP_EnableRenderDrawFlag
#define AP_DisableDrawFlag AP_DisableRenderDrawFlag
#define AP_HasDrawFlag AP_RenderDrawFlagEnabled
#define AP_SetPointSize AP_SetRenderPointSize
#define AP_GetPointSize AP_GetRenderPointSize
#define AP_SetLineWidth AP_SetRenderLineWidth
#define AP_GetLineWidth AP_GetRenderLineWidth
#define AP_SetLineCap AP_SetRenderLineCap
#define AP_GetLineCap AP_GetRenderLineCap
#define AP_SetLineJoin AP_SetRenderLineJoin
#define AP_GetLineJoin AP_GetRenderLineJoin
#define AP_SetCircleSegments AP_SetRenderCircleSegments
#define AP_GetCircleSegments AP_GetRenderCircleSegments
#define AP_DrawPointF AP_RenderPoint
#define AP_DrawPointsF AP_RenderPoints
#define AP_DrawLineF AP_RenderLine
#define AP_DrawLinesF AP_RenderLines
#define AP_DrawPolyline AP_RenderLinesClosed
#define AP_DrawCircleF AP_RenderCircle
#define AP_FillCircleF AP_RenderFillCircle
#define AP_DrawEllipseF AP_RenderEllipse
#define AP_FillEllipseF AP_RenderFillEllipse
#define AP_DrawArcF AP_RenderArc
#define AP_FillPieF AP_RenderFillPie
#define AP_FillRingF AP_RenderFillRing
#define AP_DrawTriangleF AP_RenderTriangle
#define AP_FillTriangleF AP_RenderFillTriangle
#define AP_FillTriangleColor AP_RenderFillTriangleColor
#define AP_DrawPolygon AP_RenderPolygon
#define AP_FillPolygon AP_RenderFillPolygon
#define AP_DrawQuadraticBezier AP_RenderQuadraticBezier
#define AP_DrawBezier AP_RenderBezier
#define AP_DrawCapsule AP_RenderCapsule
#define AP_FillCapsule AP_RenderFillCapsule
#define AP_SetRotation AP_SetRenderRotation
#define AP_GetRotation AP_GetRenderRotation
#define AP_SetRotationOrigin AP_SetRenderRotationOrigin
#define AP_GetRotationOrigin AP_GetRenderRotationOrigin
#define AP_SetTranslation AP_SetRenderTranslation
#define AP_Translate AP_RenderTranslate
#define AP_GetTranslation AP_GetRenderTranslation
#define AP_SetTransform AP_SetRenderTransform
#define AP_GetTransform AP_GetRenderTransform
#define AP_ResetTransform AP_ResetRenderTransform
#define AP_PushTransform AP_PushRenderTransform
#define AP_PopTransform AP_PopRenderTransform
#define AP_SetScale AP_SetRenderScale
#define AP_GetScale AP_GetRenderScale
#define AP_IsClipEnabled AP_RenderClipEnabled
#define AP_DrawGeometry AP_RenderGeometryRaw
#define AP_DrawVertices AP_RenderGeometry
#define AP_DrawRect AP_RenderRect
#define AP_DrawRects AP_RenderRects
#define AP_FillRect AP_RenderFillRect
#define AP_FillRects AP_RenderFillRects
#define AP_FillRectGradient AP_RenderFillRectGradient
#define AP_DrawRoundedRect AP_RenderRoundedRect
#define AP_FillRoundedRect AP_RenderFillRoundedRect
#define AP_DrawNGon AP_RenderNGon
#define AP_FillNGon AP_RenderFillNGon
#define AP_DrawStar AP_RenderStar
#define AP_FillStar AP_RenderFillStar
#define AP_DrawCross AP_RenderCross
#define AP_DrawGrid AP_RenderGrid
#define AP_DrawRing AP_RenderRing
#define AP_SetViewport AP_SetRenderViewport
#define AP_GetViewport AP_GetRenderViewport
#define AP_SetClipRect AP_SetRenderClipRect
#define AP_GetClipRect AP_GetRenderClipRect
#endif /* AP2_RENDERER_NO_SHORT_NAMES */

#ifdef __cplusplus
}
#endif

#endif /* AP2_RENDERER_H */
