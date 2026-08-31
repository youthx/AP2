#include <AP2/AP2.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* New post setters (add these to include/AP2/AP2_Post.h if missing). */
bool AP_SetPostExposure(float exposure);
bool AP_SetPostGamma(float gamma);
bool AP_SetPostFilmic(float amount);
bool AP_SetPostPixelate(float pixels);
bool AP_SetPostScanlines(float amount);
bool AP_SetPostBarrel(float amount);
bool AP_SetPostSepia(float amount);
bool AP_SetPostGrayscale(float amount);
bool AP_SetPostPosterize(float levels);
bool AP_SetPostEdge(float amount);

/* -------------------------------------------------------------
 * Config
 * ------------------------------------------------------------- */

#define WIN_W 1280
#define WIN_H 720
#define TITLE "AP2 — 3D Showcase"

#define GRID_SIZE 24.0f
#define GRID_DIVS 24

/* -------------------------------------------------------------
 * App state
 * ------------------------------------------------------------- */

typedef struct {
  float yaw;   /* radians around Y */
  float pitch; /* radians, clamped */
  float dist;
  AP_Vec3 target;
  float fov;
} OrbitCam;

typedef struct {
  AP_Mesh *sphere;
  AP_Mesh *plane;
  AP_Mesh *cube_mesh;

  OrbitCam cam;
  bool anim_paused;
  float time;
  float anim_speed;

  /* lighting */
  float sun_intensity;
  float point_intensity;
  float ambient;
  bool auto_orbit_light;

  /* post */
  bool post_on;
  float vignette;
  float bloom_intensity;
  float bloom_threshold;
  float grain;
  float exposure;
  float gamma;
  float filmic;
  float chromatic;
  float scanlines;
  float barrel;

  /* gui */
  bool show_panel;
  bool show_grid;
  bool show_helpers;
  int scene_mode; /* 0 = playground, 1 = pillars, 2 = orbit rings */

  int frames;
  float fps_accum;
  float fps;
} App;

static App g;

/* -------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------- */

static float wrap_tau(float a) {
  while (a > AP_TAU)
    a -= AP_TAU;
  while (a < 0.0f)
    a += AP_TAU;
  return a;
}

static AP_Vec3 orbit_eye(const OrbitCam *c) {
  float cp = cosf(c->pitch);
  float sp = sinf(c->pitch);
  float cy = cosf(c->yaw);
  float sy = sinf(c->yaw);
  return AP_V3(c->target.x + sy * cp * c->dist,
               c->target.y + sp * c->dist,
               c->target.z + cy * cp * c->dist);
}

static AP_Mat4 trs(AP_Vec3 t, float yaw_deg, float pitch_deg, AP_Vec3 s) {
  AP_Mat4 T = AP_Mat4Translate(t);
  AP_Mat4 Ry = AP_Mat4RotateY(yaw_deg);
  AP_Mat4 Rx = AP_Mat4RotateX(pitch_deg);
  AP_Mat4 S = AP_Mat4Scale(s);
  return AP_Mat4Mul(T, AP_Mat4Mul(Ry, AP_Mat4Mul(Rx, S)));
}

static AP_Color lerp_color(AP_Color a, AP_Color b, float t) {
  t = AP_Clampf(t, 0.0f, 1.0f);
  return AP_C4(AP_Lerpf(a.r, b.r, t), AP_Lerpf(a.g, b.g, t),
               AP_Lerpf(a.b, b.b, t), AP_Lerpf(a.a, b.a, t));
}

static AP_Color hue(float h, float s, float v, float a) {
  /* simple HSV -> RGB, h in [0,1) */
  float c = v * s;
  float x = c * (1.0f - fabsf(fmodf(h * 6.0f, 2.0f) - 1.0f));
  float m = v - c;
  float r = 0, g = 0, b = 0;
  int i = (int)(h * 6.0f) % 6;
  if (i < 0)
    i += 6;
  switch (i) {
  case 0:
    r = c;
    g = x;
    break;
  case 1:
    r = x;
    g = c;
    break;
  case 2:
    g = c;
    b = x;
    break;
  case 3:
    g = x;
    b = c;
    break;
  case 4:
    r = x;
    b = c;
    break;
  default:
    r = c;
    b = x;
    break;
  }
  return AP_C4(r + m, g + m, b + m, a);
}

/* -------------------------------------------------------------
 * Scene drawing
 * ------------------------------------------------------------- */

static void draw_playground(float t) {
  /* Ground plane */
  AP_DrawPlane(AP_V3(0.0f, 0.0f, 0.0f), GRID_SIZE, GRID_SIZE,
               AP_C4(0.18f, 0.20f, 0.24f, 1.0f));

  if (g.show_grid) {
    AP_DrawGrid3D(GRID_SIZE, GRID_DIVS, AP_C4(0.32f, 0.35f, 0.40f, 0.55f));
  }

  /* Central pedestal + glowing core */
  AP_DrawCube(AP_V3(0.0f, 0.35f, 0.0f), AP_V3(1.6f, 0.7f, 1.6f),
              AP_C4(0.25f, 0.28f, 0.34f, 1.0f));

  float pulse = 0.5f + 0.5f * sinf(t * 2.2f);
  AP_Color core = lerp_color(AP_C4(0.2f, 0.55f, 1.0f, 1.0f),
                             AP_C4(0.95f, 0.45f, 0.2f, 1.0f), pulse);
  AP_Set3DShininess(64.0f);
  AP_Set3DSpecular(0.85f);
  AP_DrawSphere(AP_V3(0.0f, 1.35f + 0.15f * sinf(t * 1.7f), 0.0f), 0.55f,
                core);

  /* Orbiting cubes */
  for (int i = 0; i < 8; ++i) {
    float a = t * 0.55f + (float)i * (AP_TAU / 8.0f);
    float r = 3.2f + 0.4f * sinf(t * 1.3f + i);
    float y = 0.55f + 0.35f * sinf(t * 2.0f + i * 0.7f);
    AP_Vec3 p = AP_V3(cosf(a) * r, y, sinf(a) * r);
    AP_Color col = hue((float)i / 8.0f + t * 0.04f, 0.75f, 0.95f, 1.0f);
    float s = 0.45f + 0.12f * sinf(t * 3.0f + i);
    AP_Mat4 m = trs(p, AP_RadToDeg(a * 1.5f), AP_RadToDeg(t * 40.0f + i * 20.0f),
                    AP_V3(s, s, s));
    if (g.cube_mesh) {
      AP_DrawMeshEx(g.cube_mesh, &m, col);
    } else {
      AP_DrawCube(p, AP_V3(s, s, s), col);
    }
  }

  /* Outer ring of spheres */
  for (int i = 0; i < 12; ++i) {
    float a = -t * 0.25f + (float)i * (AP_TAU / 12.0f);
    float r = 6.5f;
    AP_Vec3 p = AP_V3(cosf(a) * r, 0.4f, sinf(a) * r);
    AP_Color col = hue(0.55f + (float)i / 12.0f * 0.2f, 0.55f, 0.9f, 1.0f);
    AP_DrawSphere(p, 0.28f, col);
  }

  /* Accent pillars */
  for (int i = 0; i < 4; ++i) {
    float a = (float)i * (AP_PI * 0.5f) + AP_PI * 0.25f;
    AP_Vec3 p = AP_V3(cosf(a) * 5.0f, 1.2f, sinf(a) * 5.0f);
    AP_DrawCube(p, AP_V3(0.35f, 2.4f, 0.35f),
                AP_C4(0.22f, 0.24f, 0.30f, 1.0f));
    AP_DrawSphere(AP_V3(p.x, 2.55f, p.z), 0.22f,
                  hue(0.08f * i + t * 0.05f, 0.8f, 1.0f, 1.0f));
  }
}

static void draw_pillars(float t) {
  AP_DrawPlane(AP_V3(0.0f, 0.0f, 0.0f), 20.0f, 20.0f,
               AP_C4(0.14f, 0.15f, 0.18f, 1.0f));
  if (g.show_grid) {
    AP_DrawGrid3D(20.0f, 20, AP_C4(0.28f, 0.30f, 0.34f, 0.5f));
  }

  const int N = 9;
  for (int z = 0; z < N; ++z) {
    for (int x = 0; x < N; ++x) {
      float fx = (x - (N - 1) * 0.5f) * 1.8f;
      float fz = (z - (N - 1) * 0.5f) * 1.8f;
      float wave = sinf(t * 1.6f + fx * 0.45f + fz * 0.45f);
      float h = 0.8f + 1.6f * (0.5f + 0.5f * wave);
      AP_Vec3 c = AP_V3(fx, h * 0.5f, fz);
      AP_Color col = hue(0.55f + 0.15f * wave, 0.65f, 0.9f, 1.0f);
      AP_DrawCube(c, AP_V3(0.7f, h, 0.7f), col);
    }
  }
}

static void draw_orbit_rings(float t) {
  AP_DrawPlane(AP_V3(0.0f, -0.01f, 0.0f), 16.0f, 16.0f,
               AP_C4(0.12f, 0.13f, 0.16f, 1.0f));
  if (g.show_grid) {
    AP_DrawGrid3D(16.0f, 16, AP_C4(0.30f, 0.32f, 0.36f, 0.45f));
  }

  /* Core */
  AP_DrawSphere(AP_V3(0.0f, 1.0f, 0.0f), 0.9f,
                AP_C4(1.0f, 0.85f, 0.45f, 1.0f));

  for (int ring = 0; ring < 3; ++ring) {
    float radius = 2.2f + ring * 1.6f;
    int count = 10 + ring * 4;
    float speed = 0.7f - ring * 0.15f;
    float tilt = 15.0f + ring * 12.0f;
    for (int i = 0; i < count; ++i) {
      float a = t * speed + (float)i * (AP_TAU / (float)count);
      float x = cosf(a) * radius;
      float z = sinf(a) * radius;
      float y = 1.0f + sinf(a * 2.0f + t) * 0.15f * ring;
      /* apply tilt in XZ via simple rotate around X visually */
      float ty = y * cosf(AP_DegToRad(tilt)) - z * sinf(AP_DegToRad(tilt));
      float tz = y * sinf(AP_DegToRad(tilt)) + z * cosf(AP_DegToRad(tilt));
      AP_Vec3 p = AP_V3(x, ty + 0.5f, tz);
      AP_Color col = hue((float)ring * 0.2f + (float)i / (float)count,
                         0.7f, 0.95f, 1.0f);
      float s = 0.22f + 0.06f * ring;
      if (ring == 0) {
        AP_DrawSphere(p, s, col);
      } else if (ring == 1) {
        AP_DrawCube(p, AP_V3(s * 1.4f, s * 1.4f, s * 1.4f), col);
      } else {
        AP_DrawSphere(p, s * 0.9f, col);
      }
    }
  }
}

static void draw_scene(float t) {
  switch (g.scene_mode) {
  case 1:
    draw_pillars(t);
    break;
  case 2:
    draw_orbit_rings(t);
    break;
  default:
    draw_playground(t);
    break;
  }

  if (g.show_helpers) {
    /* Axis gizmo at origin */
    AP_DrawLine3D(AP_V3(0, 0, 0), AP_V3(1.5f, 0, 0), AP_C4(1, 0.2f, 0.2f, 1));
    AP_DrawLine3D(AP_V3(0, 0, 0), AP_V3(0, 1.5f, 0), AP_C4(0.2f, 1, 0.3f, 1));
    AP_DrawLine3D(AP_V3(0, 0, 0), AP_V3(0, 0, 1.5f), AP_C4(0.2f, 0.4f, 1, 1));
  }
}

/* -------------------------------------------------------------
 * Lighting
 * ------------------------------------------------------------- */

static void setup_lights(float t) {
  AP_ClearLights();

  AP_SetAmbientLight(AP_C4(0.10f, 0.11f, 0.14f, 1.0f));
  AP_AddLight(AP_LightAmbient(AP_C4(0.10f, 0.11f, 0.14f, 1.0f), g.ambient));

  /* Sun */
  AP_Vec3 sun_dir = AP_V3(0.45f, 1.0f, 0.25f);
  AP_AddLight(AP_LightDirectional(sun_dir, AP_C4(1.0f, 0.96f, 0.88f, 1.0f),
                                  g.sun_intensity));

  /* Moving warm point */
  float lx = cosf(t * 0.7f) * 4.5f;
  float lz = sinf(t * 0.7f) * 4.5f;
  float ly = 2.2f + 0.5f * sinf(t * 1.3f);
  if (!g.auto_orbit_light) {
    lx = 3.5f;
    ly = 2.5f;
    lz = 2.0f;
  }
  AP_AddLight(AP_LightPoint(AP_V3(lx, ly, lz),
                            AP_C4(1.0f, 0.55f, 0.25f, 1.0f),
                            g.point_intensity, 14.0f));

  /* Cool fill */
  AP_AddLight(AP_LightPoint(AP_V3(-4.0f, 3.0f, -3.0f),
                            AP_C4(0.35f, 0.55f, 1.0f, 1.0f), 0.9f, 12.0f));

  /* Spot from above looking at origin */
  AP_AddLight(AP_LightSpot(AP_V3(0.0f, 8.0f, 0.0f), AP_V3(0.0f, -1.0f, 0.0f),
                           AP_C4(0.9f, 0.95f, 1.0f, 1.0f), 1.4f, 18.0f, 18.0f,
                           32.0f));

  if (g.show_helpers && g.auto_orbit_light) {
    AP_DrawSphere(AP_V3(lx, ly, lz), 0.12f, AP_C4(1.0f, 0.7f, 0.3f, 1.0f));
  }
}

/* -------------------------------------------------------------
 * Input / camera
 * ------------------------------------------------------------- */

static void update_camera(float dt) {
  bool gui_mouse = AP_GuiWantCaptureMouse();
  bool gui_keys = AP_GuiWantCaptureKeyboard();
  bool orbit = AP_IsMouseDown(AP_MOUSE_RIGHT) ||
               (AP_IsMouseDown(AP_MOUSE_LEFT) && !gui_mouse);

  if (dt <= 0.0f) {
    dt = 1.0f / 60.0f;
  }

  if (orbit) {
    g.cam.yaw += (float)AP_GetMouseDeltaX() * 0.0055f;
    g.cam.pitch += (float)AP_GetMouseDeltaY() * 0.0055f;
    g.cam.pitch = AP_Clampf(g.cam.pitch, -1.35f, 1.35f);
    g.cam.yaw = wrap_tau(g.cam.yaw);
  }

  float wheel = (float)AP_GetMouseWheelY();
  if (wheel != 0.0f && !gui_mouse) {
    g.cam.dist = AP_Clampf(g.cam.dist - wheel * 0.9f, 2.5f, 40.0f);
  }

  float speed = 6.0f * dt;
  if (AP_IsKeyDown(AP_KEY_LEFT_SHIFT))
    speed *= 2.5f;

  float cy = cosf(g.cam.yaw);
  float sy = sinf(g.cam.yaw);
  AP_Vec3 forward = AP_V3(sy, 0.0f, cy);
  AP_Vec3 right = AP_V3(cy, 0.0f, -sy);

  if (!gui_keys) {
    if (AP_IsKeyDown(AP_KEY_W) || AP_IsKeyDown(AP_KEY_UP)) {
      g.cam.target.x += forward.x * speed;
      g.cam.target.z += forward.z * speed;
    }
    if (AP_IsKeyDown(AP_KEY_S) || AP_IsKeyDown(AP_KEY_DOWN)) {
      g.cam.target.x -= forward.x * speed;
      g.cam.target.z -= forward.z * speed;
    }
    if (AP_IsKeyDown(AP_KEY_A) || AP_IsKeyDown(AP_KEY_LEFT)) {
      g.cam.target.x -= right.x * speed;
      g.cam.target.z -= right.z * speed;
    }
    if (AP_IsKeyDown(AP_KEY_D) || AP_IsKeyDown(AP_KEY_RIGHT)) {
      g.cam.target.x += right.x * speed;
      g.cam.target.z += right.z * speed;
    }
    if (AP_IsKeyDown(AP_KEY_E))
      g.cam.target.y += speed;
    if (AP_IsKeyDown(AP_KEY_Q))
      g.cam.target.y -= speed;
  }

  if (AP_IsKeyPressed(AP_KEY_R)) {
    g.cam.yaw = 0.6f;
    g.cam.pitch = 0.35f;
    g.cam.dist = 11.0f;
    g.cam.target = AP_V3(0.0f, 0.5f, 0.0f);
    g.cam.fov = 50.0f;
  }

  if (AP_IsKeyPressed(AP_KEY_SPACE))
    g.anim_paused = !g.anim_paused;

  if (AP_IsKeyPressed(AP_KEY_TAB))
    g.show_panel = !g.show_panel;

  if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
#ifdef AP_RequestClose
    AP_RequestClose();
#else
    AP_DestroyWindow(NULL);
#endif
  }
}

/* -------------------------------------------------------------
 * GUI
 * ------------------------------------------------------------- */

static void draw_hud(void) {
  if (!g.show_panel)
    return;

  AP_GuiSetNextWindowPos(16.0f, 16.0f);
  AP_GuiSetNextWindowSize(320.0f, 520.0f);

  if (AP_GuiBeginWindow("3D Showcase", &g.show_panel, AP_GUI_WINDOW_MENU_BAR)) {
    if (AP_GuiBeginMenuBar()) {
      if (AP_GuiBeginMenu("Scene")) {
        if (AP_GuiMenuItem("Playground", "1"))
          g.scene_mode = 0;
        if (AP_GuiMenuItem("Pillar Field", "2"))
          g.scene_mode = 1;
        if (AP_GuiMenuItem("Orbit Rings", "3"))
          g.scene_mode = 2;
        AP_GuiEndMenu();
      }
      if (AP_GuiBeginMenu("View")) {
        if (AP_GuiMenuItem(g.show_grid ? "Hide Grid" : "Show Grid", "G"))
          g.show_grid = !g.show_grid;
        if (AP_GuiMenuItem(g.show_helpers ? "Hide Helpers" : "Show Helpers",
                           "H"))
          g.show_helpers = !g.show_helpers;
        AP_GuiEndMenu();
      }
      AP_GuiEndMenuBar();
    }

    AP_GuiLabelF("FPS  %.1f", g.fps);
    AP_GuiLabelF("Time %.1fs", g.time);
    AP_GuiSeparator();

    AP_GuiLabel("Camera");
    AP_GuiSliderF("Distance", &g.cam.dist, 2.5f, 40.0f);
    AP_GuiSliderF("FOV", &g.cam.fov, 30.0f, 90.0f);
    if (AP_GuiButton("Reset Camera (R)")) {
      g.cam.yaw = 0.6f;
      g.cam.pitch = 0.35f;
      g.cam.dist = 11.0f;
      g.cam.target = AP_V3(0.0f, 0.5f, 0.0f);
      g.cam.fov = 50.0f;
    }

    AP_GuiSeparator();
    AP_GuiLabel("Animation");
    AP_GuiCheckbox("Paused", &g.anim_paused);
    AP_GuiSliderF("Speed", &g.anim_speed, 0.0f, 3.0f);

    AP_GuiSeparator();
    AP_GuiLabel("Lighting");
    AP_GuiSliderF("Sun", &g.sun_intensity, 0.0f, 2.5f);
    AP_GuiSliderF("Point", &g.point_intensity, 0.0f, 4.0f);
    AP_GuiSliderF("Ambient", &g.ambient, 0.0f, 1.0f);
    AP_GuiCheckbox("Orbit point light", &g.auto_orbit_light);

    AP_GuiSeparator();
    AP_GuiLabel("Post-process");
    if (AP_GuiCheckbox("Enabled", &g.post_on)) {
      AP_SetPostEnabled(g.post_on);
    }
    AP_GuiSliderF("Vignette", &g.vignette, 0.0f, 1.0f);
    AP_GuiSliderF("Bloom", &g.bloom_intensity, 0.0f, 1.5f);
    AP_GuiSliderF("Bloom threshold", &g.bloom_threshold, 0.0f, 1.0f);
    AP_GuiSliderF("Grain", &g.grain, 0.0f, 0.35f);
    AP_GuiSliderF("Exposure", &g.exposure, 0.2f, 2.5f);
    AP_GuiSliderF("Gamma", &g.gamma, 0.4f, 2.2f);
    AP_GuiSliderF("Filmic", &g.filmic, 0.0f, 1.0f);
    AP_GuiSliderF("Chromatic", &g.chromatic, 0.0f, 0.02f);
    AP_GuiSliderF("Scanlines", &g.scanlines, 0.0f, 0.6f);
    AP_GuiSliderF("Barrel", &g.barrel, 0.0f, 0.4f);

    if (g.post_on) {
      AP_SetPostVignette(g.vignette);
      AP_SetPostBloom(g.bloom_threshold, g.bloom_intensity);
      AP_SetPostGrain(g.grain);
      AP_SetPostExposure(g.exposure);
      AP_SetPostGamma(g.gamma);
      AP_SetPostFilmic(g.filmic);
      AP_SetPostChromatic(g.chromatic);
      AP_SetPostScanlines(g.scanlines);
      AP_SetPostBarrel(g.barrel);
    }

    AP_GuiSeparator();
    AP_GuiLabel("Scene mode");
    if (AP_GuiRadio("Playground", &g.scene_mode, 0)) {
    }
    if (AP_GuiRadio("Pillar Field", &g.scene_mode, 1)) {
    }
    if (AP_GuiRadio("Orbit Rings", &g.scene_mode, 2)) {
    }

    AP_GuiSpacing();
    AP_GuiLabel("RMB/LMB orbit · Scroll zoom");
    AP_GuiLabel("WASD/Arrows move · QE height · Tab");

    AP_GuiEndWindow();
  }
}

/* -------------------------------------------------------------
 * Lifecycle
 * ------------------------------------------------------------- */

static bool app_init(void) {
  memset(&g, 0, sizeof(g));

  g.cam.yaw = 0.6f;
  g.cam.pitch = 0.35f;
  g.cam.dist = 11.0f;
  g.cam.target = AP_V3(0.0f, 0.5f, 0.0f);
  g.cam.fov = 50.0f;

  g.anim_speed = 1.0f;
  g.sun_intensity = 1.1f;
  g.point_intensity = 2.2f;
  g.ambient = 0.35f;
  g.auto_orbit_light = true;

  g.post_on = true;
  g.vignette = 0.42f;
  g.bloom_intensity = 0.55f;
  g.bloom_threshold = 0.65f;
  g.grain = 0.06f;
  g.exposure = 1.05f;
  g.gamma = 1.0f;
  g.filmic = 0.35f;
  g.chromatic = 0.0015f;
  g.scanlines = 0.0f;
  g.barrel = 0.0f;

  g.show_panel = true;
  g.show_grid = true;
  g.show_helpers = false;
  g.scene_mode = 0;

  if (!AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO)) {
    fprintf(stderr, "AP_Init failed: %s\n", AP_GetErrorMessage());
    return false;
  }

  AP_WindowConfig cfg = AP_WindowDefaultConfig();
  cfg.title = TITLE;
  cfg.width = WIN_W;
  cfg.height = WIN_H;
  cfg.flags = AP_WINDOW_RESIZABLE | AP_WINDOW_MSAA;

  if (!AP_CreateWindowEx(&cfg)) {
    /* Fallback if flags differ across versions */
    if (!AP_CreateWindow(TITLE, WIN_W, WIN_H, AP_WINDOW_RESIZABLE)) {
      fprintf(stderr, "Window failed: %s\n", AP_GetErrorMessage());
      return false;
    }
  }

  g.sphere = AP_CreateMeshSphere(0.5f, 32, 20);
  g.plane = AP_CreateMeshPlane(GRID_SIZE, GRID_SIZE);
  g.cube_mesh = AP_CreateMeshCube(1.0f);

  AP_SetPostEnabled(true);
  AP_SetPostVignette(g.vignette);
  AP_SetPostBloom(g.bloom_threshold, g.bloom_intensity);
  AP_SetPostColorGrade(1.05f, 1.06f, 0.02f);
  AP_SetPostGrain(g.grain);
  AP_SetPostExposure(g.exposure);
  AP_SetPostGamma(g.gamma);
  AP_SetPostFilmic(g.filmic);
  AP_SetPostChromatic(g.chromatic);
  AP_SetGuiLayer(AP_GUI_LAYER_OVERLAY);

  {
    AP_GuiStyle style = AP_GuiDarkStyle();
    AP_GuiSetStyle(&style);
  }

  return true;
}

static void app_shutdown(void) {
  AP_DestroyMesh(g.sphere);
  AP_DestroyMesh(g.plane);
  AP_DestroyMesh(g.cube_mesh);
  AP_DestroyWindow(NULL);
  AP_Quit();
}

static void app_frame(float dt) {
  update_camera(dt);

  if (!g.anim_paused) {
    g.time += dt * g.anim_speed;
  }

  g.frames++;
  g.fps_accum += dt;
  if (g.fps_accum >= 0.5f) {
    g.fps = (float)g.frames / g.fps_accum;
    g.frames = 0;
    g.fps_accum = 0.0f;
  }

  /* Clear (starts post capture when enabled) */
  AP_SetDrawColor(0.06f, 0.07f, 0.09f, 1.0f);
  AP_Clear();

  /* 3D pass */
  AP_Vec3 eye = orbit_eye(&g.cam);
  AP_Camera cam = AP_CameraPerspective(eye, g.cam.target, g.cam.fov);

  AP_Begin3D(&cam);
  AP_Set3DDepthTest(true);
  AP_Set3DCullFace(true);
  AP_Set3DShininess(32.0f);
  AP_Set3DSpecular(0.55f);

  setup_lights(g.time);
  draw_scene(g.time);

  AP_End3D();

  /* HUD / GUI after 3D */
  draw_hud();

  AP_Present();
}

int main(void) {
  if (!app_init()) {
    return EXIT_FAILURE;
  }

  while (AP_IsRunning()) {
    AP_PumpEvents();
    float dt = (float)AP_GetDeltaTime();
    if (dt > 0.1f)
      dt = 0.1f; /* avoid spiral after stalls */
    app_frame(dt);
  }

  app_shutdown();
  return EXIT_SUCCESS;
}