/* Copyright (C) 2014 Hans-Kristian Arntzen <maister@archlinux.us>
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "fmsynth_private.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <math.h>

#ifndef max
#define max(a, b) ((a) > (b) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifdef __GNUC__
#define FMSYNTH_ALIGNED_PRE(x)
#define FMSYNTH_ALIGNED_CACHE_PRE
#define FMSYNTH_ALIGNED_POST(x) __attribute__((aligned(x)))
#define FMSYNTH_ALIGNED_CACHE_POST FMSYNTH_ALIGNED_POST(64)
#define FMSYNTH_NOINLINE __attribute__((noinline))
#define FMSYNTH_ASSUME_ALIGNED(x, align) __builtin_assume_aligned(x, align)
#elif defined(_MSC_VER)
#define FMSYNTH_ALIGNED_PRE(x) __declspec(align(x))
#define FMSYNTH_ALIGNED_CACHE_PRE FMSYNTH_ALIGNED_PRE(64)
#define FMSYNTH_ALIGNED_POST(x)
#define FMSYNTH_ALIGNED_CACHE_POST
#define FMSYNTH_NOINLINE __declspec(noinline)
#define FMSYNTH_ASSUME_ALIGNED(x, align) x
#else
#define FMSYNTH_ALIGNED_PRE(x)
#define FMSYNTH_ALIGNED_CACHE_PRE
#define FMSYNTH_ALIGNED_POST(x)
#define FMSYNTH_ALIGNED_CACHE_POST
#define FMSYNTH_NOINLINE
#define FMSYNTH_ASSUME_ALIGNED(x, align) x
#endif

#undef PI
#define PI 3.14159265359f

#define INV_FACTORIAL_3_2PIPOW3 ((1.0f / 6.0f) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI))
#define INV_FACTORIAL_5_2PIPOW5 ((1.0f / 120.0f) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI))
#define INV_FACTORIAL_7_2PIPOW7 ((1.0f / 5040.0f) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI) * (2.0f * PI))

#define FMSYNTH_FRAMES_PER_LFO 32

enum fmsynth_voice_state
{
   FMSYNTH_VOICE_INACTIVE = 0,
   FMSYNTH_VOICE_RUNNING,
   FMSYNTH_VOICE_SUSTAINED,
   FMSYNTH_VOICE_RELEASED
};

struct fmsynth_voice
{
   enum fmsynth_voice_state state;
   uint8_t note;
   uint8_t enable;
   uint8_t dead;

   float base_freq;
   float env_speed;
   float pos;
   float speed;

   float lfo_step;
   float lfo_phase;
   unsigned count;

   // Used in process_frames(). Should be local in cache.
   FMSYNTH_ALIGNED_CACHE_PRE float phases[FMSYNTH_OPERATORS] FMSYNTH_ALIGNED_CACHE_POST;
   float env[FMSYNTH_OPERATORS];
   float read_mod[FMSYNTH_OPERATORS];
   float target_env_step[FMSYNTH_OPERATORS];
   float step_rate[FMSYNTH_OPERATORS];
   float lfo_freq_mod[FMSYNTH_OPERATORS];
   float pan_amp[2][FMSYNTH_OPERATORS];

   // Using when updating envelope (every N sample).
   float falloff[FMSYNTH_OPERATORS];
   float end_time[FMSYNTH_OPERATORS];
   float target_env[FMSYNTH_OPERATORS];

   float release_time[FMSYNTH_OPERATORS];
   float target[4][FMSYNTH_OPERATORS];
   float time[4][FMSYNTH_OPERATORS];
   float lerp[3][FMSYNTH_OPERATORS];

   float amp[FMSYNTH_OPERATORS];
   float wheel_amp[FMSYNTH_OPERATORS];
   float lfo_amp[FMSYNTH_OPERATORS];
};

struct fmsynth
{
   FMSYNTH_ALIGNED_CACHE_PRE struct fmsynth_voice_parameters params FMSYNTH_ALIGNED_CACHE_POST;
   FMSYNTH_ALIGNED_CACHE_PRE struct fmsynth_global_parameters global_params FMSYNTH_ALIGNED_CACHE_POST;

   float sample_rate;
   float inv_sample_rate;

   float bend;
   float wheel;
   bool sustained;

   unsigned max_voices;
   FMSYNTH_ALIGNED_CACHE_PRE struct fmsynth_voice voices[] FMSYNTH_ALIGNED_CACHE_POST;
};

static void *fmsynth_memory_alloc(size_t alignment, size_t size)
{
   void **place;
   uintptr_t addr = 0;
   void *ptr = malloc(alignment + size + sizeof(uintptr_t));

   if (!ptr)
   {
      return NULL;
   }

   addr = ((uintptr_t)ptr + sizeof(uintptr_t) + alignment)
      & ~(alignment - 1);
   place = (void**)addr;
   place[-1] = ptr;

   return (void*)addr;
}

static void fmsynth_memory_free(void *ptr)
{
   void **p = (void**)ptr;
   free(p[-1]);
}

static void fmsynth_init_voices(fmsynth_t *fm)
{
   memset(fm->voices, 0, fm->max_voices * sizeof(*fm->voices));

   for (unsigned v = 0; v < fm->max_voices; v++)
   {
      for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
      {
         fm->voices[v].amp[i] = 1.0f;
         fm->voices[v].pan_amp[0][i] = 1.0f;
         fm->voices[v].pan_amp[1][i] = 1.0f;
         fm->voices[v].wheel_amp[i] = 1.0f;
         fm->voices[v].lfo_amp[i] = 1.0f;
         fm->voices[v].lfo_freq_mod[i] = 1.0f;
      }
   }
   fm->bend = 1.0f;
}

static void fmsynth_set_default_parameters(
      struct fmsynth_voice_parameters *params)
{
#undef set_default
#define set_default(x, v) do { \
   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++) \
   { \
      params->x[i] = v; \
   } \
} while(0)

   set_default(amp, 1.0f);
   set_default(pan, 0.0f);

   set_default(freq_mod, 1.0f);
   set_default(freq_offset, 0.0f);

   set_default(envelope_target[0], 1.0f);
   set_default(envelope_target[1], 0.5f);
   set_default(envelope_target[2], 0.25f);
   set_default(envelope_delay[0], 0.05f);
   set_default(envelope_delay[1], 0.05f);
   set_default(envelope_delay[2], 0.25f);
   set_default(envelope_release_time, 0.50f);

   set_default(keyboard_scaling_mid_point, 440.0f);
   set_default(keyboard_scaling_low_factor, 0.0f);
   set_default(keyboard_scaling_high_factor, 0.0f);
   
   set_default(velocity_sensitivity, 1.0f);
   set_default(mod_sensitivity, 0.0f);

   set_default(lfo_amp_depth, 0.0f);
   set_default(lfo_freq_mod_depth, 0.0f);

   set_default(enable, 1.0f);

   params->carriers[0] = 1.0f;
   for (unsigned c = 1; c < FMSYNTH_OPERATORS; c++)
   {
      params->carriers[c] = 0.0f;
   }

   for (unsigned x = 0; x < FMSYNTH_OPERATORS; x++)
   {
      set_default(mod_to_carriers[x], 0.0f);
   }

#undef set_default
}

static void fmsynth_set_default_global_parameters(
      struct fmsynth_global_parameters *params)
{
   params->volume = 0.2f;
   params->lfo_freq = 0.1f;
}

void fmsynth_reset(fmsynth_t *fm)
{
   fmsynth_init_voices(fm);
   fmsynth_set_default_parameters(&fm->params);
   fmsynth_set_default_global_parameters(&fm->global_params);
}

fmsynth_t *fmsynth_new(float sample_rate, unsigned max_voices)
{
   size_t fmsynth_size = sizeof(fmsynth_t) +
      max_voices * sizeof(struct fmsynth_voice);

   fmsynth_t *fm = fmsynth_memory_alloc(64, fmsynth_size);
   if (fm == NULL)
   {
      return NULL;
   }

   memset(fm, 0, fmsynth_size);
   fm->max_voices = max_voices;

   fm->sample_rate = sample_rate;
   fm->inv_sample_rate = 1.0f / sample_rate;

   fmsynth_reset(fm);
   return fm;
}

void fmsynth_free(fmsynth_t *fm)
{
   fmsynth_memory_free(fm);
}

static float pitch_bend_to_ratio(uint16_t bend)
{
   // Two semitones range.
   return powf(2.0f, (bend - 8192.0f) / (8192.0f * 6.0f));
}

static float note_to_frequency(uint8_t note)
{
   return 440.0f * powf(2.0f, (note - 69.0f) / 12.0f);
}

static void fmsynth_update_target_envelope(struct fmsynth_voice *voice)
{
   voice->pos += voice->speed * FMSYNTH_FRAMES_PER_LFO;

   if (voice->state == FMSYNTH_VOICE_RELEASED)
   {
      for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
      {
         voice->target_env[i] *= voice->falloff[i];
         if (voice->pos >= voice->end_time[i])
         {
            voice->dead |= 1 << i;
         }
      }
   }
   else
   {
      for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
      {
         if (voice->pos >= voice->time[3][i])
         {
            voice->target_env[i] = voice->target[3][i];
         }
         else if (voice->pos >= voice->time[2][i])
         {
            voice->target_env[i] = voice->target[2][i] +
               (voice->pos - voice->time[2][i]) * voice->lerp[2][i];
         }
         else if (voice->pos >= voice->time[1][i])
         {
            voice->target_env[i] = voice->target[1][i] +
               (voice->pos - voice->time[1][i]) * voice->lerp[1][i];
         }
         else
         {
            voice->target_env[i] = voice->target[0][i] +
               (voice->pos - voice->time[0][i]) * voice->lerp[0][i];
         }
      }
   }

   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->target_env_step[i] =
         (voice->target_env[i] - voice->env[i]) * (1.0f / FMSYNTH_FRAMES_PER_LFO);
   }
}

static void fmsynth_reset_envelope(fmsynth_t *fm, struct fmsynth_voice *voice)
{
   voice->pos = 0.0f;
   voice->count = 0;
   voice->speed = fm->inv_sample_rate;
   voice->dead = 0;

   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->env[i] = voice->target[0][i] = 0.0f;
      voice->time[0][i] = 0.0f;

      for (unsigned j = 1; j <= 3; j++)
      {
         voice->target[j][i] = fm->params.envelope_target[j - 1][i];
         voice->time[j][i] = fm->params.envelope_delay[j - 1][i] +
            voice->time[j - 1][i];
      }

      for (unsigned j = 0; j < 3; j++)
      {
         voice->lerp[j][i] = (voice->target[j + 1][i] - voice->target[j][i]) /
            (voice->time[j + 1][i] - voice->time[j][i]);
      }

      voice->release_time[i] = fm->params.envelope_release_time[i];
      voice->falloff[i] = expf(logf(0.001f) * FMSYNTH_FRAMES_PER_LFO *
            fm->inv_sample_rate / voice->release_time[i]);
   }

   fmsynth_update_target_envelope(voice);
}

static void fmsynth_reset_voice(fmsynth_t *fm, struct fmsynth_voice *voice,
      float volume, float velocity, float freq)
{
   voice->enable = 0;

   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->phases[i] = 0.25f;

      float mod_amp = 1.0f - fm->params.velocity_sensitivity[i];
      mod_amp += fm->params.velocity_sensitivity[i] * velocity;

      float ratio = freq / fm->params.keyboard_scaling_mid_point[i];
      float factor = ratio > 1.0f ?
         fm->params.keyboard_scaling_high_factor[i] :
         fm->params.keyboard_scaling_low_factor[i];

      mod_amp *= powf(ratio, factor);

      bool enable = fm->params.enable[i] > 0.5f;
      voice->enable |= enable << i;

      if (enable)
      {
         voice->amp[i] = mod_amp * fm->params.amp[i];
      }
      else
      {
         voice->amp[i] = 0.0f;
      }

      voice->wheel_amp[i] = 1.0f - fm->params.mod_sensitivity[i] +
         fm->params.mod_sensitivity[i] * fm->wheel;
      voice->pan_amp[0][i] = volume * min(1.0f - fm->params.pan[i], 1.0f) *
         fm->params.carriers[i];
      voice->pan_amp[1][i] = volume * min(1.0f + fm->params.pan[i], 1.0f) *
         fm->params.carriers[i];

      voice->lfo_amp[i] = 1.0f;
      voice->lfo_freq_mod[i] = 1.0f;
   }

   voice->state = FMSYNTH_VOICE_RUNNING;
   fmsynth_reset_envelope(fm, voice);
}

static void fmsynth_voice_update_read_mod(struct fmsynth_voice *voice)
{
   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->read_mod[i] =
         voice->wheel_amp[i] * voice->lfo_amp[i] * voice->amp[i];
   }
}

static void fmsynth_trigger_voice(fmsynth_t *fm, struct fmsynth_voice *voice,
      uint8_t note, uint8_t velocity)
{
   voice->note = note;
   voice->base_freq = note_to_frequency(note);

   float freq = fm->bend * voice->base_freq;
   float mod_vel = velocity * (1.0f / 127.0f);

   for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
   {
      voice->step_rate[o] =
         (freq * fm->params.freq_mod[o] + fm->params.freq_offset[o]) *
         fm->inv_sample_rate;
   }

   fmsynth_reset_voice(fm, voice,
         fm->global_params.volume, mod_vel, voice->base_freq);
   fmsynth_voice_update_read_mod(voice);

   voice->lfo_phase = 0.25f;
   voice->lfo_step = FMSYNTH_FRAMES_PER_LFO * fm->global_params.lfo_freq * fm->inv_sample_rate;
   voice->count = 0;
}

fmsynth_status_t fmsynth_note_on(fmsynth_t *fm, uint8_t note, uint8_t velocity)
{
   struct fmsynth_voice *voice = NULL;
   for (unsigned i = 0; i < fm->max_voices; i++)
   {
      if (fm->voices[i].state == FMSYNTH_VOICE_INACTIVE)
      {
         voice = &fm->voices[i];
         break;
      }
   }

   if (voice)
   {
      fmsynth_trigger_voice(fm, voice, note, velocity);
      return FMSYNTH_STATUS_OK;
   }
   else
   {
      return FMSYNTH_STATUS_BUSY;
   }
}

static void fmsynth_release_voice(struct fmsynth_voice *voice)
{
   voice->state = FMSYNTH_VOICE_RELEASED;
   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->end_time[i] = voice->pos + voice->release_time[i];
   }
}

void fmsynth_note_off(fmsynth_t *fm, uint8_t note)
{
   for (unsigned i = 0; i < fm->max_voices; i++)
   {
      if (fm->voices[i].note == note &&
            fm->voices[i].state == FMSYNTH_VOICE_RUNNING)
      {
         if (fm->sustained)
         {
            fm->voices[i].state = FMSYNTH_VOICE_SUSTAINED;
         }
         else
         {
            fmsynth_release_voice(&fm->voices[i]);
         }
      }
   }
}

void fmsynth_set_sustain(fmsynth_t *fm, bool enable)
{
   bool releasing = fm->sustained && !enable;
   fm->sustained = enable;

   if (releasing)
   {
      for (unsigned i = 0; i < fm->max_voices; i++)
      {
         if (fm->voices[i].state == FMSYNTH_VOICE_SUSTAINED)
         {
            fmsynth_release_voice(&fm->voices[i]);
         }
      }
   }
}

void fmsynth_set_mod_wheel(fmsynth_t *fm, uint8_t wheel)
{
   float value = wheel * (1.0f / 127.0f);
   fm->wheel = value;

   for (unsigned v = 0; v < fm->max_voices; v++)
   {
      struct fmsynth_voice *voice = &fm->voices[v];

      if (voice->state != FMSYNTH_VOICE_INACTIVE)
      {
         for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
         {
            voice->wheel_amp[o] = 1.0f - fm->params.mod_sensitivity[o] +
               fm->params.mod_sensitivity[o] * value;
         }

         fmsynth_voice_update_read_mod(voice);
      }
   }
}

void fmsynth_set_pitch_bend(fmsynth_t *fm, uint16_t value)
{
   float bend = pitch_bend_to_ratio(value);
   fm->bend = bend;

   for (unsigned v = 0; v < fm->max_voices; v++)
   {
      struct fmsynth_voice *voice = &fm->voices[v];

      if (voice->state != FMSYNTH_VOICE_INACTIVE)
      {
         for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
         {
            float freq = bend * voice->base_freq;
            voice->step_rate[o] =
               (freq * fm->params.freq_mod[o] + fm->params.freq_offset[o]) *
               fm->inv_sample_rate;
         }
      }
   }
}

static void fmsynth_voice_set_lfo_value(struct fmsynth_voice *voice,
      const struct fmsynth_voice_parameters *params, float value)
{
   for (unsigned i = 0; i < FMSYNTH_OPERATORS; i++)
   {
      voice->lfo_amp[i] = 1.0f + params->lfo_amp_depth[i] * value;
      voice->lfo_freq_mod[i] = 1.0f + params->lfo_freq_mod_depth[i] * value;
   }

   fmsynth_voice_update_read_mod(voice);
}

void fmsynth_release_all(fmsynth_t *fm)
{
   for (unsigned i = 0; i < fm->max_voices; i++)
   {
      fmsynth_release_voice(&fm->voices[i]);
   }
   fm->sustained = false;
}

fmsynth_status_t fmsynth_parse_midi(fmsynth_t *fm,
      const uint8_t *data)
{
   if ((data[0] & 0xf0) == 0x90)
   {
      if (data[2] != 0)
      {
         return fmsynth_note_on(fm, data[1], data[2]);
      }
      else
      {
         fmsynth_note_off(fm, data[1]);
         return FMSYNTH_STATUS_OK;
      }
   }
   else if ((data[0] & 0xf0) == 0x80)
   {
      fmsynth_note_off(fm, data[1]);
      return FMSYNTH_STATUS_OK;
   }
   else if ((data[0] & 0xf0) == 0xb0 && data[1] == 64)
   {
      fmsynth_set_sustain(fm, data[2] >= 64);
      return FMSYNTH_STATUS_OK;
   }
   else if ((data[0] & 0xf0) == 0xb0 && data[1] == 1)
   {
      fmsynth_set_mod_wheel(fm, data[2]);
      return FMSYNTH_STATUS_OK;
   }
   else if ((data[0] == 0xff) ||
         (((data[0] & 0xf0) == 0xb0) && data[1] == 120))
   {
      // Reset, All Sound Off
      fmsynth_release_all(fm);
      return FMSYNTH_STATUS_OK;
   }
   else if ((((data[0] & 0xf0) == 0xb0) && data[1] == 123) ||
         data[0] == 0xfc)
   {
      // All Notes Off, STOP
      fmsynth_release_all(fm);
      return FMSYNTH_STATUS_OK;
   }
   else if ((data[0] & 0xf0) == 0xe0)
   {
      // Pitch bend
      uint16_t bend = data[1] | (data[2] << 7);
      fmsynth_set_pitch_bend(fm, bend);
      return FMSYNTH_STATUS_OK;
   }
   else if (data[0] == 0xf8)
   {
      // Timing message, just ignore.
      return FMSYNTH_STATUS_OK;
   }
   else
   {
      return FMSYNTH_STATUS_MESSAGE_UNKNOWN;
   }
}

struct fmsynth_parameter_data
{
    const char *name;
    float minimum;
    float maximum;
    float default_value;
    bool logarithmic;
};

static const struct fmsynth_parameter_data global_parameter_data[] = {
    { "Amp", 0.0f, 1.0f, 0.2f, false },
    { "LFO Freq", 0.1f, 64.0f, 0.1f, true },
};

static const struct fmsynth_parameter_data parameter_data[] = {
    { "Volume", 0.005f, 16.0f, 1.0f, true },
    { "Pan", -1.0f, 1.0f, 0.0f, false },
    { "FreqMod", 0.0f, 16.0f, 1.0f, false },
    { "FreqOffset", -128.0f, 128.0f, 0.0f, false },
    { "Env T0", 0.0f, 1.0f, 1.0f, false },
    { "Env T1", 0.0f, 1.0f, 0.5f, false },
    { "Env T2", 0.0f, 1.0f, 0.25f, false },
    { "Env D0", 0.005f, 8.0f, 0.05f, true },
    { "Env D1", 0.005f, 8.0f, 0.05f, true },
    { "Env D2", 0.005f, 8.0f, 0.25f, true },
    { "Env Rel", 0.005f, 8.0f, 0.5f, true },
    { "KeyScale Mid", 50.0f, 5000.0f, 440.0f, true },
    { "KeyScale LoFactor", -2.0f, 2.0f, 0.0f, false },
    { "KeyScale HiFactor", -2.0f, 2.0f, 0.0f, false },
    { "Velocity Sensitivity", 0.0f, 1.0f, 1.0f, false },
    { "ModWheel Sensitivity", 0.0f, 1.0f, 0.0f, false },
    { "LFOAmpDepth", 0.0f, 1.0f, 0.0f, false },
    { "LFOFreqDepth", 0.0f, 0.025f, 0.0f, false },
    { "Enable", 0.0f, 1.0f, 1.0f, false },
    { "Carrier", 0.0f, 1.0f, 1.0f, false },
    { "Mod0ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod1ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod2ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod3ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod4ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod5ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod6ToOperator", 0.0f, 1.0f, 0.0f, false },
    { "Mod7ToOperator", 0.0f, 1.0f, 0.0f, false },
};

void fmsynth_set_parameter(fmsynth_t *fm,
      unsigned parameter, unsigned operator_index, float value)
{
   if (parameter < FMSYNTH_PARAM_END && operator_index < FMSYNTH_OPERATORS)
   {
      float *param = fm->params.amp;
      param[parameter * FMSYNTH_OPERATORS + operator_index] = value;
   }
}

static float convert_from_normalized(const struct fmsynth_parameter_data *data, float value)
{
   if (data->logarithmic)
   {
      float minlog = log2f(data->minimum);
      float maxlog = log2f(data->maximum);
      return exp2f(minlog * (1.0f - value) + maxlog * value);
   }
   else
      return data->minimum * (1.0f - value) + data->maximum * value;
}

static float convert_to_normalized(const struct fmsynth_parameter_data *data, float value)
{
   if (data->logarithmic)
   {
      float minlog = log2f(data->minimum);
      float maxlog = log2f(data->maximum);
      float l = log2f(value);
      return (l - minlog) / (maxlog - minlog);
   }
   else
      return (value - data->minimum) / (data->maximum - data->minimum);
}

float fmsynth_convert_to_normalized_global_parameter(fmsynth_t *fm,
                                                     unsigned parameter, float value)
{
   (void)fm;
   if (parameter < FMSYNTH_GLOBAL_PARAM_END)
   {
      const struct fmsynth_parameter_data *data = &global_parameter_data[parameter];
      return convert_to_normalized(data, value);
   }
   else
      return 0.0f;
}

float fmsynth_convert_from_normalized_global_parameter(fmsynth_t *fm,
                                                       unsigned parameter, float value)
{
   (void)fm;
   if (parameter < FMSYNTH_GLOBAL_PARAM_END)
   {
      const struct fmsynth_parameter_data *data = &global_parameter_data[parameter];
      return convert_from_normalized(data, value);
   }
   else
      return 0.0f;
}

float fmsynth_convert_to_normalized_parameter(fmsynth_t *fm,
                                              unsigned parameter, float value)
{
   (void)fm;
   if (parameter < FMSYNTH_PARAM_END)
   {
      const struct fmsynth_parameter_data *data = &parameter_data[parameter];
      return convert_to_normalized(data, value);
   }
   else
      return 0.0f;
}

float fmsynth_convert_from_normalized_parameter(fmsynth_t *fm,
                                                unsigned parameter, float value)
{
   (void)fm;
   if (parameter < FMSYNTH_PARAM_END)
   {
      const struct fmsynth_parameter_data *data = &parameter_data[parameter];
      return convert_from_normalized(data, value);
   }
   else
      return 0.0f;
}

float fmsynth_get_parameter(fmsynth_t *fm,
                            unsigned parameter, unsigned operator_index)
{
   if (parameter < FMSYNTH_PARAM_END && operator_index < FMSYNTH_OPERATORS)
   {
      float *param = fm->params.amp;
      return param[parameter * FMSYNTH_OPERATORS + operator_index];
   }
   else
      return 0.0f;
}

void fmsynth_set_global_parameter(fmsynth_t *fm,
      unsigned parameter, float value)
{
   if (parameter < FMSYNTH_GLOBAL_PARAM_END)
   {
      float *param = &fm->global_params.volume;
      param[parameter] = value;
   }
}

float fmsynth_get_global_parameter(fmsynth_t *fm,
                                   unsigned parameter)
{
   if (parameter < FMSYNTH_GLOBAL_PARAM_END)
   {
      float *param = &fm->global_params.volume;
      return param[parameter];
   }
   else
      return 0.0f;
}

static bool fmsynth_voice_update_active(struct fmsynth_voice *voice)
{
   if (voice->enable & (~voice->dead))
   {
      return true;
   }
   else
   {
      voice->state = FMSYNTH_VOICE_INACTIVE;
      return false;
   }
}

static float fmsynth_oscillator(float phase)
{
   float x = phase < 0.5f ? (phase - 0.25f) : (0.75f - phase);

   float x2 = x * x;
   float x3 = x2 * x;
   x *= 2.0f * PI;
   x -= x3 * INV_FACTORIAL_3_2PIPOW3;

   float x5 = x3 * x2;
   x += x5 * INV_FACTORIAL_5_2PIPOW5;

   float x7 = x5 * x2;
   x -= x7 * INV_FACTORIAL_7_2PIPOW7;

   return x;
}

#if defined(__AVX__) && defined(FMSYNTH_SIMD)
#include "x86/fmsynth_avx.c"
#elif defined(__SSE__) && defined(FMSYNTH_SIMD)
#include "x86/fmsynth_sse.c"
#elif defined(__ARM_NEON__) && defined(FMSYNTH_SIMD)
#include "arm/fmsynth_arm.c"
#else
static void fmsynth_process_frames(fmsynth_t *fm,
      struct fmsynth_voice *voice, float *left, float *right, unsigned frames)
{
   float cached[FMSYNTH_OPERATORS];
   float cached_modulator[FMSYNTH_OPERATORS];
   float steps[FMSYNTH_OPERATORS];

   for (unsigned f = 0; f < frames; f++)
   {
      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         steps[o] = voice->lfo_freq_mod[o] * voice->step_rate[o];
      }

      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         float value = voice->env[o] * voice->read_mod[o] *
            fmsynth_oscillator(voice->phases[o]);

         cached[o] = value;
         cached_modulator[o] = value * voice->step_rate[o];
         voice->env[o] += voice->target_env_step[o];
      }

      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         float scalar = cached_modulator[o];
         const float *vec = fm->params.mod_to_carriers[o];
         for (unsigned j = 0; j < FMSYNTH_OPERATORS; j++)
            steps[j] += scalar * vec[j];
      }

      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         voice->phases[o] += steps[o];
         voice->phases[o] -= floorf(voice->phases[o]);
      }

      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         left[f]  += cached[o] * voice->pan_amp[0][o];
         right[f] += cached[o] * voice->pan_amp[1][o];
      }
   }
}
#endif

static void fmsynth_render_voice(fmsynth_t *fm, struct fmsynth_voice *voice,
      float *left, float *right, unsigned frames)
{
   while (frames)
   {
      unsigned to_render = min(FMSYNTH_FRAMES_PER_LFO - voice->count, frames);

      fmsynth_process_frames(fm, voice, left, right, to_render);

      left += to_render;
      right += to_render;
      frames -= to_render;
      voice->count += to_render;

      if (voice->count == FMSYNTH_FRAMES_PER_LFO)
      {
         float lfo_value = fmsynth_oscillator(voice->lfo_phase);
         voice->lfo_phase += voice->lfo_step;
         voice->lfo_phase -= floorf(voice->lfo_phase);
         voice->count = 0;

         fmsynth_voice_set_lfo_value(voice, &fm->params, lfo_value);
         fmsynth_update_target_envelope(voice);
      }
   }
}

unsigned fmsynth_render(fmsynth_t *fm, float *left, float *right,
      unsigned frames)
{
   unsigned active_voices = 0;
   for (unsigned i = 0; i < fm->max_voices; i++)
   {
      if (fm->voices[i].state != FMSYNTH_VOICE_INACTIVE)
      {
         fmsynth_render_voice(fm, &fm->voices[i], left, right, frames);
         if (fmsynth_voice_update_active(&fm->voices[i]))
         {
            active_voices++;
         }
      }
   }

   return active_voices;
}

size_t fmsynth_preset_size(void)
{
   return
      8 +
      sizeof(struct fmsynth_preset_metadata) +
      FMSYNTH_PARAM_END * FMSYNTH_OPERATORS * sizeof(uint32_t) +
      FMSYNTH_GLOBAL_PARAM_END * sizeof(uint32_t);
}

// We don't need full precision mantissa.
// Allows packing floating point in 32-bit in a portable way.
static uint32_t pack_float(float value)
{
   int exponent;
   float mantissa = frexpf(value, &exponent);

   int32_t fixed_mantissa = (int32_t)roundf(mantissa * 0x8000);
   int16_t fractional;
   if (fixed_mantissa > 0x7fff)
   {
      fractional = 0x7fff;
   }
   else if (fixed_mantissa < -0x8000)
   {
      fractional = -0x8000;
   }
   else
   {
      fractional = fixed_mantissa;
   }

   return (((uint32_t)exponent & 0xffff) << 16) | (uint16_t)fractional;
}

static float unpack_float(uint32_t value)
{
   if (value == 0)
   {
      return 0.0f;
   }

   int exp = (int16_t)(value >> 16);
   float fractional = (float)(int16_t)(value & 0xffff) / 0x8000;

   return ldexpf(fractional, exp);
}

static void write_u32(uint8_t *buffer, uint32_t value)
{
   buffer[0] = (uint8_t)(value >> 24);
   buffer[1] = (uint8_t)(value >> 16);
   buffer[2] = (uint8_t)(value >>  8);
   buffer[3] = (uint8_t)(value >>  0);
}

static uint32_t read_u32(const uint8_t *buffer)
{
   return (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | (buffer[3] << 0);
}

fmsynth_status_t fmsynth_preset_save(fmsynth_t *fm, const struct fmsynth_preset_metadata *metadata,
      void *buffer, size_t size)
{
   return fmsynth_preset_save_private(&fm->global_params, &fm->params,
         metadata, buffer, size);
}

fmsynth_status_t fmsynth_preset_save_private(struct fmsynth_global_parameters *global_params,
      struct fmsynth_voice_parameters *voice_params,
      const struct fmsynth_preset_metadata *metadata,
      void *buffer_, size_t size)
{
   uint8_t *buffer = buffer_;

   if (size < fmsynth_preset_size())
   {
      return FMSYNTH_STATUS_BUFFER_TOO_SMALL;
   }

   if (metadata)
   {
      if (metadata->name[FMSYNTH_PRESET_STRING_SIZE - 1] != '\0' ||
            metadata->author[FMSYNTH_PRESET_STRING_SIZE - 1] != '\0')
      {
         return FMSYNTH_STATUS_NO_NUL_TERMINATE;
      }
   }

   memcpy(buffer, "FMSYNTH1", 8);
   buffer += 8;

   if (metadata)
   {
      memcpy(buffer, metadata->name, sizeof(metadata->name));
      memcpy(buffer + sizeof(metadata->name), metadata->author, sizeof(metadata->author));
   }
   else
   {
      memset(buffer, 0, sizeof(metadata->name));
      memset(buffer + sizeof(metadata->name), 0, sizeof(metadata->author));
   }
   buffer += sizeof(metadata->name) + sizeof(metadata->author);

   const float *globals = &global_params->volume;
   for (unsigned i = 0; i < FMSYNTH_GLOBAL_PARAM_END; i++)
   {
      write_u32(buffer, pack_float(globals[i]));
      buffer += sizeof(uint32_t);
   }

   const float *params = voice_params->amp;
   for (unsigned i = 0; i < FMSYNTH_PARAM_END * FMSYNTH_OPERATORS; i++)
   {
      write_u32(buffer, pack_float(params[i]));
      buffer += sizeof(uint32_t);
   }

   return FMSYNTH_STATUS_OK;
}

fmsynth_status_t fmsynth_preset_load(fmsynth_t *fm, struct fmsynth_preset_metadata *metadata,
      const void *buffer, size_t size)
{
   return fmsynth_preset_load_private(&fm->global_params, &fm->params,
         metadata, buffer, size);
}

fmsynth_status_t fmsynth_preset_load_private(struct fmsynth_global_parameters *global_params,
      struct fmsynth_voice_parameters *voice_params,
      struct fmsynth_preset_metadata *metadata,
      const void *buffer_, size_t size)
{
   const uint8_t *buffer = buffer_;

   if (size < fmsynth_preset_size())
   {
      return FMSYNTH_STATUS_BUFFER_TOO_SMALL;
   }

   if (memcmp(buffer, "FMSYNTH1", 8) != 0)
   {
      return FMSYNTH_STATUS_INVALID_FORMAT;
   }
   buffer += 8;

   if (buffer[FMSYNTH_PRESET_STRING_SIZE - 1] != '\0')
   {
      return FMSYNTH_STATUS_NO_NUL_TERMINATE;
   }

   if (metadata)
   {
      memcpy(metadata->name, buffer, sizeof(metadata->name));
   }
   buffer += sizeof(metadata->name);

   if (buffer[FMSYNTH_PRESET_STRING_SIZE - 1] != '\0')
   {
      return FMSYNTH_STATUS_NO_NUL_TERMINATE;
   }

   if (metadata)
   {
      memcpy(metadata->author, buffer, sizeof(metadata->author));
   }
   buffer += sizeof(metadata->author);

   float *globals = &global_params->volume;
   for (unsigned i = 0; i < FMSYNTH_GLOBAL_PARAM_END; i++)
   {
      globals[i] = unpack_float(read_u32(buffer));
      buffer += sizeof(uint32_t);
   }

   float *params = voice_params->amp;
   for (unsigned i = 0; i < FMSYNTH_PARAM_END * FMSYNTH_OPERATORS; i++)
   {
      params[i] = unpack_float(read_u32(buffer));
      buffer += sizeof(uint32_t);
   }

   return FMSYNTH_STATUS_OK;
}

unsigned fmsynth_get_version(void)
{
   return FMSYNTH_VERSION;
}

