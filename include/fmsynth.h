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

/** \weakgroup libfmsynth libfmsynth public API
 * @{
 */

/**
 * Number of FM operators supported. Changing this will break ABI
 * and SIMD implementations.
 */
#define FMSYNTH_OPERATORS 8

/**
 * Opaque type which encapsulates FM synth state.
 */
typedef struct fmsynth fmsynth_t;

/**
 * Parameters for the synth which are unique per FM operator.
 */
enum fmsynth_parameter
{
   FMSYNTH_PARAM_AMP = 0, /**< Linear amplitude for the operator. */
   FMSYNTH_PARAM_PAN,     /**< Panning for operator when it's used as a carrier. -1.0 is left, +1.0 is right, +0.0 is centered. */

   FMSYNTH_PARAM_FREQ_MOD, /**< Frequency mod factor. The base frequency of the operator is note frequency times the freq mod.
                             E.g. A4 with freq mod of 2.0 would be 880 Hz. */
   FMSYNTH_PARAM_FREQ_OFFSET, /**< A constant frequency offset applied to the oscillator. */

   FMSYNTH_PARAM_ENVELOPE_TARGET0, /**< The linear amplitude reached in the envelope after FMSYNTH_PARAM_DELAY0 seconds. Initial amplitude is 0. */
   FMSYNTH_PARAM_ENVELOPE_TARGET1, /**< The linear amplitude reached in the envelope after (FMSYNTH_PARAM_DELAY0 + FMSYNTH_PARAM_DELAY1) seconds. */
   FMSYNTH_PARAM_ENVELOPE_TARGET2, /**< The linear amplitide reached in the envelope after (FMSYNTH_PARAM_DELAY0 + FMSYNTH_PARAM_DELAY1 + FMSYNTH_PARAM_DELAY2) seconds. */
   FMSYNTH_PARAM_DELAY0, /**< The time in seconds for the envelope to reach FMSYNTH_PARAM_ENVELOPE_TARGET0. */
   FMSYNTH_PARAM_DELAY1, /**< The time in seconds for the envelope to reach FMSYNTH_PARAM_ENVELOPE_TARGET1. */
   FMSYNTH_PARAM_DELAY2, /**< The time in seconds for the envelope to reach FMSYNTH_PARAM_ENVELOPE_TARGET2. */
   FMSYNTH_PARAM_RELEASE_TIME, /**< After releasing the key, the time it takes for the operator to attenuate 60 dB. */

   FMSYNTH_PARAM_KEYBOARD_SCALING_MID_POINT, /**< The frequency which splits the keyboard into a "low" and "high" section.
                                               This frequency only depends on the note itself, not FMSYNTH_PARAM_FREQ_MOD, etc. */
   FMSYNTH_PARAM_KEYBOARD_SCALING_LOW_FACTOR, /**< Amplitude scaling factor pow(note_frequency / SCALING_MID_POINT, SCALING_LOW_FACTOR) if the key pressed
                                                is in the "low" section of the keyboard.
                                                Negative values will boost amplitide for lower frequency keys and attenuate amplitude for higher frequency keys.
                                                E.g. A value of -1.0 will add a 6 dB attenuation per octave. */
   FMSYNTH_PARAM_KEYBOARD_SCALING_HIGH_FACTOR, /**< Amplitude scaling factor pow(note_frequency / SCALING_MID_POINT, SCALING_HIGH_FACTOR) if the key pressed
                                                is in the "high" section of the keyboard.
                                                Negative values will boost amplitide for lower frequency keys and attenuate amplitude for higher frequency keys.
                                                E.g. A value of -1.0 will add a 6 dB attenuation per octave. */

   FMSYNTH_PARAM_VELOCITY_SENSITIVITY, /**< Controls velocity sensitivity. If 0.0, operator amplitude is independent of key velocity.
                                         If 1.0, the operator amplitude is fully dependent on key velocity.
                                         `factor = (1.0 - VELOCITY_SENSITIVITY) + VELOCITY_SENSITIVITY * velocity`.
                                         `velocity` is normalized to [0, 1]. */
   FMSYNTH_PARAM_MOD_WHEEL_SENSITIVITY, /**< If 0.0, operator amplitude is independent of mod wheel state.
                                          If 1.0, operator amplitude is fully dependent on mod wheel state.
                                          `factor = (1.0 - MOD_WHEEL_SENSITIVITY) + MOD_WHEEL_SENSITIVITY * mod_wheel`.
                                          `mod_wheel` is normalized to [0, 1]. */

   FMSYNTH_PARAM_LFO_AMP_SENSITIVITY, /**< Specifies how much the LFO modulates amplitude.
                                        Modulation factor is: 1.0 + lfo_value * LFO_AMP_SENSITIVITY.
                                        lfo_value has a range of [-1, 1]. */
   FMSYNTH_PARAM_LFO_FREQ_MOD_DEPTH,  /**< Specifies how much the LFO modulates frequency.
                                        Modulation factor is: 1.0 + lfo_value * LFO_FREQ_MOD_DEPTH.
                                        lfo_value has a range of [-1, 1]. */

   FMSYNTH_PARAM_ENABLE,              /**< Enable operator if value > 0.5, otherwise, disable. */

   FMSYNTH_PARAM_CARRIERS,            /**< Set carrier mixing factor. If > 0.0, the operator will generate audio that is mixed into the final output. */
   FMSYNTH_PARAM_MOD_TO_CARRIERS0,    /**< Sets how much the operator will modulate carrier `N`. Use `FMSYNTH_PARAM_MOD_TO_CARRIERS0 + N` to specify which operator is the modulator target. */

   FMSYNTH_PARAM_END = FMSYNTH_PARAM_MOD_TO_CARRIERS0 + FMSYNTH_OPERATORS, /**< The number of parameters available. */
   FMSYNTH_PARAM_ENSURE_INT = INT_MAX /**< Ensure the enum is sizeof(int). */
};

/**
 * Parameters which are global to the entire synth.
 */
enum fmsynth_global_parameter
{
   FMSYNTH_GLOBAL_PARAM_VOLUME = 0, /**< Overall volume of the synth. */
   FMSYNTH_GLOBAL_PARAM_LFO_FREQ, /**< LFO frequency in Hz. */

   FMSYNTH_GLOBAL_PARAM_END, /**< The number of global parameters available. */
   FMSYNTH_GLOBAL_PARAM_ENSURE_INT = INT_MAX /**< Ensure the enum is sizeof(int). */
};

/**
 * Generic status code for certain functions.
 */
typedef enum fmsynth_status
{
   FMSYNTH_STATUS_OK = 0, /**< Operation completed successfully. */
   FMSYNTH_STATUS_BUSY, /**< Operation could not complete due to insufficient resources at the moment. */

   FMSYNTH_STATUS_BUFFER_TOO_SMALL, /**< Provided buffer is too small. */
   FMSYNTH_STATUS_NO_NUL_TERMINATE, /**< Metadata string was not properly NUL-terminated. */
   FMSYNTH_STATUS_INVALID_FORMAT,   /**< Provided buffer does not adhere to specified format. */

   FMSYNTH_STATUS_MESSAGE_UNKNOWN,  /**< Provided MIDI message is unknown. */

   FMSYNTH_STATUS_ENSURE_INT = INT_MAX /**< Ensure the enum is sizeof(int). */
} fmsynth_status_t;

/** \addtogroup libfmsynthVersion API versioning */
/** @{ */

/**
 * Current API version of libfmsynth.
 */
#define FMSYNTH_VERSION 2

/** \brief Returns current version of libfmsynth.
 *
 * If this mismatches with what the application expects, an ABI mismatch is likely.
 * @returns Version of libfmsynth.
 */
unsigned fmsynth_get_version(void);
/** @} */

/** \addtogroup libfmsynthLifetime Lifetime */
/** @{ */
/** \brief Allocate a new instance of an FM synth.
 *
 * Must be freed later with \ref fmsynth_free.
 *
 * @param sample_rate Sample rate in Hz for the synthesizer. Cannot be changed once initialized.
 * @param max_voices The maximum number of simultaneous voices (polyphony) the synth can support. Cannot be changed once initialized.
 *
 * @returns Newly allocated instance if successful, otherwise NULL.
 */
fmsynth_t *fmsynth_new(float sample_rate, unsigned max_voices);

/** \brief Reset FM synth state to initial values.
 *
 * Resets the internal state as if the instance had just been created using \ref fmsynth_new.
 *
 * @param fm Handle to an FM synth instance.
 */
void fmsynth_reset(fmsynth_t *fm);

/** \brief Free an FM synth instance.
 *
 * @param fm Handle to an FM synth instance.
 */
void fmsynth_free(fmsynth_t *fm);
/** @} */

/** \addtogroup libfmsynthParameter Parameter and preset handling */
/** @{ */

/** \brief Set a parameter specific to an operator.
 *
 * If either parameter or operator_index is out of bounds, this functions is a no-op.
 * Updated parameters will not generally be reflected in audio output until a new voice has started.
 *
 * @param fm Handle to an FM synth instance.
 * @param parameter Which parameter to modify. See \ref fmsynth_parameter for which parameters can be used.
 * @param operator_index Which operator to modify. Valid range is 0 to \ref FMSYNTH_OPERATORS - 1.
 * @param value A floating point value. The meaning is parameter-dependent.
 */
void fmsynth_set_parameter(fmsynth_t *fm,
      unsigned parameter, unsigned operator_index, float value);

float fmsynth_get_parameter(fmsynth_t *fm,
                            unsigned parameter, unsigned operator_index);

float fmsynth_convert_from_normalized_parameter(fmsynth_t *fm,
                                                unsigned parameter,
                                                float value);

float fmsynth_convert_to_normalized_parameter(fmsynth_t *fm,
                                              unsigned parameter,
                                              float value);

/** \brief Set a parameter global to the FM synth.
 *
 * If parameter is out of bounds, this functions is a no-op.
 * Updated parameters will not generally be reflected in audio output until a new voice has started.
 *
 * @param fm Handle to an FM synth instance.
 * @param parameter Which parameter to modify. See \ref fmsynth_global_parameter for which parameters can be used.
 * @param value A floating point value. The meaning is parameter-dependent.
 */
void fmsynth_set_global_parameter(fmsynth_t *fm,
      unsigned parameter, float value);

float fmsynth_get_global_parameter(fmsynth_t *fm,
                                   unsigned parameter);

float fmsynth_convert_from_normalized_global_parameter(fmsynth_t *fm,
                                                       unsigned parameter,
                                                       float value);

float fmsynth_convert_to_normalized_global_parameter(fmsynth_t *fm,
                                                     unsigned parameter,
                                                     float value);

/**
 * Maximum string size for presets, including NUL-terminator.
 * Ensures fixed size presets.
 */
#define FMSYNTH_PRESET_STRING_SIZE 64

/**
 * Metadata structure for presets.
 * UTF-8 is assumed. API does not validate that however.
 * Strings must be properly NUL-terminated.
 */
struct fmsynth_preset_metadata
{
   char name[FMSYNTH_PRESET_STRING_SIZE]; /**< Preset name. */
   char author[FMSYNTH_PRESET_STRING_SIZE]; /**< Preset author. */
};

/** \brief Size in bytes required to hold a preset in memory.
 *
 * @returns Required size.
 */
size_t fmsynth_preset_size(void);

/** \brief Saves current preset to memory.
 *
 * The current preset state of the synth is stored to memory.
 * The preset state is portable across platforms and can be stored to disk safely.
 *
 * @param fm Handle to an FM synth interface.
 * @param metadata Pointer to metadata. Can be NULL if no metadata is desired.
 * @param buffer Pointer to buffer where preset is stored.
 * @param size Size of buffer. Must be at least \ref fmsynth_preset_size.
 *
 * @returns Error code.
 */
fmsynth_status_t fmsynth_preset_save(fmsynth_t *fm, const struct fmsynth_preset_metadata *metadata,
      void *buffer, size_t size);

/** \brief Load preset from memory.
 *
 * The current preset state of the synth is stored to memory.
 * The preset state is portable across platforms and can be stored to disk safely.
 *
 * @param fm Handle to an FM synth interface.
 * @param metadata Pointer to metadata. Can be NULL if reading metadata is not necessary.
 * @param buffer Pointer to buffer where preset can be read.
 * @param size Size of buffer. Must be at least \ref fmsynth_preset_size.
 *
 * @returns Error code.
 */
fmsynth_status_t fmsynth_preset_load(fmsynth_t *fm, struct fmsynth_preset_metadata *metadata,
      const void *buffer, size_t size);
/** @} */

/** \addtogroup libfmsynthRender Audio rendering */
/** @{ */
/** \brief Render audio to buffer
 *
 * Renders audio to left and right buffers. The rendering is additive.
 * Ensure that left and right channels are cleared to zero or contains other audio before calling this function.
 *
 * @param fm Handle to an FM synth instance.
 * @param left A pointer to buffer representing the left channel.
 * @param right A pointer to buffer representing the right channel.
 * @param frames The number of frames (left and right samples) to render.
 *
 * @returns Number of voices currently active.
 */
unsigned fmsynth_render(fmsynth_t *fm, float *left, float *right, unsigned frames);
/** @} */

/** \addtogroup libfmsynthControl MIDI control interface */
/** @{ */
/** \brief Trigger a note on the FM synth.
 *
 * @param fm Handle to an FM synth instance.
 * @param note Which note to press. Note is parsed using MIDI rules, i.e. note = 69 is A4. Valid range is [0, 127].
 * @param velocity Note velocity. Velocity is parsed using MIDI rules. valie range is [0, 127].
 *
 * @returns \ref FMSYNTH_STATUS_OK or \ref FMSYNTH_STATUS_BUSY if polyphony is exhausted.
 * */
fmsynth_status_t fmsynth_note_on(fmsynth_t *fm, uint8_t note, uint8_t velocity);

/** \brief Release a note on the FM synth.
 *
 * @param fm Handle to an FM synth instance.
 * @param note Which note to release.
 *             All currently pressed notes which match this will be put into either released state or
 *             sustained state depending on if sustain is currently held. See \ref fmsynth_set_sustain.
 */
void fmsynth_note_off(fmsynth_t *fm, uint8_t note);

/** \brief Set sustain state for FM synth.
 *
 * If sustain is held and notes are released, notes will be put in a sustain state instead.
 * Sustained notes will not be released until sustain is released as well.
 *
 * @param fm Handle to an FM synth instance.
 * @param enable If true, hold sustain. If false, release sustain, potentially releasing notes as well.
 */
void fmsynth_set_sustain(fmsynth_t *fm, bool enable);

/** \brief Set modulation wheel state.
 *
 * @param fm Handle to an FM synth instance.
 * @param wheel wheel is parsed using MIDI rules. Valid range is [0, 127]. Intial state is 0.
 */
void fmsynth_set_mod_wheel(fmsynth_t *fm, uint8_t wheel);

/** \brief Set pitch bend state.
 *
 * @param fm Handle to an FM synth instance.
 * @param value value is parsed using MIDI rules. Value range is [0, 0x3fff]. Initial state is 0x2000 (centered).
 *              Range for pitch bend is two semitones.
 */
void fmsynth_set_pitch_bend(fmsynth_t *fm, uint16_t value);

/** \brief Forcibly release all notes.
 *
 * All notes are released, even if sustain is activated.
 * Sustain is also reset to unpressed state.
 *
 * @param fm Handle to an FM synth instance.
 */
void fmsynth_release_all(fmsynth_t *fm);

/** \brief Parse single MIDI message.
 *
 * @param fm Handle to an FM synth instance.
 * @param midi_data Pointer to MIDI data. Message type must always be provided.
 *                  E.g. Successive "note on" messages cannot drop the first byte.
 *
 * @returns Status code. Depends on MIDI message type or \ref FMSYNTH_STATUS_MESSAGE_UNKNOWN if unknown MIDI message is provided.
 */
fmsynth_status_t fmsynth_parse_midi(fmsynth_t *fm,
      const uint8_t *midi_data);
/** @} */

/** @} */

#ifdef __cplusplus
}
#endif

#endif

