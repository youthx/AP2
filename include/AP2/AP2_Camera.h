/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_CAMERA_H
#define AP2_CAMERA_H

#include "AP2/AP2_Math.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Camera
 *
 * One camera for 2D and 3D. 2D is Y-down (renderer space). 3D is
 * right-handed Y-up look-at. `target` is what you follow; `position`
 * is where the camera actually is. Lerp those and you have tracking.
 *
 *     AP_Camera cam = AP_Camera2D(AP_V2(player.x, player.y));
 *     AP_CameraFollow2D(&cam, AP_V2(player.x, player.y), 8.0f, dt);
 *
 *     AP_Begin2D(&cam);
 *     AP_DrawSprite(&sprite, player.x, player.y);
 *     AP_End2D();
 *
 *     AP_Camera cam3 = AP_CameraPerspective(
 *         AP_V3(0.0f, 4.0f, 8.0f), AP_V3(0.0f, 0.0f, 0.0f), 50.0f);
 *     AP_Begin3D(&cam3);
 *
 * fov_degrees > 0 selects perspective. fov_degrees == 0 selects
 * orthographic using ortho_size as the vertical world size.
 *
 * Exclude with AP2_NO_CAMERA. Requires math. 2D apply requires the
 * renderer. 3D still goes through AP_Begin3D().
 */

typedef enum AP_CameraMode { AP_CAMERA_2D = 0, AP_CAMERA_3D } AP_CameraMode;

typedef struct AP_Camera {
  AP_CameraMode mode;

  AP_Vec3 position; /* 2D: current look point (world xy). 3D: eye. */
  AP_Vec3 target;   /* 2D: follow goal (world xy). 3D: look-at. */
  AP_Vec3 up;       /* 3D only. Ignored by the 2D pass. */

  AP_F32 zoom;        /* 2D: 1 = 1:1 pixels. >1 zooms in. */
  AP_F32 rotation;    /* 2D: degrees clockwise. Unused by 3D look-at. */
  AP_F32 fov_degrees; /* 3D: >0 perspective, 0 orthographic. */
  AP_F32 ortho_size;  /* 3D orthographic vertical size. */
  AP_F32 near_z;
  AP_F32 far_z;

  AP_Vec2 offset;  /* 2D screen point that position maps to. */
  AP_FRect bounds; /* 2D world clamp. w or h <= 0 = unbounded. */
  bool center;     /* 2D: map position to viewport center + offset. */
  AP_F32 deadzone; /* Follow slack in world units. 0 = always pull. */
} AP_Camera;

/* =========================================================
 * Constructors
 * ========================================================= */

AP_Camera AP_Camera2D(AP_Vec2 target);

AP_Camera AP_Camera2DEx(AP_Vec2 target, AP_Vec2 offset, AP_F32 zoom,
                        AP_F32 rotation);

AP_Camera AP_CameraDefault2D(void);

AP_Camera AP_CameraPerspective(AP_Vec3 position, AP_Vec3 target,
                               AP_F32 fov_degrees);

AP_Camera AP_CameraOrtho(AP_Vec3 position, AP_Vec3 target, AP_F32 ortho_size);

AP_Camera AP_CameraDefault(void);

/* =========================================================
 * 3D matrices
 * ========================================================= */

AP_Mat4 AP_CameraView(const AP_Camera *camera);

AP_Mat4 AP_CameraProjection(const AP_Camera *camera, AP_F32 aspect);

AP_Mat4 AP_CameraViewProjection(const AP_Camera *camera, AP_F32 aspect);

AP_Vec3 AP_CameraForward(const AP_Camera *camera);

AP_Vec3 AP_CameraRight(const AP_Camera *camera);

/* =========================================================
 * 2D transform
 *
 * view_w / view_h <= 0 reads the current viewport (then window).
 * ========================================================= */

AP_Transform AP_CameraTransform2D(const AP_Camera *camera, AP_F32 view_w,
                                  AP_F32 view_h);

AP_Mat4 AP_CameraMatrix2D(const AP_Camera *camera, AP_F32 view_w,
                          AP_F32 view_h);

AP_Vec2 AP_CameraWorldToScreen2D(const AP_Camera *camera, AP_Vec2 world,
                                 AP_F32 view_w, AP_F32 view_h);

AP_Vec2 AP_CameraScreenToWorld2D(const AP_Camera *camera, AP_Vec2 screen,
                                 AP_F32 view_w, AP_F32 view_h);

AP_FRect AP_CameraViewRect2D(const AP_Camera *camera, AP_F32 view_w,
                             AP_F32 view_h);

AP_Vec2 AP_CameraPosition2D(const AP_Camera *camera);

bool AP_CameraSetPosition2D(AP_Camera *camera, AP_Vec2 position);

bool AP_CameraSetZoom(AP_Camera *camera, AP_F32 zoom);

/*
 * Change zoom while keeping `world` glued to the same screen point.
 */
bool AP_CameraZoomAt2D(AP_Camera *camera, AP_Vec2 world, AP_F32 zoom,
                       AP_F32 view_w, AP_F32 view_h);

/* =========================================================
 * Tracking
 *
 * Follow speed is exponential: 0 snaps, 8 is a typical chase,
 * huge values also snap. dt is seconds. Deadzone is applied
 * before the lerp. 2D bounds clamp after the move.
 * ========================================================= */

bool AP_CameraLookAt(AP_Camera *camera, AP_Vec3 target);

bool AP_CameraLookAt2D(AP_Camera *camera, AP_Vec2 target);

bool AP_CameraFollow(AP_Camera *camera, AP_Vec3 subject, AP_F32 speed,
                     AP_F32 dt);

bool AP_CameraFollow2D(AP_Camera *camera, AP_Vec2 subject, AP_F32 speed,
                       AP_F32 dt);

bool AP_CameraSetBounds(AP_Camera *camera, AP_FRect bounds);

bool AP_CameraClamp2D(AP_Camera *camera, AP_F32 view_w, AP_F32 view_h);

/* =========================================================
 * 2D pass
 *
 * Pushes the renderer transform, applies the camera, then
 * pops on End. Draw HUD after End2D. NULL uses a default 2D
 * camera at the origin, centered on the viewport.
 * ========================================================= */

bool AP_Begin2D(const AP_Camera *camera);

void AP_End2D(void);

bool AP_Is2D(void);

bool AP_ApplyCamera2D(const AP_Camera *camera);

#ifdef __cplusplus
}
#endif

#endif /* AP2_CAMERA_H */
