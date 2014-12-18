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

#define FMSYNTH_NEON_ASM 1

#if FMSYNTH_NEON_ASM
void fmsynth_process_frames_neon(const float *mod_to_carriers,
      const float *voice, float *left, float *right, unsigned frames);

static void fmsynth_process_frames(fmsynth_t *fm,
      struct fmsynth_voice *voice, float *oleft, float *oright,
      unsigned frames)
{
   fmsynth_process_frames_neon(fm->params.mod_to_carriers[0], voice->phases,
         oleft, oright, frames);
}
#else
#include <arm_neon.h>
static inline float32x4_t floor_neon(float32x4_t a)
{
#if __ARM_ARCH >= 8
   return vrndqm_f32(a);
#else
   const float32x4_t round32 = vdupq_n_f32(12582912.0f);
   const float32x4_t vhalf = vdupq_n_f32(0.5f);

   float32x4_t rounded = vsubq_f32(vaddq_f32(a, round32), round32);
   uint32x4_t mask = vceqq_f32(a, rounded);

   float32x4_t floored = vsubq_f32(vaddq_f32(vsubq_f32(a, vhalf), round32), round32);
   return vreinterpretq_f32_u32(vorrq_u32(vandq_u32(vreinterpretq_u32_f32(a), mask),
            vbicq_u32(vreinterpretq_u32_f32(floored), mask)));
#endif
}

static void fmsynth_process_frames(fmsynth_t * restrict fm_,
      struct fmsynth_voice * restrict voice_, float * restrict oleft_, float * restrict oright_, unsigned frames)
{
   fmsynth_t *fm = FMSYNTH_ASSUME_ALIGNED(fm_, 16);
   struct fmsynth_voice *voice = FMSYNTH_ASSUME_ALIGNED(voice_, 16);
   float *oleft = FMSYNTH_ASSUME_ALIGNED(oleft_, 16);
   float *oright = FMSYNTH_ASSUME_ALIGNED(oright_, 16);

   float32x4_t phases0 = vld1q_f32(voice->phases + 0);
   float32x4_t phases1 = vld1q_f32(voice->phases + 4);

   float32x4_t env0 = vld1q_f32(voice->env + 0);
   float32x4_t env1 = vld1q_f32(voice->env + 4);

   const float32x4_t step_rate0 = vld1q_f32(voice->step_rate + 0);
   const float32x4_t step_rate1 = vld1q_f32(voice->step_rate + 4);

   for (unsigned f = 0; f < frames; f++)
   {
      const float32x4_t c_sub = vdupq_n_f32(0.25f);
      const float32x4_t c_cmp = vdupq_n_f32(0.5f);
      const float32x4_t c_greater = vdupq_n_f32(0.75f);

      float32x4_t x0 = vsubq_f32(phases0, c_sub);
      float32x4_t x1 = vsubq_f32(phases1, c_sub);
      uint32x4_t cmp0 = vcltq_f32(phases0, c_cmp);
      uint32x4_t cmp1 = vcltq_f32(phases1, c_cmp);
      float32x4_t greater0 = vsubq_f32(c_greater, phases0);
      float32x4_t greater1 = vsubq_f32(c_greater, phases1);
      x0 = vreinterpretq_f32_u32(
            vorrq_u32(vandq_u32(cmp0, vreinterpretq_u32_f32(x0)),
               vbicq_u32(vreinterpretq_u32_f32(greater0), cmp0)));
      x1 = vreinterpretq_f32_u32(
            vorrq_u32(vandq_u32(cmp1, vreinterpretq_u32_f32(x1)),
               vbicq_u32(vreinterpretq_u32_f32(greater1), cmp1)));

      // Compute sine approximation.
      {
         const float32x4_t fact3 = vdupq_n_f32(INV_FACTORIAL_3_2PIPOW3);
         const float32x4_t fact5 = vdupq_n_f32(INV_FACTORIAL_5_2PIPOW5);
         const float32x4_t fact7 = vdupq_n_f32(INV_FACTORIAL_7_2PIPOW7);

         float32x4_t x20 = vmulq_f32(x0, x0);
         float32x4_t x21 = vmulq_f32(x1, x1);
         float32x4_t x30 = vmulq_f32(x0, x20);
         float32x4_t x31 = vmulq_f32(x1, x21);

         x0 = vmulq_n_f32(x0, 2.0f * PI);
         x1 = vmulq_n_f32(x1, 2.0f * PI);

         x0 = vmlsq_f32(x0, x30, fact3);
         x1 = vmlsq_f32(x1, x31, fact3);

         x30 = vmulq_f32(x30, x20);
         x31 = vmulq_f32(x31, x21);

         x0 = vmlaq_f32(x0, x30, fact5);
         x1 = vmlaq_f32(x1, x31, fact5);

         x30 = vmulq_f32(x30, x20);
         x31 = vmulq_f32(x31, x21);

         x0 = vmlsq_f32(x0, x30, fact7);
         x1 = vmlsq_f32(x1, x31, fact7);
      }

      x0 = vmulq_f32(x0, vmulq_f32(vld1q_f32(voice->read_mod + 0), env0));
      x1 = vmulq_f32(x1, vmulq_f32(vld1q_f32(voice->read_mod + 4), env1));

      env0 = vaddq_f32(env0, vld1q_f32(voice->target_env_step + 0));
      env1 = vaddq_f32(env1, vld1q_f32(voice->target_env_step + 4));

      float32x4_t xmod0 = vmulq_f32(x0, step_rate0);
      float32x4_t xmod1 = vmulq_f32(x1, step_rate1);

      float32x4_t steps0 = vmulq_f32(step_rate0, vld1q_f32(voice->lfo_freq_mod + 0));
      float32x4_t steps1 = vmulq_f32(step_rate1, vld1q_f32(voice->lfo_freq_mod + 4));
      const float *vec;

#define MAT_ACCUMULATE(steps0, steps1, i, scalar, index) \
      vec = fm->params.mod_to_carriers[i]; \
      steps0 = vmlaq_lane_f32(steps0, vld1q_f32(vec + 0), scalar, index); \
      steps1 = vmlaq_lane_f32(steps1, vld1q_f32(vec + 4), scalar, index)

      MAT_ACCUMULATE(steps0, steps1, 0,  vget_low_f32(xmod0), 0);
      MAT_ACCUMULATE(phases0, phases1, 1,  vget_low_f32(xmod0), 1);
      MAT_ACCUMULATE(steps0, steps1, 2, vget_high_f32(xmod0), 0);
      MAT_ACCUMULATE(phases0, phases1, 3, vget_high_f32(xmod0), 1);
      MAT_ACCUMULATE(steps0, steps1, 4,  vget_low_f32(xmod1), 0);
      MAT_ACCUMULATE(phases0, phases1, 5,  vget_low_f32(xmod1), 1);
      MAT_ACCUMULATE(steps0, steps1, 6, vget_high_f32(xmod1), 0);
      MAT_ACCUMULATE(phases0, phases1, 7, vget_high_f32(xmod1), 1);
#undef MAT_ACCUMULATE

      float32x4_t left  = vmulq_f32(x0, vld1q_f32(voice->pan_amp[0] + 0));
      float32x4_t right = vmulq_f32(x0, vld1q_f32(voice->pan_amp[1] + 0));

      phases0 = vaddq_f32(phases0, steps0);
      phases1 = vaddq_f32(phases1, steps1);

      left  = vmlaq_f32(left, x1, vld1q_f32(voice->pan_amp[0] + 4));
      right = vmlaq_f32(right, x1, vld1q_f32(voice->pan_amp[1] + 4));

      phases0 = vsubq_f32(phases0, floor_neon(phases0));
      phases1 = vsubq_f32(phases1, floor_neon(phases1));

      float32x2_t hleft = vadd_f32(vget_low_f32(left), vget_high_f32(left));
      float32x2_t hright = vadd_f32(vget_low_f32(right), vget_high_f32(right));
      float32x2_t out = vpadd_f32(hleft, hright);

      float32x2_t current = vld1_dup_f32(oleft + f);
      current = vld1_lane_f32(oright + f, current, 1);

      out = vadd_f32(out, current);
      vst1_lane_f32(oleft + f, out, 0);
      vst1_lane_f32(oright + f, out, 1);
   }

   vst1q_f32(voice->phases + 0, phases0);
   vst1q_f32(voice->phases + 4, phases1);
   vst1q_f32(voice->env + 0, env0);
   vst1q_f32(voice->env + 4, env1);
}
#endif

