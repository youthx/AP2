/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Math.h"

#include <math.h>
#include <string.h>

/* =========================================================
 * Scalar
 * ========================================================= */

 AP_F32 AP_Radians(AP_F32 degrees) {
  return degrees * AP_DEG2RAD;
}

AP_F32 AP_Degrees(AP_F32 radians) {
  return radians * AP_RAD2DEG;
}

AP_F32 AP_AngleNormalize360(AP_F32 degrees) {
  AP_F32 r = fmodf(degrees, 360.0f);
  return (r < 0.0f) ? r + 360.0f : r;
}

AP_F32 AP_AngleNormalize180(AP_F32 degrees) {
  AP_F32 r = AP_AngleNormalize360(degrees);
  return (r > 180.0f) ? r - 360.0f : r;
}

AP_F32 AP_AngleDelta(AP_F32 a_deg, AP_F32 b_deg) {
  return AP_AngleNormalize180(b_deg - a_deg);
}

AP_F32 AP_SmoothDampF(AP_F32 current, AP_F32 target,
  AP_F32 *velocity, AP_F32 smooth_time, AP_F32 dt)
{
smooth_time = AP_Maxf(0.0001f, smooth_time);
AP_F32 omega = 2.0f / smooth_time;

AP_F32 x = omega * dt;
AP_F32 exp = 1.0f / (1.0f + x + 0.48f*x*x + 0.235f*x*x*x);

AP_F32 change = current - target;
AP_F32 temp = (*velocity + omega * change) * dt;

*velocity = (*velocity - omega * temp) * exp;

return target + (change + temp) * exp;
}

AP_Vec3 AP_SmoothDampV3(AP_Vec3 current, AP_Vec3 target,
    AP_Vec3 *velocity, AP_F32 smooth_time, AP_F32 dt)
{
AP_Vec3 result;
result.x = AP_SmoothDampF(current.x, target.x, &velocity->x, smooth_time, dt);
result.y = AP_SmoothDampF(current.y, target.y, &velocity->y, smooth_time, dt);
result.z = AP_SmoothDampF(current.z, target.z, &velocity->z, smooth_time, dt);
return result;
}

AP_Quat AP_SmoothDampQuat(AP_Quat current, AP_Quat target,
      AP_F32 *velocity, AP_F32 smooth_time, AP_F32 dt)
{
AP_F32 t = AP_SmoothDampF(0.0f, 1.0f, velocity, smooth_time, dt);
return AP_QuatSlerp(current, target, t);
}

AP_F32 AP_AngleSmoothDamp(AP_F32 current, AP_F32 target,
                        AP_F32 *velocity, AP_F32 smooth_time, AP_F32 dt)
{
  target = current + AP_AngleDelta(current, target);
  return AP_SmoothDampF(current, target, velocity, smooth_time, dt);
}

AP_F32 AP_DegToRad(AP_F32 degrees) { return degrees * AP_DEG2RAD; }

AP_F32 AP_RadToDeg(AP_F32 radians) { return radians * AP_RAD2DEG; }

AP_F32 AP_Absf(AP_F32 value) { return value < 0.0f ? -value : value; }

AP_F32 AP_Signf(AP_F32 value) {
  if (value > 0.0f) {
    return 1.0f;
  }
  if (value < 0.0f) {
    return -1.0f;
  }
  return 0.0f;
}

AP_F32 AP_Minf(AP_F32 a, AP_F32 b) { return a < b ? a : b; }

AP_F32 AP_Maxf(AP_F32 a, AP_F32 b) { return a > b ? a : b; }

AP_F32 AP_Clampf(AP_F32 value, AP_F32 minimum, AP_F32 maximum) {
  if (value < minimum) {
    return minimum;
  }
  if (value > maximum) {
    return maximum;
  }
  return value;
}

AP_F32 AP_Saturate(AP_F32 value) { return AP_Clampf(value, 0.0f, 1.0f); }

AP_F32 AP_Lerpf(AP_F32 a, AP_F32 b, AP_F32 t) { return a + (b - a) * t; }

AP_F32 AP_LerpAngle(AP_F32 a_deg, AP_F32 b_deg, AP_F32 t) {
  AP_F32 delta = AP_Wrapf(b_deg - a_deg, -180.0f, 180.0f);
  return a_deg + delta * t;
}

AP_F32 AP_Smoothstep(AP_F32 edge0, AP_F32 edge1, AP_F32 x) {
  AP_F32 t;
  if (edge1 - edge0 == 0.0f) {
    return x < edge0 ? 0.0f : 1.0f;
  }
  t = AP_Saturate((x - edge0) / (edge1 - edge0));
  return t * t * (3.0f - 2.0f * t);
}

AP_F32 AP_Smootherstep(AP_F32 edge0, AP_F32 edge1, AP_F32 x) {
  AP_F32 t;
  if (edge1 - edge0 == 0.0f) {
    return x < edge0 ? 0.0f : 1.0f;
  }
  t = AP_Saturate((x - edge0) / (edge1 - edge0));
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

AP_F32 AP_Wrapf(AP_F32 value, AP_F32 minimum, AP_F32 maximum) {
  AP_F32 range = maximum - minimum;
  AP_F32 result;

  if (range == 0.0f) {
    return minimum;
  }

  result = value - range * floorf((value - minimum) / range);
  if (result >= maximum) {
    result = minimum;
  }
  return result;
}

AP_F32 AP_Repeat(AP_F32 value, AP_F32 length) {
  if (length == 0.0f) {
    return 0.0f;
  }
  return AP_Wrapf(value, 0.0f, length);
}

AP_F32 AP_PingPong(AP_F32 value, AP_F32 length) {
  AP_F32 cycle;
  AP_F32 t;

  if (length == 0.0f) {
    return 0.0f;
  }

  cycle = AP_Repeat(value, length * 2.0f);
  t = cycle;
  if (t > length) {
    t = length * 2.0f - t;
  }
  return t;
}

AP_F32 AP_MoveTowards(AP_F32 current, AP_F32 target, AP_F32 max_delta) {
  AP_F32 delta = target - current;
  if (AP_Absf(delta) <= max_delta) {
    return target;
  }
  return current + AP_Signf(delta) * max_delta;
}

bool AP_Approximately(AP_F32 a, AP_F32 b) {
  return AP_Absf(a - b) <=
         AP_EPSILON * AP_Maxf(1.0f, AP_Maxf(AP_Absf(a), AP_Absf(b)));
}

/* =========================================================
 * Constructors
 * ========================================================= */

AP_Vec2 AP_V2(AP_F32 x, AP_F32 y) {
  AP_Vec2 v;
  v.x = x;
  v.y = y;
  return v;
}

AP_Vec3 AP_V3(AP_F32 x, AP_F32 y, AP_F32 z) {
  AP_Vec3 v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}

AP_Vec4 AP_V4(AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w) {
  AP_Vec4 v;
  v.x = x;
  v.y = y;
  v.z = z;
  v.w = w;
  return v;
}

AP_Color AP_C4(AP_F32 r, AP_F32 g, AP_F32 b, AP_F32 a) {
  AP_Color c;
  c.r = r;
  c.g = g;
  c.b = b;
  c.a = a;
  return c;
}

AP_Quat AP_Q4(AP_F32 x, AP_F32 y, AP_F32 z, AP_F32 w) {
  AP_Quat q;
  q.x = x;
  q.y = y;
  q.z = z;
  q.w = w;
  return q;
}

/* =========================================================
 * Vec2
 * ========================================================= */

AP_Vec2 AP_Vec2Zero(void) { return AP_V2(0.0f, 0.0f); }

AP_Vec2 AP_Vec2One(void) { return AP_V2(1.0f, 1.0f); }

AP_Vec2 AP_Vec2Add(AP_Vec2 a, AP_Vec2 b) { return AP_V2(a.x + b.x, a.y + b.y); }

AP_Vec2 AP_Vec2Sub(AP_Vec2 a, AP_Vec2 b) { return AP_V2(a.x - b.x, a.y - b.y); }

AP_Vec2 AP_Vec2Mul(AP_Vec2 a, AP_Vec2 b) { return AP_V2(a.x * b.x, a.y * b.y); }

AP_Vec2 AP_Vec2Scale(AP_Vec2 v, AP_F32 s) { return AP_V2(v.x * s, v.y * s); }

AP_Vec2 AP_Vec2Negate(AP_Vec2 v) { return AP_V2(-v.x, -v.y); }

AP_F32 AP_Vec2Dot(AP_Vec2 a, AP_Vec2 b) { return a.x * b.x + a.y * b.y; }

AP_F32 AP_Vec2Cross(AP_Vec2 a, AP_Vec2 b) { return a.x * b.y - a.y * b.x; }

AP_F32 AP_Vec2LengthSq(AP_Vec2 v) { return AP_Vec2Dot(v, v); }

AP_F32 AP_Vec2Length(AP_Vec2 v) { return sqrtf(AP_Vec2LengthSq(v)); }

AP_F32 AP_Vec2Distance(AP_Vec2 a, AP_Vec2 b) {
  return AP_Vec2Length(AP_Vec2Sub(a, b));
}

AP_Vec2 AP_Vec2Normalize(AP_Vec2 v) {
  AP_F32 length = AP_Vec2Length(v);
  if (length <= AP_EPSILON) {
    return AP_Vec2Zero();
  }
  return AP_Vec2Scale(v, 1.0f / length);
}

AP_Vec2 AP_Vec2Lerp(AP_Vec2 a, AP_Vec2 b, AP_F32 t) {
  return AP_V2(AP_Lerpf(a.x, b.x, t), AP_Lerpf(a.y, b.y, t));
}

AP_Vec2 AP_Vec2Reflect(AP_Vec2 v, AP_Vec2 normal) {
  return AP_Vec2Sub(v, AP_Vec2Scale(normal, 2.0f * AP_Vec2Dot(v, normal)));
}

AP_Vec2 AP_Vec2Rotate(AP_Vec2 v, AP_F32 degrees) {
  AP_F32 r = AP_DegToRad(degrees);
  AP_F32 c = cosf(r);
  AP_F32 s = sinf(r);
  return AP_V2(v.x * c - v.y * s, v.x * s + v.y * c);
}

AP_F32 AP_Vec2Angle(AP_Vec2 v) { return AP_RadToDeg(atan2f(v.y, v.x)); }

AP_Vec2 AP_Vec2FromAngle(AP_F32 degrees) {
  AP_F32 r = AP_DegToRad(degrees);
  return AP_V2(cosf(r), sinf(r));
}

bool AP_Vec2Equal(AP_Vec2 a, AP_Vec2 b) {
  return AP_Approximately(a.x, b.x) && AP_Approximately(a.y, b.y);
}

/* =========================================================
 * Vec3
 * ========================================================= */

AP_Vec3 AP_Vec3Zero(void) { return AP_V3(0.0f, 0.0f, 0.0f); }

AP_Vec3 AP_Vec3One(void) { return AP_V3(1.0f, 1.0f, 1.0f); }

AP_Vec3 AP_Vec3Up(void) { return AP_V3(0.0f, 1.0f, 0.0f); }

AP_Vec3 AP_Vec3Right(void) { return AP_V3(1.0f, 0.0f, 0.0f); }

AP_Vec3 AP_Vec3Forward(void) { return AP_V3(0.0f, 0.0f, -1.0f); }

AP_Vec3 AP_Vec3Add(AP_Vec3 a, AP_Vec3 b) {
  return AP_V3(a.x + b.x, a.y + b.y, a.z + b.z);
}

AP_Vec3 AP_Vec3Sub(AP_Vec3 a, AP_Vec3 b) {
  return AP_V3(a.x - b.x, a.y - b.y, a.z - b.z);
}

AP_Vec3 AP_Vec3Mul(AP_Vec3 a, AP_Vec3 b) {
  return AP_V3(a.x * b.x, a.y * b.y, a.z * b.z);
}

AP_Vec3 AP_Vec3Scale(AP_Vec3 v, AP_F32 s) {
  return AP_V3(v.x * s, v.y * s, v.z * s);
}

AP_Vec3 AP_Vec3Negate(AP_Vec3 v) { return AP_V3(-v.x, -v.y, -v.z); }

AP_F32 AP_Vec3Dot(AP_Vec3 a, AP_Vec3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

AP_Vec3 AP_Vec3Cross(AP_Vec3 a, AP_Vec3 b) {
  return AP_V3(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x);
}

AP_F32 AP_Vec3LengthSq(AP_Vec3 v) { return AP_Vec3Dot(v, v); }

AP_F32 AP_Vec3Length(AP_Vec3 v) { return sqrtf(AP_Vec3LengthSq(v)); }

AP_F32 AP_Vec3Distance(AP_Vec3 a, AP_Vec3 b) {
  return AP_Vec3Length(AP_Vec3Sub(a, b));
}

AP_Vec3 AP_Vec3Normalize(AP_Vec3 v) {
  AP_F32 length = AP_Vec3Length(v);
  if (length <= AP_EPSILON) {
    return AP_Vec3Zero();
  }
  return AP_Vec3Scale(v, 1.0f / length);
}

AP_Vec3 AP_Vec3Lerp(AP_Vec3 a, AP_Vec3 b, AP_F32 t) {
  return AP_V3(AP_Lerpf(a.x, b.x, t), AP_Lerpf(a.y, b.y, t),
               AP_Lerpf(a.z, b.z, t));
}

AP_Vec3 AP_Vec3Reflect(AP_Vec3 v, AP_Vec3 normal) {
  return AP_Vec3Sub(v, AP_Vec3Scale(normal, 2.0f * AP_Vec3Dot(v, normal)));
}

AP_Vec3 AP_Vec3Project(AP_Vec3 v, AP_Vec3 onto) {
  AP_F32 denom = AP_Vec3LengthSq(onto);
  if (denom <= AP_EPSILON) {
    return AP_Vec3Zero();
  }
  return AP_Vec3Scale(onto, AP_Vec3Dot(v, onto) / denom);
}

AP_Vec3 AP_Vec3Reject(AP_Vec3 v, AP_Vec3 onto) {
  return AP_Vec3Sub(v, AP_Vec3Project(v, onto));
}

bool AP_Vec3Equal(AP_Vec3 a, AP_Vec3 b) {
  return AP_Approximately(a.x, b.x) && AP_Approximately(a.y, b.y) &&
         AP_Approximately(a.z, b.z);
}

/* =========================================================
 * Vec4
 * ========================================================= */

AP_Vec4 AP_Vec4Zero(void) { return AP_V4(0.0f, 0.0f, 0.0f, 0.0f); }

AP_Vec4 AP_Vec4Add(AP_Vec4 a, AP_Vec4 b) {
  return AP_V4(a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w);
}

AP_Vec4 AP_Vec4Sub(AP_Vec4 a, AP_Vec4 b) {
  return AP_V4(a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w);
}

AP_Vec4 AP_Vec4Scale(AP_Vec4 v, AP_F32 s) {
  return AP_V4(v.x * s, v.y * s, v.z * s, v.w * s);
}

AP_F32 AP_Vec4Dot(AP_Vec4 a, AP_Vec4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

AP_F32 AP_Vec4Length(AP_Vec4 v) { return sqrtf(AP_Vec4Dot(v, v)); }

AP_Vec4 AP_Vec4Normalize(AP_Vec4 v) {
  AP_F32 length = AP_Vec4Length(v);
  if (length <= AP_EPSILON) {
    return AP_Vec4Zero();
  }
  return AP_Vec4Scale(v, 1.0f / length);
}

AP_Vec4 AP_Vec4Lerp(AP_Vec4 a, AP_Vec4 b, AP_F32 t) {
  return AP_V4(AP_Lerpf(a.x, b.x, t), AP_Lerpf(a.y, b.y, t),
               AP_Lerpf(a.z, b.z, t), AP_Lerpf(a.w, b.w, t));
}

/* =========================================================
 * Mat3
 * ========================================================= */

AP_Mat3 AP_Mat3Identity(void) {
  AP_Mat3 m;
  memset(&m, 0, sizeof(m));
  m.m[0] = 1.0f;
  m.m[4] = 1.0f;
  m.m[8] = 1.0f;
  return m;
}

AP_Mat3 AP_Mat3Mul(AP_Mat3 a, AP_Mat3 b) {
  AP_Mat3 r;
  int col;
  int row;

  for (col = 0; col < 3; ++col) {
    for (row = 0; row < 3; ++row) {
      r.m[col * 3 + row] = a.m[0 * 3 + row] * b.m[col * 3 + 0] +
                           a.m[1 * 3 + row] * b.m[col * 3 + 1] +
                           a.m[2 * 3 + row] * b.m[col * 3 + 2];
    }
  }

  return r;
}

AP_Mat3 AP_Mat3Transpose(AP_Mat3 m) {
  AP_Mat3 r;
  int col;
  int row;

  for (col = 0; col < 3; ++col) {
    for (row = 0; row < 3; ++row) {
      r.m[row * 3 + col] = m.m[col * 3 + row];
    }
  }

  return r;
}

bool AP_Mat3Inverse(AP_Mat3 m, AP_Mat3 *out) {
  AP_F32 a00 = m.m[0];
  AP_F32 a01 = m.m[3];
  AP_F32 a02 = m.m[6];
  AP_F32 a10 = m.m[1];
  AP_F32 a11 = m.m[4];
  AP_F32 a12 = m.m[7];
  AP_F32 a20 = m.m[2];
  AP_F32 a21 = m.m[5];
  AP_F32 a22 = m.m[8];
  AP_F32 det;
  AP_F32 inv;

  if (out == NULL) {
    return false;
  }

  det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
        a02 * (a10 * a21 - a11 * a20);

  if (AP_Absf(det) <= AP_EPSILON) {
    *out = AP_Mat3Identity();
    return false;
  }

  inv = 1.0f / det;
  out->m[0] = (a11 * a22 - a12 * a21) * inv;
  out->m[1] = (a12 * a20 - a10 * a22) * inv;
  out->m[2] = (a10 * a21 - a11 * a20) * inv;
  out->m[3] = (a02 * a21 - a01 * a22) * inv;
  out->m[4] = (a00 * a22 - a02 * a20) * inv;
  out->m[5] = (a01 * a20 - a00 * a21) * inv;
  out->m[6] = (a01 * a12 - a02 * a11) * inv;
  out->m[7] = (a02 * a10 - a00 * a12) * inv;
  out->m[8] = (a00 * a11 - a01 * a10) * inv;
  return true;
}

AP_Vec2 AP_Mat3TransformPoint(AP_Mat3 m, AP_Vec2 p) {
  return AP_V2(m.m[0] * p.x + m.m[3] * p.y + m.m[6],
               m.m[1] * p.x + m.m[4] * p.y + m.m[7]);
}

AP_Vec3 AP_Mat3MulVec3(AP_Mat3 m, AP_Vec3 v) {
  return AP_V3(m.m[0] * v.x + m.m[3] * v.y + m.m[6] * v.z,
               m.m[1] * v.x + m.m[4] * v.y + m.m[7] * v.z,
               m.m[2] * v.x + m.m[5] * v.y + m.m[8] * v.z);
}

/* =========================================================
 * Mat4
 * ========================================================= */

AP_Mat4 AP_Mat4Identity(void) {
  AP_Mat4 m;
  memset(&m, 0, sizeof(m));
  m.m[0] = 1.0f;
  m.m[5] = 1.0f;
  m.m[10] = 1.0f;
  m.m[15] = 1.0f;
  return m;
}

AP_Mat4 AP_Mat4Mul(AP_Mat4 a, AP_Mat4 b) {
  AP_Mat4 r;
  int col;
  int row;

  for (col = 0; col < 4; ++col) {
    for (row = 0; row < 4; ++row) {
      r.m[col * 4 + row] = a.m[0 * 4 + row] * b.m[col * 4 + 0] +
                           a.m[1 * 4 + row] * b.m[col * 4 + 1] +
                           a.m[2 * 4 + row] * b.m[col * 4 + 2] +
                           a.m[3 * 4 + row] * b.m[col * 4 + 3];
    }
  }

  return r;
}

AP_Mat4 AP_Mat4Transpose(AP_Mat4 m) {
  AP_Mat4 r;
  int col;
  int row;

  for (col = 0; col < 4; ++col) {
    for (row = 0; row < 4; ++row) {
      r.m[row * 4 + col] = m.m[col * 4 + row];
    }
  }

  return r;
}

bool AP_Mat4Inverse(AP_Mat4 m, AP_Mat4 *out) {
  AP_F32 inv[16];
  AP_F32 det;
  int i;

  if (out == NULL) {
    return false;
  }

  inv[0] = m.m[5] * m.m[10] * m.m[15] - m.m[5] * m.m[11] * m.m[14] -
           m.m[9] * m.m[6] * m.m[15] + m.m[9] * m.m[7] * m.m[14] +
           m.m[13] * m.m[6] * m.m[11] - m.m[13] * m.m[7] * m.m[10];
  inv[4] = -m.m[4] * m.m[10] * m.m[15] + m.m[4] * m.m[11] * m.m[14] +
           m.m[8] * m.m[6] * m.m[15] - m.m[8] * m.m[7] * m.m[14] -
           m.m[12] * m.m[6] * m.m[11] + m.m[12] * m.m[7] * m.m[10];
  inv[8] = m.m[4] * m.m[9] * m.m[15] - m.m[4] * m.m[11] * m.m[13] -
           m.m[8] * m.m[5] * m.m[15] + m.m[8] * m.m[7] * m.m[13] +
           m.m[12] * m.m[5] * m.m[11] - m.m[12] * m.m[7] * m.m[9];
  inv[12] = -m.m[4] * m.m[9] * m.m[14] + m.m[4] * m.m[10] * m.m[13] +
            m.m[8] * m.m[5] * m.m[14] - m.m[8] * m.m[6] * m.m[13] -
            m.m[12] * m.m[5] * m.m[10] + m.m[12] * m.m[6] * m.m[9];
  inv[1] = -m.m[1] * m.m[10] * m.m[15] + m.m[1] * m.m[11] * m.m[14] +
           m.m[9] * m.m[2] * m.m[15] - m.m[9] * m.m[3] * m.m[14] -
           m.m[13] * m.m[2] * m.m[11] + m.m[13] * m.m[3] * m.m[10];
  inv[5] = m.m[0] * m.m[10] * m.m[15] - m.m[0] * m.m[11] * m.m[14] -
           m.m[8] * m.m[2] * m.m[15] + m.m[8] * m.m[3] * m.m[14] +
           m.m[12] * m.m[2] * m.m[11] - m.m[12] * m.m[3] * m.m[10];
  inv[9] = -m.m[0] * m.m[9] * m.m[15] + m.m[0] * m.m[11] * m.m[13] +
           m.m[8] * m.m[1] * m.m[15] - m.m[8] * m.m[3] * m.m[13] -
           m.m[12] * m.m[1] * m.m[11] + m.m[12] * m.m[3] * m.m[9];
  inv[13] = m.m[0] * m.m[9] * m.m[14] - m.m[0] * m.m[10] * m.m[13] -
            m.m[8] * m.m[1] * m.m[14] + m.m[8] * m.m[2] * m.m[13] +
            m.m[12] * m.m[1] * m.m[10] - m.m[12] * m.m[2] * m.m[9];
  inv[2] = m.m[1] * m.m[6] * m.m[15] - m.m[1] * m.m[7] * m.m[14] -
           m.m[5] * m.m[2] * m.m[15] + m.m[5] * m.m[3] * m.m[14] +
           m.m[13] * m.m[2] * m.m[7] - m.m[13] * m.m[3] * m.m[6];
  inv[6] = -m.m[0] * m.m[6] * m.m[15] + m.m[0] * m.m[7] * m.m[14] +
           m.m[4] * m.m[2] * m.m[15] - m.m[4] * m.m[3] * m.m[14] -
           m.m[12] * m.m[2] * m.m[7] + m.m[12] * m.m[3] * m.m[6];
  inv[10] = m.m[0] * m.m[5] * m.m[15] - m.m[0] * m.m[7] * m.m[13] -
            m.m[4] * m.m[1] * m.m[15] + m.m[4] * m.m[3] * m.m[13] +
            m.m[12] * m.m[1] * m.m[7] - m.m[12] * m.m[3] * m.m[5];
  inv[14] = -m.m[0] * m.m[5] * m.m[14] + m.m[0] * m.m[6] * m.m[13] +
            m.m[4] * m.m[1] * m.m[14] - m.m[4] * m.m[2] * m.m[13] -
            m.m[12] * m.m[1] * m.m[6] + m.m[12] * m.m[2] * m.m[5];
  inv[3] = -m.m[1] * m.m[6] * m.m[11] + m.m[1] * m.m[7] * m.m[10] +
           m.m[5] * m.m[2] * m.m[11] - m.m[5] * m.m[3] * m.m[10] -
           m.m[9] * m.m[2] * m.m[7] + m.m[9] * m.m[3] * m.m[6];
  inv[7] = m.m[0] * m.m[6] * m.m[11] - m.m[0] * m.m[7] * m.m[10] -
           m.m[4] * m.m[2] * m.m[11] + m.m[4] * m.m[3] * m.m[10] +
           m.m[8] * m.m[2] * m.m[7] - m.m[8] * m.m[3] * m.m[6];
  inv[11] = -m.m[0] * m.m[5] * m.m[11] + m.m[0] * m.m[7] * m.m[9] +
            m.m[4] * m.m[1] * m.m[11] - m.m[4] * m.m[3] * m.m[9] -
            m.m[8] * m.m[1] * m.m[7] + m.m[8] * m.m[3] * m.m[5];
  inv[15] = m.m[0] * m.m[5] * m.m[10] - m.m[0] * m.m[6] * m.m[9] -
            m.m[4] * m.m[1] * m.m[10] + m.m[4] * m.m[2] * m.m[9] +
            m.m[8] * m.m[1] * m.m[6] - m.m[8] * m.m[2] * m.m[5];

  det = m.m[0] * inv[0] + m.m[1] * inv[4] + m.m[2] * inv[8] + m.m[3] * inv[12];
  if (AP_Absf(det) <= AP_EPSILON) {
    *out = AP_Mat4Identity();
    return false;
  }

  det = 1.0f / det;
  for (i = 0; i < 16; ++i) {
    out->m[i] = inv[i] * det;
  }

  return true;
}

AP_Mat4 AP_Mat4Translate(AP_Vec3 translation) {
  AP_Mat4 m = AP_Mat4Identity();
  m.m[12] = translation.x;
  m.m[13] = translation.y;
  m.m[14] = translation.z;
  return m;
}

AP_Mat4 AP_Mat4Scale(AP_Vec3 scale) {
  AP_Mat4 m = AP_Mat4Identity();
  m.m[0] = scale.x;
  m.m[5] = scale.y;
  m.m[10] = scale.z;
  return m;
}

AP_Mat4 AP_Mat4RotateX(AP_F32 degrees) {
  AP_Mat4 m = AP_Mat4Identity();
  AP_F32 r = AP_DegToRad(degrees);
  AP_F32 c = cosf(r);
  AP_F32 s = sinf(r);
  m.m[5] = c;
  m.m[6] = s;
  m.m[9] = -s;
  m.m[10] = c;
  return m;
}

AP_Mat4 AP_Mat4RotateY(AP_F32 degrees) {
  AP_Mat4 m = AP_Mat4Identity();
  AP_F32 r = AP_DegToRad(degrees);
  AP_F32 c = cosf(r);
  AP_F32 s = sinf(r);
  m.m[0] = c;
  m.m[2] = -s;
  m.m[8] = s;
  m.m[10] = c;
  return m;
}

AP_Mat4 AP_Mat4RotateZ(AP_F32 degrees) {
  AP_Mat4 m = AP_Mat4Identity();
  AP_F32 r = AP_DegToRad(degrees);
  AP_F32 c = cosf(r);
  AP_F32 s = sinf(r);
  m.m[0] = c;
  m.m[1] = s;
  m.m[4] = -s;
  m.m[5] = c;
  return m;
}

AP_Mat4 AP_Mat4RotateAxis(AP_Vec3 axis, AP_F32 degrees) {
  return AP_QuatToMat4(AP_QuatFromAxisAngle(axis, degrees));
}

AP_Mat4 AP_Mat4TRS(AP_Vec3 translation, AP_Quat rotation, AP_Vec3 scale) {
  AP_Mat4 t = AP_Mat4Translate(translation);
  AP_Mat4 r = AP_QuatToMat4(rotation);
  AP_Mat4 s = AP_Mat4Scale(scale);
  return AP_Mat4Mul(t, AP_Mat4Mul(r, s));
}

AP_Mat4 AP_Mat4LookAt(AP_Vec3 eye, AP_Vec3 target, AP_Vec3 up) {
  AP_Vec3 f = AP_Vec3Normalize(AP_Vec3Sub(target, eye));
  AP_Vec3 s = AP_Vec3Normalize(AP_Vec3Cross(f, up));
  AP_Vec3 u = AP_Vec3Cross(s, f);
  AP_Mat4 m = AP_Mat4Identity();

  if (AP_Vec3LengthSq(s) <= AP_EPSILON) {
    return m;
  }

  m.m[0] = s.x;
  m.m[4] = s.y;
  m.m[8] = s.z;
  m.m[1] = u.x;
  m.m[5] = u.y;
  m.m[9] = u.z;
  m.m[2] = -f.x;
  m.m[6] = -f.y;
  m.m[10] = -f.z;
  m.m[12] = -AP_Vec3Dot(s, eye);
  m.m[13] = -AP_Vec3Dot(u, eye);
  m.m[14] = AP_Vec3Dot(f, eye);
  return m;
}

AP_Mat4 AP_Mat4Perspective(AP_F32 fov_degrees, AP_F32 aspect, AP_F32 near_z,
                           AP_F32 far_z) {
  AP_Mat4 m;
  AP_F32 f;
  AP_F32 range;

  memset(&m, 0, sizeof(m));

  if (aspect <= AP_EPSILON || far_z == near_z) {
    return AP_Mat4Identity();
  }

  f = 1.0f / tanf(AP_DegToRad(fov_degrees) * 0.5f);
  range = near_z - far_z;
  m.m[0] = f / aspect;
  m.m[5] = f;
  m.m[10] = (far_z + near_z) / range;
  m.m[11] = -1.0f;
  m.m[14] = (2.0f * far_z * near_z) / range;
  return m;
}

AP_Mat4 AP_Mat4Ortho(AP_F32 left, AP_F32 right, AP_F32 bottom, AP_F32 top,
                     AP_F32 near_z, AP_F32 far_z) {
  AP_Mat4 m = AP_Mat4Identity();
  AP_F32 rl = right - left;
  AP_F32 tb = top - bottom;
  AP_F32 fn = far_z - near_z;

  if (AP_Absf(rl) <= AP_EPSILON || AP_Absf(tb) <= AP_EPSILON ||
      AP_Absf(fn) <= AP_EPSILON) {
    return m;
  }

  m.m[0] = 2.0f / rl;
  m.m[5] = 2.0f / tb;
  m.m[10] = -2.0f / fn;
  m.m[12] = -(right + left) / rl;
  m.m[13] = -(top + bottom) / tb;
  m.m[14] = -(far_z + near_z) / fn;
  return m;
}

AP_Mat4 AP_Mat4OrthoSize(AP_F32 height, AP_F32 aspect, AP_F32 near_z,
                         AP_F32 far_z) {
  AP_F32 half_h = height * 0.5f;
  AP_F32 half_w = half_h * aspect;
  return AP_Mat4Ortho(-half_w, half_w, -half_h, half_h, near_z, far_z);
}

AP_Vec4 AP_Mat4MulVec4(AP_Mat4 m, AP_Vec4 v) {
  return AP_V4(m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
               m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
               m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
               m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w);
}

AP_Vec3 AP_Mat4TransformPoint(AP_Mat4 m, AP_Vec3 p) {
  AP_Vec4 t = AP_Mat4MulVec4(m, AP_V4(p.x, p.y, p.z, 1.0f));
  if (AP_Absf(t.w) > AP_EPSILON) {
    return AP_V3(t.x / t.w, t.y / t.w, t.z / t.w);
  }
  return AP_V3(t.x, t.y, t.z);
}

AP_Vec3 AP_Mat4TransformVector(AP_Mat4 m, AP_Vec3 v) {
  AP_Vec4 t = AP_Mat4MulVec4(m, AP_V4(v.x, v.y, v.z, 0.0f));
  return AP_V3(t.x, t.y, t.z);
}

AP_Mat3 AP_Mat4NormalMatrix(AP_Mat4 model) {
  AP_Mat3 n;
  AP_Mat3 inverse;
  AP_Mat4 inv;

  n.m[0] = model.m[0];
  n.m[1] = model.m[1];
  n.m[2] = model.m[2];
  n.m[3] = model.m[4];
  n.m[4] = model.m[5];
  n.m[5] = model.m[6];
  n.m[6] = model.m[8];
  n.m[7] = model.m[9];
  n.m[8] = model.m[10];

  if (AP_Mat3Inverse(n, &inverse)) {
    return AP_Mat3Transpose(inverse);
  }

  if (AP_Mat4Inverse(model, &inv)) {
    n.m[0] = inv.m[0];
    n.m[1] = inv.m[4];
    n.m[2] = inv.m[8];
    n.m[3] = inv.m[1];
    n.m[4] = inv.m[5];
    n.m[5] = inv.m[9];
    n.m[6] = inv.m[2];
    n.m[7] = inv.m[6];
    n.m[8] = inv.m[10];
    return n;
  }

  return AP_Mat3Identity();
}

AP_Vec3 AP_Mat4GetTranslation(AP_Mat4 m) {
  return AP_V3(m.m[12], m.m[13], m.m[14]);
}

/* =========================================================
 * Quaternion
 * ========================================================= */

AP_Quat AP_QuatIdentity(void) { return AP_Q4(0.0f, 0.0f, 0.0f, 1.0f); }

AP_Quat AP_QuatNormalize(AP_Quat q) {
  AP_F32 length = sqrtf(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (length <= AP_EPSILON) {
    return AP_QuatIdentity();
  }
  return AP_Q4(q.x / length, q.y / length, q.z / length, q.w / length);
}

AP_Quat AP_QuatConjugate(AP_Quat q) { return AP_Q4(-q.x, -q.y, -q.z, q.w); }

AP_Quat AP_QuatInverse(AP_Quat q) {
  AP_F32 length_sq = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  AP_Quat conjugate;
  if (length_sq <= AP_EPSILON) {
    return AP_QuatIdentity();
  }
  conjugate = AP_QuatConjugate(q);
  return AP_Q4(conjugate.x / length_sq, conjugate.y / length_sq,
               conjugate.z / length_sq, conjugate.w / length_sq);
}

AP_Quat AP_QuatMul(AP_Quat a, AP_Quat b) {
  return AP_Q4(a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
               a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
               a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
               a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z);
}

AP_Quat AP_QuatFromAxisAngle(AP_Vec3 axis, AP_F32 degrees) {
  AP_Vec3 n = AP_Vec3Normalize(axis);
  AP_F32 half = AP_DegToRad(degrees) * 0.5f;
  AP_F32 s = sinf(half);
  return AP_QuatNormalize(AP_Q4(n.x * s, n.y * s, n.z * s, cosf(half)));
}

AP_Quat AP_QuatFromEuler(AP_F32 pitch_deg, AP_F32 yaw_deg, AP_F32 roll_deg) {
  AP_Quat yaw = AP_QuatFromAxisAngle(AP_Vec3Up(), yaw_deg);
  AP_Quat pitch = AP_QuatFromAxisAngle(AP_Vec3Right(), pitch_deg);
  AP_Quat roll = AP_QuatFromAxisAngle(AP_V3(0.0f, 0.0f, 1.0f), roll_deg);
  return AP_QuatNormalize(AP_QuatMul(yaw, AP_QuatMul(pitch, roll)));
}

AP_Vec3 AP_QuatToEuler(AP_Quat q) {
  AP_Quat n = AP_QuatNormalize(q);
  AP_F32 sinp = 2.0f * (n.w * n.x - n.z * n.y);
  AP_F32 pitch;
  AP_F32 yaw;
  AP_F32 roll;

  if (AP_Absf(sinp) >= 1.0f) {
    pitch = AP_RadToDeg(copysignf(AP_PI * 0.5f, sinp));
  } else {
    pitch = AP_RadToDeg(asinf(sinp));
  }

  yaw = AP_RadToDeg(atan2f(2.0f * (n.w * n.y + n.x * n.z),
                           1.0f - 2.0f * (n.x * n.x + n.y * n.y)));
  roll = AP_RadToDeg(atan2f(2.0f * (n.w * n.z + n.x * n.y),
                            1.0f - 2.0f * (n.x * n.x + n.z * n.z)));
  return AP_V3(pitch, yaw, roll);
}

AP_Quat AP_QuatSlerp(AP_Quat a, AP_Quat b, AP_F32 t) {
  AP_Quat qa = AP_QuatNormalize(a);
  AP_Quat qb = AP_QuatNormalize(b);
  AP_F32 dot = qa.x * qb.x + qa.y * qb.y + qa.z * qb.z + qa.w * qb.w;
  AP_F32 theta;
  AP_F32 sin_theta;
  AP_F32 wa;
  AP_F32 wb;

  if (dot < 0.0f) {
    qb = AP_Q4(-qb.x, -qb.y, -qb.z, -qb.w);
    dot = -dot;
  }

  if (dot > 0.9995f) {
    return AP_QuatNormalize(AP_Q4(AP_Lerpf(qa.x, qb.x, t),
                                  AP_Lerpf(qa.y, qb.y, t),
                                  AP_Lerpf(qa.z, qb.z, t),
                                  AP_Lerpf(qa.w, qb.w, t)));
  }

  theta = acosf(AP_Clampf(dot, -1.0f, 1.0f));
  sin_theta = sinf(theta);
  if (AP_Absf(sin_theta) <= AP_EPSILON) {
    return qa;
  }

  wa = sinf((1.0f - t) * theta) / sin_theta;
  wb = sinf(t * theta) / sin_theta;
  return AP_Q4(qa.x * wa + qb.x * wb, qa.y * wa + qb.y * wb,
               qa.z * wa + qb.z * wb, qa.w * wa + qb.w * wb);
}

AP_Vec3 AP_QuatRotate(AP_Quat q, AP_Vec3 v) {
  AP_Quat p = AP_Q4(v.x, v.y, v.z, 0.0f);
  AP_Quat r = AP_QuatMul(AP_QuatMul(q, p), AP_QuatInverse(q));
  return AP_V3(r.x, r.y, r.z);
}

AP_Mat4 AP_QuatToMat4(AP_Quat q) {
  AP_Quat n = AP_QuatNormalize(q);
  AP_F32 xx = n.x * n.x;
  AP_F32 yy = n.y * n.y;
  AP_F32 zz = n.z * n.z;
  AP_F32 xy = n.x * n.y;
  AP_F32 xz = n.x * n.z;
  AP_F32 yz = n.y * n.z;
  AP_F32 wx = n.w * n.x;
  AP_F32 wy = n.w * n.y;
  AP_F32 wz = n.w * n.z;
  AP_Mat4 m = AP_Mat4Identity();

  m.m[0] = 1.0f - 2.0f * (yy + zz);
  m.m[1] = 2.0f * (xy + wz);
  m.m[2] = 2.0f * (xz - wy);
  m.m[4] = 2.0f * (xy - wz);
  m.m[5] = 1.0f - 2.0f * (xx + zz);
  m.m[6] = 2.0f * (yz + wx);
  m.m[8] = 2.0f * (xz + wy);
  m.m[9] = 2.0f * (yz - wx);
  m.m[10] = 1.0f - 2.0f * (xx + yy);
  return m;
}

AP_Quat AP_QuatFromMat4(AP_Mat4 m) {
  AP_F32 trace = m.m[0] + m.m[5] + m.m[10];
  AP_Quat q;

  if (trace > 0.0f) {
    AP_F32 s = sqrtf(trace + 1.0f) * 2.0f;
    q.w = 0.25f * s;
    q.x = (m.m[6] - m.m[9]) / s;
    q.y = (m.m[8] - m.m[2]) / s;
    q.z = (m.m[1] - m.m[4]) / s;
  } else if (m.m[0] > m.m[5] && m.m[0] > m.m[10]) {
    AP_F32 s = sqrtf(1.0f + m.m[0] - m.m[5] - m.m[10]) * 2.0f;
    q.w = (m.m[6] - m.m[9]) / s;
    q.x = 0.25f * s;
    q.y = (m.m[4] + m.m[1]) / s;
    q.z = (m.m[8] + m.m[2]) / s;
  } else if (m.m[5] > m.m[10]) {
    AP_F32 s = sqrtf(1.0f + m.m[5] - m.m[0] - m.m[10]) * 2.0f;
    q.w = (m.m[8] - m.m[2]) / s;
    q.x = (m.m[4] + m.m[1]) / s;
    q.y = 0.25f * s;
    q.z = (m.m[9] + m.m[6]) / s;
  } else {
    AP_F32 s = sqrtf(1.0f + m.m[10] - m.m[0] - m.m[5]) * 2.0f;
    q.w = (m.m[1] - m.m[4]) / s;
    q.x = (m.m[8] + m.m[2]) / s;
    q.y = (m.m[9] + m.m[6]) / s;
    q.z = 0.25f * s;
  }

  return AP_QuatNormalize(q);
}

/* =========================================================
 * Ray / AABB / plane
 * ========================================================= */

AP_Ray AP_RayCreate(AP_Vec3 origin, AP_Vec3 direction) {
  AP_Ray ray;
  ray.origin = origin;
  ray.direction = AP_Vec3Normalize(direction);
  return ray;
}

AP_AABB AP_AABBCreate(AP_Vec3 min, AP_Vec3 max) {
  AP_AABB box;
  box.min = AP_V3(AP_Minf(min.x, max.x), AP_Minf(min.y, max.y),
                  AP_Minf(min.z, max.z));
  box.max = AP_V3(AP_Maxf(min.x, max.x), AP_Maxf(min.y, max.y),
                  AP_Maxf(min.z, max.z));
  return box;
}

AP_AABB AP_AABBFromCenter(AP_Vec3 center, AP_Vec3 extents) {
  AP_Vec3 half = AP_V3(AP_Absf(extents.x), AP_Absf(extents.y), AP_Absf(extents.z));
  return AP_AABBCreate(AP_Vec3Sub(center, half), AP_Vec3Add(center, half));
}

bool AP_AABBContains(AP_AABB box, AP_Vec3 point) {
  return point.x >= box.min.x && point.x <= box.max.x && point.y >= box.min.y &&
         point.y <= box.max.y && point.z >= box.min.z && point.z <= box.max.z;
}

AP_AABB AP_AABBUnion(AP_AABB a, AP_AABB b) {
  return AP_AABBCreate(AP_V3(AP_Minf(a.min.x, b.min.x), AP_Minf(a.min.y, b.min.y),
                             AP_Minf(a.min.z, b.min.z)),
                       AP_V3(AP_Maxf(a.max.x, b.max.x), AP_Maxf(a.max.y, b.max.y),
                             AP_Maxf(a.max.z, b.max.z)));
}

AP_Plane AP_PlaneCreate(AP_Vec3 normal, AP_F32 distance) {
  AP_Plane plane;
  AP_Vec3 n = AP_Vec3Normalize(normal);
  plane.normal = n;
  plane.distance = distance;
  return plane;
}

AP_F32 AP_PlaneDistanceToPoint(AP_Plane plane, AP_Vec3 point) {
  return AP_Vec3Dot(plane.normal, point) + plane.distance;
}

bool AP_RayIntersectPlane(AP_Ray ray, AP_Plane plane, AP_F32 *out_t) {
  AP_F32 denom = AP_Vec3Dot(plane.normal, ray.direction);
  AP_F32 t;

  if (AP_Absf(denom) <= AP_EPSILON) {
    return false;
  }

  t = -(AP_Vec3Dot(plane.normal, ray.origin) + plane.distance) / denom;
  if (t < 0.0f) {
    return false;
  }

  if (out_t != NULL) {
    *out_t = t;
  }
  return true;
}

bool AP_RayIntersectAABB(AP_Ray ray, AP_AABB box, AP_F32 *out_t) {
  AP_F32 tmin = 0.0f;
  AP_F32 tmax = 1.0e30f;
  int axis;

  for (axis = 0; axis < 3; ++axis) {
    AP_F32 origin = axis == 0 ? ray.origin.x : (axis == 1 ? ray.origin.y : ray.origin.z);
    AP_F32 dir = axis == 0 ? ray.direction.x
                           : (axis == 1 ? ray.direction.y : ray.direction.z);
    AP_F32 min_b = axis == 0 ? box.min.x : (axis == 1 ? box.min.y : box.min.z);
    AP_F32 max_b = axis == 0 ? box.max.x : (axis == 1 ? box.max.y : box.max.z);
    AP_F32 inv;
    AP_F32 t1;
    AP_F32 t2;

    if (AP_Absf(dir) <= AP_EPSILON) {
      if (origin < min_b || origin > max_b) {
        return false;
      }
      continue;
    }

    inv = 1.0f / dir;
    t1 = (min_b - origin) * inv;
    t2 = (max_b - origin) * inv;
    if (t1 > t2) {
      AP_F32 tmp = t1;
      t1 = t2;
      t2 = tmp;
    }

    tmin = AP_Maxf(tmin, t1);
    tmax = AP_Minf(tmax, t2);
    if (tmin > tmax) {
      return false;
    }
  }

  if (out_t != NULL) {
    *out_t = tmin;
  }
  return true;
}

