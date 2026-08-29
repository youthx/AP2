#include <AP2/AP2.h>

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
      bool plus = (local_x > 12 && local_x < 20) ||
                  (local_y > 12 && local_y < 20);

      pixels[index + 0] = plus ? 255 : colors[tile][0];
      pixels[index + 1] = plus ? 255 : colors[tile][1];
      pixels[index + 2] = plus ? 255 : colors[tile][2];
      pixels[index + 3] = 255;
    }
  }

  texture = AP_CreateTextureFromPixels(SIZE, SIZE, pixels, SIZE * 4);
  if (texture != NULL) {
    AP_SetTextureScaleMode(texture, AP_SCALEMODE_NEAREST);
  }

  return texture;
}

static void AP_RenderSprites(AP_Texture *atlas, float cx, float cy,
                             float spin) {
  AP_Sprite sprite;
  int frame;

  if (atlas == NULL) {
    return;
  }

  sprite = AP_CreateSprite(atlas);
  AP_SpriteSetOriginNormalized(&sprite, 0.5f, 0.5f);
  AP_SpriteSetScale(&sprite, 3.0f);

  for (frame = 0; frame < 4; ++frame) {
    float x = cx - 180.0f + (float)frame * 120.0f;
    float y = cy + 40.0f;
    AP_SpriteSetFrame(&sprite, 2, 2, frame);
    AP_SpriteSetRotation(&sprite, spin + (float)frame * 25.0f);
    AP_RenderSprite(&sprite, x, y);
  }

  AP_RenderTextureRotated(atlas, NULL,
                          &(AP_FRect){cx + 280.0f, cy - 40.0f, 96.0f, 96.0f},
                          spin * 0.5f, NULL, AP_FLIP_NONE);
}

static void AP_RenderScene(AP_Texture *atlas, double time) {
  int pixel_w = 0;
  int pixel_h = 0;
  float width;
  float height;
  float cx;
  float cy;
  float spin = (float)time * 45.0f;
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

  AP_SetRenderDrawColorFloat(0.16f, 0.18f, 0.22f, 1.0f);
  AP_SetRenderLineWidth(1.0f);
  AP_RenderGrid(&(AP_FRect){0.0f, 0.0f, width, height}, 16, 9);

  AP_RenderFillRectGradient(
      &(AP_FRect){32.0f, 32.0f, 280.0f, 160.0f}, AP_FColorRGB(0.20f, 0.45f, 0.85f),
      AP_FColorRGB(0.45f, 0.20f, 0.80f), AP_FColorRGB(0.90f, 0.30f, 0.45f),
      AP_FColorRGB(0.15f, 0.70f, 0.55f));
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
  AP_RenderSprites(atlas, cx, cy, spin);
}

int main(void) {
  AP_Texture *atlas;

  if (!AP_Init(AP_INIT_VIDEO)) {
    return 1;
  }

  if (!AP_CreateWindow("AP2 Renderer", 1280, 720,
                       AP_WINDOW_RESIZABLE | AP_WINDOW_HIGH_PIXEL_DENSITY |
                           AP_WINDOW_MSAA)) {
    AP_Quit();
    return 1;
  }

  atlas = AP_MakeDemoAtlas();

  while (AP_IsRunning()) {
    AP_PumpEvents();
    AP_RenderScene(atlas, AP_GetTime());

    if (AP_IsKeyDown(AP_KEY_E)) {
      AP_INFO("E key pressed");
    }
    AP_RenderPresent();
  }

  AP_DestroyTexture(atlas);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
