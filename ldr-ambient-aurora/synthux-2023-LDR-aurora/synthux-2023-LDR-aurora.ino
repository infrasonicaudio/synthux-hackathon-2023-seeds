// Title: SynthUX Hackathon 2023 - LDR Aurora Borealis Template
// Description: Template project with sound design for LDR-based aurora borealis light sculpture project
// Hardware: Daisy Seed
// Author: Nick Donaldson

#include <DaisyDuino.h>
// This are utility classes, the files have to be in the same directory
// as the main .ino sketch file.
#include "Smooth.h"
#include "Panner.h"

// ===============================
// GLOBAL DEFINES AND VARIABLES
// ===============================

// Change this to 0 to disable serial logging
#define LOGGING_ENABLED 1 

enum Channel {
  LEFT = 0,
  RIGHT
};

// Number of LDRs in the circuit.
// All of the code and constant definitions below can easily
// scale for more LDRs, see comments for notes.
static const size_t NUM_LDRS = 4;

// These are the pin numbers to use for the LDRs.
// NOTE: Currently I'm faking 4 LDRs by having each one trigger two voices,
// but these can be different pin numbers to add more actual LDRs
static const int LDR_PINS[NUM_LDRS] = {A0, A1, A0, A1};

// Threshold for LDR minimum. This should be slightly above the ambient light reading.
// Check the serial monitor to see what kind of raw numbers you're getting with no direct light
// shining on the LDRs, and set this to something that's a little bit above all of them.
// The higher this is, the less resolution we will get out of the LDR for controlling the synth,
// so darker rooms tend to be better. Should be a number between 0 and 1, but ideally the lower the better.
static constexpr float LDR_AMBIENT_THRESH = 0.67f;

// Threshold for the LDR detecting direct light. This turns the voice gates/envelopes "on"
// when the LDR reading exceeds this value *after being scaled to 0-1 for ambient compensation*
// Both should be a number betweeen 0 and 1, but the OFF thresh should be less than the ON.
static constexpr float LDR_GATE_ON_THRESH  = 0.7f;
static constexpr float LDR_GATE_OFF_THRESH = 0.35f;

// These are the possible MIDI note number values for the oscillator associated with each
// LDR. To add more, add a new row with 3 notes. Every time the oscillator associated with
// the voice goes on, it will randomly choose a new note from its row.
static const size_t NUM_POSSIBLE_NOTES = 4;
static const int NOTE_TABLE[NUM_LDRS][NUM_POSSIBLE_NOTES] = 
{
  // -- Based around GMaj7 --
  // G1,   G2    D3    E3     << Voice 1
  {  31,   43,   50,   52 },
  // A3    B3    D4,   F#4    << Voice 2
  {  57,   59,   62,   66 },
  // G4    A5    B5,   D5     << Voice 3
  {  67,   69,   71,   74 },
  // F#5   G5,   A6    B6       << Voice 4
  {  78,   79,   81,   83},
};

// Time variables used to determine if enough time has passed to log a sensor reading.
// We can't log data as fast as possible or the system will lock up.
static const uint32_t SENSOR_LOG_INTERVAL = 250;  // milliseconds (250ms = 4x per second)
static uint32_t       last_sensor_log_at  = 0;   // milliseconds

// This object allows us to configure the Daisy Seed hardware.
static DaisyHardware hw;

// Whether the LDR has enough light to be considered "on"
static bool gate_on[NUM_LDRS] = {false};

// SmoothedValue objects for significantly slewing the light amount readings
static SmoothedValue smoothed_light_amt[NUM_LDRS];

// SmoothedValue objects for pitch slew when oscillators change notes
static SmoothedValue smoothed_pitch[NUM_LDRS];

// You can change the number of harmonics in the oscillators here
static const int NUM_HARMONICS = 8;
static HarmonicOscillator<NUM_HARMONICS> oscillators[NUM_LDRS];

// Envelopes for the voice amplitude
static Adsr envelopes[NUM_LDRS];

// Panner per voice for stereo spread
static Panner panner[NUM_LDRS];

// Random generators for voice harmonics, maximum levels scaled by the smoothed light amount.
// There will be one for each harmonic of each voice minus the fundamental, 
// e.g. 4 voices * 6 harmonics = 4 * (6-1) = 20 total random generators.
// These are pretty cheap in terms of CPU so that *should* be fine.
static SmoothRandomGenerator harmonic_rng[NUM_LDRS][NUM_HARMONICS-1];

// Stereo delay lines long enough for 60s of delay at 48kHz (way more than we probably need)
static DelayLine<float, 2880000> DSY_SDRAM_BSS delay_line[2];

// Filters for delay stereo crossfeed
static Svf delay_filt[2];

// ===============================
// ARDUINO SETUP() AND LOOP()
// ===============================

void setup() {
  float sample_rate;

  // Initialize hardware for Daisy Seed at 48kHz sample rate
  hw = DAISY.init(DAISY_SEED, AUDIO_SR_48K);
  sample_rate = DAISY.get_samplerate();

  // Initialize serial interface for debugging
  #if LOGGING_ENABLED
  Serial.begin(115200);  
  delay(500);
  Serial.println("Daisy Seed STARTED");
  #endif

  // --- Initialize LDR ADC pins ---
  analogReadResolution(12); // 12-bit instead of 10-bit readings
  for (size_t i = 0; i < NUM_LDRS; i++) {
    pinMode(LDR_PINS[i], INPUT);
  }
  
  // --- Initialize DSP Code ---
  for (size_t i = 0; i < NUM_LDRS; i++) {
    oscillators[i].Init(sample_rate);

    envelopes[i].Init(sample_rate);
    envelopes[i].SetAttackTime(20.0f);
    envelopes[i].SetDecayTime(8.0f);
    envelopes[i].SetSustainLevel(0.2f);
    envelopes[i].SetReleaseTime(5.0f);

    panner[i].Init();
    panner[i].SetPan(-1.0f + (i / ((float)NUM_LDRS - 1.0f)) * 2.0f); // even spread across stereo space

    smoothed_light_amt[i].Init(sample_rate);
    smoothed_light_amt[i].SetSlewMs(10000.0f); // 10s of slew, takes a long time to build up and fall

    smoothed_pitch[i].Init(sample_rate);
    smoothed_pitch[i].SetSlewMs(3000.0f); // 3 second of glide to new pitch
    smoothed_pitch[i].Set((float)NOTE_TABLE[i][0], true); // initialize with starting notes

    // Initialize harmonic random value generators for each harmonic in each voice
    for (size_t h = 0; h < NUM_HARMONICS-1; h++) {
      harmonic_rng[i][h].Init(sample_rate);
      harmonic_rng[i][h].SetFreq(0.1f + h * 0.1f); // higher harmonics modulate faster
    }
  }

  // Initialize delay lines and crossfeed filters
  for (size_t c = 0; c < 2; c++) {
    delay_line[c].Init();
    delay_filt[c].Init(sample_rate);
    delay_filt[c].SetFreq(1200.0f);
    delay_filt[c].SetRes(0.1f);
    delay_filt[c].SetDrive(0.25f);
  }

  // Staggered delay times will create interesting cross feed patterns
  delay_line[LEFT].SetDelay(sample_rate * 7.0f);  // 7 seconds
  delay_line[RIGHT].SetDelay(sample_rate * 5.6f); // 5.6 seconds

  // Start Audio
  DAISY.begin(AudioCallback);
}

void loop() {

  float ldr_raw[NUM_LDRS];
  float ldr_scaled[NUM_LDRS];

  for (size_t i = 0; i < NUM_LDRS; i++) {
    // locally store raw and scaled LDR readings
    ldr_raw[i]    = readLDR(i);
    ldr_scaled[i] = daisysp::fmax(ldr_raw[i] - LDR_AMBIENT_THRESH, 0.0f) / (1.0f - LDR_AMBIENT_THRESH);

    // Set scaled amount for smoothing
    smoothed_light_amt[i].Set(ldr_scaled[i]);

    // This works like a Schmitt trigger. The light amount must exceed a threshold
    // for the voice to go on, but it must drop below a lower threshold to turn it
    // off again. This is to avoid retriggering the envelopes constantly if the light
    // hovers around the threshold and jumps above and below it a lot.
    bool prev_on  = gate_on[i];
    if (prev_on) {
      if (ldr_scaled[i] < LDR_GATE_OFF_THRESH) {
        gate_on[i] = false;
      }
    } else {
      if (ldr_scaled[i] > LDR_GATE_ON_THRESH) {
        // update to a random pitch from the lookup table if the gate is going from off to on
        int new_nn = NOTE_TABLE[i][(uint)rand() % NUM_POSSIBLE_NOTES];
        smoothed_pitch[i].Set((float)new_nn);
        gate_on[i] = true;
      }
    }
  }


  // Log the raw data readings periodically for debugging
  #if LOGGING_ENABLED
  // If it's been at least SENSOR_LOG_INTERVAL milliseconds since our last reading, we can
  // log this reading. We can't log too quickly or it will overwhelm the system.
  uint32_t now = millis();
  if (now - last_sensor_log_at > SENSOR_LOG_INTERVAL) {
    for (size_t i = 0; i < NUM_LDRS; i++) {
      Serial.print("[LDR");
      Serial.print(i);
      Serial.print("] Raw: ");
      Serial.print(ldr_raw[i], 2);
      Serial.print("  ");
      Serial.print("Scaled: ");
      Serial.print(ldr_scaled[i], 2);
      Serial.print("  ");
    }
    Serial.println();
    last_sensor_log_at = now;
  }
  #endif
}

// ===============================
// AUDIO CALLBACK
// ===============================

/// The audio callback is where we process/synthesize the audio
void AudioCallback(float **in, float **out, size_t size) {
  
  for (size_t i = 0; i < size; i++) {
    float voice_mix[2] = {0.0f, 0.0f};

    // Loop through voices
    for (size_t v = 0; v < NUM_LDRS; v++) {
      // First update the smoothed values for pitch and harmonic scaling
      float light_amt  = smoothed_light_amt[v].Process();
      float pitch_nn   = smoothed_pitch[v].Process();

      // Update the oscillator freq for this voice
      oscillators[v].SetFreq(mtof(pitch_nn));

      // Randomize haromincs scaled by light amount for each harmonic in each voice.
      // The longer the light is present on a voice, the more each harmonic will modulate.
      for (size_t h = 0; h < NUM_HARMONICS-1; h++) {
        float hamt = harmonic_rng[v][h].Process() * light_amt;
        hamt *= hamt * hamt; // cubic scaling
        oscillators[v].SetSingleAmp(hamt, h+1); // skip the first harmonic (h+1) - it should always be on
      }

      // Get an envelope sample
      float env = envelopes[v].Process(gate_on[v]);

      // VCA = oscillator * envelope * attenuation
      const float voice_gain = 0.25f;
      float mono_samp = oscillators[v].Process() * env * voice_gain;
      
      // Pan the voice across the stereo image
      float l_samp, r_samp;
      panner[v].Process(mono_samp, &l_samp, &r_samp);

      // Sum into voice mix
      voice_mix[LEFT] += l_samp;
      voice_mix[RIGHT] += r_samp;
    }

    // Process mix through crossfed stereo delay
    const float delay_wet_lvl = 0.3f;
    const float delay_fb      = 0.5f;
    
    float delay_out_l = delay_line[LEFT].Read();
    float delay_out_r = delay_line[RIGHT].Read();

    delay_filt[LEFT].Process(delay_out_l);
    delay_filt[RIGHT].Process(delay_out_r);

    delay_out_l = delay_filt[LEFT].Band();
    delay_out_r = delay_filt[RIGHT].Band();

    // cross-feed the feedback, band pass filter in the loop
    delay_line[LEFT].Write(voice_mix[LEFT] + delay_out_r * delay_fb);
    delay_line[RIGHT].Write(voice_mix[RIGHT] + delay_out_l * delay_fb);

    // Copy to outputs with soft clip applied in case mix gets too loud.
    // Can change the voice gain in the loop above or the number here to
    // reduce pre-clip level or post-clip (final output) volume
    out[LEFT][i] = SoftClip(voice_mix[LEFT] + delay_out_l * delay_wet_lvl) * 0.5f;
    out[RIGHT][i] = SoftClip(voice_mix[RIGHT] + delay_out_r * delay_wet_lvl) * 0.5f;
  }
}

// ===============================
// HELPER FUNCTIONS
// ===============================

float readLDR(int index) {
  // Dividing by 4093 instead of 1023 since we setup
  // our analog read resolution for 12-bit
  return analogRead(LDR_PINS[index]) / 4093.0f;
}