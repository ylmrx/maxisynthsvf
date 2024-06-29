# maxisynthsvf: logue SDK 2.0 not so basic Synth unit using Maximilian library

This repository shows how to use the Maximilian library to develop a synth unit on the KORG logue SDK v2 for drumlogue.

Since logue SDK v2 currently provides only the APIs to communicate with the drumlogue, you must develop all components, such as oscillators, filters, and envelopes, to build your synth unit. You can reduce many tasks to implement these components with the Maximilian library.

This forks boochow' excellent work from maxisynth

## Changes

- Uses two oscillators (with detune and transpose)
- Adds a dedicated Amplitude Envelope
- Switch the biquad filter to the SVF filter
- The SVF can be set to bandpass/highpass/lowpass/notch/custom
- The `custom` filter mode let you set a mix of the four filters states

## Notes

This is my first attempt at such project, it's working fine for
me but glitches and bugs might happen which, within a synth engine
can be loud noise and such, which might break your ears/equipments.

Just be careful and rehearse accordingly if you're taking this
on stage or in the booth.

## TODO

- I'm not happy with the way the filter cutoff scrolls
- I'll need much bigger release envelopes at a later stage

> This means the presets/motions will break across versions!!!

## How to build

Since Maximilian currently has some minor issues to use with logue SDK, use the patched fork of Maximilian, which I am providing at:

https://github.com/boochow/Maximilian

Place these repositories under `logue-sdk/platforms/drumlogue/` like this:

```
drumlogue/
├── common
├── Maximilian
└── maxisynth
```

then type

`../../docker/run_cmd.sh build drumlogue/maxisynth`

and copy `maxisynth/maxisynth.drmlgunit` to your drumlogue.

The `maxisynth` repo has three branches:

* `main` : a complete synthesizer unit with one oscillator, one low-pass filter, one envelope generator, and  12 parameters

* `band-limited-osc` : a minimum synthesizer unit with one sawtooth oscillator and Note parameter

* `maximilian-synth-template` : a modified version of `dummy-synth` as a starting point to develop a new synth unit with Maximilian
