#pragma once
/*
 *  File: synth.h
 *
 *  Simple Synth using Maximilian
 *
 */

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <algorithm>

#include <arm_neon.h>

#include "unit.h"  // Note: Include common definitions for all units

#include "maximilian.h"
#include "libs/maxiPolyBLEP.h"

// Uncomment this line to use maxiEnvGen class
// #define USE_MAXIENVGEN

// Use this instead of mtof() in maxmilian to allow note number in float
inline double note2freq(float note) {
    return (440.f / 32) * powf(2, (note - 9.0) / 12);
}

enum Params {
    Note = 0,
    Waveform,
    Cutoff,
    Resonance,
    Attack,
    Decay,
    Sustain,
    Release,
    Filter,
    EnvPitchInt,
    EnvCutoffInt,
    EnvPWMInt,
    AmpAttack,
    AmpDecay,
    AmpSustain,
    AmpRelease,
    Waveform2,
    Cents,
    Semis,
    Balance,
    MixLP,
    MixBP,
    MixHP,
    MixNotch
};

class Synth {
 public:
  /*===========================================================================*/
  /* Public Data Structures/Types. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Lifecycle Methods. */
  /*===========================================================================*/

  Synth(void) {}
  ~Synth(void) {}

  inline int8_t Init(const unit_runtime_desc_t * desc) {
    // Check compatibility of samplerate with unit, for drumlogue should be 48000
    if (desc->samplerate != 48000)
      return k_unit_err_samplerate;

    // Check compatibility of frame geometry
    if (desc->output_channels != 2)  // should be stereo output
      return k_unit_err_geometry;

    maxiSettings::sampleRate = 48000;
    osc1_.setSampleRate(48000);
    osc2_.setSampleRate(48000);
#ifdef USE_MAXIENVGEN
    envelope_.setup({0, 1, 1, 1, 0, 0},
                    {1, 1, maxiEnvGen::HOLD, 1, 1},
                    {1, 1, 1, 1, 1},
                    false, true);
#endif
    // Note: if need to allocate some memory can do it here and return k_unit_err_memory if getting allocation errors

    return k_unit_err_none;
  }

  inline void Teardown() {
    // Note: cleanup and release resources if any
  }

  inline void Reset() {
    // Note: Reset synth state. I.e.: Clear filter memory, reset oscillator
    // phase etc.
    osc1_.setWaveform(maxiPolyBLEP::Waveform::SAWTOOTH);
    osc2_.setWaveform(maxiPolyBLEP::Waveform::SAWTOOTH);
    gate_ = 0;
  }

  inline void Resume() {
    // Note: Synth will resume and exit suspend state. Usually means the synth
    // was selected and the render callback will be called again
  }

  inline void Suspend() {
    // Note: Synth will enter suspend state. Usually means another synth was
    // selected and thus the render callback will not be called
  }

  /*===========================================================================*/
  /* Other Public Methods. */
  /*===========================================================================*/

  fast_inline void Render(float * out, size_t frames) {
    float * __restrict out_p = out;
    const float * out_e = out_p + (frames << 1);  // assuming stereo output

#ifdef USE_MAXIENVGEN
    const float trigger = gate_ ? 1.0 : -1.0;
#else
    const int trigger = (gate_ > 0);
#endif

    for (; out_p != out_e; out_p += 2) {
      // Envelope
#ifdef USE_MAXIENVGEN
      float env = envelope_.play(trigger);
#else
      float env = envelope_.adsr(1.0, trigger);
      float ampEnv = ampEnvelope_.adsr(1.0, trigger);
#endif
      // Oscillator
      float pitch1 = note2freq(note_ + egPitch_ * env);
      float pitch2 = note2freq(note_ + semis_ + cents_ + egPitch_ * env);
      osc1_.setPulseWidth(0.5f + egPwm_ * env);
      osc2_.setPulseWidth(0.5f + egPwm_ * env);
      float sig = (1.f - balance_) * osc1_.play(pitch1) + balance_ * osc2_.play(pitch2);
      // Filter
      double cutoff_note = note_ + cutoffOffset_ + egCutoff_ * env;
      float cutoff = min(23999., note2freq(max(0., cutoff_note)));
      filter_.setCutoff(cutoff);
      filter_.setResonance(resonance_);

      sig = filter_.play(sig * amp_, mxLp_, mxBp_, mxHp_, mxNt_);
      // Amplifier
      sig = sig * ampEnv;
      // Note: should take advantage of NEON ArmV7 instructions
      vst1_f32(out_p, vdup_n_f32(sig));
    }
  }

  inline void setParameter(uint8_t index, int32_t value) {
    p_[index] = value;
    switch (index) {
    case Note:
      note_ = value;
      break;
    case Waveform2:
      if (value == 2) {
        osc2_.setWaveform(maxiPolyBLEP::Waveform::TRIANGLE);
      } else if (value == 1) {
        osc2_.setWaveform(maxiPolyBLEP::Waveform::RECTANGLE);
      } else {
        osc2_.setWaveform(maxiPolyBLEP::Waveform::SAWTOOTH);
      }
      break;

    case Waveform:
      if (value == 2) {
        osc1_.setWaveform(maxiPolyBLEP::Waveform::TRIANGLE);
      } else if (value == 1) {
        osc1_.setWaveform(maxiPolyBLEP::Waveform::RECTANGLE);
      } else {
        osc1_.setWaveform(maxiPolyBLEP::Waveform::SAWTOOTH);
      }
      break;
    case Filter:
      if (value == 0) {
        custom_ = 0;
        mxLp_ = 1.f;
        mxHp_ = 0.f;
        mxBp_ = 0.f;
        mxNt_ = 0.f;
      } else if (value == 1) {
        custom_ = 0;
        mxLp_ = 0.f;
        mxHp_ = 1.f;
        mxBp_ = 0.f;
        mxNt_ = 0.f;
      } else if (value == 2) {
        custom_ = 0;
        mxLp_ = 0.f;
        mxHp_ = 0.f;
        mxBp_ = 1.f;
        mxNt_ = 0.f;
      } else if (value == 3) {
        custom_ = 0;
        mxLp_ = 0.f;
        mxHp_ = 0.f;
        mxBp_ = 0.f;
        mxNt_ = 1.f;
      } else {
        custom_ = 1;
        mxLp_ = cmxLp_; 
        mxHp_ = cmxHp_;
        mxBp_ = cmxBp_;
        mxNt_ = cmxNt_;
      }
      break;
    case Cutoff:
      cutoffOffset_ = 1.27f * value - 63.5; // -63.5 .. +63.5
      break;
    case Resonance:
      resonance_ = powf(2, 1.f / (1<<5) * value); // 2^(-4) .. 2^4
      break;
#ifdef USE_MAXIENVGEN
    case Attack:
      envelope_.setTime(0, value + 1);
      break;
    case Decay:
      envelope_.setTime(1, value + 1);
      break;
    case Sustain:
      envelope_.setLevel(2, 0.01 * value);
      envelope_.setLevel(3, 0.01 * value);
      break;
    case Release:
      envelope_.setTime(3, value + 1);
      break;
#else
    case Attack:
      envelope_.setAttackMS(value + 1);
      break;
    case Decay:
      envelope_.setDecay(value + 1);
      break;
    case Sustain:
      envelope_.setSustain(0.01 * value);
      break;
    case Release:
      envelope_.setRelease(value + 1);
      break;
    case AmpAttack:
      ampEnvelope_.setAttackMS(value + 1);
      break;
    case AmpDecay:
      ampEnvelope_.setDecay(value + 1);
      break;
    case AmpSustain:
      ampEnvelope_.setSustain(0.01 * value);
      break;
    case AmpRelease:
      ampEnvelope_.setRelease(value + 1);
      break;
#endif
    case EnvPitchInt:
      egPitch_ = 0.24f * value; // 24 semitones / 100%
      break;
    case EnvCutoffInt:
      egCutoff_ = 0.6 * value;  // 60 semitones / 100%
      break;
    case EnvPWMInt:
      egPwm_ = 0.0049 * value;  // 0.49 / 100%
      break;
    case Balance:
      balance_ = 0.01f * value;
      break;
    case Semis:
      semis_ = value;
      break;
    case Cents:
      cents_ = 0.01f * value;
      break;
    case MixLP:
      cmxLp_ = 0.01f * value;
      if (custom_ == 1) {
        mxLp_ = cmxLp_;
      }
      break;
    case MixHP:
      cmxHp_ = 0.01f * value;
      if (custom_ == 1) {
        mxHp_ = cmxHp_;
      }
      break;
    case MixBP:
      cmxBp_ = 0.01f * value;
      if (custom_ == 1) {
        mxBp_ = cmxBp_;
      }
      break;
    case MixNotch:
      cmxNt_ = 0.01f * value;
      if (custom_ == 1) {
        mxNt_ = cmxNt_;
      }
      break;
    default:
      break;
    }
  }

  inline int32_t getParameterValue(uint8_t index) const {
    return p_[index];
  }

  inline const char * getParameterStrValue(uint8_t index, int32_t value) const {
    (void)value;
    switch (index) {
      case Filter:
        if (value < 5) {
          return FilterStr[value];
        } else {
          return nullptr;
        }
        break;
      case Waveform2:
        if (value < 3) {
          return WaveformStr[value];
        } else {
          return nullptr;
        }
        break;
      case Waveform:
        if (value < 3) {
          return WaveformStr[value];
        } else {
          return nullptr;
        }
      // Note: String memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the string
      //       before the next call to getParameterStrValue
      default:
        break;
    }
    return nullptr;
  }

  inline const uint8_t * getParameterBmpValue(uint8_t index,
                                              int32_t value) const {
    (void)value;
    switch (index) {
      // Note: Bitmap memory must be accessible even after function returned.
      //       It can be assumed that caller will have copied or used the bitmap
      //       before the next call to getParameterBmpValue
      // Note: Not yet implemented upstream
      default:
        break;
    }
    return nullptr;
  }

  inline void NoteOn(uint8_t note, uint8_t velocity) {
    note_ = note;
    GateOn(velocity);
  }

  inline void NoteOff(uint8_t note) {
    (void)note;
    GateOff();
  }

  inline void GateOn(uint8_t velocity) {
    amp_ = 1. / 127 * velocity;
    gate_ += 1;
  }

  inline void GateOff() {
    if (gate_ > 0 ) {
      gate_ -= 1;
    }
  }

  inline void AllNoteOff() {}

  inline void PitchBend(uint16_t bend) { (void)bend; }

  inline void ChannelPressure(uint8_t pressure) { (void)pressure; }

  inline void Aftertouch(uint8_t note, uint8_t aftertouch) {
    (void)note;
    (void)aftertouch;
  }

  inline void LoadPreset(uint8_t idx) { (void)idx; }

  inline uint8_t getPresetIndex() const { return 0; }

  /*===========================================================================*/
  /* Static Members. */
  /*===========================================================================*/

  static inline const char * getPresetName(uint8_t idx) {
    (void)idx;
    // Note: String memory must be accessible even after function returned.
    //       It can be assumed that caller will have copied or used the string
    //       before the next call to getPresetName
    return nullptr;
  }

 private:
  /*===========================================================================*/
  /* Private Member Variables. */
  /*===========================================================================*/

  std::atomic_uint_fast32_t flags_;

  int32_t p_[24];
  maxiPolyBLEP osc1_;
  maxiPolyBLEP osc2_;
  // maxiBiquad filter_;
  maxiSVF filter_;
#ifdef USE_MAXIENVGEN
  maxiEnvGen envelope_;
#else
  maxiEnv envelope_;
  maxiEnv ampEnvelope_;
#endif

  int32_t note_;
  float amp_;
  uint32_t gate_;
  uint32_t custom_; // 0 = fixed filter, 1 = custom
  float cutoffOffset_;
  float resonance_;

  float mxLp_;
  float mxBp_;
  float mxHp_;
  float mxNt_;

  float cmxLp_;
  float cmxBp_;
  float cmxHp_;
  float cmxNt_;

  float egPitch_;
  float egCutoff_;
  float egPwm_;

  float balance_; // 0 = osc1 only, 1= osc2 only
  float semis_;
  float cents_;
  /*===========================================================================*/
  /* Private Methods. */
  /*===========================================================================*/

  /*===========================================================================*/
  /* Constants. */
  /*===========================================================================*/
  const char *WaveformStr[3] = {
    "Saw",
    "Sqr",
    "Tri"
  };

  const char *FilterStr[5] = {
    "Low",
    "High",
    "Band",
    "Notch",
    "Custom"
  };
};
