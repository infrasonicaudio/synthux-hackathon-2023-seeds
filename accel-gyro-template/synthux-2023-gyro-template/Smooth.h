#ifndef DSY_SMOOTH_H
#define DSY_SMOOTH_H

#include <DaisyDuino.h>

/// Utility class for exponentially smoothing (slewing) a float value.
class SmoothedValue {
 
  public:
    SmoothedValue() {};
    ~SmoothedValue() {}

    /// Initialize the slew with the audio sample rate
    void Init(float sample_rate) {
      sr_    = sample_rate;
      input_ = 0.0f;
      value_ = 0.0f;
      SetSlewMs(100.0f);
    }

    /// Set the slew time in milliesconds.
    /// Defaults to 100ms
    void SetSlewMs(float slew_ms) {
      const float time_s = slew_ms / 1000.0f;
      if (time_s <= 0.0f || sr_ <= 0.0f) { 
        coef_ = 1.0f;
        return;
      }
      coef_ = daisysp::fmin(1.0f / (time_s * sr_ * 0.1447597f), 1.0f);      
    }

    /// Generate and return a new slewed output value.
    /// The output will track the most recent value passed to Set()
    /// with slew/lag applied.
    float Process() {
      fonepole(value_, input_, coef_);
      return value_;
    }    
  
    /// Set the target value.
    ///
    /// If we pass a second argument as `true` the tracked value will immediately 
    /// jump to the input without slew and he next Process() call will return the value
    /// in the first argument.
    void Set(float input, bool immediately = false) {
      input_ = input;
      if (immediately) {
        value_ = input;
      }
    }
    
    /// Return the most recent output value without processing the slew again.
    float Get() const {
      return value_;      
    }

  private:
    float sr_;
    float coef_;
    float input_;
    float value_;
};

#endif