
# SDI-12 for Arduino<!-- {#mainpage} -->

## Introduction<!-- {#mainpage_intro} -->

This is an Arduino library for SDI-12 communication with a wide variety of environmental sensors.
It provides a general software solution, without requiring any additional hardware, to implement the SDI-12 communication protocol between an Arduino-based data logger and SDI-12-enabled sensors.

[SDI-12](http://www.sdi-12.org/) is an asynchronous, ASCII, serial communications protocol that was developed for intelligent sensory instruments that typically monitor environmental data.
[Advantages of SDI-12](http://en.wikipedia.org/wiki/SDI-12) include the ability to use a single available data channel for many sensors.

This work is motivated by the [EnviroDIY community](http://envirodiy.org/) vision to create an open source hardware and software stack to deliver near real time environmental data from wireless sensor networks, such as the Arduino-compatible [EnviroDIY™ Mayfly Data Logger](http://envirodiy.org/mayfly/).

[//]: # ( @cond GitHub )

## Documentation

Extensive documentation on the SDI-12 functions and classes is available here:  <https://envirodiy.github.io/Arduino-SDI-12/index.html>

[//]: # ( @endcond )

### Renaming Notice<!-- {#mainpage_rename} -->

> [!IMPORTANT]
> **As of version 2.0.0 this library was renamed from "Arduino-SDI-12" to simply "SDI-12" to comply with requirements for inclusion in the Arduino.cc's IDE and Library Manager.**

[//]: # ( @tableofcontents )

[//]: # ( @cond GitHub )

- [SDI-12 for Arduino](#sdi-12-for-arduino)
  - [Introduction](#introduction)
  - [Documentation](#documentation)
    - [Renaming Notice](#renaming-notice)
  - [Getting Started](#getting-started)
  - [Origins and Inherited Limitations](#origins-and-inherited-limitations)
  - [Supported Boards](#supported-boards)
    - [Tested Boards](#tested-boards)
    - [Untested Boards](#untested-boards)
    - [Unsupported Boards](#unsupported-boards)
    - [Usable Pins](#usable-pins)
  - [Variants and Branches](#variants-and-branches)
    - [EnviroDIY\_SDI12](#envirodiy_sdi12)
    - [EnviroDIY\_SDI12\_PCINT3](#envirodiy_sdi12_pcint3)
  - [Contribute](#contribute)
  - [License](#license)
  - [Credits](#credits)

[//]: # ( @endcond )

## Getting Started<!-- {#mainpage_getting_started} -->

Learn more, below, about this library's:

- [Origins and Inherited Limitations](https://github.com/EnviroDIY/Arduino-SDI-12#origins-and-inherited-limitations);
- [Compatibility Considerations](https://github.com/EnviroDIY/Arduino-SDI-12#compatibility-considerations);
- [Variants and Branches](https://github.com/EnviroDIY/Arduino-SDI-12#variants-and-branches) we created to overcome some limitations.

Try running our [Example sketches](https://github.com/EnviroDIY/Arduino-SDI-12/tree/master/examples) with your Arduino board and SDI-12 sensor.

Full details on the library functionality can be found on github pages: <https://envirodiy.github.io/Arduino-SDI-12/>

## Origins and Inherited Limitations<!-- {#mainpage_origins} -->

This library was developed from the [SoftwareSerial](https://github.com/arduino/Arduino/tree/master/hardware/arduino/avr/libraries/SoftwareSerial) library that is a built-in [standard Arduino library](https://www.arduino.cc/en/Reference/Libraries).
It was further modified to use a timer to improve read stability and decrease the amount of time universal interrupts are disabled using logic from [NeoSWSerial](https://github.com/SlashDevin/NeoSWSerial).
In August of 2024, the [EnableInterrupt](https://github.com/GreyGnome/EnableInterrupt) library was made a required dependency for this library for AVR boards.

The most obvious "limitation" is that this library will conflict with all other libraries that make use of pin change interrupts on **AVR** boards.
You will be unable to compile them together.
Some other libraries using pin change interrupts include [SoftwareSerial](https://github.com/arduino/Arduino/tree/master/hardware/arduino/avr/libraries/SoftwareSerial), [NeoSWSerial](https://github.com/SlashDevin/NeoSWSerial), [PinChangeInt](https://playground.arduino.cc/Main/PinChangeInt), [Servo](https://www.arduino.cc/en/Reference/Servo), and quite a number of other libraries.
See the notes under [Variants and Branches](https://github.com/EnviroDIY/Arduino-SDI-12#variants-and-branches) below for advice in using this library in combination with such libraries.

The one exception for pin change libraries is [EnableInterrupt](https://github.com/GreyGnome/EnableInterrupt), which this library depends on.

Another non-trivial, but hidden limitation is that _all_ interrupts are disabled during most of the transmission of each character, which can interfere with other processes.
That includes other pin-change interrupts, clock/timer interrupts, external interrupts, and every other type of processor interrupt.
This is particularly problematic for SDI-12, because SDI-12 operates at a very slow baud rate (only 1200 baud).
This translates to ~8.3 mS of "radio silence" from the processor for each character that goes out via SDI-12, which adds up to ~380-810ms per command!  Interrupts are enabled for the majority of the time while the processor is listening for responses.

For most AVR boards, this library will also conflict with the [tone](https://www.arduino.cc/reference/en/language/functions/advanced-io/tone/) function because of its utilization of timer 2.
There will be no obvious compile error, but because SDI-12 and the tone library may use different clock-prescaler functions, the results for both might be rather unexpected.
All 8MHz AVR boards will also have unresolvable prescaler conflicts with [NeoSWSerial](https://github.com/SlashDevin/NeoSWSerial).
The pre-scaler values needed for the SDI-12 functionality are set in the begin() function and reset to their original values in the end() function.

## Supported Boards<!-- {#mainpage_supported} -->

### Tested Boards

This library has been tested and verified for:

- Arduino Uno (AtMega328p)
- EnviroDIY Mayfly (AtMega1284p)
- Adafruit Feather 32u4 (AtMega32u4, identical to Arduino Leonardo)
- Seeeduino Mega (AtMega 12850)
- Adafruit Feather M0 (SAMD21G18, identical to Arduino Zero)EnviroDIY Stonefly (SAMD51N19A)
- Adafruit Feather M0 (SAMD51J19A)
- Adafruit Feather Huzzah (ESP12S/ESP8266)
- Adafruit Feather Huzzah32 (ESP-WROOM-32/ESP32)

### Untested Boards

Other untested boards that _should_ work:

- Arduino Mega (AtMega2560)
- Adafruit Gemma/AtTiny
- Most other AVR processors
- Other flavors of ESP8266 and ESP32
- Arduino framework boards with clock speeds over ~48MHz

### Unsupported Boards

These boards are currently unsupported.
If you are interested in adding support for those boards, please send pull requests.

- Arduino Due
- Arduino 101
- Teensy boards with speeds <48Mhz

### Usable Pins

Due to the use of pin change interrupts, not all data pins are available for use with this SDI-12 library.
Pin availability depends on the micro-controller.

For AVR boards, the ["Bestiary" of the EnableInterrupt library](https://github.com/GreyGnome/EnableInterrupt?tab=readme-ov-file#pin--port-bestiary) is one of the best lists of usable pins.

**AtMega328p / Arduino Uno:**

- Any pin

**AtMega1284p / EnviroDIY Mayfly**

- Any pin

**ATmega2560 / Arduino Mega or Mega 2560:**

- 0, 11, 12, 13, 14, 15, 50, 51, 52, 53, A8 (62), A9 (63), A10 (64), A11 (65), A12 (66), A13 (67), A14 (68), A15 (69)

**AtMega32u4 / Arduino Leonardo or Adafruit Feather 32u4:**

- 8, 9, 10, 11, 14 (MISO), 15 (SCK), 16 (MOSI)

**SAMD21xxx / Arduino Zero:**

- Any pin (except 4 on the zero)

**SAMD51xxx / Arduino Zero:**

- Any pin

**ESP8266:**

- Any GPIO, except GPIO16

**ESP32:**

- Any GPIO

Note that not all of these pins are available with our [Variants and Branches](https://github.com/EnviroDIY/Arduino-SDI-12#variants-and-branches), below.

## Variants and Branches<!-- {#mainpage_variants} -->

As we've described, the default "master" branch of this library will conflict with SoftwareSerial and any other library that monopolizes all pin change interrupt vectors for all AVR boards.
To allow simultaneous use of SDI-12 and SoftwareSerial, we have created additional variants of these libraries that we maintain as separate branches of this repository.
For background information, my be helpful to read our [Overview of Interrupts](https://github.com/EnviroDIY/Arduino-SDI-12/wiki/2b.-Overview-of-Interrupts) wiki page or this [Arduino Pin Change Interrupts article](https://thewanderingengineer.com/2014/08/11/arduino-pin-change-interrupts/).

### EnviroDIY_SDI12<!-- {#mainpage_master} -->

EnviroDIY_SDI12 is the default master branch of this repository.
It controls and monopolizes all pin change interrupt vectors, and can therefore have conflicts with any variant of SoftwareSerial and other libraries that use interrupts.

### EnviroDIY_SDI12_PCINT3<!-- {#mainpage_pcint3} -->

EnviroDIY_SDI12_PCINT3 is in the Mayfly branch of this repository, and was historically was called "SDI12_mod".
It's been cropped to only control interrupt vector 3, or PCINT3 (D), which on the Mayfly (or Sodaq Mbili) corresponds to Pins D0-D7.
It is designed to be compatible with [EnviroDIY_SoftwareSerial_PCINT12](https://github.com/EnviroDIY/SoftwareSerial_PCINT12) library (which controls interrupt vectors PCINT1 (B) & PCINT2 (C) / Mayfly pins D08-D15 & D16-D23) and [EnviroDIY PcInt PCINT0](https://github.com/EnviroDIY/PcInt_PCINT0) (which controls interrupt vectors PCINT0 (A) / Mayfly pins D24-D31/A0-A7).
Note that different AtMega1284p boards have a different mapping from the physical PIN numbers to the listed digital PIN numbers that are printed on the board.
One of the most helpful lists of pins and interrupts vectors is in the the [Pin/Port Bestiary wiki page for the Enable Interrupt library](https://github.com/GreyGnome/EnableInterrupt/wiki/Usage#PIN__PORT_BESTIARY).

## Contribute<!-- {#mainpage_contribute} -->

Open an [issue](https://github.com/EnviroDIY/Arduino-SDI-12/issues) to suggest and discuss potential changes/additions.

For power contributors:

1. Fork it!
2. Create your feature branch: `git checkout -b my-new-feature`
3. Commit your changes: `git commit -am 'Add some feature'`
4. Push to the branch: `git push origin my-new-feature`
5. Submit a pull request :D

## License<!-- {#mainpage_license} -->

The SDI12 library code is released under the GNU Lesser Public License (LGPL 2.1) -- See [LICENSE-examples.md](https://github.com/EnviroDIY/Arduino-SDI-12/blob/master/LICENSE) file for details.

Example Arduino sketches are released under the BSD 3-Clause License -- See [LICENSE-examples.md](https://github.com/EnviroDIY/Arduino-SDI-12/blob/master/LICENSE.md) file for details.

Documentation is licensed as [Creative Commons Attribution-ShareAlike 4.0](https://creativecommons.org/licenses/by-sa/4.0/) (CC-BY-SA) copyright.

## Credits<!-- {#mainpage_credits} -->

[EnviroDIY](http://envirodiy.org/)™ is presented by the Stroud Water Research Center, with contributions from a community of enthusiasts sharing do-it-yourself ideas for environmental science and monitoring.

[Kevin M. Smith](https://github.com/Kevin-M-Smith) was the primary developer of the SDI-12 library, with input from [S. Hicks](https://github.com/s-hicks2) and [Anthony Aufdenkampe](https://github.com/aufdenkampe).

[Sara Damiano](https://github.com/SRGDamia1) is now the primary maintainer, with input from many [other contributors](https://github.com/EnviroDIY/Arduino-SDI-12/graphs/contributors).

This project has benefited from the support from the following funders:

- National Science Foundation, awards [EAR-0724971](http://www.nsf.gov/awardsearch/showAward?AWD_ID=0724971), [EAR-1331856](http://www.nsf.gov/awardsearch/showAward?AWD_ID=1331856), [ACI-1339834](http://www.nsf.gov/awardsearch/showAward?AWD_ID=1339834)
- William Penn Foundation, grant 158-15
- Stroud Water Research Center endowment
