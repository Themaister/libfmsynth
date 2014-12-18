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

#ifndef FMSYNTH_PRIVATE_H__
#define FMSYNTH_PRIVATE_H__

#include "fmsynth.h"

#ifdef __cplusplus
extern "C" {
#endif

struct fmsynth_voice_parameters
{
   float amp[FMSYNTH_OPERATORS];
   float pan[FMSYNTH_OPERATORS];
   float freq_mod[FMSYNTH_OPERATORS];
   float freq_offset[FMSYNTH_OPERATORS];

   float envelope_target[3][FMSYNTH_OPERATORS];
   float envelope_delay[3][FMSYNTH_OPERATORS];
   float envelope_release_time[FMSYNTH_OPERATORS];

   float keyboard_scaling_mid_point[FMSYNTH_OPERATORS];
   float keyboard_scaling_low_factor[FMSYNTH_OPERATORS];
   float keyboard_scaling_high_factor[FMSYNTH_OPERATORS];

   float velocity_sensitivity[FMSYNTH_OPERATORS];
   float mod_sensitivity[FMSYNTH_OPERATORS];

   float lfo_amp_depth[FMSYNTH_OPERATORS];
   float lfo_freq_mod_depth[FMSYNTH_OPERATORS];

   float enable[FMSYNTH_OPERATORS];

   float carriers[FMSYNTH_OPERATORS];
   float mod_to_carriers[FMSYNTH_OPERATORS][FMSYNTH_OPERATORS];
};

struct fmsynth_global_parameters
{
   float volume;
   float lfo_freq;
};

fmsynth_status_t fmsynth_preset_load_private(struct fmsynth_global_parameters *global_params,
      struct fmsynth_voice_parameters *params,
      struct fmsynth_preset_metadata *metadata,
      const void *buffer_, size_t size);

fmsynth_status_t fmsynth_preset_save_private(struct fmsynth_global_parameters *global_params,
      struct fmsynth_voice_parameters *params,
      const struct fmsynth_preset_metadata *metadata,
      void *buffer_, size_t size);

#ifdef __cplusplus
}
#endif

#endif

