# libfmsynth

libfmsynth is a C library which implements an FM synthesizer.
Unlike most FM synth implementations in software, this FM synthesizer does not aim to emulate or replicate a particular synth (like DX7) or FM chip.

The synth was designed primarily to be used as an instrument for my own purposes, hooked up with MIDI to my electric piano.
It was also designed to be potentially useful as a synth backend in other projects.

The synth core supports:

  - Arbitrary amounts of polyphony
  - 8 operators
  - No fixed "algorithms"
  - Arbitrary modulation, every operator can modulate any other operator, even itself
  - Arbitrary carrier selection, every operator can be a carrier
  - Sine LFO, separate LFO per voice, modulates amplitude and frequency of operators
  - Envelope per operator
  - Carrier stereo panning
  - Velocity sensitivity per operator
  - Mod wheel sensitivity per operator
  - Pitch bend
  - Keyboard scaling
  - Sustain, sustained keys can overlap each other for a very rich sound
  - Full floating point implementation optimized for SIMD
  - Hard real-time constraints

In addition, support for some useful auxillary features are implemented:

  - Linux LV2 plugin implementation with simple GTKmm GUI
  - Patch load/save as well as direct parameter control through API
  - MIDI messages API as well as direct control of the synth with key on/off, etc

## Sample sounds/presets

Some of the sounds you can create using this synth can be heard on my [SoundCloud](https://soundcloud.com/zoned-music/sets/fm-synth) FM synth playlist.

The sounds do have some effects like delay, reverb and chorus applied to them.

Some presets can be found in `presets/` directory.
Their raw data can be passed directly to libfmsynths preset API.

## License

libfmsynth is licensed under the permissive MIT license.

## Documentation

The public libfmsynth API is documented with doxygen. Run `doxygen` or `make docs` to generate documentation.
Doxygen 1.8.3 is required.

After running Doxygen, documents are found in `docs/`.

## Optimizations

libfmsynth is currently optimized for

  - SSE (intrinsics)
  - SSE 4.1 (intrinsics)
  - AVX (intrinsics)
  - ARMv7 NEON (intrinsics and hand coded assembly)
  - ARMv8 NEON (intrinsics, untested)

The SIMD implementations are typically 3-6x faster than the equivalent -Ofast optimized C implementation, even with autovectorization enabled.

## Performance

IPC was measured with `perf` on Linux.

### SSE

At 44.1 kHz, a single core of a 2.66 GHz Core i7 920 can do 750 voice polyphony when fully saturated with SSE 4.1 path.
Throughput is 34.5 Msamples / s (approx. 11 GFlops).

#### IPC
1.91 instructions per cycle.

### AVX

The 256-bit vector AVX implementation is roughly 10-15 % faster than SSE (tested on a Sandy Bridge laptop). The reason it's just 10-15 % is because the dependencies between stages in the synth and latency in floating point processing ensure that the execution pipes cannot be fully saturated with useful work.

#### IPC
1.00 instructions per cycle.

### NEON

At 44.1 kHz, a single core of a 1.7 GHz Cortex-A15 can do 300 voice polyphony when fully saturated with NEON.
Throughput is 13.1 Msamples / s (approx. 4.75 GFlops).

#### IPC
1.00 instructions per cycle. From benchmarking, it does not seem to be possible to get more than one FP32x4 instruction per cycle on NEON.

## Signal path

For a voice of polyphony, LFOs and envelopes are updated every 32nd sample.
Between LFO and envelope updates, a tight loop runs unless it has to exit early due to MIDI updates.
Per sample:

  - Compute sine for 8 operators w/ 4th order Taylor approximation.
  - Multiply in envelope, LFO, etc.
  - Increment envelope (linear ramp).
  - Apply 8-by-8 modulator matrix to result of the sine computation (8-by-8 matrix-vector multiply).
  - Accumulate modulation result and base frequencies to operator phases (proper FM, phase integration).
  - Mix carrier outputs to left and right channels.
  - Wrap around oscillator phase for stability.

## Building and installing

To build, run `make` to build the static library `libfmsynth.a`. The static library has `-fPIC` enabled, to allow linking into a shared library. To build a benchmark/test app, run `make test`. The main purpose of this tool is to benchmark and validate that outputs for C and SIMD paths are adequately similar and that performance is as expected.

To cross-compile, use `TOOLCHAIN_PREFIX`, e.g. cross-compiling to ARMv7:

    make TOOLCHAIN_PREFIX=arm-linux-gnueabihf- ARCH=armv7 TUNE=cortex-a15
    
If `TUNE=` is not set to something, `-march=native` will be assumed.
Note that binaries built with `-march=native` will enable code paths which might not be supported by other processors, especially on x86 if SSE 4.1 or AVX is enabled.

To install library and header, use `make install PREFIX=$YOUR_PREFIX`.

### Building LV2 plugin

To build libfmsynth as an LV2 plugin you will need:

 - lvtk
 - GTKmm 2
 - Fairly recent C++11 compiler

Run:

    cd lv2
    make
    sudo make install
    
The plugin will be installed to `/usr/lib/lv2/`.
Presets in `presets/` are installed to the bundle as well. When attempting to load presets in the UI, one of the shortcuts will point to the LV2 bundle for easy access.
