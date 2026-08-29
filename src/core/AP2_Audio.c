/*
 * AP2 — Application Primitives
 * Copyright (c) 2026 Jack Waechter
 *
 * Licensed under the MIT License.
 * See LICENSE in the project root for full terms.
 */

#include "AP2/AP2_Audio.h"

#include "AP2_Internal.h"

#include "AP2/AP2_Error.h"
#include "AP2/AP2_Logger.h"

#include "miniaudio.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define AP_AUDIO_WAVE_RATE 48000
#define AP_AUDIO_LPF_ORDER 2
#define AP_AUDIO_DEG2RAD ((float)(M_PI / 180.0))

struct AP_Sound {
  AP_Sound *next;
  bool streaming;
  char *path;
  void *file_bytes;
  size_t file_size;
  float *pcm;
  ma_uint32 channels;
  ma_uint32 sample_rate;
  ma_uint64 frame_count;
  float duration;
};

struct AP_Voice {
  AP_Voice *next;
  AP_Sound *sound;
  AP_AudioBus bus;
  ma_sound ma;
  ma_audio_buffer buffer;
  ma_decoder decoder;
  ma_lpf_node lpf;
  bool has_buffer;
  bool has_decoder;
  bool has_lpf;
  bool in_use;
  bool fire_and_forget;
  volatile bool finished;
  bool spatial;
  bool muffle;
  bool paused;
  float volume;
  float lowpass_hz;
  float min_distance;
  float max_distance;
  AP_Vec3 position;
  AP_AudioEndCallback on_end;
  void *userdata;
};

typedef struct AP_AudioDuckState {
  bool enabled;
  AP_AudioBus target;
  AP_AudioBus trigger;
  float duck_gain;
  float attack_ms;
  float release_ms;
  float current;
} AP_AudioDuckState;

static bool g_audio_initialized = false;
static bool g_audio_suspended = false;
static ma_engine g_engine;
static ma_sound_group g_groups[AP_AUDIO_BUS_COUNT];
static bool g_group_ready[AP_AUDIO_BUS_COUNT];
static AP_Sound *g_sounds = NULL;
static AP_Voice *g_voices = NULL;
static float g_bus_volume[AP_AUDIO_BUS_COUNT];
static bool g_bus_mute[AP_AUDIO_BUS_COUNT];
static bool g_bus_paused[AP_AUDIO_BUS_COUNT];
static AP_AudioConfig g_audio_config;
static AP_AudioInfo g_audio_info;
static char g_device_name[256];
static char g_backend_name[64];
static AP_AudioListener g_listener;
static AP_AudioDuckState g_duck;
static ma_uint64 g_last_engine_ms = 0;

static float AP_AudioClampf(float value, float lo, float hi) {
  if (value < lo) {
    return lo;
  }
  if (value > hi) {
    return hi;
  }
  return value;
}

static float AP_AudioNyquist(void) {
  ma_uint32 rate = ma_engine_get_sample_rate(&g_engine);
  if (rate < 2) {
    return 20000.0f;
  }
  return (float)rate * 0.49f;
}

static char *AP_AudioStrDup(const char *text) {
  size_t length;
  char *copy;

  if (text == NULL) {
    return NULL;
  }

  length = strlen(text);
  copy = (char *)malloc(length + 1);
  if (copy == NULL) {
    return NULL;
  }

  memcpy(copy, text, length + 1);
  return copy;
}

static bool AP_AudioRequire(void) {
  if (!g_audio_initialized) {
    AP_SET_ERROR(AP_ERROR_NOT_INITIALIZED, "Audio subsystem is not initialized");
    return false;
  }
  return true;
}

static bool AP_AudioBusOk(AP_AudioBus bus) {
  if (bus < 0 || bus >= AP_AUDIO_BUS_COUNT) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid audio bus");
    return false;
  }
  return true;
}

static ma_sound_group *AP_AudioGroup(AP_AudioBus bus) {
  if (bus <= AP_AUDIO_BUS_MASTER || bus >= AP_AUDIO_BUS_COUNT) {
    return NULL;
  }
  if (!g_group_ready[bus]) {
    return NULL;
  }
  return &g_groups[bus];
}

static ma_attenuation_model AP_AudioToMaAttenuation(AP_AudioAttenuation model) {
  switch (model) {
  case AP_AUDIO_ATTENUATION_NONE:
    return ma_attenuation_model_none;
  case AP_AUDIO_ATTENUATION_LINEAR:
    return ma_attenuation_model_linear;
  case AP_AUDIO_ATTENUATION_EXPONENTIAL:
    return ma_attenuation_model_exponential;
  case AP_AUDIO_ATTENUATION_INVERSE:
  default:
    return ma_attenuation_model_inverse;
  }
}

static const char *AP_AudioBackendName(ma_backend backend) {
  switch (backend) {
  case ma_backend_wasapi:
    return "WASAPI";
  case ma_backend_dsound:
    return "DirectSound";
  case ma_backend_winmm:
    return "WinMM";
  case ma_backend_coreaudio:
    return "CoreAudio";
  case ma_backend_pulseaudio:
    return "PulseAudio";
  case ma_backend_alsa:
    return "ALSA";
  case ma_backend_jack:
    return "JACK";
  case ma_backend_aaudio:
    return "AAudio";
  case ma_backend_opensl:
    return "OpenSL";
  case ma_backend_webaudio:
    return "WebAudio";
  case ma_backend_null:
    return "Null";
  default:
    return "Unknown";
  }
}

static void AP_AudioApplyBusGain(AP_AudioBus bus) {
  float gain = g_bus_mute[bus] ? 0.0f : g_bus_volume[bus];

  if (g_duck.enabled && bus == g_duck.target) {
    gain *= g_duck.current;
  }

  if (bus == AP_AUDIO_BUS_MASTER) {
    ma_engine_set_volume(&g_engine, gain);
    return;
  }

  if (g_group_ready[bus]) {
    ma_sound_group_set_volume(&g_groups[bus], gain);
  }
}

static bool AP_AudioDecodeDecoder(ma_decoder *decoder, float **out_pcm,
                                  ma_uint64 *out_frames) {
  ma_uint64 length = 0;
  ma_uint32 channels = decoder->outputChannels;
  float *pcm;
  ma_uint64 read = 0;

  if (channels == 0) {
    return false;
  }

  ma_decoder_get_length_in_pcm_frames(decoder, &length);
  if (length > 0) {
    pcm = (float *)malloc(length * channels * sizeof(float));
    if (pcm == NULL) {
      return false;
    }
    if (ma_decoder_read_pcm_frames(decoder, pcm, length, &read) != MA_SUCCESS &&
        read == 0) {
      free(pcm);
      return false;
    }
    *out_pcm = pcm;
    *out_frames = read;
    return read > 0;
  }

  {
    ma_uint64 cap = 48000;
    ma_uint64 count = 0;
    pcm = (float *)malloc(cap * channels * sizeof(float));
    if (pcm == NULL) {
      return false;
    }

    for (;;) {
      ma_uint64 got = 0;
      ma_result result;

      if (count + 4096 > cap) {
        float *grown;
        cap *= 2;
        grown = (float *)realloc(pcm, cap * channels * sizeof(float));
        if (grown == NULL) {
          free(pcm);
          return false;
        }
        pcm = grown;
      }

      result = ma_decoder_read_pcm_frames(
          decoder, pcm + count * channels, 4096, &got);
      count += got;
      if (got == 0 || result == MA_AT_END) {
        break;
      }
    }

    *out_pcm = pcm;
    *out_frames = count;
    return count > 0;
  }
}

static AP_Sound *AP_AudioSoundAlloc(void) {
  AP_Sound *sound = (AP_Sound *)calloc(1, sizeof(AP_Sound));
  if (sound == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate sound");
    return NULL;
  }
  sound->next = g_sounds;
  g_sounds = sound;
  return sound;
}

static void AP_AudioSoundUnlink(AP_Sound *sound) {
  AP_Sound **cursor = &g_sounds;
  while (*cursor != NULL) {
    if (*cursor == sound) {
      *cursor = sound->next;
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static AP_Sound *AP_AudioLoadDecoded(ma_decoder *decoder) {
  AP_Sound *sound;
  float *pcm = NULL;
  ma_uint64 frames = 0;

  if (!AP_AudioDecodeDecoder(decoder, &pcm, &frames)) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to decode audio");
    return NULL;
  }

  sound = AP_AudioSoundAlloc();
  if (sound == NULL) {
    free(pcm);
    return NULL;
  }

  sound->pcm = pcm;
  sound->channels = decoder->outputChannels;
  sound->sample_rate = decoder->outputSampleRate;
  sound->frame_count = frames;
  if (sound->sample_rate > 0) {
    sound->duration = (float)frames / (float)sound->sample_rate;
  }
  return sound;
}

static void AP_AudioVoiceUninit(AP_Voice *voice) {
  if (voice == NULL || !voice->in_use) {
    return;
  }

  ma_sound_stop(&voice->ma);
  ma_sound_uninit(&voice->ma);

  if (voice->has_lpf) {
    ma_lpf_node_uninit(&voice->lpf, NULL);
    voice->has_lpf = false;
  }
  if (voice->has_buffer) {
    ma_audio_buffer_uninit(&voice->buffer);
    voice->has_buffer = false;
  }
  if (voice->has_decoder) {
    ma_decoder_uninit(&voice->decoder);
    voice->has_decoder = false;
  }

  voice->in_use = false;
}

static void AP_AudioVoiceUnlink(AP_Voice *voice) {
  AP_Voice **cursor = &g_voices;
  while (*cursor != NULL) {
    if (*cursor == voice) {
      *cursor = voice->next;
      return;
    }
    cursor = &(*cursor)->next;
  }
}

static void AP_AudioVoiceFree(AP_Voice *voice) {
  AP_AudioVoiceUninit(voice);
  AP_AudioVoiceUnlink(voice);
  free(voice);
}

static void AP_AudioOnSoundEnd(void *userdata, ma_sound *sound) {
  AP_Voice *voice = (AP_Voice *)userdata;
  (void)sound;
  if (voice != NULL) {
    voice->finished = true;
  }
}

static bool AP_AudioEnsureLowPass(AP_Voice *voice, float cutoff_hz) {
  ma_uint32 channels;
  ma_uint32 rate;
  float nyquist;
  float cutoff;
  ma_node *group;

  cutoff_hz = AP_AudioClampf(cutoff_hz, 20.0f, AP_AudioNyquist());
  voice->lowpass_hz = cutoff_hz;

  if (voice->has_lpf) {
    ma_lpf_config config = ma_lpf_config_init(
        ma_format_f32, ma_engine_get_channels(&g_engine),
        ma_engine_get_sample_rate(&g_engine), (double)cutoff_hz,
        AP_AUDIO_LPF_ORDER);
    return ma_lpf_node_reinit(&config, &voice->lpf) == MA_SUCCESS;
  }

  channels = ma_engine_get_channels(&g_engine);
  rate = ma_engine_get_sample_rate(&g_engine);
  nyquist = AP_AudioNyquist();
  cutoff = cutoff_hz > 0.0f ? cutoff_hz : nyquist;

  {
    ma_lpf_node_config config =
        ma_lpf_node_config_init(channels, rate, (double)cutoff, AP_AUDIO_LPF_ORDER);
    if (ma_lpf_node_init(ma_engine_get_node_graph(&g_engine), &config, NULL,
                         &voice->lpf) != MA_SUCCESS) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to create low-pass node");
      return false;
    }
  }

  group = (ma_node *)AP_AudioGroup(voice->bus);
  if (group == NULL) {
    group = ma_engine_get_endpoint(&g_engine);
  }

  ma_node_attach_output_bus(&voice->lpf, 0, group, 0);
  ma_node_attach_output_bus(&voice->ma, 0, &voice->lpf, 0);
  voice->has_lpf = true;
  voice->lowpass_hz = cutoff;
  return true;
}

static void AP_AudioApplyMuffle(AP_Voice *voice) {
  float dx;
  float dy;
  float dz;
  float dist;
  float t;
  float cutoff;
  float span;

  if (!voice->muffle || !voice->spatial) {
    return;
  }

  dx = voice->position.x - g_listener.position.x;
  dy = voice->position.y - g_listener.position.y;
  dz = voice->position.z - g_listener.position.z;
  dist = sqrtf(dx * dx + dy * dy + dz * dz);

  span = voice->max_distance - voice->min_distance;
  if (span < 0.001f) {
    span = 0.001f;
  }
  t = AP_AudioClampf((dist - voice->min_distance) / span, 0.0f, 1.0f);
  cutoff = AP_AudioNyquist() * (1.0f - t) + 400.0f * t;
  AP_AudioEnsureLowPass(voice, cutoff);
}

static bool AP_AudioInitVoiceSource(AP_Voice *voice, AP_Sound *sound) {
  if (sound->streaming) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 0, 0);
    ma_result result;

    if (sound->path != NULL) {
      result = ma_decoder_init_file(sound->path, &config, &voice->decoder);
    } else if (sound->file_bytes != NULL) {
      result = ma_decoder_init_memory(sound->file_bytes, sound->file_size,
                                      &config, &voice->decoder);
    } else {
      AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Stream has no source data");
      return false;
    }

    if (result != MA_SUCCESS) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, ma_result_description(result));
      return false;
    }

    voice->has_decoder = true;
    return true;
  }

  if (sound->pcm == NULL || sound->frame_count == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_STATE, "Sound has no PCM data");
    return false;
  }

  {
    ma_audio_buffer_config config = ma_audio_buffer_config_init(
        ma_format_f32, sound->channels, sound->frame_count, sound->pcm, NULL);
    config.sampleRate = sound->sample_rate;
    if (ma_audio_buffer_init(&config, &voice->buffer) != MA_SUCCESS) {
      AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to create audio buffer");
      return false;
    }
  }

  voice->has_buffer = true;
  return true;
}

static AP_Voice *AP_AudioSpawnVoice(AP_Sound *sound, const AP_PlaySoundDesc *desc) {
  AP_Voice *voice;
  ma_sound_config config;
  ma_uint32 flags;
  ma_node *attachment;
  ma_result result;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (sound == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sound cannot be NULL");
    return NULL;
  }
  if (!AP_AudioBusOk(desc->bus)) {
    return NULL;
  }

  voice = (AP_Voice *)calloc(1, sizeof(AP_Voice));
  if (voice == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate voice");
    return NULL;
  }

  voice->sound = sound;
  voice->bus = desc->bus;
  voice->fire_and_forget = desc->fire_and_forget;
  voice->spatial = desc->spatial;
  voice->muffle = desc->muffle;
  voice->volume = desc->volume > 0.0f ? desc->volume : 1.0f;
  voice->min_distance = desc->min_distance > 0.0f ? desc->min_distance : 1.0f;
  voice->max_distance = desc->max_distance > voice->min_distance
                            ? desc->max_distance
                            : voice->min_distance * 100.0f;
  voice->position = desc->position;
  voice->on_end = desc->on_end;
  voice->userdata = desc->userdata;
  voice->in_use = true;

  if (!AP_AudioInitVoiceSource(voice, sound)) {
    free(voice);
    return NULL;
  }

  flags = 0;
  if (!desc->spatial) {
    flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
  }

  attachment = (ma_node *)AP_AudioGroup(desc->bus);
  config = ma_sound_config_init_2(&g_engine);
  config.pDataSource =
      voice->has_decoder ? (ma_data_source *)&voice->decoder
                         : (ma_data_source *)&voice->buffer;
  config.pInitialAttachment = attachment;
  config.flags = flags;
  config.isLooping = desc->loop ? MA_TRUE : MA_FALSE;
  config.endCallback = AP_AudioOnSoundEnd;
  config.pEndCallbackUserData = voice;

  result = ma_sound_init_ex(&g_engine, &config, &voice->ma);
  if (result != MA_SUCCESS) {
    if (voice->has_buffer) {
      ma_audio_buffer_uninit(&voice->buffer);
    }
    if (voice->has_decoder) {
      ma_decoder_uninit(&voice->decoder);
    }
    free(voice);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, ma_result_description(result));
    return NULL;
  }

  ma_sound_set_volume(&voice->ma, voice->volume);
  ma_sound_set_pitch(&voice->ma, desc->pitch > 0.0f ? desc->pitch : 1.0f);
  ma_sound_set_pan(&voice->ma, AP_AudioClampf(desc->pan, -1.0f, 1.0f));
  ma_sound_set_looping(&voice->ma, desc->loop ? MA_TRUE : MA_FALSE);

  if (desc->spatial) {
    ma_sound_set_position(&voice->ma, desc->position.x, desc->position.y,
                          desc->position.z);
    ma_sound_set_velocity(&voice->ma, desc->velocity.x, desc->velocity.y,
                          desc->velocity.z);
    ma_sound_set_min_distance(&voice->ma, voice->min_distance);
    ma_sound_set_max_distance(&voice->ma, voice->max_distance);
    ma_sound_set_rolloff(&voice->ma, desc->rolloff > 0.0f ? desc->rolloff : 1.0f);
    ma_sound_set_attenuation_model(&voice->ma,
                                   AP_AudioToMaAttenuation(desc->attenuation));
  }

  if (desc->fade_in_ms > 0.0f) {
    ma_sound_set_fade_in_milliseconds(&voice->ma, 0.0f, voice->volume,
                                      (ma_uint64)desc->fade_in_ms);
  }

  if (desc->lowpass_hz > 0.0f) {
    AP_AudioEnsureLowPass(voice, desc->lowpass_hz);
  }

  if (g_bus_paused[desc->bus] && desc->bus != AP_AUDIO_BUS_MASTER) {
    voice->paused = true;
  } else if (ma_sound_start(&voice->ma) != MA_SUCCESS) {
    ma_sound_uninit(&voice->ma);
    if (voice->has_buffer) {
      ma_audio_buffer_uninit(&voice->buffer);
    }
    if (voice->has_decoder) {
      ma_decoder_uninit(&voice->decoder);
    }
    free(voice);
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to start voice");
    return NULL;
  }

  voice->next = g_voices;
  g_voices = voice;
  return voice;
}

static void AP_AudioStopVoicesForSound(AP_Sound *sound) {
  AP_Voice *voice = g_voices;
  while (voice != NULL) {
    AP_Voice *next = voice->next;
    if (voice->sound == sound) {
      AP_AudioVoiceFree(voice);
    }
    voice = next;
  }
}

static bool AP_AudioBusHasPlaying(AP_AudioBus bus) {
  AP_Voice *voice = g_voices;
  while (voice != NULL) {
    if (voice->in_use && voice->bus == bus && !voice->paused &&
        ma_sound_is_playing(&voice->ma)) {
      return true;
    }
    voice = voice->next;
  }
  return false;
}

static void AP_AudioUpdateDucking(float dt_ms) {
  float target;
  float speed;

  if (!g_duck.enabled) {
    return;
  }

  target = AP_AudioBusHasPlaying(g_duck.trigger) ? g_duck.duck_gain : 1.0f;
  if (target < g_duck.current) {
    speed = (g_duck.attack_ms > 0.0f) ? (dt_ms / g_duck.attack_ms) : 1.0f;
  } else {
    speed = (g_duck.release_ms > 0.0f) ? (dt_ms / g_duck.release_ms) : 1.0f;
  }

  if (speed >= 1.0f) {
    g_duck.current = target;
  } else {
    g_duck.current += (target - g_duck.current) * speed;
  }

  AP_AudioApplyBusGain(g_duck.target);
}

static void AP_AudioFillWave(AP_Waveform type, float frequency, float amplitude,
                             float *pcm, int frames, int rate) {
  int i;
  unsigned int rng = 0xA341316Cu;

  for (i = 0; i < frames; ++i) {
    float t = (float)i / (float)rate;
    float phase = t * frequency;
    float frac = phase - floorf(phase);
    float sample = 0.0f;

    switch (type) {
    case AP_WAVEFORM_SQUARE:
      sample = frac < 0.5f ? 1.0f : -1.0f;
      break;
    case AP_WAVEFORM_TRIANGLE:
      sample = 4.0f * fabsf(frac - 0.5f) - 1.0f;
      break;
    case AP_WAVEFORM_SAWTOOTH:
      sample = 2.0f * frac - 1.0f;
      break;
    case AP_WAVEFORM_NOISE:
      rng = rng * 1664525u + 1013904223u;
      sample = ((float)(rng >> 8) / 16777215.0f) * 2.0f - 1.0f;
      break;
    case AP_WAVEFORM_SINE:
    default:
      sample = sinf(phase * 2.0f * (float)M_PI);
      break;
    }

    pcm[i] = sample * amplitude;
  }

  if (frames > 64 && type != AP_WAVEFORM_NOISE) {
    int fade = frames < 256 ? frames / 8 : 64;
    for (i = 0; i < fade; ++i) {
      float g = (float)i / (float)fade;
      pcm[i] *= g;
      pcm[frames - 1 - i] *= g;
    }
  }
}

static bool AP_AudioCopyBytes(AP_Sound *sound, const void *data, AP_Size size) {
  sound->file_bytes = malloc(size);
  if (sound->file_bytes == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to copy audio bytes");
    return false;
  }
  memcpy(sound->file_bytes, data, size);
  sound->file_size = size;
  return true;
}

AP_AudioConfig AP_AudioDefaultConfig(void) {
  AP_AudioConfig config;
  memset(&config, 0, sizeof(config));
  config.period_ms = 10;
  return config;
}

AP_PlaySoundDesc AP_PlaySoundDescDefault(void) {
  AP_PlaySoundDesc desc;
  memset(&desc, 0, sizeof(desc));
  desc.bus = AP_AUDIO_BUS_SFX;
  desc.volume = 1.0f;
  desc.pitch = 1.0f;
  desc.min_distance = 1.0f;
  desc.max_distance = 100.0f;
  desc.rolloff = 1.0f;
  desc.attenuation = AP_AUDIO_ATTENUATION_INVERSE;
  return desc;
}

bool AP_AudioInit(const AP_AudioConfig *config) {
  ma_engine_config engine_config;
  ma_device *device;
  int bus;

  if (g_audio_initialized) {
    return true;
  }

  g_audio_config = config != NULL ? *config : AP_AudioDefaultConfig();
  engine_config = ma_engine_config_init();
  engine_config.sampleRate =
      g_audio_config.sample_rate > 0 ? (ma_uint32)g_audio_config.sample_rate : 0;
  engine_config.channels =
      g_audio_config.channels > 0 ? (ma_uint32)g_audio_config.channels : 0;
  if (g_audio_config.period_ms > 0) {
    engine_config.periodSizeInMilliseconds = (ma_uint32)g_audio_config.period_ms;
  }

  if (ma_engine_init(&engine_config, &g_engine) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED,
                 "Failed to initialize audio engine");
    return false;
  }

  memset(g_group_ready, 0, sizeof(g_group_ready));
  for (bus = AP_AUDIO_BUS_MUSIC; bus < AP_AUDIO_BUS_COUNT; ++bus) {
    if (ma_sound_group_init(&g_engine, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL,
                            &g_groups[bus]) != MA_SUCCESS) {
      ma_engine_uninit(&g_engine);
      AP_SET_ERROR(AP_ERROR_INITIALIZATION_FAILED, "Failed to create audio bus");
      return false;
    }
    g_group_ready[bus] = true;
    g_bus_volume[bus] = 1.0f;
    g_bus_mute[bus] = false;
    g_bus_paused[bus] = false;
  }

  g_bus_volume[AP_AUDIO_BUS_MASTER] = 1.0f;
  g_bus_mute[AP_AUDIO_BUS_MASTER] = false;
  g_bus_paused[AP_AUDIO_BUS_MASTER] = false;

  g_listener.position.x = 0.0f;
  g_listener.position.y = 0.0f;
  g_listener.position.z = 0.0f;
  g_listener.velocity.x = 0.0f;
  g_listener.velocity.y = 0.0f;
  g_listener.velocity.z = 0.0f;
  g_listener.forward.x = 0.0f;
  g_listener.forward.y = 0.0f;
  g_listener.forward.z = -1.0f;
  g_listener.world_up.x = 0.0f;
  g_listener.world_up.y = 1.0f;
  g_listener.world_up.z = 0.0f;

  ma_engine_listener_set_position(&g_engine, 0, 0.0f, 0.0f, 0.0f);
  ma_engine_listener_set_direction(&g_engine, 0, 0.0f, 0.0f, -1.0f);
  ma_engine_listener_set_world_up(&g_engine, 0, 0.0f, 1.0f, 0.0f);

  memset(&g_duck, 0, sizeof(g_duck));
  g_duck.current = 1.0f;

  device = ma_engine_get_device(&g_engine);
  g_device_name[0] = '\0';
  g_backend_name[0] = '\0';
  if (device != NULL) {
    if (device->playback.name[0] != '\0') {
      strncpy(g_device_name, device->playback.name, sizeof(g_device_name) - 1);
    }
    if (device->pContext != NULL) {
      strncpy(g_backend_name, AP_AudioBackendName(device->pContext->backend),
              sizeof(g_backend_name) - 1);
    }
  }
  if (g_device_name[0] == '\0') {
    strncpy(g_device_name, "Default", sizeof(g_device_name) - 1);
  }
  if (g_backend_name[0] == '\0') {
    strncpy(g_backend_name, "Unknown", sizeof(g_backend_name) - 1);
  }

  g_audio_info.device_name = g_device_name;
  g_audio_info.backend = g_backend_name;
  g_audio_info.sample_rate = (int)ma_engine_get_sample_rate(&g_engine);
  g_audio_info.channels = (int)ma_engine_get_channels(&g_engine);
  g_last_engine_ms = ma_engine_get_time_in_milliseconds(&g_engine);
  g_audio_suspended = false;
  g_audio_initialized = true;

  AP_INFO("Audio initialized (%s, %s, %d Hz, %d ch)", g_backend_name,
          g_device_name, g_audio_info.sample_rate, g_audio_info.channels);
  return true;
}

void AP_AudioClose(void) {
  int bus;

  if (!g_audio_initialized) {
    return;
  }

  while (g_voices != NULL) {
    AP_AudioVoiceFree(g_voices);
  }

  while (g_sounds != NULL) {
    AP_Sound *sound = g_sounds;
    g_sounds = sound->next;
    free(sound->path);
    free(sound->file_bytes);
    free(sound->pcm);
    free(sound);
  }

  for (bus = AP_AUDIO_BUS_MUSIC; bus < AP_AUDIO_BUS_COUNT; ++bus) {
    if (g_group_ready[bus]) {
      ma_sound_group_uninit(&g_groups[bus]);
      g_group_ready[bus] = false;
    }
  }

  ma_engine_uninit(&g_engine);
  g_audio_initialized = false;
  g_audio_suspended = false;
  AP_INFO("Audio shutdown complete");
}

bool AP_AudioIsInitialized(void) { return g_audio_initialized; }

const AP_AudioInfo *AP_AudioGetInfo(void) {
  if (!g_audio_initialized) {
    return NULL;
  }
  return &g_audio_info;
}

const AP_AudioConfig *AP_AudioGetConfig(void) {
  if (!g_audio_initialized) {
    return NULL;
  }
  return &g_audio_config;
}

void AP_AudioUpdate(void) {
  AP_Voice *voice;
  ma_uint64 now;
  float dt_ms;

  if (!g_audio_initialized) {
    return;
  }

  now = ma_engine_get_time_in_milliseconds(&g_engine);
  dt_ms = now >= g_last_engine_ms ? (float)(now - g_last_engine_ms) : 0.0f;
  g_last_engine_ms = now;

  voice = g_voices;
  while (voice != NULL) {
    AP_Voice *next = voice->next;
    if (voice->finished) {
      if (voice->on_end != NULL) {
        voice->on_end(voice, voice->userdata);
        voice->on_end = NULL;
      }
      if (voice->fire_and_forget) {
        AP_AudioVoiceFree(voice);
        voice = next;
        continue;
      }
    } else {
      AP_AudioApplyMuffle(voice);
    }
    voice = next;
  }

  AP_AudioUpdateDucking(dt_ms);
}

void AP_AudioPump(void) { AP_AudioUpdate(); }

bool AP_AudioSuspend(void) {
  if (!AP_AudioRequire()) {
    return false;
  }
  if (g_audio_suspended) {
    return true;
  }
  if (ma_engine_stop(&g_engine) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to suspend audio");
    return false;
  }
  g_audio_suspended = true;
  return true;
}

bool AP_AudioResume(void) {
  if (!AP_AudioRequire()) {
    return false;
  }
  if (!g_audio_suspended) {
    return true;
  }
  if (ma_engine_start(&g_engine) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to resume audio");
    return false;
  }
  g_audio_suspended = false;
  return true;
}

bool AP_AudioIsSuspended(void) { return g_audio_suspended; }

bool AP_SetMasterVolume(float volume) {
  if (!AP_AudioRequire()) {
    return false;
  }
  g_bus_volume[AP_AUDIO_BUS_MASTER] = AP_AudioClampf(volume, 0.0f, 4.0f);
  AP_AudioApplyBusGain(AP_AUDIO_BUS_MASTER);
  return true;
}

float AP_GetMasterVolume(void) { return g_bus_volume[AP_AUDIO_BUS_MASTER]; }

bool AP_SetMasterMute(bool mute) {
  if (!AP_AudioRequire()) {
    return false;
  }
  g_bus_mute[AP_AUDIO_BUS_MASTER] = mute;
  AP_AudioApplyBusGain(AP_AUDIO_BUS_MASTER);
  return true;
}

bool AP_GetMasterMute(void) { return g_bus_mute[AP_AUDIO_BUS_MASTER]; }

void AP_StopAllSounds(void) {
  while (g_voices != NULL) {
    AP_AudioVoiceFree(g_voices);
  }
}

int AP_GetPlayingVoiceCount(void) {
  int count = 0;
  AP_Voice *voice = g_voices;
  while (voice != NULL) {
    if (voice->in_use && ma_sound_is_playing(&voice->ma)) {
      count += 1;
    }
    voice = voice->next;
  }
  return count;
}

bool AP_SetBusVolume(AP_AudioBus bus, float volume) {
  if (!AP_AudioRequire() || !AP_AudioBusOk(bus)) {
    return false;
  }
  g_bus_volume[bus] = AP_AudioClampf(volume, 0.0f, 4.0f);
  AP_AudioApplyBusGain(bus);
  return true;
}

float AP_GetBusVolume(AP_AudioBus bus) {
  if (bus < 0 || bus >= AP_AUDIO_BUS_COUNT) {
    return 0.0f;
  }
  return g_bus_volume[bus];
}

bool AP_SetBusMute(AP_AudioBus bus, bool mute) {
  if (!AP_AudioRequire() || !AP_AudioBusOk(bus)) {
    return false;
  }
  g_bus_mute[bus] = mute;
  AP_AudioApplyBusGain(bus);
  return true;
}

bool AP_GetBusMute(AP_AudioBus bus) {
  if (bus < 0 || bus >= AP_AUDIO_BUS_COUNT) {
    return false;
  }
  return g_bus_mute[bus];
}

bool AP_SetBusPause(AP_AudioBus bus, bool pause) {
  AP_Voice *voice;

  if (!AP_AudioRequire() || !AP_AudioBusOk(bus)) {
    return false;
  }

  g_bus_paused[bus] = pause;
  voice = g_voices;
  while (voice != NULL) {
    if (voice->bus == bus || bus == AP_AUDIO_BUS_MASTER) {
      if (pause) {
        ma_sound_stop(&voice->ma);
        voice->paused = true;
      } else if (voice->paused) {
        ma_sound_start(&voice->ma);
        voice->paused = false;
      }
    }
    voice = voice->next;
  }
  return true;
}

bool AP_GetBusPause(AP_AudioBus bus) {
  if (bus < 0 || bus >= AP_AUDIO_BUS_COUNT) {
    return false;
  }
  return g_bus_paused[bus];
}

void AP_StopBus(AP_AudioBus bus) {
  AP_Voice *voice = g_voices;
  while (voice != NULL) {
    AP_Voice *next = voice->next;
    if (voice->bus == bus || bus == AP_AUDIO_BUS_MASTER) {
      AP_AudioVoiceFree(voice);
    }
    voice = next;
  }
}

const char *AP_AudioBusName(AP_AudioBus bus) {
  switch (bus) {
  case AP_AUDIO_BUS_MASTER:
    return "Master";
  case AP_AUDIO_BUS_MUSIC:
    return "Music";
  case AP_AUDIO_BUS_SFX:
    return "SFX";
  case AP_AUDIO_BUS_VOICE:
    return "Voice";
  case AP_AUDIO_BUS_AMBIENT:
    return "Ambient";
  case AP_AUDIO_BUS_USER0:
    return "User0";
  case AP_AUDIO_BUS_USER1:
    return "User1";
  case AP_AUDIO_BUS_USER2:
    return "User2";
  case AP_AUDIO_BUS_USER3:
    return "User3";
  default:
    return "Unknown";
  }
}

bool AP_SetBusDuck(AP_AudioBus target, AP_AudioBus trigger, float duck_gain,
                   float attack_ms, float release_ms) {
  if (!AP_AudioRequire() || !AP_AudioBusOk(target) || !AP_AudioBusOk(trigger)) {
    return false;
  }

  if (duck_gain >= 1.0f) {
    g_duck.enabled = false;
    g_duck.current = 1.0f;
    AP_AudioApplyBusGain(target);
    return true;
  }

  g_duck.enabled = true;
  g_duck.target = target;
  g_duck.trigger = trigger;
  g_duck.duck_gain = AP_AudioClampf(duck_gain, 0.0f, 1.0f);
  g_duck.attack_ms = attack_ms > 0.0f ? attack_ms : 40.0f;
  g_duck.release_ms = release_ms > 0.0f ? release_ms : 200.0f;
  return true;
}

AP_Sound *AP_LoadSound(const char *path) {
  ma_decoder decoder;
  ma_decoder_config config;
  AP_Sound *sound;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Sound path cannot be NULL");
    return NULL;
  }

  config = ma_decoder_config_init(ma_format_f32, 0, 0);
  if (ma_decoder_init_file(path, &config, &decoder) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Failed to open audio file");
    return NULL;
  }

  sound = AP_AudioLoadDecoded(&decoder);
  ma_decoder_uninit(&decoder);
  if (sound != NULL) {
    sound->path = AP_AudioStrDup(path);
  }
  return sound;
}

AP_Sound *AP_LoadSoundFromMemory(const void *data, AP_Size size) {
  ma_decoder decoder;
  ma_decoder_config config;
  AP_Sound *sound;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (data == NULL || size == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Audio memory is empty");
    return NULL;
  }

  config = ma_decoder_config_init(ma_format_f32, 0, 0);
  if (ma_decoder_init_memory(data, size, &config, &decoder) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to decode audio memory");
    return NULL;
  }

  sound = AP_AudioLoadDecoded(&decoder);
  ma_decoder_uninit(&decoder);
  return sound;
}

AP_Sound *AP_LoadStream(const char *path) {
  AP_Sound *sound;
  ma_decoder decoder;
  ma_decoder_config config;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (path == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Stream path cannot be NULL");
    return NULL;
  }

  config = ma_decoder_config_init(ma_format_f32, 0, 0);
  if (ma_decoder_init_file(path, &config, &decoder) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_NOT_FOUND, "Failed to open audio stream");
    return NULL;
  }

  sound = AP_AudioSoundAlloc();
  if (sound == NULL) {
    ma_decoder_uninit(&decoder);
    return NULL;
  }

  sound->streaming = true;
  sound->path = AP_AudioStrDup(path);
  sound->channels = decoder.outputChannels;
  sound->sample_rate = decoder.outputSampleRate;
  {
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    sound->frame_count = frames;
    if (sound->sample_rate > 0 && frames > 0) {
      sound->duration = (float)frames / (float)sound->sample_rate;
    }
  }
  ma_decoder_uninit(&decoder);
  return sound;
}

AP_Sound *AP_LoadStreamFromMemory(const void *data, AP_Size size) {
  AP_Sound *sound;
  ma_decoder decoder;
  ma_decoder_config config;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (data == NULL || size == 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Stream memory is empty");
    return NULL;
  }

  config = ma_decoder_config_init(ma_format_f32, 0, 0);
  if (ma_decoder_init_memory(data, size, &config, &decoder) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to open audio stream memory");
    return NULL;
  }

  sound = AP_AudioSoundAlloc();
  if (sound == NULL) {
    ma_decoder_uninit(&decoder);
    return NULL;
  }

  sound->streaming = true;
  sound->channels = decoder.outputChannels;
  sound->sample_rate = decoder.outputSampleRate;
  {
    ma_uint64 frames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &frames);
    sound->frame_count = frames;
    if (sound->sample_rate > 0 && frames > 0) {
      sound->duration = (float)frames / (float)sound->sample_rate;
    }
  }
  ma_decoder_uninit(&decoder);

  if (!AP_AudioCopyBytes(sound, data, size)) {
    AP_AudioSoundUnlink(sound);
    free(sound);
    return NULL;
  }

  return sound;
}

AP_Sound *AP_CreateSoundFromPCM(const AP_AudioPCM *pcm) {
  AP_Sound *sound;
  size_t bytes;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (pcm == NULL || pcm->samples == NULL || pcm->frame_count <= 0 ||
      pcm->channels <= 0 || pcm->sample_rate <= 0) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid PCM descriptor");
    return NULL;
  }

  sound = AP_AudioSoundAlloc();
  if (sound == NULL) {
    return NULL;
  }

  bytes = (size_t)pcm->frame_count * (size_t)pcm->channels * sizeof(float);
  sound->pcm = (float *)malloc(bytes);
  if (sound->pcm == NULL) {
    AP_AudioSoundUnlink(sound);
    free(sound);
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to copy PCM");
    return NULL;
  }

  memcpy(sound->pcm, pcm->samples, bytes);
  sound->channels = (ma_uint32)pcm->channels;
  sound->sample_rate = (ma_uint32)pcm->sample_rate;
  sound->frame_count = (ma_uint64)pcm->frame_count;
  sound->duration = (float)pcm->frame_count / (float)pcm->sample_rate;
  return sound;
}

AP_Sound *AP_CreateSoundWave(AP_Waveform type, float frequency, float duration,
                             float amplitude) {
  AP_AudioPCM pcm;
  int frames;
  float *samples;
  AP_Sound *sound;

  if (!AP_AudioRequire()) {
    return NULL;
  }
  if (duration <= 0.0f) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Wave duration must be positive");
    return NULL;
  }

  frames = (int)(duration * (float)AP_AUDIO_WAVE_RATE + 0.5f);
  if (frames < 16) {
    frames = 16;
  }

  samples = (float *)malloc((size_t)frames * sizeof(float));
  if (samples == NULL) {
    AP_SET_ERROR(AP_ERROR_OUT_OF_MEMORY, "Failed to allocate waveform");
    return NULL;
  }

  AP_AudioFillWave(type, frequency > 0.0f ? frequency : 440.0f,
                   AP_AudioClampf(amplitude, 0.0f, 1.0f), samples, frames,
                   AP_AUDIO_WAVE_RATE);

  pcm.samples = samples;
  pcm.frame_count = frames;
  pcm.channels = 1;
  pcm.sample_rate = AP_AUDIO_WAVE_RATE;
  sound = AP_CreateSoundFromPCM(&pcm);
  free(samples);
  return sound;
}

void AP_DestroySound(AP_Sound *sound) {
  if (sound == NULL) {
    return;
  }

  AP_AudioStopVoicesForSound(sound);
  AP_AudioSoundUnlink(sound);
  free(sound->path);
  free(sound->file_bytes);
  free(sound->pcm);
  free(sound);
}

bool AP_SoundIsValid(const AP_Sound *sound) {
  const AP_Sound *cursor = g_sounds;
  while (cursor != NULL) {
    if (cursor == sound) {
      return true;
    }
    cursor = cursor->next;
  }
  return false;
}

bool AP_SoundIsStreaming(const AP_Sound *sound) {
  return sound != NULL && sound->streaming;
}

float AP_GetSoundDuration(const AP_Sound *sound) {
  return sound != NULL ? sound->duration : 0.0f;
}

int AP_GetSoundChannels(const AP_Sound *sound) {
  return sound != NULL ? (int)sound->channels : 0;
}

int AP_GetSoundSampleRate(const AP_Sound *sound) {
  return sound != NULL ? (int)sound->sample_rate : 0;
}

AP_Voice *AP_PlaySound(AP_Sound *sound) {
  AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
  return AP_PlaySoundEx(sound, &desc);
}

AP_Voice *AP_PlaySoundOnBus(AP_Sound *sound, AP_AudioBus bus) {
  AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
  desc.bus = bus;
  return AP_PlaySoundEx(sound, &desc);
}

AP_Voice *AP_PlaySoundEx(AP_Sound *sound, const AP_PlaySoundDesc *desc) {
  AP_PlaySoundDesc local;

  if (desc == NULL) {
    local = AP_PlaySoundDescDefault();
    desc = &local;
  }

  return AP_AudioSpawnVoice(sound, desc);
}

bool AP_PlayOneShot(AP_Sound *sound) {
  AP_PlaySoundDesc desc = AP_PlaySoundDescDefault();
  desc.fire_and_forget = true;
  return AP_PlaySoundEx(sound, &desc) != NULL;
}

bool AP_PlayOneShotEx(AP_Sound *sound, const AP_PlaySoundDesc *desc) {
  AP_PlaySoundDesc local;

  if (desc == NULL) {
    local = AP_PlaySoundDescDefault();
  } else {
    local = *desc;
  }
  local.fire_and_forget = true;
  return AP_PlaySoundEx(sound, &local) != NULL;
}

void AP_DestroyVoice(AP_Voice *voice) {
  if (voice == NULL || !AP_VoiceIsValid(voice)) {
    return;
  }
  AP_AudioVoiceFree(voice);
}

bool AP_VoiceIsValid(const AP_Voice *voice) {
  const AP_Voice *cursor = g_voices;
  while (cursor != NULL) {
    if (cursor == voice && cursor->in_use) {
      return true;
    }
    cursor = cursor->next;
  }
  return false;
}

bool AP_StopVoice(AP_Voice *voice) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_stop(&voice->ma);
  voice->paused = false;
  voice->finished = true;
  return true;
}

bool AP_PauseVoice(AP_Voice *voice) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_stop(&voice->ma);
  voice->paused = true;
  return true;
}

bool AP_ResumeVoice(AP_Voice *voice) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  if (voice->finished && !ma_sound_is_looping(&voice->ma)) {
    ma_sound_seek_to_pcm_frame(&voice->ma, 0);
    voice->finished = false;
  }
  if (ma_sound_start(&voice->ma) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to resume voice");
    return false;
  }
  voice->paused = false;
  return true;
}

bool AP_VoiceIsPlaying(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) && ma_sound_is_playing(&voice->ma);
}

bool AP_VoiceAtEnd(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) &&
         (voice->finished || ma_sound_at_end(&voice->ma));
}

bool AP_SetVoiceVolume(AP_Voice *voice, float volume) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  voice->volume = AP_AudioClampf(volume, 0.0f, 4.0f);
  ma_sound_set_volume(&voice->ma, voice->volume);
  return true;
}

float AP_GetVoiceVolume(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? voice->volume : 0.0f;
}

bool AP_SetVoicePitch(AP_Voice *voice, float pitch) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_pitch(&voice->ma, AP_AudioClampf(pitch, 0.01f, 8.0f));
  return true;
}

float AP_GetVoicePitch(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? ma_sound_get_pitch(&voice->ma) : 1.0f;
}

bool AP_SetVoicePan(AP_Voice *voice, float pan) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_pan(&voice->ma, AP_AudioClampf(pan, -1.0f, 1.0f));
  return true;
}

float AP_GetVoicePan(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? ma_sound_get_pan(&voice->ma) : 0.0f;
}

bool AP_SetVoiceLoop(AP_Voice *voice, bool loop) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_looping(&voice->ma, loop ? MA_TRUE : MA_FALSE);
  return true;
}

bool AP_GetVoiceLoop(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) && ma_sound_is_looping(&voice->ma);
}

bool AP_SeekVoice(AP_Voice *voice, float seconds) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  if (seconds < 0.0f) {
    seconds = 0.0f;
  }
  if (ma_sound_seek_to_second(&voice->ma, seconds) != MA_SUCCESS) {
    AP_SET_ERROR(AP_ERROR_OPERATION_FAILED, "Failed to seek voice");
    return false;
  }
  voice->finished = false;
  return true;
}

float AP_GetVoiceCursor(const AP_Voice *voice) {
  float cursor = 0.0f;
  if (!AP_VoiceIsValid(voice)) {
    return 0.0f;
  }
  ma_sound_get_cursor_in_seconds(&voice->ma, &cursor);
  return cursor;
}

float AP_GetVoiceDuration(const AP_Voice *voice) {
  float length = 0.0f;
  if (!AP_VoiceIsValid(voice)) {
    return 0.0f;
  }
  if (ma_sound_get_length_in_seconds(&voice->ma, &length) != MA_SUCCESS) {
    return voice->sound != NULL ? voice->sound->duration : 0.0f;
  }
  return length;
}

bool AP_FadeVoice(AP_Voice *voice, float to_volume, float duration_ms) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  to_volume = AP_AudioClampf(to_volume, 0.0f, 4.0f);
  ma_sound_set_fade_in_milliseconds(&voice->ma, -1.0f, to_volume,
                                    (ma_uint64)(duration_ms > 0.0f ? duration_ms
                                                                   : 1.0f));
  voice->volume = to_volume;
  return true;
}

bool AP_StopVoiceFaded(AP_Voice *voice, float duration_ms) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_stop_with_fade_in_milliseconds(
      &voice->ma, (ma_uint64)(duration_ms > 0.0f ? duration_ms : 1.0f));
  voice->fire_and_forget = true;
  return true;
}

bool AP_SetVoiceLowPass(AP_Voice *voice, float cutoff_hz) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  return AP_AudioEnsureLowPass(voice, cutoff_hz);
}

float AP_GetVoiceLowPass(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? voice->lowpass_hz : 0.0f;
}

AP_AudioBus AP_GetVoiceBus(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? voice->bus : AP_AUDIO_BUS_MASTER;
}

AP_Sound *AP_GetVoiceSound(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) ? voice->sound : NULL;
}

bool AP_SetListener(const AP_AudioListener *listener) {
  if (!AP_AudioRequire()) {
    return false;
  }
  if (listener == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Listener cannot be NULL");
    return false;
  }

  g_listener = *listener;
  ma_engine_listener_set_position(&g_engine, 0, listener->position.x,
                                  listener->position.y, listener->position.z);
  ma_engine_listener_set_velocity(&g_engine, 0, listener->velocity.x,
                                  listener->velocity.y, listener->velocity.z);
  ma_engine_listener_set_direction(&g_engine, 0, listener->forward.x,
                                   listener->forward.y, listener->forward.z);
  ma_engine_listener_set_world_up(&g_engine, 0, listener->world_up.x,
                                  listener->world_up.y, listener->world_up.z);
  return true;
}

bool AP_GetListener(AP_AudioListener *listener) {
  if (!AP_AudioRequire()) {
    return false;
  }
  if (listener == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Listener cannot be NULL");
    return false;
  }
  *listener = g_listener;
  return true;
}

bool AP_SetListenerPosition(float x, float y, float z) {
  if (!AP_AudioRequire()) {
    return false;
  }
  g_listener.position.x = x;
  g_listener.position.y = y;
  g_listener.position.z = z;
  ma_engine_listener_set_position(&g_engine, 0, x, y, z);
  return true;
}

bool AP_SetListenerPosition2D(float x, float y) {
  if (!AP_AudioRequire()) {
    return false;
  }

  g_listener.world_up.x = 0.0f;
  g_listener.world_up.y = 0.0f;
  g_listener.world_up.z = 1.0f;
  g_listener.forward.x = 0.0f;
  g_listener.forward.y = -1.0f;
  g_listener.forward.z = 0.0f;
  ma_engine_listener_set_world_up(&g_engine, 0, 0.0f, 0.0f, 1.0f);
  ma_engine_listener_set_direction(&g_engine, 0, 0.0f, -1.0f, 0.0f);
  return AP_SetListenerPosition(x, y, 0.0f);
}

bool AP_SetListenerVelocity(float x, float y, float z) {
  if (!AP_AudioRequire()) {
    return false;
  }
  g_listener.velocity.x = x;
  g_listener.velocity.y = y;
  g_listener.velocity.z = z;
  ma_engine_listener_set_velocity(&g_engine, 0, x, y, z);
  return true;
}

bool AP_SetListenerOrientation(float forward_x, float forward_y,
                               float forward_z, float up_x, float up_y,
                               float up_z) {
  if (!AP_AudioRequire()) {
    return false;
  }
  g_listener.forward.x = forward_x;
  g_listener.forward.y = forward_y;
  g_listener.forward.z = forward_z;
  g_listener.world_up.x = up_x;
  g_listener.world_up.y = up_y;
  g_listener.world_up.z = up_z;
  ma_engine_listener_set_direction(&g_engine, 0, forward_x, forward_y, forward_z);
  ma_engine_listener_set_world_up(&g_engine, 0, up_x, up_y, up_z);
  return true;
}

bool AP_SetListenerCone(AP_AudioCone cone) {
  if (!AP_AudioRequire()) {
    return false;
  }
  ma_engine_listener_set_cone(&g_engine, 0, cone.inner_degrees * AP_AUDIO_DEG2RAD,
                              cone.outer_degrees * AP_AUDIO_DEG2RAD,
                              cone.outer_gain);
  return true;
}

bool AP_SetVoiceSpatial(AP_Voice *voice, bool enabled) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  voice->spatial = enabled;
  ma_sound_set_spatialization_enabled(&voice->ma, enabled ? MA_TRUE : MA_FALSE);
  return true;
}

bool AP_GetVoiceSpatial(const AP_Voice *voice) {
  return AP_VoiceIsValid(voice) && voice->spatial;
}

bool AP_SetVoicePosition(AP_Voice *voice, float x, float y, float z) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  voice->position.x = x;
  voice->position.y = y;
  voice->position.z = z;
  ma_sound_set_position(&voice->ma, x, y, z);
  if (!voice->spatial) {
    AP_SetVoiceSpatial(voice, true);
  }
  return true;
}

bool AP_SetVoicePosition2D(AP_Voice *voice, float x, float y) {
  return AP_SetVoicePosition(voice, x, y, 0.0f);
}

bool AP_GetVoicePosition(const AP_Voice *voice, AP_Vec3 *position) {
  if (!AP_VoiceIsValid(voice) || position == NULL) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Invalid voice position query");
    return false;
  }
  *position = voice->position;
  return true;
}

bool AP_SetVoiceVelocity(AP_Voice *voice, float x, float y, float z) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_velocity(&voice->ma, x, y, z);
  return true;
}

bool AP_SetVoiceDirection(AP_Voice *voice, float x, float y, float z) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_direction(&voice->ma, x, y, z);
  return true;
}

bool AP_SetVoiceMinMaxDistance(AP_Voice *voice, float min_distance,
                               float max_distance) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  if (min_distance < 0.0f) {
    min_distance = 0.0f;
  }
  if (max_distance < min_distance) {
    max_distance = min_distance;
  }
  voice->min_distance = min_distance;
  voice->max_distance = max_distance;
  ma_sound_set_min_distance(&voice->ma, min_distance);
  ma_sound_set_max_distance(&voice->ma, max_distance);
  return true;
}

bool AP_SetVoiceAttenuation(AP_Voice *voice, AP_AudioAttenuation model,
                            float rolloff) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_attenuation_model(&voice->ma, AP_AudioToMaAttenuation(model));
  ma_sound_set_rolloff(&voice->ma, rolloff > 0.0f ? rolloff : 1.0f);
  return true;
}

bool AP_SetVoiceCone(AP_Voice *voice, AP_AudioCone cone) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_cone(&voice->ma, cone.inner_degrees * AP_AUDIO_DEG2RAD,
                    cone.outer_degrees * AP_AUDIO_DEG2RAD, cone.outer_gain);
  return true;
}

bool AP_SetVoiceDoppler(AP_Voice *voice, float factor) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  ma_sound_set_doppler_factor(&voice->ma, AP_AudioClampf(factor, 0.0f, 8.0f));
  return true;
}

bool AP_SetVoiceMuffle(AP_Voice *voice, bool enabled) {
  if (!AP_VoiceIsValid(voice)) {
    AP_SET_ERROR(AP_ERROR_INVALID_ARGUMENT, "Voice is not valid");
    return false;
  }
  voice->muffle = enabled;
  return true;
}

static bool AP_AudioSubsystemInit(void) { return AP_AudioInit(NULL); }

const AP_SubsystemMetadata AP_AudioSubsystem = {
    .init = AP_AudioSubsystemInit,
    .close = AP_AudioClose,
};
