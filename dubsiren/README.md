# Dub Siren Demo

Simple DaisyDuino emulatino of a "dub siren".

This project require two potentiometers wired as voltage dividers, with the wiper pins connected to ADC0 and ADC1 on the Seed respectively, as well as a normally-open SPDT push button with one of the switch circuits wired between pin D14 and GND (so it closes a circuit to GND when pushed).

## Installation

Copy the entire `daisy-dubsiren` folder into your Arduino sketchbook directory.
This is usually found in `<your_system_documents_folder>/Arduino`.

Open the sketch in the Arduino IDE, either from the Sketchbook panel or using `File->Open`
and choosing the `.ino` file.

You'll need to have the [DaisyDuino](https://github.com/electro-smith/DaisyWiki/wiki/1a.-Getting-Started-(Arduino-Edition)) toolchain setup.
