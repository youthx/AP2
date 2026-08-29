# Tutorial 10 — Breakout

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

A small game from primitives: paddle, ball, bricks, score, a blip on hit. Copy into a `.c` file and build against AP2.

This is a sketch, not a finished product. Tune sizes to your window.

## Setup

```c
#include <AP2/AP2.h>
#include <stdio.h>
#include <string.h>

#define BRICK_COLS 10
#define BRICK_ROWS 5

typedef struct Brick {
  AP_FRect rect;
  bool alive;
} Brick;

static int HitsBrick(AP_FRect ball, Brick *bricks, int count) {
  int i;
  for (i = 0; i < count; ++i) {
    if (!bricks[i].alive) {
      continue;
    }
    AP_FRect b = bricks[i].rect;
    if (ball.x < b.x + b.w && ball.x + ball.w > b.x &&
        ball.y < b.y + b.h && ball.y + ball.h > b.y) {
      bricks[i].alive = false;
      return 1;
    }
  }
  return 0;
}
```

## Loop

```c
int main(void) {
  AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
  AP_CreateWindow("Breakout", 800, 600, AP_WINDOW_VSYNC);

  AP_Sound *hit = AP_CreateSoundWave(AP_WAVEFORM_SQUARE, 660.0f, 0.06f, 0.35f);
  AP_Sound *wall = AP_CreateSoundWave(AP_WAVEFORM_SINE, 220.0f, 0.08f, 0.3f);

  Brick bricks[BRICK_COLS * BRICK_ROWS];
  int i, r, c;
  for (r = 0; r < BRICK_ROWS; ++r) {
    for (c = 0; c < BRICK_COLS; ++c) {
      i = r * BRICK_COLS + c;
      bricks[i].alive = true;
      bricks[i].rect = (AP_FRect){
          40.0f + (float)c * 72.0f,
          40.0f + (float)r * 28.0f,
          64.0f, 20.0f};
    }
  }

  AP_FRect paddle = {350.0f, 540.0f, 100.0f, 16.0f};
  AP_FRect ball = {390.0f, 400.0f, 12.0f, 12.0f};
  float vx = 220.0f;
  float vy = -280.0f;
  int score = 0;
  int lives = 3;
  double last = AP_GetTime();

  while (AP_IsRunning()) {
    AP_PumpEvents();
    if (AP_IsKeyPressed(AP_KEY_ESCAPE)) {
      AP_RequestClose();
    }

    double now = AP_GetTime();
    float dt = (float)(now - last);
    last = now;
    if (dt > 0.05f) {
      dt = 0.05f;
    }

    float px = 0.0f;
    if (AP_IsKeyDown(AP_KEY_LEFT) || AP_IsKeyDown(AP_KEY_A)) {
      px -= 1.0f;
    }
    if (AP_IsKeyDown(AP_KEY_RIGHT) || AP_IsKeyDown(AP_KEY_D)) {
      px += 1.0f;
    }
    int pad = AP_GetFirstGamepad();
    if (pad >= 0) {
      px += AP_GetGamepadStick(pad, AP_GAMEPAD_STICK_LEFT).x;
    }
    paddle.x = AP_Clampf(paddle.x + px * 520.0f * dt, 8.0f, 800.0f - paddle.w - 8.0f);

    ball.x += vx * dt;
    ball.y += vy * dt;

    if (ball.x < 0.0f || ball.x + ball.w > 800.0f) {
      vx = -vx;
      AP_PlayOneShot(wall);
    }
    if (ball.y < 0.0f) {
      vy = -vy;
      AP_PlayOneShot(wall);
    }
    if (ball.y > 600.0f) {
      lives -= 1;
      ball.x = paddle.x + paddle.w * 0.5f;
      ball.y = 400.0f;
      vy = -280.0f;
      if (lives <= 0) {
        AP_RequestClose();
      }
    }

    if (ball.x < paddle.x + paddle.w && ball.x + ball.w > paddle.x &&
        ball.y < paddle.y + paddle.h && ball.y + ball.h > paddle.y && vy > 0.0f) {
      vy = -vy;
      vx += px * 80.0f;
      AP_PlayOneShot(hit);
    }

    if (HitsBrick(ball, bricks, BRICK_COLS * BRICK_ROWS)) {
      vy = -vy;
      score += 10;
      AP_PlayOneShot(hit);
    }

    AP_SetDrawColor(0.07f, 0.08f, 0.10f, 1.0f);
    AP_Clear();

    AP_SetDrawColor(0.95f, 0.35f, 0.40f, 1.0f);
    AP_FillRect(&paddle);

    AP_SetDrawColor(0.95f, 0.90f, 0.40f, 1.0f);
    AP_FillRect(&ball);

    for (i = 0; i < BRICK_COLS * BRICK_ROWS; ++i) {
      if (!bricks[i].alive) {
        continue;
      }
      float u = (float)(i % BRICK_COLS) / (float)BRICK_COLS;
      AP_SetDrawColor(0.3f + 0.6f * u, 0.55f, 0.95f - 0.4f * u, 1.0f);
      AP_FillRect(&bricks[i].rect);
    }

    char hud[64];
    snprintf(hud, sizeof hud, "score %d   lives %d", score, lives);
    AP_SetDrawColor(1.0f, 1.0f, 1.0f, 1.0f);
    AP_DrawText(16.0f, 8.0f, hud);

    AP_Present();
  }

  AP_DestroySound(hit);
  AP_DestroySound(wall);
  AP_DestroyWindow(NULL);
  AP_Quit();
  return 0;
}
```

## Ideas to extend

- Reset bricks when the field is empty
- Speed the ball after N hits
- `AP_FillRoundedRect` for the paddle
- A short `AP_LoadStream` jingle on win

## Next

[Top-down walker](11-top-down.md)
