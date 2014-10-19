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

#include "net_themaister_fmsynthtest_NativeInterface.h"

#include <time.h>
#include <stddef.h>

int fmsynth_android_main(int argc, char *argv[]);

JNIEXPORT jfloat JNICALL Java_net_themaister_fmsynthtest_NativeInterface_runTest
  (JNIEnv *env, jclass clazz)
{
   (void)env;
   (void)clazz;

   struct timespec start, end;
   clock_gettime(CLOCK_MONOTONIC, &start);

   char name[] = "fmsynth_test";
   char *argv[] = { name, NULL };
   fmsynth_android_main(1, argv);

   clock_gettime(CLOCK_MONOTONIC, &end);

   return end.tv_sec - start.tv_sec + 0.000000001 * (end.tv_nsec - start.tv_nsec);
}

