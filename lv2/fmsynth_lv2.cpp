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
#include "fmsynth.peg"

#include <lvtk/plugin.hpp>
#include <cstring>
#include <stdexcept>
#include <cstdio>

struct FMSynth : public lvtk::Plugin<FMSynth, lvtk::URID<true>>
{
   using Parent = lvtk::Plugin<FMSynth, lvtk::URID<true>>;

   FMSynth(double rate) : Parent(peg_n_ports)
   {
      m_midi_type = map(LV2_MIDI__MidiEvent);

      fm = fmsynth_new(rate, 128);
      if (fm == nullptr)
      {
         throw std::bad_alloc();
      }
   }

   ~FMSynth()
   {
      fmsynth_free(fm);
   }

   float clamp(float v, float lo, float hi)
   {
      return std::min(std::max(v, lo), hi);
   }

   float get_param(unsigned index)
   {
      return clamp(*p(index), peg_ports[index].min, peg_ports[index].max);
   }

   void update_parameters()
   {
      fmsynth_set_global_parameter(fm, FMSYNTH_GLOBAL_PARAM_VOLUME, get_param(peg_volume));
      fmsynth_set_global_parameter(fm, FMSYNTH_GLOBAL_PARAM_LFO_FREQ, get_param(peg_lfofreq));

      for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
      {
         unsigned base_port = peg_amp__op__0_ + o * (peg_amp__op__1_ - peg_amp__op__0_);
         for (unsigned i = 0; i < peg_amp__op__1_ - peg_amp__op__0_; i++)
         {
            fmsynth_set_parameter(fm, i, o, get_param(i + base_port));
         }
      }
   }

   void run(uint32_t sample_count)
   {
      const LV2_Atom_Sequence *seq = p<LV2_Atom_Sequence>(peg_midi);
      uint32_t samples_done = 0;

      float *left  = p(peg_output_left);
      float *right = p(peg_output_right);

      std::memset(left, 0, sample_count * sizeof(float));
      std::memset(right, 0, sample_count * sizeof(float));

      for (LV2_Atom_Event *ev = lv2_atom_sequence_begin(&seq->body);
            !lv2_atom_sequence_is_end(&seq->body, seq->atom.size, ev);
            ev = lv2_atom_sequence_next(ev))
      {
         update_parameters();
         uint32_t to = ev->time.frames;

         if (to > samples_done)
         {
            uint32_t to_render = to - samples_done;
            fmsynth_render(fm, left, right, to_render);

            samples_done += to_render;
            left += to_render;
            right += to_render;
         }

         if (ev->body.type == m_midi_type)
         {
            const uint8_t *body = (const uint8_t*)LV2_ATOM_BODY(&ev->body);
            fmsynth_parse_midi(fm, body);
#if 0
            if (fmsynth_parse_midi(fm, body, size) == FMSYNTH_STATUS_MESSAGE_UNKNOWN)
            {
               fprintf(stderr, "FM Synth: Unknown message: ");
               for (size_t i = 0; i < size; i++)
               {
                  fprintf(stderr, "0x%02x ", body[i]);
               }
               fprintf(stderr, "\n");
            }
#endif
         }
      }

      if (sample_count > samples_done)
      {
         uint32_t to_render = sample_count - samples_done;
         fmsynth_render(fm, left, right, to_render);
      }
   }

   fmsynth_t *fm;
   LV2_URID m_midi_type;
};

int fmsynth_register = FMSynth::register_class("git://github.com/Themaister/fmsynth");

