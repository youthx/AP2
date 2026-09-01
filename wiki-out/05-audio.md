# Tutorial 05 — Audio

Copyright (c) 2024-2026 Jack Waechter. MIT licensed.

AP2’s mixer is independent of video. Initialize it with `AP_INIT_AUDIO`. `AP_PumpEvents()` already calls `AP_AudioUpdate()`.

## Init

```c
AP_Init(AP_INIT_VIDEO | AP_INIT_AUDIO);
```

Buses: Master, Music, SFX, Voice, Ambient, plus four user buses. Default playback lands on SFX unless you say otherwise.

```c
AP_SetMasterVolume(0.8f);
AP_SetBusVolume(AP_AUDIO_BUS_MUSIC, 0.6f);
AP_SetBusVolume(AP_AUDIO_BUS_SFX, 1.0f);
```

## Procedural blips

No asset required:

```c
AP_Sound *blip = AP_CreateSoundWave(AP_WAVEFORM_SINE, 880.0f, 0.12f, 0.4f);

if (AP_IsKeyPressed(AP_KEY_SPACE)) {
  AP_PlayOneShot(blip);
}
```

Waveforms: `AP_WAVEFORM_SINE`, `SQUARE`, `TRIANGLE`, `SAWTOOTH`, `NOISE`. Destroy sounds at shutdown with `AP_DestroySound`.

## Files

```c
AP_Sound *jump = AP_LoadSound("jump.wav");           /* decode into RAM */
AP_Sound *music = AP_LoadStream("theme.mp3");        /* stream from disk */
```

WAV, FLAC, and MP3 decode through miniaudio. Use streams for long music.

```c
AP_Voice *bgm = AP_PlaySoundOnBus(music, AP_AUDIO_BUS_MUSIC);
AP_SetVoiceLoop(bgm, true);
AP_SetVoiceVolume(bgm, 0.5f);
```

`AP_PlayOneShot` is fire-and-forget: the voice is reaped when it ends. `AP_PlaySound` returns a voice you own until you stop or destroy it.

## Play descriptors

```c
AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
desc.bus = AP_AUDIO_BUS_SFX;
desc.volume = 0.8f;
desc.pitch = 1.1f;
desc.pan = -0.3f;
desc.fade_in_ms = 40.0f;
AP_PlayOneShotEx(blip, &desc);
```

## Spatial (2D)

World layout for 2D helpers is screen XY, with Z as world-up so left/right is X.

```c
AP_SetListenerPosition2D(player_x, player_y);

AP_PlaySoundDesc d = AP_PlaySoundDescDefault();
d.spatial = true;
d.position = AP_V3(crate_x, crate_y, 0.0f);
d.min_distance = 32.0f;
d.max_distance = 400.0f;
AP_PlayOneShotEx(blip, &d);
```

Keep the listener on the camera or the player each frame. For a looping emitter:

```c
AP_Voice *drone = AP_PlaySoundEx(hum, &d);
AP_SetVoiceLoop(drone, true);
/* later */
AP_SetVoicePosition2D(drone, crate_x, crate_y);
```

## Ducking

When voice-over plays, pull music down:

```c
AP_SetBusDuck(AP_AUDIO_BUS_MUSIC, AP_AUDIO_BUS_VOICE, 0.25f, 80.0f, 400.0f);
```

## Next

[Text and fonts](06-text-and-fonts)
