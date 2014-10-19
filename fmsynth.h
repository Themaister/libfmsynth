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

#ifndef FMSYNTH_H__
#define FMSYNTH_H__

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

#ifdef __cplusplus
extern "C" {
#else
#include <stdbool.h>
#endif

#define FMSYNTH_OPERATORS 8

typedef struct fmsynth fmsynth_t;

enum fmsynth_parameter
{
   FMSYNTH_PARAM_AMP = 0,
   FMSYNTH_PARAM_PAN,

   FMSYNTH_PARAM_FREQ_MOD,
   FMSYNTH_PARAM_FREQ_OFFSET,

   FMSYNTH_PARAM_ENVELOPE_TARGET0,
   FMSYNTH_PARAM_ENVELOPE_TARGET1,
   FMSYNTH_PARAM_ENVELOPE_TARGET2,
   FMSYNTH_PARAM_DELAY0,
   FMSYNTH_PARAM_DELAY1,
   FMSYNTH_PARAM_DELAY2,
   FMSYNTH_PARAM_RELEASE_TIME,

   FMSYNTH_PARAM_KEYBOARD_SCALING_MID_POINT,
   FMSYNTH_PARAM_KEYBOARD_SCALING_LOW_FACTOR,
   FMSYNTH_PARAM_KEYBOARD_SCALING_HIGH_FACTOR,

   FMSYNTH_PARAM_VELOCITY_SENSITIVITY,
   FMSYNTH_PARAM_MOD_WHEEL_SENSITIVITY,

   FMSYNTH_PARAM_LFO_AMP_SENSITIVITY,
   FMSYNTH_PARAM_LFO_FREQ_MOD_DEPTH,

   FMSYNTH_PARAM_ENABLE,

   FMSYNTH_PARAM_CARRIERS,
   FMSYNTH_PARAM_MOD_TO_CARRIERS0,

   FMSYNTH_PARAM_END = FMSYNTH_PARAM_MOD_TO_CARRIERS0 + FMSYNTH_OPERATORS,
   FMSYNTH_PARAM_ENSURE_INT = INT_MAX
};

enum fmsynth_global_parameter
{
   FMSYNTH_GLOBAL_PARAM_VOLUME = 0,
   FMSYNTH_GLOBAL_PARAM_LFO_FREQ,

   FMSYNTH_GLOBAL_PARAM_END,
   FMSYNTH_GLOBAL_PARAM_ENSURE_INT = INT_MAX
};

typedef enum fmsynth_status
{
   FMSYNTH_STATUS_OK = 0,
   FMSYNTH_STATUS_BUSY,

   FMSYNTH_STATUS_BUFFER_TOO_SMALL,
   FMSYNTH_STATUS_NO_NUL_TERMINATE,
   FMSYNTH_STATUS_INVALID_FORMAT,

   FMSYNTH_STATUS_ENSURE_INT = INT_MAX
} fmsynth_status_t;

// Allocation, destruction
fmsynth_t *fmsynth_new(float sample_rate, unsigned max_voices);
void fmsynth_reset(fmsynth_t *fm);
void fmsynth_free(fmsynth_t *fm);

// Manual parameter control
void fmsynth_set_parameter(fmsynth_t *fm,
      unsigned parameter, unsigned operator_index, float value);

void fmsynth_set_global_parameter(fmsynth_t *fm,
      unsigned parameter, float value);

// Rendering function
unsigned fmsynth_render(fmsynth_t *fm, float *left, float *right, unsigned frames);

// Instrument control
fmsynth_status_t fmsynth_note_on(fmsynth_t *fm, uint8_t note, uint8_t velocity);
void fmsynth_note_off(fmsynth_t *fm, uint8_t note);
void fmsynth_set_sustain(fmsynth_t *fm, bool enable);
void fmsynth_set_mod_wheel(fmsynth_t *fm, uint8_t wheel);
void fmsynth_set_pitch_bend(fmsynth_t *fm, uint16_t value);
void fmsynth_release_all(fmsynth_t *fm);

// Instrument control via MIDI messages.
void fmsynth_parse_midi(fmsynth_t *fm,
      const uint8_t *midi_data, size_t midi_size);

// Preset loading and saving.
#define FMSYNTH_PRESET_STRING_SIZE 64
struct fmsynth_preset_metadata
{
   // UTF-8 is assumed. API does not validate that however.
   // Strings must be properly NUL-terminated.
   char name[FMSYNTH_PRESET_STRING_SIZE];
   char author[FMSYNTH_PRESET_STRING_SIZE];
};

size_t fmsynth_preset_size(void);
fmsynth_status_t fmsynth_preset_save(fmsynth_t *fm, const struct fmsynth_preset_metadata *metadata,
      void *buffer, size_t size);

// Allocates fields in struct fmsynth_preset_metadata if successful.
// Must be freed with fmsynth_preset_metadata_free().
fmsynth_status_t fmsynth_preset_load(fmsynth_t *fm, struct fmsynth_preset_metadata *metadata,
      const void *buffer, size_t size);

void fmsynth_preset_metadata_free(struct fmsynth_preset_metadata *metadata);

#ifdef __cplusplus
}
#endif

#endif

