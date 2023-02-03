// Title: dubsiren
// Description: Two knobs + button to emulate a classic dubsiren 
//              sound, with filtered delay
// Hardware: Daisy Seed
// Author: Nick Donaldson

#include "DaisyDuino.h"

// Pin number definition constants
static const int PITCH_KNOB_PIN = A0;
static const int ENV_KNOB_PIN   = A1;
static const int BTN_IN_PIN     = D14;
static const int LED_OUT_PIN    = D13;

static DaisyHardware hw;
static VariableShapeOscillator osc;
static Adsr env;
static Svf filt;

// This class is a C++ template which must be parameterized with
// a type (float) and a maximum duration in samples (24000 = 0.5 seconds @ 48kHz).
// The greater the max delay, the more memory is used. If this
// becomes too big it will not fit in standard SRAM anymore and
// we will need to move it into SDRAM.
static DelayLine<float, 24000> delayline;

size_t num_channels;
bool gate_on = false;
float pitch_nn, env_time;

void MyCallback(float **in, float **out, size_t size) {
  // Set the attack and release time for the envelope.
  // This is not changing on every sample so we only need to do this
  // once at the start of the callback.
  env.SetAttackTime(env_time);
  env.SetReleaseTime(env_time);

  for (size_t i = 0; i < size; i++) {
    // Process the envelope to get a new sample, passing
    // it the variable for whether the gate is on (button is held down)
    float env_level = env.Process(gate_on);

    // Set the pulse width of the oscillator based on the level of the envelope.
    // As the envelope increases, the pulse width decreases from 0.4 to 0.2.
    osc.SetPW(0.4f - env_level * 0.2f);

    // Set the frequency of the oscillator by converting the pitch knob
    // reading from MIDI note number into frequench in Hz. We also add
    // the envelope level scaled by 24 semitones (2 octaves) so the pitch
    // increases by two octaves based on the envelope level, regardless of
    // its starting point.
    osc.SetSyncFreq(mtof(pitch_nn + env_level * 24.0f));
    
    // Set the LED on or off based on whether the envelope is active or zero
    digitalWrite(LED_OUT_PIN, env.IsRunning() ? HIGH : LOW);
    
    // Process the oscillator and read a sample out of the delay line
    float dry = osc.Process();
    float wet = delayline.Read();

    // Process the delayed sample through the filter
    filt.Process(wet);
  
    // Write the new input plus the filtered delayed sample (feedback) 
    // into the delay line
    delayline.Write(dry + filt.Low() * 0.66f);

    // Calculate the final signal mix and copy into the output buffer
    float sig = (dry + 0.5f * wet) * 0.5f;
    for (size_t chn = 0; chn < num_channels; chn++) {
      out[chn][i] = sig;
    }
  }
}

void setup() {
  float sample_rate;

  // Setup the button input pin with an internal pullup resistor
  pinMode(BTN_IN_PIN, INPUT_PULLUP);
  // Setup the LED pin as an output
  pinMode(LED_OUT_PIN, OUTPUT);

  // Initialize for Daisy pod at 48kHz
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  num_channels = hw.num_channels;
  sample_rate = DAISY.get_samplerate();

  osc.Init(sample_rate);
  osc.SetSyncFreq(440);
  osc.SetWaveshape(1.0f);
  osc.SetPW(0.4f);

  env.Init(sample_rate);
  env.SetAttackTime(0.5);
  env.SetSustainLevel(1.0);
  env.SetReleaseTime(0.5);

  delayline.Init();
  delayline.SetDelay(12000.0f);

  filt.Init(sample_rate);
  filt.SetDrive(0.5f);
  filt.SetRes(0.1f);
  filt.SetFreq(6000.0f);

  DAISY.begin(MyCallback);
}

void loop() { 
  // Read the pitch knob ADC and scale it so it sweeps from midi note numbers
  // 24 to 84. This is a convenient way to convert from a linear scale (note 
  // numbers which represent 12-tet semitones) to an exponential scale (raw 
  // frequency in Hz) - by using the `mtof` function in the audio callback.
  pitch_nn = 24.0f + ((analogRead(PITCH_KNOB_PIN) / 1023.0f) * 60.0f);

  // Read the envelope time knob ADC and scale it using `fmap` from 0.1 seconds
  // to 3 seconds, with an exponential curve. Exponential curves tend to feel a
  // little more "natural" for timing parameters since it allows to have more 
  // granularity for the smaller end of the time range.
  env_time = fmap(analogRead(ENV_KNOB_PIN) / 1023.0f, 0.1f, 3.0f, Mapping::EXP);

  // Read the button pin to set if the envelope gate is on or not.
  // Note:
  //   - This does not handle debouncing
  //   - We need to invert the read since the GPIO input has a pullup resistor,
  //     hence gate_on == true if the pin reading is zero (low)
  gate_on = digitalRead(BTN_IN_PIN) == 0;
}
