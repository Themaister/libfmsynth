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

.text
.arm

#define PHASE0 q8
#define PHASE1 q9
#define ENV0 q10
#define ENV1 q11

#define X0 q4
#define X1 q5

// Only used in sine computation.
// Contents can be discarded after taylor approx.
#define X20 q6
#define X21 q7
#define X30 q12
#define X31 q13

#define XMOD0 q6
#define XMOD1 q7
#define LDREG0 q0
#define LDREG1 q1
#define LDREG2 q2
#define LDREG3 q3
#define LEFT q12
#define RIGHT q13
#define TMP0 q14
#define TMP1 q15

// Used for fract() computation only.
#define ROUND32 LDREG0
#define HALF LDREG1
#define FLOORED0 LDREG2
#define FLOORED1 LDREG3
#define ROUNDED0 TMP0
#define ROUNDED1 TMP1
#define MASK0 XMOD0
#define MASK1 XMOD1

#define XMOD_0 d12[0]
#define XMOD_1 d12[1]
#define XMOD_2 d13[0]
#define XMOD_3 d13[1]
#define XMOD_4 d14[0]
#define XMOD_5 d14[1]
#define XMOD_6 d15[0]
#define XMOD_7 d15[1]

#define LDREG_0 d0[0]
#define LDREG_1 d0[1]
#define LDREG_2 d1[0]
#define LDREG_3 d1[1]

#define LEFT_0 d24
#define LEFT_1 d25
#define RIGHT_0 d26
#define RIGHT_1 d27

#define TMP_0 d28

#define PHASE_OFFSET 0
#define ENV_OFFSET 32
#define BASE_OFFSET 64

.align 4
.globl fmsynth_process_frames_neon
fmsynth_process_frames_neon:

   vpush {d8 - d15}
   push {r4 - r12, lr}

   ldr r4, [sp, #(10 * 4 + 8 * 8)]

   adr r6, CONSTANTS

   add r8, r1, #PHASE_OFFSET
   add r9, r1, #ENV_OFFSET

   vld1.32 {PHASE0-PHASE1}, [r8, :128]
   vld1.32 {ENV0-ENV1}, [r9, :128]

.align 4
1:
   // Base offset for READ_MOD.
   add r11, r1, #BASE_OFFSET

   // Talyor sine approximation.
   // No LUT.
   vmov.f32 LDREG0, #0.25
   vmov.f32 LDREG1, #0.50
   vmov.f32 LDREG2, #0.75

   vclt.f32 MASK0, PHASE0, LDREG1
   vclt.f32 MASK1, PHASE1, LDREG1
   vsub.f32 X0, PHASE0, LDREG0
   vsub.f32 X1, PHASE1, LDREG0
   vsub.f32 TMP0, LDREG2, PHASE0
   vsub.f32 TMP1, LDREG2, PHASE1

   vand X0, X0, MASK0
   vand X1, X1, MASK1
   vbic TMP0, TMP0, MASK0
   vbic TMP1, TMP1, MASK1
   vorr X0, X0, TMP0
   vorr X1, X1, TMP1

   vld1.32 {LDREG0-LDREG1}, [r6, :128]

   vmul.f32 X20, X0, X0
   vmul.f32 X21, X1, X1
   vmul.f32 X30, X0, X20
   vmul.f32 X31, X1, X21

   vmul.f32 X0, X0, LDREG1
   vmul.f32 X1, X1, LDREG1

   vmls.f32 X0, X30, LDREG_0
   vmls.f32 X1, X31, LDREG_0
   
   vmul.f32 X30, X20, X30
   vmul.f32 X31, X21, X31

   vmla.f32 X0, X30, LDREG_1
   vmla.f32 X1, X31, LDREG_1

   vmul.f32 X30, X20, X30
   vmul.f32 X31, X21, X31

   vmls.f32 X0, X30, LDREG_2
   vmls.f32 X1, X31, LDREG_2
   // End Taylor sine approximation.

   vld1.32 {LDREG0-LDREG1}, [r11, :128]!
   vmul.f32 TMP0, ENV0, LDREG0
   vmul.f32 TMP1, ENV1, LDREG1

   // Apply read_mod and envelope.
   vmul.f32 X0, X0, TMP0
   vmul.f32 X1, X1, TMP1

   // Increment envelope.
   vld1.32 {LDREG0-LDREG1}, [r11, :128]!
   vadd.f32 ENV0, ENV0, LDREG0
   vadd.f32 ENV1, ENV1, LDREG1

   // Modulator version of oscillators.
   vld1.32 {LDREG0-LDREG1}, [r11, :128]!
   vmul.f32 XMOD0, X0, LDREG0
   vmul.f32 XMOD1, X1, LDREG1

   // Initial phase step value.
   vld1.32 {LDREG2-LDREG3}, [r11, :128]!
   vmul.f32 TMP0, LDREG0, LDREG2
   vmul.f32 TMP1, LDREG1, LDREG3

   // 8-by-8 matrix-vector multiply.
   // Alternate MLA target to improve pipelining.
   // 5 cycle latency for MLA with dependent adder on Cortex-A15.
   // Out-of-order should help fill the missing cycles with something useful.
   mov r10, r0
   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 PHASE0, LDREG0, XMOD_0
   vmla.f32 PHASE1, LDREG1, XMOD_0

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 TMP0, LDREG0, XMOD_1
   vmla.f32 TMP1, LDREG1, XMOD_1

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 PHASE0, LDREG0, XMOD_2
   vmla.f32 PHASE1, LDREG1, XMOD_2

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 TMP0, LDREG0, XMOD_3
   vmla.f32 TMP1, LDREG1, XMOD_3

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 PHASE0, LDREG0, XMOD_4
   vmla.f32 PHASE1, LDREG1, XMOD_4

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 TMP0, LDREG0, XMOD_5
   vmla.f32 TMP1, LDREG1, XMOD_5

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 PHASE0, LDREG0, XMOD_6
   vmla.f32 PHASE1, LDREG1, XMOD_6

   vld1.32 {LDREG0-LDREG1}, [r10, :128]!
   vmla.f32 TMP0, LDREG0, XMOD_7
   vmla.f32 TMP1, LDREG1, XMOD_7
   ///////

   vld1.32 {LDREG0-LDREG1}, [r11, :128]!
   vld1.32 {LDREG2-LDREG3}, [r11, :128]!

   vmul.f32 LEFT, X0, LDREG0
   vmul.f32 RIGHT, X0, LDREG2

   // Add in interleaved MLA register.
   vadd.f32 PHASE0, PHASE0, TMP0 
   vadd.f32 PHASE1, PHASE1, TMP1 

   vmla.f32 LEFT, X1, LDREG1
   vmla.f32 RIGHT, X1, LDREG3

   // Compute fract() for phase registers.
   // AArch64 gives us proper vrndm in a single operation ...
   adr r12, ROUND32_CONSTANT
   vld1.f32 {ROUND32}, [r12, :128]
   vmov.f32 HALF, #0.5

   vsub.f32 FLOORED0, PHASE0, HALF
   vsub.f32 FLOORED1, PHASE1, HALF

   vadd.f32 ROUNDED0, PHASE0, ROUND32
   vadd.f32 ROUNDED1, PHASE1, ROUND32
   vadd.f32 FLOORED0, FLOORED0, ROUND32
   vadd.f32 FLOORED1, FLOORED1, ROUND32

   vsub.f32 ROUNDED0, ROUNDED0, ROUND32
   vsub.f32 ROUNDED1, ROUNDED1, ROUND32
   vsub.f32 FLOORED0, FLOORED0, ROUND32
   vsub.f32 FLOORED1, FLOORED1, ROUND32

   vceq.f32 MASK0, PHASE0, ROUNDED0
   vceq.f32 MASK1, PHASE1, ROUNDED1

   vand     ROUNDED0, PHASE0, MASK0
   vand     ROUNDED1, PHASE1, MASK1
   vbic     FLOORED0, FLOORED0, MASK0
   vbic     FLOORED1, FLOORED1, MASK1

   vorr     FLOORED0, FLOORED0, ROUNDED0
   vorr     FLOORED1, FLOORED1, ROUNDED1

   vsub.f32 PHASE0, PHASE0, FLOORED0
   vsub.f32 PHASE1, PHASE1, FLOORED1
   ///////

   // Mix-down
   vadd.f32 LEFT_0, LEFT_0, LEFT_1
   vadd.f32 LEFT_1, RIGHT_0, RIGHT_1
   vpadd.f32 LEFT_0, LEFT_0, LEFT_1

   // Mix in result to buffer.
   vld1.32 {TMP_0[0]}, [r2, :32]
   vld1.32 {TMP_0[1]}, [r3, :32]
   vadd.f32 TMP_0, TMP_0, LEFT_0
   vst1.32 {TMP_0[0]}, [r2, :32]!
   vst1.32 {TMP_0[1]}, [r3, :32]!

   subs r4, r4, #1
   bgt 1b

   // Save back phase and envelopes.
   vst1.32 {PHASE0-PHASE1}, [r8, :128]
   vst1.32 {ENV0-ENV1}, [r9, :128]

   pop {r4 - r12, lr}
   vpop {d8 - d15}

   bx lr

.align 4
CONSTANTS:
   .float 41.341702240399755
   .float 81.60524927607504
   .float 76.70585975306136
   .float 0.0
   .float 6.28318530717958
   .float 6.28318530717958
   .float 6.28318530717958
   .float 6.28318530717958
ROUND32_CONSTANT:
   .float 12582912.0
   .float 12582912.0
   .float 12582912.0
   .float 12582912.0

