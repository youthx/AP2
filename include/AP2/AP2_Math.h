/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_MATH_H
#define AP2_MATH_H

#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Math
 *
 * Scalar, vector, matrix, and quaternion helpers for 2D and 3D.
 * Matrices are column-major, matching GLSL / OpenGL:
 *
 *     AP_Mat4 mvp = AP_Mat4Mul(proj, AP_Mat4Mul(view, model));
 *     AP_SetUniformMat4("u_mvp", mvp.m, false);
 *
 * 3D space is right-handed and Y-up. This is independent of the
 * 2D renderer, which uses a top-left origin. Mixing them is normal.
 *
 * Exclude with AP2_NO_MATH before including <AP2/AP2.h>.
 */

#ifndef AP_PI
#define AP_PI 3.14159265358979323846f
#endif

#ifndef AP_TAU
#define AP_TAU (AP_PI * 2.0f) /* a full turn. π is the half-constant. */
#endif

#ifndef AP_DEG2RAD
#define AP_DEG2RAD (AP_PI / 180.0f)
#endif

#ifndef AP_RAD2DEG
#define AP_RAD2DEG (180.0f / AP_PI)
#endif

#ifndef AP_EPSILON
#define AP_EPSILON 1.0e-6f
#endif

/* =========================================================
 * Types
 *
 * Mat3 / Mat4 store columns in m[0..], so element (row, col)
 * is m[col * 3 + row] / m[col * 4 + row].
 * Quaternion is (x, y, z, w) with w as the scalar.
 * ========================================================= */

typedef struct AP_Mat3 {
  AP_F32 m[9];
} AP_Mat3;

typedef struct AP_Mat4 {
  AP_F32 m[16];
} AP_Mat4;

typedef struct AP_Quat {
  AP_F32 x;
  AP_F32 y;
  AP_F32 z;
  AP_F32 w;
} AP_Quat;

typedef struct AP_Ray {
  AP_Vec3 origin;
  AP_Vec3 direction;
} AP_Ray;

typedef struct AP_AABB {
  AP_Vec3 min;
  AP_Vec3 max;
} AP_AABB;

typedef struct AP_Plane {
  AP_Vec3 normal;
  AP_F32 distance;
} AP_Plane;

/* =========================================================
 * Scalar
 * ========================================================= */

/* =========================================================
 * Angle / Radian Helpers
 * ========================================================= */

AP_F32 AP_SmoothDampF(AP_F32 current, AP_F32 target, AP_F32 *velocity,
                      AP_F32 smooth_time, AP_F32 dt);

AP_Vec3 AP_SmoothDampV3(AP_Vec3 current, AP_Vec3 target, AP_Vec3 *velocity,
                        AP_F32 smooth_time, AP_F32 dt);
AP_Quat AP_SmoothDampQuat(AP_Quat current, AP_Quat target, AP_F32 *velocity,
                          AP_F32 smooth_time, AP_F32 dt);
AP_F32 AP_Radians(AP_F32 degrees);
AP_F32 AP_Degrees(AP_F32 radians);

/* Normalize angle to [0, 360) */
AP_F32 AP_AngleNormalize360(AP_F32 degrees);

/* Normalize angle to [-180, 180) */
AP_F32 AP_AngleNormalize180(AP_F32 degrees);

/* Shortest angle difference (degrees) */
AP_F32 AP_AngleDelta(AP_F32 a_deg, AP_F32 b_deg);

/* Smooth damp angle (degrees) */
AP_F32 AP_AngleSmoothDamp(AP_F32 current, AP_F32 target, AP_F32 *velocity,
                          AP_F32 smooth_time, AP_F32 dt);

AP_F32 AP_DegToRad(AP_F32 degrees);

AP_F32 AP_RadToDeg(AP_F32 radians);

AP_F32 AP_Absf(AP_F32 value);

AP_F32 AP_Signf(AP_F32 value);

AP_F32 AP_Minf(AP_F32 a, AP_F32 b);

AP_F32 AP_Maxf(AP_F32 a, AP_F32 b);

AP_F32 AP_Clampf(AP_F32 value, AP_F32 minimum, AP_F32 maximum);

AP_F32 AP_Saturate(AP_F32 value);

AP_F32 AP_Lerpf(AP_F32 a, AP_F32 b, AP_F32 t);

AP_F32 AP_LerpAngle(AP_F32 a_deg, AP_F32 b_deg, AP_F32 t);

AP_F32 AP_Smoothstep(AP_F32 edge0, AP_F32 edge1, AP_F32 x);

AP_F32 AP_Smootherstep(AP_F32 edge0, AP_F32 edge1, AP_F32 x);

AP_F32 AP_Wrapf(AP_F32 value, AP_F32 minimum, AP_F32 maximum);

AP_F32 AP_Repeat(AP_F32 value, AP_F32 length);

AP_F32 AP_PingPong(AP_F32 value, AP_F32 length);

AP_F32 AP_MoveTowards(AP_F32 current, AP_F32 target, AP_F32 max_delta);

bool AP_Approximately(AP_F32 a, AP_F32 b);

/* =========================================================
 * Constructors
 * ========================================================= */

AP_Vec2 AP_V2(AP_F32 x, AP_F32 y);

AP_Vec3 AP_V3(AP_F32 x, AP_F32 y, AP_F32 z);

AP_Vec4 AP_V4(AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w);

AP_Color AP_C4(AP_F32 r, AP_F32 g, AP_F32 b, AP_F32 a);

AP_Quat AP_Q4(AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w);

/* =========================================================
 * Vec2
 * ========================================================= */

AP_Vec2 AP_Vec2Zero(void);

AP_Vec2 AP_Vec2One(void);

AP_Vec2 AP_Vec2Add(AP_Vec2 a, AP_Vec2 b);

AP_Vec2 AP_Vec2Sub(AP_Vec2 a, AP_Vec2 b);

AP_Vec2 AP_Vec2Mul(AP_Vec2 a, AP_Vec2 b);

AP_Vec2 AP_Vec2Scale(AP_Vec2 v, AP_F32 s);

AP_Vec2 AP_Vec2Negate(AP_Vec2 v);

AP_F32 AP_Vec2Dot(AP_Vec2 a, AP_Vec2 b);

AP_F32 AP_Vec2Cross(AP_Vec2 a, AP_Vec2 b);

AP_F32 AP_Vec2LengthSq(AP_Vec2 v);

AP_F32 AP_Vec2Length(AP_Vec2 v);

AP_F32 AP_Vec2Distance(AP_Vec2 a, AP_Vec2 b);

AP_Vec2 AP_Vec2Normalize(AP_Vec2 v);

AP_Vec2 AP_Vec2Lerp(AP_Vec2 a, AP_Vec2 b, AP_F32 t);

AP_Vec2 AP_Vec2Reflect(AP_Vec2 v, AP_Vec2 normal);

AP_Vec2 AP_Vec2Rotate(AP_Vec2 v, AP_F32 degrees);

AP_F32 AP_Vec2Angle(AP_Vec2 v);

AP_Vec2 AP_Vec2FromAngle(AP_F32 degrees);

bool AP_Vec2Equal(AP_Vec2 a, AP_Vec2 b);

/* =========================================================
 * Vec3
 * ========================================================= */

AP_Vec3 AP_Vec3Zero(void);

AP_Vec3 AP_Vec3One(void);

AP_Vec3 AP_Vec3Up(void);

AP_Vec3 AP_Vec3Right(void);

AP_Vec3 AP_Vec3Forward(void);

AP_Vec3 AP_Vec3Add(AP_Vec3 a, AP_Vec3 b);

AP_Vec3 AP_Vec3Sub(AP_Vec3 a, AP_Vec3 b);

AP_Vec3 AP_Vec3Mul(AP_Vec3 a, AP_Vec3 b);

AP_Vec3 AP_Vec3Scale(AP_Vec3 v, AP_F32 s);

AP_Vec3 AP_Vec3Negate(AP_Vec3 v);

AP_F32 AP_Vec3Dot(AP_Vec3 a, AP_Vec3 b);

AP_Vec3 AP_Vec3Cross(AP_Vec3 a, AP_Vec3 b);

AP_F32 AP_Vec3LengthSq(AP_Vec3 v);

AP_F32 AP_Vec3Length(AP_Vec3 v);

AP_F32 AP_Vec3Distance(AP_Vec3 a, AP_Vec3 b);

AP_Vec3 AP_Vec3Normalize(AP_Vec3 v);

AP_Vec3 AP_Vec3Lerp(AP_Vec3 a, AP_Vec3 b, AP_F32 t);

AP_Vec3 AP_Vec3Reflect(AP_Vec3 v, AP_Vec3 normal);

AP_Vec3 AP_Vec3Project(AP_Vec3 v, AP_Vec3 onto);

AP_Vec3 AP_Vec3Reject(AP_Vec3 v, AP_Vec3 onto);

bool AP_Vec3Equal(AP_Vec3 a, AP_Vec3 b);

/* =========================================================
 * Vec4
 * ========================================================= */

AP_Vec4 AP_Vec4Zero(void);

AP_Vec4 AP_Vec4Add(AP_Vec4 a, AP_Vec4 b);

AP_Vec4 AP_Vec4Sub(AP_Vec4 a, AP_Vec4 b);

AP_Vec4 AP_Vec4Scale(AP_Vec4 v, AP_F32 s);

AP_F32 AP_Vec4Dot(AP_Vec4 a, AP_Vec4 b);

AP_F32 AP_Vec4Length(AP_Vec4 v);

AP_Vec4 AP_Vec4Normalize(AP_Vec4 v);

AP_Vec4 AP_Vec4Lerp(AP_Vec4 a, AP_Vec4 b, AP_F32 t);

/* =========================================================
 * Mat3
 * ========================================================= */

AP_Mat3 AP_Mat3Identity(void);

AP_Mat3 AP_Mat3Mul(AP_Mat3 a, AP_Mat3 b);

AP_Mat3 AP_Mat3Transpose(AP_Mat3 m);

bool AP_Mat3Inverse(AP_Mat3 m, AP_Mat3 *out);

AP_Vec2 AP_Mat3TransformPoint(AP_Mat3 m, AP_Vec2 p);

AP_Vec3 AP_Mat3MulVec3(AP_Mat3 m, AP_Vec3 v);

/* =========================================================
 * Mat4
 * ========================================================= */

AP_Mat4 AP_Mat4Identity(void);

AP_Mat4 AP_Mat4Mul(AP_Mat4 a, AP_Mat4 b);

AP_Mat4 AP_Mat4Transpose(AP_Mat4 m);

bool AP_Mat4Inverse(AP_Mat4 m, AP_Mat4 *out);

AP_Mat4 AP_Mat4Translate(AP_Vec3 translation);

AP_Mat4 AP_Mat4Scale(AP_Vec3 scale);

AP_Mat4 AP_Mat4RotateX(AP_F32 degrees);

AP_Mat4 AP_Mat4RotateY(AP_F32 degrees);

AP_Mat4 AP_Mat4RotateZ(AP_F32 degrees);

AP_Mat4 AP_Mat4RotateAxis(AP_Vec3 axis, AP_F32 degrees);

AP_Mat4 AP_Mat4TRS(AP_Vec3 translation, AP_Quat rotation, AP_Vec3 scale);

AP_Mat4 AP_Mat4LookAt(AP_Vec3 eye, AP_Vec3 target, AP_Vec3 up);

AP_Mat4 AP_Mat4Perspective(AP_F32 fov_degrees, AP_F32 aspect, AP_F32 near_z,
                           AP_F32 far_z);

AP_Mat4 AP_Mat4Ortho(AP_F32 left, AP_F32 right, AP_F32 bottom, AP_F32 top,
                     AP_F32 near_z, AP_F32 far_z);

AP_Mat4 AP_Mat4OrthoSize(AP_F32 height, AP_F32 aspect, AP_F32 near_z,
                         AP_F32 far_z);

AP_Vec3 AP_Mat4TransformPoint(AP_Mat4 m, AP_Vec3 p);

AP_Vec3 AP_Mat4TransformVector(AP_Mat4 m, AP_Vec3 v);

AP_Vec4 AP_Mat4MulVec4(AP_Mat4 m, AP_Vec4 v);

AP_Mat3 AP_Mat4NormalMatrix(AP_Mat4 model);

AP_Vec3 AP_Mat4GetTranslation(AP_Mat4 m);

/* =========================================================
 * Quaternion
 * ========================================================= */

AP_Quat AP_QuatIdentity(void);

AP_Quat AP_QuatNormalize(AP_Quat q);

AP_Quat AP_QuatConjugate(AP_Quat q);

AP_Quat AP_QuatInverse(AP_Quat q);

AP_Quat AP_QuatMul(AP_Quat a, AP_Quat b);

AP_Quat AP_QuatFromAxisAngle(AP_Vec3 axis, AP_F32 degrees);

/*
 * Euler angles in degrees, applied Y (yaw), X (pitch), Z (roll).
 */
AP_Quat AP_QuatFromEuler(AP_F32 pitch_deg, AP_F32 yaw_deg, AP_F32 roll_deg);

AP_Vec3 AP_QuatToEuler(AP_Quat q);

AP_Quat AP_QuatSlerp(AP_Quat a, AP_Quat b, AP_F32 t);

AP_Vec3 AP_QuatRotate(AP_Quat q, AP_Vec3 v);

AP_Mat4 AP_QuatToMat4(AP_Quat q);

AP_Quat AP_QuatFromMat4(AP_Mat4 m);

/* =========================================================
 * Ray / AABB / plane
 * ========================================================= */

AP_Ray AP_RayCreate(AP_Vec3 origin, AP_Vec3 direction);

AP_AABB AP_AABBCreate(AP_Vec3 min, AP_Vec3 max);

AP_AABB AP_AABBFromCenter(AP_Vec3 center, AP_Vec3 extents);

bool AP_AABBContains(AP_AABB box, AP_Vec3 point);

AP_AABB AP_AABBUnion(AP_AABB a, AP_AABB b);

AP_Plane AP_PlaneCreate(AP_Vec3 normal, AP_F32 distance);

AP_F32 AP_PlaneDistanceToPoint(AP_Plane plane, AP_Vec3 point);

bool AP_RayIntersectPlane(AP_Ray ray, AP_Plane plane, AP_F32 *out_t);

bool AP_RayIntersectAABB(AP_Ray ray, AP_AABB box, AP_F32 *out_t);

#ifdef __cplusplus
}
#endif

#endif /* AP2_MATH_H */
