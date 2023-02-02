# LDR-Based "Ambient Aurora" Project

This project uses up to 4 "light dependent resistors" (LDRs) connected to the Seed's ADC pins
to generate a pseudo-random ambient soundscape using harmonic oscillators.

[Demo Video](https://drive.google.com/file/d/1iLb81qLtIb-eksQNX8Oys_45TiklrAD5/view?usp=share_link)

## Installation

Copy the entire `synthux-2023-LDR-aurora` folder into your Arduino sketchbook directory.
This is usually found in `<your_system_documents_folder>/Arduino`.

Open the sketch in the Arduino IDE, either from the Sketchbook panel or using `File->Open`
and choosing the `.ino` file.

You'll need to have the [DaisyDuino](https://github.com/electro-smith/DaisyWiki/wiki/1a.-Getting-Started-(Arduino-Edition)) toolchain setup.

## Additional Notes

* Designed for 4 LDRs wired with 100k ohm resistors, voltage from each one being read by an ADC pin on the Seed.
    * Any resistor 47k ohm or greater should be fine, I think, but they should all be the same.
* Each LDR is mapped to a harmonic oscillator voice (oscillator + envelope).
    * The code is written for 4 LDRs but for testing I only had two, so I’m reusing the same pin definitions to “pretend” like there are 4 LDRs (each one is acting as two voices). You just have to change the pin numbers for additional ADCs for each LDR.
* Each voice plays one of 4 preset notes, randomly chosen when it retriggers. There is slewing (pitch glide) on the oscillator note so when it changes it will slide to the new note.
* The (amplitude) envelope for each voice is turned on when the normalized light level present on its corresponding LDR exceeds a threshold, and turned off when it drops below a different threshold.
    * This works like a Schmitt trigger, which helps avoid the envelope from toggling on and off really quickly if the light level is noisy and hovering around a single comparison threshold.
* Each oscillator has 8 harmonics. The levels of these harmonics are determined by random smoothed value generators (like smooth random LFOs), which modulate the higher harmonics faster than the lower ones.
* The direct normalized light amount at each LDR is also heavily slewed, and this value is used to determine how much random modulation is applied to the harmonics. The effect is that the longer and more intense the light is shining on an LDR, the more additional harmonics will emerge on that voice as the note sustains.
* Each LDR voice is panned across the stereo image, the first one hard left and the last one hard right, with the middle ones in between.
* The final dry stereo mix of voices is fed through cross-feeding stereo delay with different delay times, and a bandpass filter in the feedback loop.
