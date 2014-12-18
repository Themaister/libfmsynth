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

#include <immintrin.h>

static void fmsynth_process_frames(fmsynth_t *fm,
      struct fmsynth_voice *voice, float *oleft, float *oright, unsigned frames)
{
   __m256 phases = _mm256_load_ps(voice->phases);
   __m256 env = _mm256_load_ps(voice->env);

   for (unsigned f = 0; f < frames; f++)
   {
      __m256 x = _mm256_sub_ps(phases, _mm256_set1_ps(0.25f));
      __m256 cmp = _mm256_cmp_ps(phases, _mm256_set1_ps(0.5f), _CMP_LT_OS);
      __m256 greater = _mm256_sub_ps(_mm256_set1_ps(0.75f), phases);
      x = _mm256_or_ps(_mm256_and_ps(cmp, x), _mm256_andnot_ps(cmp, greater));

      // Compute sine approximation.
      {
         __m256 x2 = _mm256_mul_ps(x, x);
         __m256 x3 = _mm256_mul_ps(x, x2);
         x = _mm256_mul_ps(x, _mm256_set1_ps(2.0f * PI));

         x = _mm256_sub_ps(x, _mm256_mul_ps(x3, _mm256_set1_ps(INV_FACTORIAL_3_2PIPOW3)));

         x3 = _mm256_mul_ps(x3, x2);
         x = _mm256_add_ps(x, _mm256_mul_ps(x3, _mm256_set1_ps(INV_FACTORIAL_5_2PIPOW5)));

         x3 = _mm256_mul_ps(x3, x2);
         x = _mm256_sub_ps(x, _mm256_mul_ps(x3, _mm256_set1_ps(INV_FACTORIAL_7_2PIPOW7)));
      }

      x = _mm256_mul_ps(x, _mm256_mul_ps(env, _mm256_load_ps(voice->read_mod)));

      env = _mm256_add_ps(env, _mm256_load_ps(voice->target_env_step));

      __m256 step_rate = _mm256_load_ps(voice->step_rate);
      __m256 xmod = _mm256_mul_ps(x, step_rate);
      __m256 steps = _mm256_mul_ps(step_rate, _mm256_load_ps(voice->lfo_freq_mod));

      __m256 perm, lo, hi;
#define MAT_ACCUMULATE(scalar, index) \
      perm = _mm256_permute_ps(scalar, _MM_SHUFFLE(index, index, index, index)); \
      lo = _mm256_permute2f128_ps(perm, perm, 0); \
      hi = _mm256_permute2f128_ps(perm, perm, 17); \
      phases = _mm256_add_ps(phases, \
            _mm256_mul_ps(_mm256_load_ps(fm->params.mod_to_carriers[index + 0]), lo)); \
      steps = _mm256_add_ps(steps, \
            _mm256_mul_ps(_mm256_load_ps(fm->params.mod_to_carriers[index + 4]), hi)); \

      MAT_ACCUMULATE(xmod, 0);
      MAT_ACCUMULATE(xmod, 1);
      MAT_ACCUMULATE(xmod, 2);
      MAT_ACCUMULATE(xmod, 3);
#undef MAT_ACCUMULATE

      __m256 sleft  = _mm256_mul_ps(x, _mm256_load_ps(voice->pan_amp[0]));
      __m256 sright = _mm256_mul_ps(x, _mm256_load_ps(voice->pan_amp[1]));

      phases = _mm256_add_ps(phases, steps);
      phases = _mm256_sub_ps(phases, _mm256_floor_ps(phases));

      __m128 left = _mm_add_ps(_mm256_extractf128_ps(sleft, 0), _mm256_extractf128_ps(sleft, 1));
      __m128 right = _mm_add_ps(_mm256_extractf128_ps(sright, 0), _mm256_extractf128_ps(sright, 1));

      __m128 out = _mm_add_ps(_mm_shuffle_ps(left, right,
               _MM_SHUFFLE(1, 0, 1, 0)),
            _mm_shuffle_ps(left, right, _MM_SHUFFLE(3, 2, 3, 2)));
      out = _mm_add_ps(_mm_permute_ps(out, _MM_SHUFFLE(3, 3, 1, 1)), out);
      _mm_store_ss(oleft + f, _mm_add_ss(out, _mm_load_ss(oleft + f)));
      _mm_store_ss(oright + f, _mm_add_ss(_mm_movehl_ps(out, out), _mm_load_ss(oright + f)));
   }

   _mm256_store_ps(voice->phases, phases);
   _mm256_store_ps(voice->env, env);
}

