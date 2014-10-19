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

#if defined(__SSE4_1__)
#include <smmintrin.h>
#else
#include <xmmintrin.h>
#endif

#ifndef __SSE4_1__
static __m128 floor_sse(__m128 v)
{
   const __m128 round32 = _mm_set1_ps(12582912.0f);
   __m128 rounded = _mm_sub_ps(_mm_add_ps(v, round32), round32);
   __m128 mask = _mm_cmpeq_ps(v, rounded);

   __m128 floored = _mm_sub_ps(_mm_add_ps(_mm_sub_ps(v, _mm_set1_ps(0.5f)), round32), round32);
   return _mm_or_ps(_mm_and_ps(v, mask), _mm_andnot_ps(mask, floored));
}
#endif

static void fmsynth_process_frames(fmsynth_t *fm,
      struct fmsynth_voice *voice, float *oleft, float *oright, unsigned frames)
{
   __m128 phases0 = _mm_load_ps(voice->phases + 0);
   __m128 phases1 = _mm_load_ps(voice->phases + 4);

   __m128 env0 = _mm_load_ps(voice->env + 0);
   __m128 env1 = _mm_load_ps(voice->env + 4);

   for (unsigned f = 0; f < frames; f++)
   {
      __m128 x0 = _mm_sub_ps(phases0, _mm_set1_ps(0.25f));
      __m128 x1 = _mm_sub_ps(phases1, _mm_set1_ps(0.25f));
      __m128 cmp0 = _mm_cmplt_ps(phases0, _mm_set1_ps(0.5f));
      __m128 cmp1 = _mm_cmplt_ps(phases1, _mm_set1_ps(0.5f));
      __m128 greater0 = _mm_sub_ps(_mm_set1_ps(0.75f), phases0);
      __m128 greater1 = _mm_sub_ps(_mm_set1_ps(0.75f), phases1);
      x0 = _mm_or_ps(_mm_and_ps(cmp0, x0), _mm_andnot_ps(cmp0, greater0));
      x1 = _mm_or_ps(_mm_and_ps(cmp1, x1), _mm_andnot_ps(cmp1, greater1));

      // Compute sine approximation.
      {
         __m128 x20 = _mm_mul_ps(x0, x0);
         __m128 x21 = _mm_mul_ps(x1, x1);
         __m128 x30 = _mm_mul_ps(x0, x20);
         __m128 x31 = _mm_mul_ps(x1, x21);

         x0 = _mm_mul_ps(x0, _mm_set1_ps(2.0f * PI));
         x1 = _mm_mul_ps(x1, _mm_set1_ps(2.0f * PI));

         x0 = _mm_sub_ps(x0, _mm_mul_ps(x30, _mm_set1_ps(INV_FACTORIAL_3_2PIPOW3)));
         x1 = _mm_sub_ps(x1, _mm_mul_ps(x31, _mm_set1_ps(INV_FACTORIAL_3_2PIPOW3)));

         x30 = _mm_mul_ps(x30, x20);
         x31 = _mm_mul_ps(x31, x21);
         x0 = _mm_add_ps(x0, _mm_mul_ps(x30, _mm_set1_ps(INV_FACTORIAL_5_2PIPOW5)));
         x1 = _mm_add_ps(x1, _mm_mul_ps(x31, _mm_set1_ps(INV_FACTORIAL_5_2PIPOW5)));

         x30 = _mm_mul_ps(x30, x20);
         x31 = _mm_mul_ps(x31, x21);
         x0 = _mm_sub_ps(x0, _mm_mul_ps(x30, _mm_set1_ps(INV_FACTORIAL_7_2PIPOW7)));
         x1 = _mm_sub_ps(x1, _mm_mul_ps(x31, _mm_set1_ps(INV_FACTORIAL_7_2PIPOW7)));
      }

      x0 = _mm_mul_ps(x0, _mm_mul_ps(env0, _mm_load_ps(voice->read_mod + 0)));
      x1 = _mm_mul_ps(x1, _mm_mul_ps(env1, _mm_load_ps(voice->read_mod + 4)));

      env0 = _mm_add_ps(env0, _mm_load_ps(voice->target_env_step + 0));
      env1 = _mm_add_ps(env1, _mm_load_ps(voice->target_env_step + 4));

      __m128 step_rate0 = _mm_load_ps(voice->step_rate + 0);
      __m128 step_rate1 = _mm_load_ps(voice->step_rate + 4);

      __m128 xmod0 = _mm_mul_ps(x0, step_rate0);
      __m128 xmod1 = _mm_mul_ps(x1, step_rate1);

      __m128 steps0 = _mm_mul_ps(step_rate0, _mm_load_ps(voice->lfo_freq_mod + 0));
      __m128 steps1 = _mm_mul_ps(step_rate1, _mm_load_ps(voice->lfo_freq_mod + 4));
      const float *vec;

#define MAT_ACCUMULATE(steps0, steps1, i, scalar, index) \
      vec = fm->params.mod_to_carriers[i]; \
      steps0 = _mm_add_ps(steps0, _mm_mul_ps(_mm_load_ps(vec + 0), \
               _mm_shuffle_ps(scalar, scalar, \
                  _MM_SHUFFLE(index, index, index, index)))); \
      steps1 = _mm_add_ps(steps1, _mm_mul_ps(_mm_load_ps(vec + 4), \
               _mm_shuffle_ps(scalar, scalar, \
                  _MM_SHUFFLE(index, index, index, index))))

      MAT_ACCUMULATE(steps0, steps1,   0, xmod0, 0);
      MAT_ACCUMULATE(phases0, phases1, 1, xmod0, 1);
      MAT_ACCUMULATE(steps0, steps1,   2, xmod0, 2);
      MAT_ACCUMULATE(phases0, phases1, 3, xmod0, 3);
      MAT_ACCUMULATE(steps0, steps1,   4, xmod1, 0);
      MAT_ACCUMULATE(phases0, phases1, 5, xmod1, 1);
      MAT_ACCUMULATE(steps0, steps1,   6, xmod1, 2);
      MAT_ACCUMULATE(phases0, phases1, 7, xmod1, 3);
#undef MAT_ACCUMULATE

      __m128 left  = _mm_mul_ps(x0, _mm_load_ps(voice->pan_amp[0] + 0));
      __m128 right = _mm_mul_ps(x0, _mm_load_ps(voice->pan_amp[1] + 0));

      phases0 = _mm_add_ps(phases0, steps0);
      phases1 = _mm_add_ps(phases1, steps1);
#ifdef __SSE4_1__
      phases0 = _mm_sub_ps(phases0, _mm_floor_ps(phases0));
      phases1 = _mm_sub_ps(phases1, _mm_floor_ps(phases1));
#else
      phases0 = _mm_sub_ps(phases0, floor_sse(phases0));
      phases1 = _mm_sub_ps(phases1, floor_sse(phases1));
#endif

      left  = _mm_add_ps(left, _mm_mul_ps(x1, _mm_load_ps(voice->pan_amp[0] + 4)));
      right = _mm_add_ps(right, _mm_mul_ps(x1, _mm_load_ps(voice->pan_amp[1] + 4)));

      __m128 out = _mm_add_ps(_mm_shuffle_ps(left, right,
               _MM_SHUFFLE(1, 0, 1, 0)),
            _mm_shuffle_ps(left, right, _MM_SHUFFLE(3, 2, 3, 2)));
      out = _mm_add_ps(_mm_shuffle_ps(out, out, _MM_SHUFFLE(3, 3, 1, 1)), out);
      _mm_store_ss(oleft + f, _mm_add_ss(out, _mm_load_ss(oleft + f)));
      _mm_store_ss(oright + f, _mm_add_ss(_mm_movehl_ps(out, out), _mm_load_ss(oright + f)));
   }

   _mm_store_ps(voice->phases + 0, phases0);
   _mm_store_ps(voice->phases + 4, phases1);
   _mm_store_ps(voice->env + 0, env0);
   _mm_store_ps(voice->env + 4, env1);
}

