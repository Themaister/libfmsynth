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

#include <lvtk/gtkui.hpp>
#include "fmsynth.peg"
#include "fmsynth_private.h"

#include <functional>
#include <utility>
#include <unordered_map>
#include <string>
#include <cmath>
#include <vector>
#include <stdexcept>

#include <stdio.h>

using namespace sigc;
using namespace Gtk;

using EventMap = std::unordered_map<uint32_t, std::function<void (float)>>;
using ValueMap = std::unordered_map<uint32_t, std::function<float ()>>;
class FMSynthGUI;

class Slider : public VBox
{
   public:
      Slider(uint32_t port, Gtk::Orientation orient = Gtk::ORIENTATION_VERTICAL) : port(port)
      {
         slider = manage(orient == Gtk::ORIENTATION_VERTICAL ? static_cast<Scale*>(new VScale) : static_cast<Scale*>(new HScale));
         slider->set_range(0, 1);
         slider->set_draw_value(false);

         if (orient == Gtk::ORIENTATION_VERTICAL)
         {
            slider->set_size_request(-1, 120);
            slider->set_inverted();
         }
         else if (orient == Gtk::ORIENTATION_HORIZONTAL)
            slider->set_size_request(120, -1);
         
         entry = manage(new Entry);
         entry->set_size_request(50, -1);

         slider->signal_value_changed().connect([this] {
            entry->set_text(get_value_string());
         });

         entry->signal_activate().connect([this] {
            float value = get_entry();
            set_value(value);
         });

         pack_start(*entry);
         pack_start(*slider);
      }

      Scale& get_slider() { return *slider; }

      float get_value() const
      {
         float vscale = slider->get_value();
         auto& peg = peg_ports[port];

         if (peg.logarithmic)
         {
            float min_log = std::log(peg.min);
            float max_log = std::log(peg.max);
            float lerp_log = min_log + (max_log - min_log) * vscale;
            return std::exp(lerp_log);
         }
         else
            return peg.min + (peg.max - peg.min) * vscale;
      }

      void set_value(float value)
      {
         auto& peg = peg_ports[port];
         value = std::min(std::max(value, peg.min), peg.max);

         if (peg.logarithmic)
         {
            float min_log = std::log(peg.min);
            float max_log = std::log(peg.max);
            float value_log = std::log(value);
            float lerp = (value_log - min_log) / (max_log - min_log);
            slider->set_value(lerp);
         }
         else
         {
            float lerp = (value - peg.min) / (peg.max - peg.min);
            slider->set_value(lerp);
         }

         entry->set_text(get_value_string());
      }

   private:
      uint32_t port;
      Scale *slider;
      Entry *entry;

      float get_entry()
      {
         try {
            return std::stof(entry->get_text().c_str());
         } catch(...) {
            return peg_ports[port].default_value;
         }
      }

      Glib::ustring get_value_string() const
      {
         char buf[64];
         std::snprintf(buf, sizeof(buf), "%.3f", get_value());
         return buf;
      }
};

class SlidersFrame : public Frame
{
   public:
      SlidersFrame(const char *frame_desc, uint32_t start_port, const std::vector<const char*>& descs,
            EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : Frame(frame_desc)
      {
         auto table = manage(new Table(2, descs.size()));
         table->set_col_spacings(5);
         table->set_border_width(5);

         unsigned i = 0;
         for (auto& desc : descs)
         {
            uint32_t port = base_port + start_port + i;
            auto slider = manage(new Slider(port));

            slider->get_slider().signal_value_changed().connect([this, slider, func, port] {
               func(port, slider->get_value());
            });

            events[port] = [this, slider, port](float value) {
               slider->set_value(value);
            };

            values[port] = [this, slider, port] { return slider->get_value(); };

            auto label = manage(new Label(desc));
            table->attach(*label, i, i + 1, 0, 1);
            table->attach(*slider, i, i + 1, 1, 2);
            i++;
         }

         set_shadow_type(Gtk::SHADOW_ETCHED_OUT);
         set_label_align(Gtk::ALIGN_END, Gtk::ALIGN_START);
         add(*table);
      }
};

class AlgorithmMatrix : public Frame
{
   public:
      AlgorithmMatrix(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, unsigned operators)
         : Frame("FM Matrix")
      {
         set_border_width(5);
         auto vbox = manage(new VBox);
         auto table = manage(new Table(operators, operators));

         auto carrier_frame = manage(new Frame("Carriers"));
         carrier_frame->set_border_width(5);
         auto carriers = manage(new HBox);
         carrier_frame->add(*carriers);

         for (unsigned i = 0; i < operators; i++)
         {
            unsigned port = (peg_amp__op__1_ - peg_amp__op__0_) * i + peg_carriers__op__0_;
            auto btn = manage(new ToggleButton(std::string("#") + std::to_string(i + 1)));
            btn->signal_clicked().connect([this, func, port, btn] {
               func(port, btn->get_active() ? 1.0f : 0.0f);
            });
            events[port] = [this, btn](float value) {
               btn->set_active(value > 0.5f);
            };
            values[port] = [this, btn] { return btn->get_active() ? 1.0f : 0.0f; };
            carriers->pack_start(*btn);
         }

         vbox->pack_start(*carrier_frame);

         for (unsigned c = 0; c < operators; c++)
         {
            for (unsigned r = 0; r < operators; r++)
            {
               unsigned port = (peg_amp__op__1_ - peg_amp__op__0_) * c + peg_mod0tooperator__op__0_ + r;
               auto btn = manage(new ToggleButton(std::to_string(r + 1) + " -> " + std::to_string(c + 1)));
               btn->signal_clicked().connect([this, func, port, btn] {
                  func(port, btn->get_active() ? 1.0f : 0.0f);
               });
               events[port] = [this, btn](float value) {
                  btn->set_active(value > 0.5f);
               };
               values[port] = [this, btn] { return btn->get_active() ? 1.0f : 0.0f; };

               table->attach(*btn, c, c + 1, r, r + 1);
            }
         }

         auto matrix_frame = manage(new Frame("Modulators"));
         matrix_frame->set_border_width(5);
         matrix_frame->add(*table);
         vbox->pack_start(*manage(new HSeparator));
         vbox->pack_start(*matrix_frame);

         add(*vbox);
      }
};

class BasicParameters : public Frame
{
   public:
      BasicParameters(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func)
         : Frame("Global Parameters")
      {
         auto hbox = manage(new HBox);
         set_border_width(5);

         vol = manage(new Slider(peg_volume, Gtk::ORIENTATION_HORIZONTAL));
         lfofreq = manage(new Slider(peg_lfofreq, Gtk::ORIENTATION_HORIZONTAL));

         auto frame = manage(new Frame("Volume"));
         frame->set_border_width(5);
         frame->add(*vol);
         hbox->pack_start(*frame);

         frame = manage(new Frame("LFO Frequency"));
         frame->set_border_width(5);
         frame->add(*lfofreq);
         hbox->pack_start(*frame);

         hbox->pack_start(*frame);

         vol->get_slider().signal_value_changed().connect([this, func] { func(peg_volume, vol->get_value()); });
         lfofreq->get_slider().signal_value_changed().connect([this, func] { func(peg_lfofreq, lfofreq->get_value()); });

         events[peg_volume]  = [this](float value) { vol->set_value(value); };
         events[peg_lfofreq] = [this](float value) { lfofreq->set_value(value); };

         values[peg_volume]  = [this] { return vol->get_value(); };
         values[peg_lfofreq] = [this] { return lfofreq->get_value(); };

         add(*hbox);
      }

   private:
      Slider *vol;
      Slider *lfofreq;
};

class Presets : public Frame
{
   public:
      Presets(const char *bundle_path, EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func)
         : Frame("Presets"), events(events), values(values), lv2_func(func), bundle_path(bundle_path)
      {
         auto vbox = manage(new VBox);
         auto hbox = manage(new HBox);
         set_border_width(5);

         auto table = manage(new Table(2, 2));
         table->set_border_width(5);

         filter.set_name("FMSynth presets");
         filter.add_pattern("*.fmp");
         any_filter.set_name("Any file");
         any_filter.add_pattern("*");

         auto load = manage(new Button("Load Preset"));
         auto save = manage(new Button("Save Preset"));
         load->signal_clicked().connect([this] { load_preset(); });
         save->signal_clicked().connect([this] { save_preset(); });

         table->attach(*manage(new Label("Preset Name:")), 0, 1, 0, 1);
         table->attach(*manage(new Label("Preset Author:")), 1, 2, 0, 1);
         table->attach(name, 0, 1, 1, 2);
         table->attach(author, 1, 2, 1, 2);

         name.set_size_request(50, -1);
         author.set_size_request(50, -1);

         hbox->add(*load);
         hbox->add(*save);
         vbox->add(*hbox);
         vbox->add(*table);
         add(*vbox);
      }

   private:
      FileFilter filter;
      FileFilter any_filter;
      EventMap& events;
      ValueMap& values;
      std::function<void (uint32_t, float)> lv2_func;
      const char *bundle_path;

      Entry name;
      Entry author;

      void load_preset()
      {
         FileChooserDialog dialog("Load Preset", FILE_CHOOSER_ACTION_OPEN);
         dialog.add_filter(filter);
         dialog.add_filter(any_filter);
         dialog.add_button("Cancel", RESPONSE_CANCEL);
         dialog.add_button("Open", RESPONSE_OK);

         if (bundle_path)
            dialog.add_shortcut_folder(bundle_path);

         int status = dialog.run();
         if (status == RESPONSE_OK)
         {
            try
            {
               load_preset_from(dialog.get_uri());
            }
            catch (const std::exception& e)
            {
               MessageDialog error(e.what(), false, MESSAGE_ERROR);
               error.run();
            }
            catch (const Glib::Error &e)
            {
               MessageDialog error(e.what(), false, MESSAGE_ERROR);
               error.run();
            }
         }
      }

      static Glib::ustring fixup_suffix(const Glib::ustring& uri)
      {
         // Add .fmp extension automatically if not provided.
         size_t len = uri.length();
         if (len >= 4 && (uri.substr(len - 4) != ".fmp"))
            return uri + ".fmp";
         else
            return uri;
      }

      void save_preset()
      {
         FileChooserDialog dialog("Save Preset", FILE_CHOOSER_ACTION_SAVE);
         dialog.set_do_overwrite_confirmation(true);
         dialog.add_filter(filter);
         dialog.add_filter(any_filter);
         dialog.add_button("Cancel", RESPONSE_CANCEL);
         dialog.add_button("Save", RESPONSE_OK);

         int status = dialog.run();
         if (status == RESPONSE_OK)
         {
            try
            {
               save_preset_to(fixup_suffix(dialog.get_uri()));
            }
            catch (const std::exception& e)
            {
               MessageDialog error(e.what(), false, MESSAGE_ERROR);
               error.run();
            }
            catch (const Glib::Error &e)
            {
               MessageDialog error(e.what(), false, MESSAGE_ERROR);
               error.run();
            }
         }
      }

      void load_preset_from(const Glib::ustring& uri)
      {
         auto file = Gio::File::create_for_uri(uri);
         if (!file)
            throw std::runtime_error("Failed to open file.");

         auto stream = file->read();
         if (!stream)
            throw std::runtime_error("Failed to open file.");

         std::vector<uint8_t> buffer(fmsynth_preset_size());

         gsize did_read;
         bool ret = stream->read_all(buffer.data(), buffer.size(), did_read) && did_read == buffer.size();
         stream->close();

         if (!ret)
            throw std::runtime_error("Failed to read file.");

         struct fmsynth_voice_parameters params;
         struct fmsynth_global_parameters global_params;
         struct fmsynth_preset_metadata metadata;
         memset(&metadata, 0, sizeof(metadata));

         fmsynth_status_t status = fmsynth_preset_load_private(&global_params, &params,
               &metadata, buffer.data(), buffer.size());
         ret = status == FMSYNTH_STATUS_OK;

         if (ret)
         {
            Glib::ustring name_str(metadata.name);
            Glib::ustring author_str(metadata.author);
            if (!name_str.validate() || !author_str.validate())
               throw std::logic_error("Preset string is not valid UTF-8.");
            name.set_text(name_str);
            author.set_text(author_str);

            const float *values = params.amp;

            set_parameter(peg_volume, global_params.volume);
            set_parameter(peg_lfofreq, global_params.lfo_freq);
            for (unsigned p = 0; p < FMSYNTH_PARAM_END; p++)
            {
               for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
               {
                  set_parameter(peg_amp__op__0_ + (peg_amp__op__1_ - peg_amp__op__0_) * o + p,
                        values[p * FMSYNTH_OPERATORS + o]);
               }
            }
         }
         else
            throw std::runtime_error("Failed to parse preset.");
      }

      static void put_raw_string(char *buffer, const Entry& entry)
      {
         auto text = entry.get_text();
         size_t len = text.bytes();

         if (len >= FMSYNTH_PRESET_STRING_SIZE)
         {
            // TODO: Should find a better way to make this limit more intuitive.
            throw std::logic_error("Preset string overflows buffer.");
         }

         // We've already verified, so memcpy is fine.
         memcpy(buffer, text.c_str(), len);
         buffer[len] = '\0';
      }

      void save_preset_to(const Glib::ustring& uri)
      {
         std::vector<uint8_t> buffer(fmsynth_preset_size());

         struct fmsynth_voice_parameters params;
         struct fmsynth_global_parameters global_params;
         struct fmsynth_preset_metadata metadata;
         memset(&metadata, 0, sizeof(metadata));

         put_raw_string(metadata.name, name);
         put_raw_string(metadata.author, author);

         global_params.volume = get_parameter(peg_volume);
         global_params.lfo_freq = get_parameter(peg_lfofreq);

         float *values = params.amp;
         for (unsigned p = 0; p < FMSYNTH_PARAM_END; p++)
         {
            for (unsigned o = 0; o < FMSYNTH_OPERATORS; o++)
            {
               values[p * FMSYNTH_OPERATORS + o] =
                  get_parameter(peg_amp__op__0_ + (peg_amp__op__1_ - peg_amp__op__0_) * o + p);
            }
         }

         fmsynth_status_t status = fmsynth_preset_save_private(&global_params, &params,
               &metadata, buffer.data(), buffer.size());

         if (status != FMSYNTH_STATUS_OK)
            throw std::runtime_error("Failed to save preset to binary format.");

         auto file = Gio::File::create_for_uri(uri);
         if (!file)
            throw std::runtime_error("Failed to save preset to disk.");

         auto stream = file->replace();
         if (!stream)
            throw std::runtime_error("Failed to save preset to disk.");

         gsize ret;
         if (!stream->write_all(buffer.data(), buffer.size(), ret) || ret != buffer.size())
            throw std::runtime_error("Failed to write preset to disk.");
      }

      void set_parameter(uint32_t id, float value)
      {
         auto func = events[id];
         if (func)
            func(value);
         lv2_func(id, value);
      }

      float get_parameter(uint32_t id)
      {
         auto func = values[id];
         if (func)
            return func();
         else
            return peg_ports[id].default_value;
      }
};

class AmpPan : public SlidersFrame
{
   public:
      AmpPan(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : SlidersFrame("Amplifier", peg_amp__op__0_,
               { "Volume", "Pan" },
               events, values, func, base_port)
      {}
};

class FreqMod : public Frame
{
   public:
      FreqMod(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : Frame("Oscillator"), func(func), base_port(base_port)
      {
         auto table = manage(new Table(4, 5));
         table->set_border_width(5);

         entry = manage(new Entry);
         entry->set_text("1.00");
         auto button_plus1 = manage(new Button("+1"));
         auto button_minus1 = manage(new Button("-1"));
         auto button_plus10 = manage(new Button("+0.1"));
         auto button_minus10 = manage(new Button("-0.1"));
         auto button_plus100 = manage(new Button("+0.01"));
         auto button_minus100 = manage(new Button("-0.01"));

         table->attach(*button_plus1, 1, 2, 1, 2);
         table->attach(*button_plus10, 2, 3, 1, 2);
         table->attach(*button_plus100, 3, 4, 1, 2);
         table->attach(*manage(new Label("Ratio:")), 0, 1, 2, 3);
         table->attach(*entry, 1, 4, 2, 3);
         table->attach(*button_minus1, 1, 2, 3, 4);
         table->attach(*button_minus10, 2, 3, 3, 4);
         table->attach(*button_minus100, 3, 4, 3, 4);

         entry->signal_activate().connect([this] {
            float value = get_entry();
            value = std::min(std::max(value, peg_ports[peg_freqmod__op__0_].min), peg_ports[peg_freqmod__op__0_].max);
            entry->set_text(std::to_string(value));
            this->func(this->base_port + peg_freqmod__op__0_, value);
         });
         button_plus1->signal_clicked().connect([this] {
            add_entry(1.0f);
         });
         button_minus1->signal_clicked().connect([this] {
            add_entry(-1.0f);
         });
         button_plus10->signal_clicked().connect([this] {
            add_entry(0.1f);
         });
         button_minus10->signal_clicked().connect([this] {
            add_entry(-0.1f);
         });
         button_plus100->signal_clicked().connect([this] {
            add_entry(0.01f);
         });
         button_minus100->signal_clicked().connect([this] {
            add_entry(-0.01f);
         });

         uint32_t offset_port = base_port + peg_freqoffset__op__0_;
         auto offset = manage(new Slider(offset_port));

         offset->get_slider().signal_value_changed().connect([this, offset_port, offset] {
            this->func(offset_port, offset->get_value());
         });

         table->attach(*manage(new Label("Offset")), 4, 5, 0, 1);
         table->attach(*offset, 4, 5, 1, 4);

         events[offset_port] = [this, offset](float value) { offset->set_value(value); };
         events[base_port + peg_freqmod__op__0_] = [this](float value) { entry->set_text(std::to_string(value)); };

         values[offset_port] = [this, offset] { return offset->get_value(); };
         values[base_port + peg_freqmod__op__0_] = [this] { return get_entry(); };

         add(*table);
      }

   private:
      Entry *entry;
      std::function<void (uint32_t, float)> func;
      uint32_t base_port;

      float get_entry()
      {
         try {
            return std::stof(entry->get_text().c_str());
         } catch(...) {
            return 1.0f;
         }
      }

      void add_entry(float delta)
      {
         float value = get_entry();
         value += delta;
         value = std::min(std::max(value, peg_ports[peg_freqmod__op__0_].min), peg_ports[peg_freqmod__op__0_].max);
         value = std::round(value / delta) * delta;

         func(base_port + peg_freqmod__op__0_, value);

         char buf[64];
         std::snprintf(buf, sizeof(buf), "%.2f", value);
         entry->set_text(buf);
      }
};

class LFODepth : public SlidersFrame
{
   public:
      LFODepth(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : SlidersFrame("LFO Sensitivity", peg_lfoampdepth__op__0_,
               { "Amp", "Frequency" },
               events, values, func, base_port)
      {}
};

class KeyboardScaling : public SlidersFrame
{
   public:
      KeyboardScaling(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : SlidersFrame("Keyboard Scaling", peg_keyboardscalingmidpoint__op__0_,
               { "Mid", "Lower", "Upper", "Velocity", "Mod Wheel" },
               events, values, func, base_port)
      {}
};

class EnvelopeTargets : public SlidersFrame
{
   public:
      EnvelopeTargets(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : SlidersFrame("Envelope Targets", peg_envelopetarget0__op__0_,
               { "T1", "T2", "T3" },
               events, values, func, base_port)
      {}
};

class EnvelopeRates : public SlidersFrame
{
   public:
      EnvelopeRates(EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : SlidersFrame("Envelope Rates", peg_envelopedelay0__op__0_,
               { "R1", "R2", "R3", "R4" },
               events, values, func, base_port)
      {}
};

class OperatorWidget : public Frame
{
   public:
      OperatorWidget(const char *label, EventMap& events, ValueMap& values, std::function<void (uint32_t, float)> func, uint32_t base_port)
         : Frame(label), port(base_port)
      {
         auto hbox = manage(new HBox);
         hbox->set_spacing(5);

         auto enable = manage(new CheckButton("Enable"));
         enable->signal_toggled().connect([this, func, enable] {
            func(port + peg_enable__op__0_, enable->get_active() ? 1.0f : 0.0f);
         });

         events[port + peg_enable__op__0_] = [this, enable](float value) {
            enable->set_active(value > 0.5f);
         };

         values[port + peg_enable__op__0_] = [this, enable] { return enable->get_active() ? 1.0f : 0.0f; };

         auto default_op = manage(new Button("Default"));
         unsigned start_port = peg_amp__op__0_ + base_port;
         default_op->signal_clicked().connect([this, &events, func, start_port] {
            for (unsigned i = start_port; i < start_port + peg_amp__op__1_ - peg_amp__op__0_; i++)
            {
               auto fn = events[i];
               if (fn)
                  fn(peg_ports[i].default_value);
               func(i, peg_ports[i].default_value);
            }
         });

         auto vbox = manage(new VBox);
         vbox->pack_start(*enable);
         vbox->pack_start(*default_op);
         vbox->pack_start(*manage(new HSeparator));
         vbox->pack_start(*manage(new AmpPan(events, values, func, port)));

         hbox->pack_start(*vbox);
         hbox->pack_start(*manage(new FreqMod(events, values, func, port)));
         hbox->pack_start(*manage(new EnvelopeRates(events, values, func, port)));
         hbox->pack_start(*manage(new EnvelopeTargets(events, values, func, port)));
         hbox->pack_start(*manage(new LFODepth(events, values, func, port)));
         hbox->pack_start(*manage(new KeyboardScaling(events, values, func, port)));

         add(*hbox);
      }

   private:
      uint32_t port;
};

class FMSynthGUI : public lvtk::UI<FMSynthGUI, lvtk::GtkUI<true>, lvtk::URID<true>>
{
   public:
      FMSynthGUI(const std::string&)
      {
         auto write_ctrl = [this](uint32_t port, float value) { write_control(port, value); };

         auto hbox = manage(new HBox);

         auto vbox = manage(new VBox);
         auto notebook = manage(new Notebook);
         unsigned operators = (peg_n_ports - peg_amp__op__0_) / (peg_amp__op__1_ - peg_amp__op__0_);

         for (unsigned i = 0; i < operators; i++)
         {
            unsigned base_port = (peg_amp__op__1_ - peg_amp__op__0_) * i;
            auto str = std::string("Operator #") + std::to_string(i + 1);
            auto op = manage(new OperatorWidget(str.c_str(), event_map, value_map, write_ctrl, base_port));

            notebook->append_page(*op, str);
         }

         auto top_hbox = manage(new HBox);

         top_hbox->add(*manage(new BasicParameters(event_map, value_map, write_ctrl)));
         top_hbox->add(*manage(new Presets(bundle_path(), event_map, value_map, write_ctrl)));

         vbox->pack_start(*top_hbox);
         vbox->pack_start(*notebook);

         hbox->pack_start(*vbox);
         hbox->pack_start(*manage(new AlgorithmMatrix(event_map, value_map, write_ctrl, operators)));

         add(*hbox);
      }

      void port_event(uint32_t port, uint32_t, uint32_t, const void *buffer)
      {
         //fprintf(stderr, "Port event() %u -> %.3f\n", port, *static_cast<const float*>(buffer));
         auto itr = event_map.find(port);
         if (itr != end(event_map))
            itr->second(*static_cast<const float*>(buffer));
      }

   private:
       EventMap event_map;
       ValueMap value_map;
};

int ui_class = FMSynthGUI::register_class("git://github.com/Themaister/fmsynth/gui");

