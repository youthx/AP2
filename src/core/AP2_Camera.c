/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Camera.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Renderer.h"
#include "AP2/AP2_Window.h"

#include <math.h>

static int g_2d_depth = 0;

/* =========================================================
 * Internals
 * ========================================================= */

static AP_Camera AP_CameraMake(AP_CameraMode mode, AP_Vec3 position,
                               AP_Vec3 target) {
  AP_Camera camera;

  camera.mode = mode;
  camera.position = position;
  camera.target = target;
  camera.up = AP_Vec3Up();
  camera.zoom = 1.0f;
  camera.rotation = 0.0f;
  camera.fov_degrees = mode == AP_CAMERA_2D ? 0.0f : 60.0f;
  camera.ortho_size = 10.0f;
  camera.near_z = 0.1f;
  camera.far_z = 250.0f;
  camera.offset = AP_Vec2Zero();
  camera.bounds.x = 0.0f;
  camera.bounds.y = 0.0f;
  camera.bounds.w = 0.0f;
  camera.bounds.h = 0.0f;
  camera.center = mode == AP_CAMERA_2D;
  camera.deadzone = 0.0f;
  return camera;
}

static AP_F32 AP_CameraZoomValue(const AP_Camera *camera) {
  if (camera == NULL || camera->zoom <= 0.0f) {
    return 1.0f;
  }
  return camera->zoom;
}

static bool AP_CameraQueryViewSize(AP_F32 *out_w, AP_F32 *out_h) {
  AP_Rect viewport;
  int width;
  int height;

  if (out_w == NULL || out_h == NULL) {
    return false;
  }

  if (AP_GetRenderViewport(&viewport) && viewport.w > 0 && viewport.h > 0) {
    *out_w = (AP_F32)viewport.w;
    *out_h = (AP_F32)viewport.h;
    return true;
  }

  AP_ClearError();
  width = AP_GetWindowPixelWidth();
  height = AP_GetWindowPixelHeight();
  if (width > 0 && height > 0) {
    *out_w = (AP_F32)width;
    *out_h = (AP_F32)height;
    return true;
  }

  *out_w = 1.0f;
  *out_h = 1.0f;
  return false;
}

static void AP_CameraResolveViewSize(AP_F32 *view_w, AP_F32 *view_h) {
  AP_F32 width;
  AP_F32 height;

  if (view_w == NULL || view_h == NULL) {
    return;
  }

  if (*view_w > 0.0f && *view_h > 0.0f) {
    return;
  }

  if (AP_CameraQueryViewSize(&width, &height)) {
    if (*view_w <= 0.0f) {
      *view_w = width;
    }
    if (*view_h <= 0.0f) {
      *view_h = height;
    }
    return;
  }

  if (*view_w <= 0.0f) {
    *view_w = 1.0f;
  }
  if (*view_h <= 0.0f) {
    *view_h = 1.0f;
  }
}

static AP_Vec2 AP_CameraFocus2D(const AP_Camera *camera, AP_F32 view_w,
                                AP_F32 view_h) {
  AP_Vec2 offset = camera != NULL ? camera->offset : AP_Vec2Zero();

  if (camera != NULL && camera->center) {
    return AP_V2(view_w * 0.5f + offset.x, view_h * 0.5f + offset.y);
  }

  return offset;
}

static AP_F32 AP_CameraFollowT(AP_F32 speed, AP_F32 dt) {
  if (dt <= 0.0f) {
    return 0.0f;
  }
  if (speed <= 0.0f) {
    return 1.0f;
  }
  return 1.0f - expf(-speed * dt);
}

static bool AP_CameraHasBounds(const AP_Camera *camera) {
  return camera != NULL && camera->bounds.w > 0.0f && camera->bounds.h > 0.0f;
}

static void AP_CameraSetXY(AP_Vec3 *v, AP_Vec2 xy) {
  v->x = xy.x;
  v->y = xy.y;
}

/* =========================================================
 * Constructors
 * ========================================================= */

AP_Camera AP_Camera2D(AP_Vec2 target) {
  AP_Camera camera =
      AP_CameraMake(AP_CAMERA_2D, AP_V3(target.x, target.y, 0.0f),
                    AP_V3(target.x, target.y, 0.0f));
  return camera;
}

AP_Camera AP_Camera2DEx(AP_Vec2 target, AP_Vec2 offset, AP_F32 zoom,
                        AP_F32 rotation) {
  AP_Camera camera = AP_Camera2D(target);
  camera.offset = offset;
  camera.zoom = zoom > 0.0f ? zoom : 1.0f;
  camera.rotation = rotation;
  camera.center = false;
  return camera;
}

AP_Camera AP_CameraDefault2D(void) { return AP_Camera2D(AP_Vec2Zero()); }

AP_Camera AP_CameraPerspective(AP_Vec3 position, AP_Vec3 target,
                               AP_F32 fov_degrees) {
  AP_Camera camera = AP_CameraMake(AP_CAMERA_3D, position, target);
  camera.fov_degrees = fov_degrees > 0.0f ? fov_degrees : 60.0f;
  return camera;
}

AP_Camera AP_CameraOrtho(AP_Vec3 position, AP_Vec3 target, AP_F32 ortho_size) {
  AP_Camera camera = AP_CameraPerspective(position, target, 0.0f);
  camera.fov_degrees = 0.0f;
  camera.ortho_size = ortho_size > 0.0f ? ortho_size : 10.0f;
  return camera;
}

AP_Camera AP_CameraDefault(void) {
  return AP_CameraPerspective(AP_V3(0.0f, 4.0f, 8.0f), AP_Vec3Zero(), 50.0f);
}

/* =========================================================
 * 3D matrices
 * ========================================================= */

AP_Mat4 AP_CameraView(const AP_Camera *camera) {
  if (camera == NULL) {
    return AP_Mat4Identity();
  }

  if (camera->mode == AP_CAMERA_2D) {
    AP_Vec3 eye = AP_V3(camera->position.x, camera->position.y, 8.0f);
    AP_Vec3 at = AP_V3(camera->position.x, camera->position.y, 0.0f);
    AP_Vec3 up =
        AP_Vec3LengthSq(camera->up) > AP_EPSILON ? camera->up : AP_Vec3Up();
    return AP_Mat4LookAt(eye, at, up);
  }

  return AP_Mat4LookAt(camera->position, camera->target, camera->up);
}

AP_Mat4 AP_CameraProjection(const AP_Camera *camera, AP_F32 aspect) {
  if (camera == NULL) {
    return AP_Mat4Identity();
  }

  if (camera->fov_degrees <= 0.0f) {
    return AP_Mat4OrthoSize(camera->ortho_size, aspect, camera->near_z,
                            camera->far_z);
  }

  return AP_Mat4Perspective(camera->fov_degrees, aspect, camera->near_z,
                            camera->far_z);
}

AP_Mat4 AP_CameraViewProjection(const AP_Camera *camera, AP_F32 aspect) {
  return AP_Mat4Mul(AP_CameraProjection(camera, aspect), AP_CameraView(camera));
}

AP_Vec3 AP_CameraForward(const AP_Camera *camera) {
  AP_Vec3 forward;

  if (camera == NULL) {
    return AP_Vec3Forward();
  }

  if (camera->mode == AP_CAMERA_2D) {
    return AP_V3(0.0f, 0.0f, -1.0f);
  }

  forward = AP_Vec3Sub(camera->target, camera->position);
  if (AP_Vec3LengthSq(forward) <= AP_EPSILON) {
    return AP_Vec3Forward();
  }
  return AP_Vec3Normalize(forward);
}

AP_Vec3 AP_CameraRight(const AP_Camera *camera) {
  AP_Vec3 forward = AP_CameraForward(camera);
  AP_Vec3 up = camera != NULL && AP_Vec3LengthSq(camera->up) > AP_EPSILON
                   ? camera->up
                   : AP_Vec3Up();
  AP_Vec3 right = AP_Vec3Cross(forward, up);

  if (AP_Vec3LengthSq(right) <= AP_EPSILON) {
    return AP_Vec3Right();
  }
  return AP_Vec3Normalize(right);
}

/* =========================================================
 * 2D transform
 * ========================================================= */

AP_Transform AP_CameraTransform2D(const AP_Camera *camera, AP_F32 view_w,
                                  AP_F32 view_h) {
  AP_Transform transform;
  AP_Vec2 position;
  AP_Vec2 focus;
  AP_F32 zoom;

  AP_CameraResolveViewSize(&view_w, &view_h);
  position = AP_CameraPosition2D(camera);
  focus = AP_CameraFocus2D(camera, view_w, view_h);
  zoom = AP_CameraZoomValue(camera);

  transform.translate_x = focus.x - position.x;
  transform.translate_y = focus.y - position.y;
  transform.scale_x = zoom;
  transform.scale_y = zoom;
  transform.rotation = camera != NULL ? camera->rotation : 0.0f;
  transform.origin_x = position.x;
  transform.origin_y = position.y;
  return transform;
}

AP_Mat4 AP_CameraMatrix2D(const AP_Camera *camera, AP_F32 view_w,
                          AP_F32 view_h) {
  AP_Vec2 position;
  AP_Vec2 focus;
  AP_F32 zoom;
  AP_F32 rotation;
  AP_Mat4 view;
  AP_Mat4 proj;

  AP_CameraResolveViewSize(&view_w, &view_h);
  position = AP_CameraPosition2D(camera);
  focus = AP_CameraFocus2D(camera, view_w, view_h);
  zoom = AP_CameraZoomValue(camera);
  rotation = camera != NULL ? camera->rotation : 0.0f;

  view = AP_Mat4Translate(AP_V3(-position.x, -position.y, 0.0f));
  view = AP_Mat4Mul(AP_Mat4Scale(AP_V3(zoom, zoom, 1.0f)), view);
  view = AP_Mat4Mul(AP_Mat4RotateZ(rotation), view);
  view = AP_Mat4Mul(AP_Mat4Translate(AP_V3(focus.x, focus.y, 0.0f)), view);

  proj = AP_Mat4Ortho(0.0f, view_w, view_h, 0.0f, -1.0f, 1.0f);
  return AP_Mat4Mul(proj, view);
}

AP_Vec2 AP_CameraWorldToScreen2D(const AP_Camera *camera, AP_Vec2 world,
                                 AP_F32 view_w, AP_F32 view_h) {
  AP_Vec2 position;
  AP_Vec2 focus;
  AP_Vec2 scaled;
  AP_F32 zoom;
  AP_F32 rotation;

  AP_CameraResolveViewSize(&view_w, &view_h);
  position = AP_CameraPosition2D(camera);
  focus = AP_CameraFocus2D(camera, view_w, view_h);
  zoom = AP_CameraZoomValue(camera);
  rotation = camera != NULL ? camera->rotation : 0.0f;

  scaled = AP_Vec2Scale(AP_Vec2Sub(world, position), zoom);
  return AP_Vec2Add(AP_Vec2Rotate(scaled, rotation), focus);
}

AP_Vec2 AP_CameraScreenToWorld2D(const AP_Camera *camera, AP_Vec2 screen,
                                 AP_F32 view_w, AP_F32 view_h) {
  AP_Vec2 position;
  AP_Vec2 focus;
  AP_Vec2 local;
  AP_F32 zoom;
  AP_F32 rotation;

  AP_CameraResolveViewSize(&view_w, &view_h);
  position = AP_CameraPosition2D(camera);
  focus = AP_CameraFocus2D(camera, view_w, view_h);
  zoom = AP_CameraZoomValue(camera);
  rotation = camera != NULL ? camera->rotation : 0.0f;

  local = AP_Vec2Rotate(AP_Vec2Sub(screen, focus), -rotation);
  local = AP_Vec2Scale(local, 1.0f / zoom);
  return AP_Vec2Add(local, position);
}

AP_FRect AP_CameraViewRect2D(const AP_Camera *camera, AP_F32 view_w,
                             AP_F32 view_h) {
  AP_Vec2 corners[4];
  AP_F32 min_x;
  AP_F32 min_y;
  AP_F32 max_x;
  AP_F32 max_y;
  AP_FRect rect;
  int i;

  AP_CameraResolveViewSize(&view_w, &view_h);
  corners[0] = AP_CameraScreenToWorld2D(camera, AP_Vec2Zero(), view_w, view_h);
  corners[1] =
      AP_CameraScreenToWorld2D(camera, AP_V2(view_w, 0.0f), view_w, view_h);
  corners[2] =
      AP_CameraScreenToWorld2D(camera, AP_V2(view_w, view_h), view_w, view_h);
  corners[3] =
      AP_CameraScreenToWorld2D(camera, AP_V2(0.0f, view_h), view_w, view_h);

  min_x = max_x = corners[0].x;
  min_y = max_y = corners[0].y;
  for (i = 1; i < 4; ++i) {
    min_x = AP_Minf(min_x, corners[i].x);
    min_y = AP_Minf(min_y, corners[i].y);
    max_x = AP_Maxf(max_x, corners[i].x);
    max_y = AP_Maxf(max_y, corners[i].y);
  }

  rect.x = min_x;
  rect.y = min_y;
  rect.w = max_x - min_x;
  rect.h = max_y - min_y;
  return rect;
}

AP_Vec2 AP_CameraPosition2D(const AP_Camera *camera) {
  if (camera == NULL) {
    return AP_Vec2Zero();
  }
  return AP_V2(camera->position.x, camera->position.y);
}

bool AP_CameraSetPosition2D(AP_Camera *camera, AP_Vec2 position) {
  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  AP_CameraSetXY(&camera->position, position);
  AP_CameraSetXY(&camera->target, position);
  return true;
}

bool AP_CameraSetZoom(AP_Camera *camera, AP_F32 zoom) {
  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  camera->zoom = zoom > 0.0f ? zoom : 1.0f;
  return true;
}

bool AP_CameraZoomAt2D(AP_Camera *camera, AP_Vec2 world, AP_F32 zoom,
                       AP_F32 view_w, AP_F32 view_h) {
  AP_Vec2 screen;
  AP_Vec2 shifted;
  AP_Vec2 position;

  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  AP_CameraResolveViewSize(&view_w, &view_h);
  screen = AP_CameraWorldToScreen2D(camera, world, view_w, view_h);
  AP_CameraSetZoom(camera, zoom);
  shifted = AP_CameraScreenToWorld2D(camera, screen, view_w, view_h);
  position =
      AP_Vec2Add(AP_CameraPosition2D(camera), AP_Vec2Sub(world, shifted));
  AP_CameraSetXY(&camera->position, position);
  return true;
}

/* =========================================================
 * Tracking
 * ========================================================= */

bool AP_CameraLookAt(AP_Camera *camera, AP_Vec3 target) {
  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  if (camera->mode == AP_CAMERA_2D) {
    return AP_CameraLookAt2D(camera, AP_V2(target.x, target.y));
  }

  camera->target = target;
  return true;
}

bool AP_CameraLookAt2D(AP_Camera *camera, AP_Vec2 target) {
  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  AP_CameraSetXY(&camera->position, target);
  AP_CameraSetXY(&camera->target, target);
  return true;
}

bool AP_CameraFollow(AP_Camera *camera, AP_Vec3 subject, AP_F32 speed,
                     AP_F32 dt) {
  AP_Vec3 offset;
  AP_Vec3 delta;
  AP_F32 distance;
  AP_F32 t;

  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  if (camera->mode == AP_CAMERA_2D) {
    return AP_CameraFollow2D(camera, AP_V2(subject.x, subject.y), speed, dt);
  }

  delta = AP_Vec3Sub(subject, camera->target);
  distance = AP_Vec3Length(delta);
  if (camera->deadzone > 0.0f && distance <= camera->deadzone) {
    return true;
  }

  t = AP_CameraFollowT(speed, dt);
  offset = AP_Vec3Sub(camera->position, camera->target);
  camera->target = AP_Vec3Lerp(camera->target, subject, t);
  camera->position = AP_Vec3Add(camera->target, offset);
  return true;
}

bool AP_CameraFollow2D(AP_Camera *camera, AP_Vec2 subject, AP_F32 speed,
                       AP_F32 dt) {
  AP_Vec2 position;
  AP_Vec2 delta;
  AP_F32 distance;
  AP_F32 t;

  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  AP_CameraSetXY(&camera->target, subject);
  position = AP_CameraPosition2D(camera);
  delta = AP_Vec2Sub(subject, position);
  distance = AP_Vec2Length(delta);
  if (camera->deadzone > 0.0f && distance <= camera->deadzone) {
    return AP_CameraClamp2D(camera, 0.0f, 0.0f);
  }

  t = AP_CameraFollowT(speed, dt);
  position = AP_Vec2Lerp(position, subject, t);
  AP_CameraSetXY(&camera->position, position);
  return AP_CameraClamp2D(camera, 0.0f, 0.0f);
}

bool AP_CameraSetBounds(AP_Camera *camera, AP_FRect bounds) {
  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  camera->bounds = bounds;
  return true;
}

bool AP_CameraClamp2D(AP_Camera *camera, AP_F32 view_w, AP_F32 view_h) {
  AP_Vec2 position;
  AP_Vec2 min_pos;
  AP_Vec2 max_pos;
  AP_FRect view;

  if (camera == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Camera cannot be NULL");
    return false;
  }

  if (!AP_CameraHasBounds(camera)) {
    return true;
  }

  AP_CameraResolveViewSize(&view_w, &view_h);
  view = AP_CameraViewRect2D(camera, view_w, view_h);
  position = AP_CameraPosition2D(camera);

  min_pos.x = camera->bounds.x + (position.x - view.x);
  min_pos.y = camera->bounds.y + (position.y - view.y);
  max_pos.x =
      camera->bounds.x + camera->bounds.w - (view.x + view.w - position.x);
  max_pos.y =
      camera->bounds.y + camera->bounds.h - (view.y + view.h - position.y);

  if (max_pos.x < min_pos.x) {
    position.x = camera->bounds.x + camera->bounds.w * 0.5f;
  } else {
    position.x = AP_Clampf(position.x, min_pos.x, max_pos.x);
  }

  if (max_pos.y < min_pos.y) {
    position.y = camera->bounds.y + camera->bounds.h * 0.5f;
  } else {
    position.y = AP_Clampf(position.y, min_pos.y, max_pos.y);
  }

  AP_CameraSetXY(&camera->position, position);
  return true;
}

/* =========================================================
 * 2D pass
 * ========================================================= */

bool AP_ApplyCamera2D(const AP_Camera *camera) {
  AP_Transform transform;
  AP_F32 view_w;
  AP_F32 view_h;
  AP_Camera fallback;

  if (camera == NULL) {
    fallback = AP_CameraDefault2D();
    camera = &fallback;
  }

  if (!AP_CameraQueryViewSize(&view_w, &view_h)) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "2D camera requires a viewport");
    return false;
  }

  transform = AP_CameraTransform2D(camera, view_w, view_h);
  return AP_SetRenderTransform(&transform);
}

bool AP_Begin2D(const AP_Camera *camera) {
  AP_RendererFlushCurrent();

  if (!AP_PushRenderTransform()) {
    return false;
  }

  if (!AP_ApplyCamera2D(camera)) {
    AP_PopRenderTransform();
    return false;
  }

  g_2d_depth += 1;
  return true;
}

void AP_End2D(void) {
  if (g_2d_depth <= 0) {
    return;
  }

  AP_RendererFlushCurrent();
  AP_PopRenderTransform();
  g_2d_depth -= 1;
}

bool AP_Is2D(void) { return g_2d_depth > 0; }

#include "AP2/AP2_Camera.h"
#include "AP2/AP2_Math.h"

/* =========================================================
 * Basic setters
 * ========================================================= */

bool AP_CameraSetPosition(AP_Camera *camera, AP_Vec3 position) {
  if (!camera)
    return false;
  camera->position = position;
  return true;
}

void AP_CameraTranslate(AP_Camera *camera, AP_Vec3 delta) {
  if (!camera)
    return;
  camera->position = AP_Vec3Add(camera->position, delta);
  camera->target = AP_Vec3Add(camera->target, delta);
}

/* =========================================================
 * Local-space movement
 * ========================================================= */

void AP_CameraMoveLocal(AP_Camera *camera, AP_F32 forward, AP_F32 right,
                        AP_F32 up) {
  if (!camera)
    return;

  AP_Vec3 f = AP_CameraForward(camera);
  AP_Vec3 r = AP_CameraRight(camera);
  AP_Vec3 u = camera->up;

  AP_Vec3 delta = AP_V3(f.x * forward + r.x * right + u.x * up,
                        f.y * forward + r.y * right + u.y * up,
                        f.z * forward + r.z * right + u.z * up);

  camera->position = AP_Vec3Add(camera->position, delta);
  camera->target = AP_Vec3Add(camera->target, delta);
}

/* =========================================================
 * Yaw/pitch rotation
 * ========================================================= */

void AP_CameraRotateYawPitch(AP_Camera *camera, AP_F32 yaw_deg,
                             AP_F32 pitch_deg) {
  if (!camera)
    return;

  AP_F32 yaw = AP_DegToRad(yaw_deg);
  AP_F32 pitch = AP_DegToRad(pitch_deg);

  AP_Vec3 dir = {cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch)};

  camera->target = AP_Vec3Add(camera->position, dir);
}

AP_Vec2 AP_CameraYawPitch(const AP_Camera *camera) {
  AP_Vec3 f = AP_CameraForward(camera);

  AP_F32 yaw = atan2f(f.z, f.x);
  AP_F32 pitch = asinf(f.y);

  return AP_V2(AP_RadToDeg(yaw), AP_RadToDeg(pitch));
}

void AP_CameraSetYawPitch(AP_Camera *camera, AP_F32 yaw_deg, AP_F32 pitch_deg) {
  if (!camera)
    return;

  AP_F32 yaw = AP_DegToRad(yaw_deg);
  AP_F32 pitch = AP_DegToRad(pitch_deg);

  AP_Vec3 dir = {cosf(yaw) * cosf(pitch), sinf(pitch), sinf(yaw) * cosf(pitch)};

  camera->target = AP_Vec3Add(camera->position, dir);
}

/* =========================================================
 * View-projection
 * ========================================================= */

AP_Mat4 AP_CameraVP(const AP_Camera *camera, AP_F32 aspect) {
  AP_Mat4 v = AP_CameraView(camera);
  AP_Mat4 p = AP_CameraProjection(camera, aspect);
  return AP_Mat4Mul(p, v);
}

/* =========================================================
 * Screen ray (for picking)
 * ========================================================= */

AP_Ray AP_CameraScreenRay(const AP_Camera *camera, AP_F32 aspect,
                          AP_Vec2 screen01) {
  AP_Ray ray;
  ray.origin = camera->position;

  AP_Mat4 proj = AP_CameraProjection(camera, aspect);
  AP_Mat4 view = AP_CameraView(camera);
  AP_Mat4 inv;
  AP_Mat4Inverse(AP_Mat4Mul(proj, view), &inv);

  AP_Vec4 ndc = {screen01.x * 2.0f - 1.0f, 1.0f - screen01.y * 2.0f, 1.0f,
                 1.0f};

  AP_Vec4 world = AP_Mat4MulVec4(inv, ndc);
  world.x /= world.w;
  world.y /= world.w;
  world.z /= world.w;

  ray.direction = AP_Vec3Normalize(AP_V3(world.x - camera->position.x,
                                         world.y - camera->position.y,
                                         world.z - camera->position.z));

  return ray;
}
