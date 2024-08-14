# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/) and its stricter, better defined, brother [Common Changelog](https://common-changelog.org/).

This project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

***

## [Unreleased]

### Changed

- **BREAKING** Made [EnableInterrupt](https://github.com/GreyGnome/EnableInterrupt) a required dependency for AVR boards.

### Added

### Removed

### Fixed

***

## [2.2.0] - 2024-08-14

_CRC and SAMD51 Support_

### Changed

- Added python version to GitHub actions (for PlatformIO)
- Switched to reusable workflows for GitHub actions
- Consolidated timer prescaler math
- Moved bitTimes and mul8x8to16 functions to the timers files
- Replace c-style casts with c++ style casts
- Do not disable interrupts during Tx for processors over 48MHz
- Cast all timer values and math to the proper type for the processor timer being read
  - SAMD21 boards now use the full 16-bits of the timer rather than only the first 8-bits.
  - AVR xU4 boards (ie, 32u4/Leonardo) still use the 10-bit timer as an 8-bit timer.
- Shortened name of espressif ISR access macro from `ESPFAMILY_USE_INSTRUCTION_RAM` to `ISR_MEM_ACCESS`
  - This does not change any functionality of the macro, just the name.
- Moved defines to the top of the SDI12_boards.h file
- Renamed the "tools" directory to "extras" in compliance with Arduino library standards.
- Updated copyright and license texts
- SAMD boards now *partially* revert clock and prescaler settings when an SDI-12 instance is ended.
  - Prescalers are reset to factory settings, the clock divisor is not reset

### Added

- Added CRC checking
- Added support for SAMD51 processors using dedicated timers
- Added parity checking on character reception
  - This can be disabled by defining the macro `SDI12_IGNORE_PARITY`
- Allowing (_**without testing**_) processors over 48MHz to use `micros()` function
- Added a 'yield' function within stream functions to allow the buffer to fill
  - The yield time can be modified using the macro `SDI12_YIELD_MS`

### Removed

- Offloaded some internal header file documentation to markdown files
- Consolidated redundant `READTIME` and `TCNTX` macros, removing `TCNTX`
- Removed documation m_span commands

### Fixed

- Added an extra addition/removal of interrupts for espressif boards in the `begin` function to properly initialize the interrupts and avoid a later error with the `gpio_install_isr_service`.

***

## [2.1.4] - 2021-05-05

_Revert wake delay to 0ms_

### Changed

- **BREAKING** Reverted the default wake delay to 0ms.
  - In 92055d377b26fa862c43d1429de1ccbef054af01 this was bumped up to 10ms, which caused problems for several people.
  - The delay can now also be set using the build flag `-D SDI12_WAKE_DELAY=#`

## [2.1.3] - 2021-03-24

_Migrate to GitHub Actions_

### Changed

- Migrate from Travis to GitHub actions

## [2.1.1] - 2020-08-20

_Patches for ATTiny_

### Fixed

- fixes for the timer and pre-scaler for the ATTiny, courtesy of \@gabbas1

## [2.1.0] - 2020-07-10

_Library Rename and ESP support_

[![DOI](https://zenodo.org/badge/DOI/10.5281/zenodo.3939731.svg)](https://doi.org/10.5281/zenodo.3939731)

**To comply with requirements for inclusion in the Arduino IDE, the word Arduino has been removed from the name of this library!**  The repository name is unchanged.

### Changed

- Adds support for Espressif ESP8266 and ESP32
- Add option of adding a delay before sending a command to allow the sensor to wake.  Take advantage of this by calling the function `sendCommand(command, extraWakeTime)`. This may resolve issues with some Campbell sensors that would not previous communicate with this library.  See <https://www.envirodiy.org/topic/campbell-scientific-cs-215-sdi-12-communication-issues-w-mayfly/#post-14103>
- Adds Doxygen (Javadoc) style comments to **ALL** members of the library.  The generated documentation is available at <https://envirodiy.github.io/Arduino-SDI-12/>.

## [1.3.6] - 2019-08-29

_Fixed extra compiler warnings_

### Fixed

- A very minor update to fix compiler warnings found when using -Wextra in addition to -Wall.

## [1.3.5] - 2019-07-01

_Removed SAMD Tone Conflict_

### Changed

- SAMD boards will no longer have a conflict with the Tone functions in the Arduino core. AVR boards will still conflict. If you need to use Tone and SDI-12 together for some reason on an AVR boards, you must use the "delayBase" branch.
- Examples were also updated and given platformio.ini files.

## [1.3.4] - 2019-10-29

_Timer class_

### Changed

- Made the timer changes into a compiled class.
- NOTE: Maintaining interrupt control for SAMD processors as there are no interrupt vectors to be in conflict.
Because the pin mode changes from input to output and back, allowing another library to control interrupts doesn't work.

## [1.3.3] - 2018-05-11

_Unset prescalers_

### Changed

- Now unsetting timer prescalers and setting the isActive pointer to NULL in both the end and the destructor functions.
- Also some clean-up of the examples.

## [1.3.1] - 2018-04-06

Added processor timer for greater stability

### Added

- Changed the incoming data ISR to use a processor timer, this makes the reception more stable, especially when the ISR is controlled by an external library. This also creates some conflicts with other libraries that use Timer2.

### Changed

- Made changes to the write functions to use the timer to reduce the amount of time that all system interrupts are off.
- Forcing all SDI-12 objects to use the same buffer to reduce ram usage.

## [1.1.0] - 2018-03-15

_Better integration inside other libraries_

### Improvements

- Added notes and an empty constructor/populated begin method to allow this library to be more easily called inside of other libraries.

## [1.0.6] - 2018-03-09

Fixed timeout values

### Bug Fixes

- Fixes the time-out values for the ParseInt and ParseFloat to be -9999. This was the intended behavior all along, but at some point those functions changed in the stream library and the identically named functions within SDI-12 intended to "hide" the stream functions ceased to be called.

## [1.0.1] - 2017-05-16

_Initial Release_

The first "official" release of this interrupt-based SDI-12 library for AVR and SAMD Arduino boards.

***

[Unreleased]: https://github.com/EnviroDIY/Arduino-SDI-12/compare/v2.2.0...HEAD
[2.2.0]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v2.2.0
[2.1.4]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v2.1.4
[2.1.3]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v2.1.3
[2.1.1]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v2.1.1
[2.1.0]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v2.1.0
[1.3.6]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.3.6
[1.3.5]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.3.5
[1.3.4]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.3.4
[1.3.3]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.3.3
[1.3.1]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.3.1
[1.1.0]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.1.0
[1.0.6]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.0.6
[1.0.1]: https://github.com/EnviroDIY/Arduino-SDI-12/releases/tag/v1.0.1
