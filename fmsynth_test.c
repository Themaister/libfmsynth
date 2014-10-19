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

#include "fmsynth.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef ANDROID
int fmsynth_android_main(int argc, char *argv[])
#else
int main(int argc, char *argv[])
#endif
{
   FILE *file = NULL;

   if (argc >= 2)
   {
      file = fopen(argv[1], "w");
      if (!file)
      {
         return EXIT_FAILURE;
      }
   }

   fmsynth_t *fm = fmsynth_new(44100.0f, 64);
   if (fm == NULL)
   {
      return EXIT_FAILURE;
   }

   for (unsigned i = 0; i < 8; i++)
   {
      fmsynth_set_parameter(fm, FMSYNTH_PARAM_MOD_TO_CARRIERS0 + i, (i + 1) & 7, 2.0f);
      fmsynth_set_parameter(fm, FMSYNTH_PARAM_FREQ_MOD, i, i + 1.0f);
      fmsynth_set_parameter(fm, FMSYNTH_PARAM_PAN, i, (i & 1) ? -0.5f : +0.5f);
      fmsynth_set_parameter(fm, FMSYNTH_PARAM_CARRIERS, i, 1.0f);
      fmsynth_set_parameter(fm, FMSYNTH_PARAM_LFO_FREQ_MOD_DEPTH, i, 0.15f);
   }

   for (unsigned i = 0; i < 64; i++)
   {
      fmsynth_note_on(fm, i + 20, 127);
   }

   float left[2048];
   float right[2048];
   for (unsigned i = 0; i < 100; i++)
   {
      memset(left, 0, sizeof(left));
      memset(right, 0, sizeof(right));
      fmsynth_render(fm, left, right, 2048);

      if (file)
      {
         for (unsigned x = 0; x < 2048; x++)
         {
            fprintf(file, "%12.6f %12.6f\n", left[x], right[x]);
         }
      }
   }

   fmsynth_free(fm);

   if (file)
   {
      fclose(file);
   }

   return EXIT_SUCCESS;
}

