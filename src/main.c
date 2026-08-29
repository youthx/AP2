/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include <AP2/AP2.h>

#include <math.h>
#include <string.h>

typedef struct AP_DemoGfx {
  AP_Texture *atlas;
  AP_Texture *panel;
  AP_Texture *stream;
  AP_Texture *target;
  AP_Sprite anim;
} AP_DemoGfx;

static AP_FColor AP_FColorRGB(float r, float g, float b) {
  AP_FColor color;
  color.r = r;
  color.g = g;
  color.b = b;
  color.a = 1.0f;
  return color;
}

static AP_Texture *AP_MakeDemoAtlas(void) {
  enum { SIZE = 64, TILE = 32 };
  unsigned char pixels[SIZE * SIZE * 4];
  const unsigned char colors[4][3] = {
      {232, 76, 79}, {46, 196, 126}, {56, 132, 245}, {245, 196, 52}};
  int y;
  int x;
  AP_Texture *texture;

  for (y = 0; y < SIZE; ++y) {
    for (x = 0; x < SIZE; ++x) {
      int tile = (y / TILE) * 2 + (x / TILE);
      int index = (y * SIZE + x) * 4;
      int local_x = x % TILE;
      int local_y = y % TILE;
      bool plus =
          (local_x > 12 && local_x < 20) || (local_y > 12 && local_y < 20);

      pixels[index + 0] = plus ? 255 : colors[tile][0];
      pixels[index + 1] = plus ? 255 : colors[tile][1];
      pixels[index + 2] = plus ? 255 : colors[tile][2];
      pixels[index + 3] = 255;
    }
  }

  texture = AP_CreateTextureFromPixels(SIZE, SIZE, pixels, SIZE * 4);
  if (texture != NULL) {
    AP_SetTextureScaleMode(texture, AP_SCALEMODE_NEAREST);
    AP_SetTextureAddressMode(texture, AP_TEXTUREADDRESS_WRAP);
  }

  return texture;
}

static AP_Texture *AP_MakeDemoPanel(void) {
  enum { SIZE = 48, BORDER = 16 };
  unsigned char pixels[SIZE * SIZE * 4];
  int y;
  int x;
  AP_Texture *texture;

  for (y = 0; y < SIZE; ++y) {
    for (x = 0; x < SIZE; ++x) {
      int index = (y * SIZE + x) * 4;
      bool edge_x = x < BORDER || x >= SIZE - BORDER;
      bool edge_y = y < BORDER || y >= SIZE - BORDER;
      unsigned char tone = (edge_x && edge_y)   ? 230
                           : (edge_x || edge_y) ? 150
                                                : 70;

      pixels[index + 0] = tone;
      pixels[index + 1] = (unsigned char)(tone * 0.85f);
      pixels[index + 2] = (unsigned char)(tone * 0.55f);
      pixels[index + 3] = 255;
    }
  }

  texture = AP_CreateTextureFromPixels(SIZE, SIZE, pixels, SIZE * 4);
  if (texture != NULL) {
    AP_SetTextureScaleMode(texture, AP_SCALEMODE_NEAREST);
  }
  return texture;
}

static void AP_DemoGfxInit(AP_DemoGfx *gfx) {
  memset(gfx, 0, sizeof(*gfx));
  gfx->atlas = AP_MakeDemoAtlas();
  gfx->panel = AP_MakeDemoPanel();
  gfx->stream = AP_CreateTextureWithAccess(64, 64, AP_TEXTUREACCESS_STREAMING);
  gfx->target = AP_CreateTextureWithAccess(192, 192, AP_TEXTUREACCESS_TARGET);
  if (gfx->stream != NULL) {
    AP_SetTextureScaleMode(gfx->stream, AP_SCALEMODE_NEAREST);
  }
  if (gfx->atlas != NULL) {
    gfx->anim = AP_CreateSprite(gfx->atlas);
    AP_SpriteSetOriginNormalized(&gfx->anim, 0.5f, 0.5f);
    AP_SpriteSetScale(&gfx->anim, 2.5f);
    AP_SpritePlay(&gfx->anim, 2, 2, 0, 4, 8.0f, true);
  }
}

static void AP_DemoGfxShutdown(AP_DemoGfx *gfx) {
  AP_DestroyTexture(gfx->target);
  AP_DestroyTexture(gfx->stream);
  AP_DestroyTexture(gfx->panel);
  AP_DestroyTexture(gfx->atlas);
  memset(gfx, 0, sizeof(*gfx));
}

typedef struct AP_DemoAudio {
  AP_Sound *blip;
  AP_Sound *drone;
  AP_Voice *drone_voice;
  float master;
  float sfx;
  bool drone_on;
} AP_DemoAudio;

static void AP_DemoAudioInit(AP_DemoAudio *audio) {
  memset(audio, 0, sizeof(*audio));
  audio->master = 0.85f;
  audio->sfx = 1.0f;
  audio->blip = AP_CreateSoundWave(AP_WAVEFORM_SINE, 880.0f, 0.12f, 0.35f);
  audio->drone = AP_CreateSoundWave(AP_WAVEFORM_TRIANGLE, 110.0f, 0.5f, 0.18f);
  AP_SetMasterVolume(audio->master);
  AP_SetBusVolume(AP_AUDIO_BUS_SFX, audio->sfx);
  AP_SetBusDuck(AP_AUDIO_BUS_MUSIC, AP_AUDIO_BUS_SFX, 0.25f, 40.0f, 280.0f);
}

static void AP_DemoPlayBlip(AP_DemoAudio *audio) {
  int width = 0;
  int height = 0;
  AP_PlaySoundDesc desc;

  if (audio->blip == NULL) {
    return;
  }

  AP_GetWindowSizeInPixels(&width, &height);
  AP_SetListenerPosition2D((float)width * 0.5f, (float)height * 0.5f);

  desc = AP_PlaySoundDescDefault();
  desc.spatial = true;
  desc.muffle = true;
  desc.fire_and_forget = true;
  desc.position.x = (float)AP_GetMouseX();
  desc.position.y = (float)AP_GetMouseY();
  desc.min_distance = 80.0f;
  desc.max_distance = 900.0f;
  desc.pitch = 0.82f + (float)((int)(AP_GetTime() * 19.0) % 8) * 0.06f;
  AP_PlaySoundEx(audio->blip, &desc);
}

static void AP_DemoAudioUpdate(AP_DemoAudio *audio) {
  AP_SetMasterVolume(audio->master);
  AP_SetBusVolume(AP_AUDIO_BUS_SFX, audio->sfx);

  if (AP_IsKeyPressed(AP_KEY_SPACE) && !AP_GuiWantCaptureKeyboard()) {
    AP_DemoPlayBlip(audio);
  }
  if (AP_IsMousePressed(AP_MOUSE_LEFT) && !AP_GuiWantCaptureMouse()) {
    AP_DemoPlayBlip(audio);
  }

  if (audio->drone_on) {
    if (audio->drone_voice == NULL && audio->drone != NULL) {
      AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
      desc.bus = AP_AUDIO_BUS_MUSIC;
      desc.loop = true;
      desc.volume = 0.4f;
      audio->drone_voice = AP_PlaySoundEx(audio->drone, &desc);
    }
  } else if (audio->drone_voice != NULL) {
    AP_StopVoiceFaded(audio->drone_voice, 180.0f);
    audio->drone_voice = NULL;
  }
}

static void AP_DemoAudioShutdown(AP_DemoAudio *audio) {
  if (audio->drone_voice != NULL) {
    AP_DestroyVoice(audio->drone_voice);
  }
  AP_DestroySound(audio->blip);
  AP_DestroySound(audio->drone);
  memset(audio, 0, sizeof(*audio));
}

static void AP_UpdateStreamTexture(AP_Texture *texture, float time) {
  unsigned char *pixels = NULL;
  int pitch = 0;
  int width;
  int height;
  int y;

  if (!AP_LockTexture(texture, NULL, (void **)&pixels, &pitch)) {
    return;
  }

  width = AP_GetTextureWidth(texture);
  height = AP_GetTextureHeight(texture);
  for (y = 0; y < height; ++y) {
    int x;
    for (x = 0; x < width; ++x) {
      int index = y * pitch + x * 4;
      int wave = (x + y + (int)(time * 40.0f)) & 31;
      pixels[index + 0] = (unsigned char)(40 + wave * 6);
      pixels[index + 1] = (unsigned char)(80 + wave * 4);
      pixels[index + 2] = (unsigned char)(160 + wave * 2);
      pixels[index + 3] = 255;
    }
  }

  AP_UnlockTexture(texture);
}

static void AP_RenderOffscreen(AP_DemoGfx *gfx, float spin) {
  if (gfx->target == NULL) {
    return;
  }

  AP_SetRenderTarget(gfx->target);
  AP_SetRenderDrawColorFloat(0.08f, 0.10f, 0.16f, 1.0f);
  AP_RenderClear();
  AP_SetRenderDrawColorFloat(0.95f, 0.45f, 0.20f, 1.0f);
  AP_RenderFillStar(96.0f, 96.0f, 70.0f, 28.0f, 5);
  if (gfx->atlas != NULL) {
    AP_RenderTextureRotated(gfx->atlas, NULL,
                            &(AP_FRect){48.0f, 48.0f, 96.0f, 96.0f}, spin, NULL,
                            AP_FLIP_NONE);
  }
  AP_SetRenderTarget(NULL);
}

static void AP_RenderSprites(AP_DemoGfx *gfx, float cx, float cy, float spin) {
  AP_Sprite sprite;
  int frame;

  if (gfx->atlas == NULL) {
    return;
  }

  sprite = AP_CreateSprite(gfx->atlas);
  AP_SpriteSetOriginNormalized(&sprite, 0.5f, 0.5f);
  AP_SpriteSetScale(&sprite, 3.0f);

  for (frame = 0; frame < 4; ++frame) {
    float x = cx - 180.0f + (float)frame * 120.0f;
    float y = cy + 40.0f;
    AP_SpriteSetFrame(&sprite, 2, 2, frame);
    AP_SpriteSetRotation(&sprite, spin + (float)frame * 25.0f);
    AP_RenderSprite(&sprite, x, y);
  }

  AP_SpriteSetRotation(&gfx->anim, spin * 0.35f);
  AP_RenderSprite(&gfx->anim, cx + 280.0f, cy + 40.0f);
}

static void AP_RenderAdvancedTextures(AP_DemoGfx *gfx, float width,
                                      float height, float time, float spin) {
  AP_FRect tile_src;
  AP_FPoint origin;
  AP_FPoint right;
  AP_FPoint down;
  AP_Vertex mesh[3];
  int indices[3];

  if (gfx->atlas != NULL) {
    tile_src.x = 0.0f;
    tile_src.y = 0.0f;
    tile_src.w = 32.0f;
    tile_src.h = 32.0f;
    AP_RenderTextureTiled(gfx->atlas, &tile_src, 2.0f,
                          &(AP_FRect){340.0f, height - 150.0f, 200.0f, 120.0f});
  }

  if (gfx->panel != NULL) {
    AP_RenderTexture9Grid(gfx->panel, NULL, 16.0f, 16.0f, 16.0f, 16.0f, 1.0f,
                          &(AP_FRect){560.0f, height - 150.0f, 220.0f, 120.0f});
  }

  if (gfx->stream != NULL) {
    AP_UpdateStreamTexture(gfx->stream, time);
    AP_RenderTexture(gfx->stream, NULL,
                     &(AP_FRect){800.0f, height - 150.0f, 96.0f, 96.0f});
  }

  AP_RenderOffscreen(gfx, spin);
  if (gfx->target != NULL) {
    AP_RenderTexture(gfx->target, NULL,
                     &(AP_FRect){32.0f, height - 150.0f, 128.0f, 128.0f});
  }

  if (gfx->atlas != NULL) {
    origin.x = width - 260.0f;
    origin.y = height - 150.0f;
    right.x = origin.x + 90.0f + sinf(time) * 18.0f;
    right.y = origin.y + 8.0f;
    down.x = origin.x - 10.0f;
    down.y = origin.y + 70.0f;
    AP_RenderTextureAffine(gfx->atlas, NULL, &origin, &right, &down);

    mesh[0].position.x = width - 140.0f;
    mesh[0].position.y = height - 150.0f;
    mesh[0].color = AP_FColorRGB(1.0f, 1.0f, 1.0f);
    mesh[0].tex_coord.x = 0.0f;
    mesh[0].tex_coord.y = 0.0f;
    mesh[1].position.x = width - 40.0f;
    mesh[1].position.y = height - 150.0f;
    mesh[1].color = AP_FColorRGB(1.0f, 0.85f, 0.40f);
    mesh[1].tex_coord.x = 1.0f;
    mesh[1].tex_coord.y = 0.0f;
    mesh[2].position.x = width - 90.0f;
    mesh[2].position.y = height - 40.0f;
    mesh[2].color = AP_FColorRGB(0.55f, 0.80f, 1.00f);
    mesh[2].tex_coord.x = 0.5f;
    mesh[2].tex_coord.y = 1.0f;
    indices[0] = 0;
    indices[1] = 1;
    indices[2] = 2;
    AP_RenderTextureGeometry(gfx->atlas, mesh, 3, indices, 3);
  }
}

static void AP_RenderScene(AP_DemoGfx *gfx, double time, bool show_grid,
                           float spin_speed) {
  int pixel_w = 0;
  int pixel_h = 0;
  float width;
  float height;
  float cx;
  float cy;
  float spin = (float)time * 45.0f * spin_speed;
  AP_Vertex mesh[3];
  AP_FPoint poly[5];
  int indices[3];

  AP_GetWindowSizeInPixels(&pixel_w, &pixel_h);
  width = (float)pixel_w;
  height = (float)pixel_h;
  cx = width * 0.5f;
  cy = height * 0.5f;

  AP_SetRenderDrawColorFloat(0.07f, 0.08f, 0.10f, 1.0f);
  AP_RenderClear();

  if (show_grid) {
    AP_SetRenderDrawColorFloat(0.16f, 0.18f, 0.22f, 1.0f);
    AP_SetRenderLineWidth(1.0f);
    AP_RenderGrid(&(AP_FRect){0.0f, 0.0f, width, height}, 16, 9);
  }

  AP_RenderFillRectGradient(
      &(AP_FRect){32.0f, 32.0f, 280.0f, 160.0f},
      AP_FColorRGB(0.20f, 0.45f, 0.85f), AP_FColorRGB(0.45f, 0.20f, 0.80f),
      AP_FColorRGB(0.90f, 0.30f, 0.45f), AP_FColorRGB(0.15f, 0.70f, 0.55f));
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.85f);
  AP_SetRenderLineWidth(2.0f);
  AP_RenderRoundedRect(&(AP_FRect){32.0f, 32.0f, 280.0f, 160.0f}, 18.0f);

  AP_SetRenderDrawColorFloat(0.95f, 0.75f, 0.20f, 1.0f);
  AP_RenderFillRoundedRect(&(AP_FRect){32.0f, 220.0f, 280.0f, 90.0f}, 20.0f);

  AP_SetRenderDrawColorFloat(0.30f, 0.85f, 0.70f, 1.0f);
  AP_RenderFillStar(170.0f, 400.0f, 70.0f, 30.0f, 5);
  AP_SetRenderDrawColorFloat(0.10f, 0.20f, 0.18f, 1.0f);
  AP_SetRenderLineWidth(2.0f);
  AP_RenderStar(170.0f, 400.0f, 70.0f, 30.0f, 5);

  AP_SetRenderDrawColorFloat(0.40f, 0.65f, 1.00f, 1.0f);
  AP_RenderFillNGon(170.0f, 540.0f, 55.0f, 6);
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.7f);
  AP_RenderNGon(170.0f, 540.0f, 55.0f, 6);

  AP_PushRenderTransform();
  AP_SetRenderRotationOrigin(cx, cy);
  AP_SetRenderRotation(spin);
  AP_EnableRenderDrawFlag(AP_DRAW_CENTERED);
  AP_SetRenderDrawColorFloat(0.95f, 0.35f, 0.35f, 0.90f);
  AP_RenderFillRect(&(AP_FRect){cx, cy, 180.0f, 80.0f});
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 1.0f);
  AP_SetRenderLineWidth(3.0f);
  AP_RenderRect(&(AP_FRect){cx, cy, 180.0f, 80.0f});
  AP_DisableRenderDrawFlag(AP_DRAW_CENTERED);
  AP_PopRenderTransform();

  AP_SetRenderDrawColorFloat(0.95f, 0.55f, 0.20f, 1.0f);
  AP_RenderFillPie(cx + 260.0f, 140.0f, 70.0f, spin, spin + 220.0f);
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.85f);
  AP_RenderArc(cx + 260.0f, 140.0f, 70.0f, spin, spin + 220.0f);
  AP_SetRenderDrawColorFloat(0.20f, 0.75f, 0.95f, 0.85f);
  AP_RenderFillRing(cx + 260.0f, 140.0f, 28.0f, 42.0f);

  AP_SetRenderDrawColorFloat(0.85f, 0.40f, 0.90f, 1.0f);
  AP_RenderFillEllipse(cx + 260.0f, 320.0f, 90.0f, 40.0f);
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.8f);
  AP_RenderEllipse(cx + 260.0f, 320.0f, 90.0f, 40.0f);

  AP_SetRenderLineCap(AP_LINE_CAP_ROUND);
  AP_SetRenderLineJoin(AP_LINE_JOIN_ROUND);
  AP_SetRenderLineWidth(8.0f);
  AP_SetRenderDrawColorFloat(0.30f, 0.90f, 0.55f, 1.0f);
  AP_RenderBezier(cx - 80.0f, height - 80.0f, cx, height - 220.0f, cx + 160.0f,
                  height - 40.0f, width - 80.0f, height - 120.0f);

  AP_SetRenderDrawColorFloat(0.95f, 0.85f, 0.35f, 1.0f);
  AP_RenderFillCapsule(cx - 40.0f, 80.0f, cx + 140.0f, 80.0f, 18.0f);
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.9f);
  AP_SetRenderLineWidth(2.0f);
  AP_SetRenderLineCap(AP_LINE_CAP_BUTT);
  AP_RenderCapsule(cx - 40.0f, 80.0f, cx + 140.0f, 80.0f, 18.0f);

  mesh[0].position.x = width - 280.0f;
  mesh[0].position.y = height - 220.0f;
  mesh[0].color = AP_FColorRGB(1.0f, 0.30f, 0.35f);
  mesh[0].tex_coord.x = 0.0f;
  mesh[0].tex_coord.y = 0.0f;
  mesh[1].position.x = width - 80.0f;
  mesh[1].position.y = height - 220.0f;
  mesh[1].color = AP_FColorRGB(0.30f, 0.90f, 0.45f);
  mesh[1].tex_coord.x = 1.0f;
  mesh[1].tex_coord.y = 0.0f;
  mesh[2].position.x = width - 180.0f;
  mesh[2].position.y = height - 80.0f;
  mesh[2].color = AP_FColorRGB(0.35f, 0.55f, 1.00f);
  mesh[2].tex_coord.x = 0.5f;
  mesh[2].tex_coord.y = 1.0f;
  indices[0] = 0;
  indices[1] = 1;
  indices[2] = 2;
  AP_RenderGeometry(mesh, 3, indices, 3);

  poly[0].x = width - 300.0f;
  poly[0].y = 80.0f;
  poly[1].x = width - 180.0f;
  poly[1].y = 50.0f;
  poly[2].x = width - 80.0f;
  poly[2].y = 110.0f;
  poly[3].x = width - 140.0f;
  poly[3].y = 200.0f;
  poly[4].x = width - 260.0f;
  poly[4].y = 180.0f;
  AP_SetRenderDrawColorFloat(0.55f, 0.35f, 0.95f, 0.85f);
  AP_RenderFillPolygon(poly, 5);
  AP_SetRenderDrawColorFloat(1.0f, 1.0f, 1.0f, 0.9f);
  AP_SetRenderLineWidth(2.0f);
  AP_RenderPolygon(poly, 5);

  AP_SetRenderPointSize(8.0f);
  AP_EnableRenderDrawFlag(AP_DRAW_ROUND_POINTS);
  AP_SetRenderDrawColorFloat(1.0f, 0.85f, 0.20f, 1.0f);
  AP_RenderPoint(cx, cy);
  AP_DisableRenderDrawFlag(AP_DRAW_ROUND_POINTS);
  AP_SetRenderPointSize(1.0f);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderSprites(gfx, cx, cy, spin);
  AP_RenderAdvancedTextures(gfx, width, height, (float)time, spin);
}

static void AP_DemoGui(AP_DemoGfx *gfx, AP_DemoAudio *audio, double now,
                       bool *show_grid, float *spin_speed, char *title,
                       int title_size) {
  static bool settings_open = true;
  static bool objects_open = true;
  static bool about_open = false;
  static bool light_style = false;
  static bool paused = false;
  static int quality = 1;
  static int object = 0;
  static int shade = 0;
  static float tint = 1.0f;
  static AP_FColor accent = {0.35f, 0.55f, 1.0f, 1.0f};
  static const char *qualities[] = {"Low", "Medium", "High", "Ultra"};
  static const char *object_names[] = {"Star", "N-gon", "Capsule", "Bezier",
                                       "Sprites"};
  const AP_AudioInfo *audio_info = AP_AudioGetInfo();
  int pixel_w = 0;
  int pixel_h = 0;

  AP_GetWindowSizeInPixels(&pixel_w, &pixel_h);

  if (settings_open) {
    AP_GuiSetNextWindowFlags(AP_GUI_WINDOW_MENU_BAR);
    if (AP_GuiBeginPanel("Settings", 24.0f, 24.0f, 340.0f, 560.0f)) {
      if (AP_GuiBeginMenuBar()) {
        if (AP_GuiBeginMenu("File")) {
          if (AP_GuiMenuItem("About", NULL)) {
            about_open = true;
          }
          if (AP_GuiMenuItem("Quit", "Esc")) {
            AP_Quit();
          }
          AP_GuiEndMenu();
        }
        if (AP_GuiBeginMenu("View")) {
          if (AP_GuiMenuItem(*show_grid ? "Hide Grid" : "Show Grid", "G")) {
            *show_grid = !*show_grid;
          }
          if (AP_GuiMenuItem(light_style ? "Dark Style" : "Light Style",
                             NULL)) {
            AP_GuiStyle style;
            light_style = !light_style;
            style = light_style ? AP_GuiLightStyle() : AP_GuiDarkStyle();
            AP_GuiSetStyle(&style);
          }
          AP_GuiEndMenu();
        }
        AP_GuiEndMenuBar();
      }

      if (AP_GuiBeginTabBar("settings_tabs")) {
        if (AP_GuiTab("Display")) {
          AP_GuiLabelF("Time %.1fs", now);
          AP_GuiSeparator();
          AP_GuiToggle("Light style", &light_style);
          if (AP_GuiIsItemHovered()) {
            AP_GuiSetTooltip("Switch between dark and light GUI colors");
          }
          if (AP_GuiIsItemClicked()) {
            AP_GuiStyle style =
                light_style ? AP_GuiLightStyle() : AP_GuiDarkStyle();
            AP_GuiSetStyle(&style);
          }
          AP_GuiCheckbox("Show grid", show_grid);
          AP_GuiCombo("Quality", &quality, qualities, 4);
          AP_GuiSliderF("Spin", spin_speed, 0.0f, 3.0f);
          AP_GuiProgress(*spin_speed / 3.0f);
          AP_GuiInputText("Title", title, title_size);
        }
        if (AP_GuiTab("Audio")) {
          AP_GuiSeparatorText("Device");
          if (audio_info != NULL) {
            AP_GuiLabelF("%s", audio_info->backend);
            AP_GuiLabelF("%s", audio_info->device_name);
          }
          AP_GuiSliderF("Master", &audio->master, 0.0f, 1.0f);
          AP_GuiSliderF("SFX", &audio->sfx, 0.0f, 1.0f);
          AP_GuiCheckbox("Drone", &audio->drone_on);
          if (AP_GuiButton("Play blip")) {
            AP_DemoPlayBlip(audio);
          }
          AP_GuiLabel("Space / click: spatial blip");
        }
        AP_GuiEndTabBar();
      }

      if (AP_GuiCollapsingHeader("Advanced", false)) {
        AP_GuiDragF("Tint", &tint, 0.01f, 0.0f, 2.0f);
        AP_GuiColorEdit("Accent", &accent);
        AP_GuiBeginDisabled(paused);
        AP_GuiSliderI("Shade", &shade, 0, 8);
        AP_GuiEndDisabled();
        AP_GuiToggle("Paused", &paused);
      }
    }
    AP_GuiEndPanel();
  }

  if (objects_open) {
    if (AP_GuiBeginPanel("Objects", (float)pixel_w - 280.0f, 24.0f, 256.0f,
                         420.0f)) {
      AP_GuiListBox("Scene items", &object, object_names, 5, 5);
      if (AP_GuiBeginChild("preview", 0.0f, 120.0f, true)) {
        int row;
        for (row = 0; row < 12; ++row) {
          AP_GuiPushIdInt(row);
          AP_GuiLabelF("Layer %d  z=%.2f", row, (float)row * 0.15f);
          AP_GuiPopId();
        }
        AP_GuiEndChild();
      }
      AP_GuiSeparator();
      AP_GuiColumns(2);
      AP_GuiLabel("Atlas");
      AP_GuiNextColumn();
      if (gfx->atlas != NULL) {
        AP_GuiImage(gfx->atlas, 64.0f, 64.0f);
      }
      AP_GuiColumns(1);
      if (AP_GuiButton("Reset object")) {
        object = 0;
      }
    }
    AP_GuiEndPanel();
  }

  if (about_open) {
    if (AP_GuiBeginPopupModal("About AP2", &about_open)) {
      AP_GuiLabel("AP2 immediate GUI");
      AP_GuiLabel("Menus, tabs, lists, popups,");
      AP_GuiLabel("and layout for real apps.");
      AP_GuiSpacing();
      if (AP_GuiButton("OK")) {
        about_open = false;
      }
      AP_GuiEndPopup();
    }
  }

  AP_SetWindowTitle(title[0] != '\0' ? title : "AP2 Renderer");
}

int main(void) {
  AP_DemoGfx gfx;
  AP_DemoAudio audio;
  bool show_grid = true;
  float spin_speed = 1.0f;
  double last_time;
  char title[48] = "AP2";

  if (!AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO)) {
    return 1;
  }

  if (!AP_CreateWindow("AP2 Renderer", 1280, 720,
                       AP_WINDOW_RESIZABLE | AP_WINDOW_HIGH_PIXEL_DENSITY |
                           AP_WINDOW_MSAA)) {
    AP_Quit();
    return 1;
  }

  AP_DemoGfxInit(&gfx);
  AP_DemoAudioInit(&audio);
  last_time = AP_GetTime();

  while (AP_IsRunning()) {
    double now = AP_GetTime();
    float dt = (float)(now - last_time);
    last_time = now;

    AP_PumpEvents();
    AP_DemoAudioUpdate(&audio);
    AP_SpriteUpdate(&gfx.anim, dt);
    AP_RenderScene(&gfx, now, show_grid, spin_speed);

    AP_DemoGui(&gfx, &audio, now, &show_grid, &spin_speed, title,
               (int)sizeof(title));

    AP_RenderPresent();
  }

  AP_DemoAudioShutdown(&audio);
  AP_DemoGfxShutdown(&gfx);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
