/*
 * AP2 — Application Primitives
 * Copyright (c) 2024-2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#ifndef AP2_AUDIO_H
#define AP2_AUDIO_H

#include "AP2/AP2_Init.h"
#include "AP2/AP2_Types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * AP2 Audio
 *
 * Device I/O, mixing, buses, streaming, and 3D spatialization.
 * Independent of the graphics stack — initialize with AP_INIT_AUDIO.
 *
 *     AP_Init(AP_INIT_AUDIO);
 *     AP_Sound *blip = AP_CreateSoundWave(AP_WAVEFORM_SINE, 880.0f, 0.12f, 0.4f);
 *     AP_PlayOneShot(blip);
 *
 *     AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
 *     desc.spatial = true;
 *     desc.position.x = 4.0f;
 *     AP_PlaySoundEx(blip, &desc);
 *
 * Decoded buffers: WAV, FLAC, MP3. Use AP_LoadStream() for long music.
 * AP_AudioUpdate() is called from AP_PumpEvents(); call it yourself in
 * audio-only programs.
 *
 * Spatial layout is right-handed: +Y up, -Z forward (OpenGL). 2D helpers
 * place sounds in XY with +Z as world-up so screen X is left/right.
 */

typedef struct AP_Sound AP_Sound;
typedef struct AP_Voice AP_Voice;

typedef void (*AP_AudioEndCallback)(AP_Voice *voice, void *userdata);

/* =========================================================
 * Buses
 * ========================================================= */

typedef enum AP_AudioBus {
  AP_AUDIO_BUS_MASTER = 0,
  AP_AUDIO_BUS_MUSIC,
  AP_AUDIO_BUS_SFX,
  AP_AUDIO_BUS_VOICE,
  AP_AUDIO_BUS_AMBIENT,
  AP_AUDIO_BUS_USER0,
  AP_AUDIO_BUS_USER1,
  AP_AUDIO_BUS_USER2,
  AP_AUDIO_BUS_USER3,
  AP_AUDIO_BUS_COUNT
} AP_AudioBus;

/* =========================================================
 * Spatial attenuation
 * ========================================================= */

typedef enum AP_AudioAttenuation {
  AP_AUDIO_ATTENUATION_NONE = 0,
  AP_AUDIO_ATTENUATION_INVERSE,
  AP_AUDIO_ATTENUATION_LINEAR,
  AP_AUDIO_ATTENUATION_EXPONENTIAL
} AP_AudioAttenuation;

/* =========================================================
 * Waveforms (procedural buffers)
 * ========================================================= */

typedef enum AP_Waveform {
  AP_WAVEFORM_SINE = 0,
  AP_WAVEFORM_SQUARE,
  AP_WAVEFORM_TRIANGLE,
  AP_WAVEFORM_SAWTOOTH,
  AP_WAVEFORM_NOISE
} AP_Waveform;

/* =========================================================
 * Configuration
 * ========================================================= */

typedef struct AP_AudioConfig {
  /*
   * Mix sample rate. 0 = device native.
   */
  int sample_rate;

  /*
   * Mix channels. 0 = device native (typically 2).
   */
  int channels;

  /*
   * Device period in milliseconds. 0 = backend default.
   */
  int period_ms;
} AP_AudioConfig;

typedef struct AP_AudioInfo {
  const char *device_name;
  const char *backend;
  int sample_rate;
  int channels;
} AP_AudioInfo;

typedef struct AP_AudioPCM {
  const float *samples;
  int frame_count;
  int channels;
  int sample_rate;
} AP_AudioPCM;

typedef struct AP_AudioListener {
  AP_Vec3 position;
  AP_Vec3 velocity;
  AP_Vec3 forward;
  AP_Vec3 world_up;
} AP_AudioListener;

typedef struct AP_AudioCone {
  float inner_degrees;
  float outer_degrees;
  float outer_gain;
} AP_AudioCone;

typedef struct AP_PlaySoundDesc {
  AP_AudioBus bus;
  float volume;
  float pitch;
  float pan;
  bool loop;
  bool spatial;
  bool fire_and_forget;
  bool muffle;
  float fade_in_ms;
  AP_Vec3 position;
  AP_Vec3 velocity;
  float min_distance;
  float max_distance;
  float rolloff;
  AP_AudioAttenuation attenuation;
  float lowpass_hz;
  AP_AudioEndCallback on_end;
  void *userdata;
} AP_PlaySoundDesc;

/* =========================================================
 * Subsystem
 * ========================================================= */

bool AP_AudioInit(const AP_AudioConfig *config);

void AP_AudioClose(void);

bool AP_AudioIsInitialized(void);

const AP_AudioInfo *AP_AudioGetInfo(void);

const AP_AudioConfig *AP_AudioGetConfig(void);

AP_AudioConfig AP_AudioDefaultConfig(void);

/*
 * Reap finished one-shots, apply ducking and distance muffling.
 * Invoked automatically from AP_PumpEvents().
 */
void AP_AudioUpdate(void);

bool AP_AudioSuspend(void);

bool AP_AudioResume(void);

bool AP_AudioIsSuspended(void);

/* =========================================================
 * Master
 * ========================================================= */

bool AP_SetMasterVolume(float volume);

float AP_GetMasterVolume(void);

bool AP_SetMasterMute(bool mute);

bool AP_GetMasterMute(void);

void AP_StopAllSounds(void);

int AP_GetPlayingVoiceCount(void);

/* =========================================================
 * Buses
 * ========================================================= */

bool AP_SetBusVolume(AP_AudioBus bus, float volume);

float AP_GetBusVolume(AP_AudioBus bus);

bool AP_SetBusMute(AP_AudioBus bus, bool mute);

bool AP_GetBusMute(AP_AudioBus bus);

bool AP_SetBusPause(AP_AudioBus bus, bool pause);

bool AP_GetBusPause(AP_AudioBus bus);

void AP_StopBus(AP_AudioBus bus);

const char *AP_AudioBusName(AP_AudioBus bus);

/*
 * When any voice on `trigger` is playing, `target` volume is scaled
 * toward `duck_gain` over `attack_ms`, then restored over `release_ms`.
 * Pass duck_gain >= 1 to disable.
 */
bool AP_SetBusDuck(AP_AudioBus target, AP_AudioBus trigger, float duck_gain,
                   float attack_ms, float release_ms);

/* =========================================================
 * Sounds
 * ========================================================= */

AP_Sound *AP_LoadSound(const char *path);

AP_Sound *AP_LoadSoundFromMemory(const void *data, AP_Size size);

AP_Sound *AP_LoadStream(const char *path);

AP_Sound *AP_LoadStreamFromMemory(const void *data, AP_Size size);

AP_Sound *AP_CreateSoundFromPCM(const AP_AudioPCM *pcm);

AP_Sound *AP_CreateSoundWave(AP_Waveform type, float frequency, float duration,
                             float amplitude);

void AP_DestroySound(AP_Sound *sound);

bool AP_SoundIsValid(const AP_Sound *sound);

bool AP_SoundIsStreaming(const AP_Sound *sound);

float AP_GetSoundDuration(const AP_Sound *sound);

int AP_GetSoundChannels(const AP_Sound *sound);

int AP_GetSoundSampleRate(const AP_Sound *sound);

/* =========================================================
 * Playback
 * ========================================================= */

AP_PlaySoundDesc AP_PlaySoundDescDefault(void);

AP_Voice *AP_PlaySound(AP_Sound *sound);

AP_Voice *AP_PlaySoundOnBus(AP_Sound *sound, AP_AudioBus bus);

AP_Voice *AP_PlaySoundEx(AP_Sound *sound, const AP_PlaySoundDesc *desc);

bool AP_PlayOneShot(AP_Sound *sound);

bool AP_PlayOneShotEx(AP_Sound *sound, const AP_PlaySoundDesc *desc);

void AP_DestroyVoice(AP_Voice *voice);

bool AP_VoiceIsValid(const AP_Voice *voice);

bool AP_StopVoice(AP_Voice *voice);

bool AP_PauseVoice(AP_Voice *voice);

bool AP_ResumeVoice(AP_Voice *voice);

bool AP_VoiceIsPlaying(const AP_Voice *voice);

bool AP_VoiceAtEnd(const AP_Voice *voice);

bool AP_SetVoiceVolume(AP_Voice *voice, float volume);

float AP_GetVoiceVolume(const AP_Voice *voice);

bool AP_SetVoicePitch(AP_Voice *voice, float pitch);

float AP_GetVoicePitch(const AP_Voice *voice);

bool AP_SetVoicePan(AP_Voice *voice, float pan);

float AP_GetVoicePan(const AP_Voice *voice);

bool AP_SetVoiceLoop(AP_Voice *voice, bool loop);

bool AP_GetVoiceLoop(const AP_Voice *voice);

bool AP_SeekVoice(AP_Voice *voice, float seconds);

float AP_GetVoiceCursor(const AP_Voice *voice);

float AP_GetVoiceDuration(const AP_Voice *voice);

bool AP_FadeVoice(AP_Voice *voice, float to_volume, float duration_ms);

bool AP_StopVoiceFaded(AP_Voice *voice, float duration_ms);

bool AP_SetVoiceLowPass(AP_Voice *voice, float cutoff_hz);

float AP_GetVoiceLowPass(const AP_Voice *voice);

AP_AudioBus AP_GetVoiceBus(const AP_Voice *voice);

AP_Sound *AP_GetVoiceSound(const AP_Voice *voice);

/* =========================================================
 * Listener / 3D
 * ========================================================= */

bool AP_SetListener(const AP_AudioListener *listener);

bool AP_GetListener(AP_AudioListener *listener);

bool AP_SetListenerPosition(float x, float y, float z);

bool AP_SetListenerPosition2D(float x, float y);

bool AP_SetListenerVelocity(float x, float y, float z);

bool AP_SetListenerOrientation(float forward_x, float forward_y,
                               float forward_z, float up_x, float up_y,
                               float up_z);

bool AP_SetListenerCone(AP_AudioCone cone);

bool AP_SetVoiceSpatial(AP_Voice *voice, bool enabled);

bool AP_GetVoiceSpatial(const AP_Voice *voice);

bool AP_SetVoicePosition(AP_Voice *voice, float x, float y, float z);

bool AP_SetVoicePosition2D(AP_Voice *voice, float x, float y);

bool AP_GetVoicePosition(const AP_Voice *voice, AP_Vec3 *position);

bool AP_SetVoiceVelocity(AP_Voice *voice, float x, float y, float z);

bool AP_SetVoiceDirection(AP_Voice *voice, float x, float y, float z);

bool AP_SetVoiceMinMaxDistance(AP_Voice *voice, float min_distance,
                               float max_distance);

bool AP_SetVoiceAttenuation(AP_Voice *voice, AP_AudioAttenuation model,
                            float rolloff);

bool AP_SetVoiceCone(AP_Voice *voice, AP_AudioCone cone);

bool AP_SetVoiceDoppler(AP_Voice *voice, float factor);

bool AP_SetVoiceMuffle(AP_Voice *voice, bool enabled);

extern const AP_SubsystemMetadata AP_AudioSubsystem;

#ifdef __cplusplus
}
#endif

#endif /* AP2_AUDIO_H */
