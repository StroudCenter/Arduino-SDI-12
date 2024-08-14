/**
 * @file SDI12.cpp
 * @copyright Stroud Water Research Center
 * @license This library is published under the BSD-3 license.
 * @date August 2013
 * @author Kevin M.Smith <SDI12@ethosengineering.org>
 *
 * @brief This file implements the main class for the SDI-12 implementation.
 *
 * ========================== Arduino SDI-12 ==================================
 *
 * An Arduino library for SDI-12 communication with a wide variety of environmental
 * sensors. This library provides a general software solution, without requiring any
 * additional hardware.
 *
 * ======================== Attribution & License =============================
 *
 * Copyright (C) 2013  Stroud Water Research Center
 * Available at https://github.com/EnviroDIY/Arduino-SDI-12
 *
 * Authored initially in August 2013 by:
 *          Kevin M. Smith (http://ethosengineering.org)
 *          Inquiries: SDI12@ethosengineering.org
 *
 * Modified 2017 by Manuel Jimenez Buendia to work with ARM based processors (Arduino
 * Zero)
 *
 * Maintenance and merging 2017 by Sara Damiano
 *
 * based on the SoftwareSerial library (formerly NewSoftSerial), authored by:
 *         ladyada (http://ladyada.net)
 *         Mikal Hart (http://www.arduiniana.org)
 *         Paul Stoffregen (http://www.pjrc.com)
 *         Garrett Mace (http://www.macetech.com)
 *         Brett Hagman (http://www.roguerobotics.com/)
 *
 * This library is free software; you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free Software
 * Foundation; either version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this library; if not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA  02110-1301  USA
 */


#include "SDI12.h"  //  Header file for this library

/* ================  Set static constants ===========================================*/

// Pointer to active SDI12 object
SDI12* SDI12::_activeObject = nullptr;
// Timer functions
SDI12Timer SDI12::sdi12timer;

// The size of a bit in microseconds
// 1200 baud = 1200 bits/second ~ 833.333 µs/bit
const uint16_t SDI12::bitWidth_micros = static_cast<uint16_t>(833);
// The required "break" before sending commands, >= 12ms
const uint16_t SDI12::lineBreak_micros = static_cast<uint16_t>(12300);
// The required mark before a command or response, >= 8.33ms
const uint16_t SDI12::marking_micros = static_cast<uint16_t>(8500);

// the width of a single bit in "ticks" of the cpu clock.
const sdi12timer_t SDI12::txBitWidth = TICKS_PER_BIT;
// A mask waiting for a start bit; 0b11111111
const uint8_t SDI12::WAITING_FOR_START_BIT = 0xFF;

sdi12timer_t SDI12::prevBitTCNT;  // previous RX transition in micros
uint8_t      SDI12::rxState = WAITING_FOR_START_BIT;  // 0: got start bit; >0: bits rcvd
uint8_t      SDI12::rxMask;   // bit mask for building received character
uint8_t      SDI12::rxValue;  // character being built

/* ================ Buffer Setup ====================================================*/
uint8_t          SDI12::_rxBuffer[SDI12_BUFFER_SIZE];  // The Rx buffer
volatile uint8_t SDI12::_rxBufferTail = 0;             // index of buff tail
volatile uint8_t SDI12::_rxBufferHead = 0;             // index of buff head

/* ================ Reading from the SDI-12 Buffer ==================================*/

// reveals the number of characters available in the buffer
int SDI12::available() {
  SDI12_YIELD()
  if (_bufferOverflow) return -1;
  return (_rxBufferTail + SDI12_BUFFER_SIZE - _rxBufferHead) % SDI12_BUFFER_SIZE;
}

// reveals the next character in the buffer without consuming
int SDI12::peek() {
  SDI12_YIELD()
  if (_rxBufferHead == _rxBufferTail) return -1;  // Empty buffer? If yes, -1
  return _rxBuffer[_rxBufferHead];                // Otherwise, read from "head"
}

// a public function that clears the buffer contents and resets the status of the buffer
// overflow.
void SDI12::clearBuffer() {
  _rxBufferHead = _rxBufferTail = 0;
  _bufferOverflow               = false;
}

// reads in the next character from the buffer (and moves the index ahead)
int SDI12::read() {
  SDI12_YIELD()
  _bufferOverflow = false;                        // Reading makes room in the buffer
  if (_rxBufferHead == _rxBufferTail) return -1;  // Empty buffer? If yes, -1
  uint8_t nextChar = _rxBuffer[_rxBufferHead];    // Otherwise, grab char at head
  _rxBufferHead    = (_rxBufferHead + 1) % SDI12_BUFFER_SIZE;  // increment head
  return nextChar;                                             // return the char
}

// these functions HIDE the stream equivalents to return a custom timeout value
// This peekNextDigit function is identical to the Stream version
int SDI12::peekNextDigit(LookaheadMode lookahead, bool detectDecimal) {
  int c;
  while (true) {
    c = timedPeek();

    if (c < 0 || c == '-' || (c >= '0' && c <= '9') || (detectDecimal && c == '.'))
      return c;

    switch (lookahead) {
      case SKIP_NONE: return -1;  // Fail code.
      case SKIP_WHITESPACE:
        switch (c) {
          case ' ':
          case '\t':
          case '\r':
          case '\n': break;
          default: return -1;  // Fail code.
        }
      case SKIP_ALL: break;
    }
    read();  // discard non-numeric
  }
}

long SDI12::parseInt(LookaheadMode lookahead, char ignore) {
  bool     isNegative = false;
  uint16_t value      = 0;
  int      c;

  c = peekNextDigit(lookahead, false);
  // ignore non numeric leading characters
  if (c < 0) return TIMEOUT;  // TIMEOUT returned if timeout
  //  THIS IS THE ONLY DIFFERENCE BETWEEN THIS FUNCTION AND THE STREAM DEFAULT!

  do {
    if (c == ignore) {  // ignore this character
    } else if (c == '-') {
      isNegative = true;
    } else if (c >= '0' && c <= '9') {  // is c a digit?
      value = value * 10 + c - '0';
    }
    read();  // consume the character we got with peek
    c = timedPeek();
  } while ((c >= '0' && c <= '9') || c == ignore);

  if (isNegative) value = -value;
  return value;
}

// the same as parseInt but returns a floating point value
float SDI12::parseFloat(LookaheadMode lookahead, char ignore) {
  bool  isNegative = false;
  bool  isFraction = false;
  long  value      = 0;
  int   c;
  float fraction = 1.0;

  c = peekNextDigit(lookahead, true);
  // ignore non numeric leading characters
  if (c < 0) return TIMEOUT;  // TIMEOUT returned if timeout
  //  THIS IS THE ONLY DIFFERENCE BETWEEN THIS FUNCTION AND THE STREAM DEFAULT!

  do {
    if (c == ignore) {  // ignore
    } else if (c == '-') {
      isNegative = true;
    } else if (c == '.') {
      isFraction = true;
    } else if (c >= '0' && c <= '9') {  // is c a digit?
      value = value * 10 + c - '0';
      if (isFraction) fraction *= 0.1;
    }
    read();  // consume the character we got with peek
    c = timedPeek();
  } while ((c >= '0' && c <= '9') || (c == '.' && !isFraction) || c == ignore);

  if (isNegative) value = -value;
  if (isFraction)
    return value * fraction;
  else
    return value;
}

/* ================ Constructor, Destructor, begin(), end(), and timeout ============*/
// Constructor
SDI12::SDI12() {
  // SDI-12 protocol says sensors must respond within 15 milliseconds
  // We'll bump that up to 150, just for good measure, but we don't want to
  // wait the whole stream default of 1s for a response.
  setTimeout(150);
  // Because SDI-12 is mostly used for environmental sensors, we want to be able
  // to distinguish between the '0' that parseInt and parseFloat usually return
  // on timeouts and a real measured 0 value.  So we force the timeout response
  // to be -9999, which is not a common value for most variables measured by
  // in-site environmental sensors.
  setTimeoutValue(-9999);
}

SDI12::SDI12(int8_t dataPin) {
  setDataPin(dataPin);
  // SDI-12 protocol says sensors must respond within 15 milliseconds
  // We'll bump that up to 150, just for good measure, but we don't want to
  // wait the whole stream default of 1s for a response.
  setTimeout(150);
  // Because SDI-12 is mostly used for environmental sensors, we want to be able
  // to distinguish between the '0' that parseInt and parseFloat usually return
  // on timeouts and a real measured 0 value.  So we force the timeout response
  // to be -9999, which is not a common value for most variables measured by
  // in-site environmental sensors.
  setTimeoutValue(-9999);
}

// Destructor
SDI12::~SDI12() {
  setState(SDI12_DISABLED);
  if (isActive()) { _activeObject = nullptr; }
  // Set the timer prescalers back to original values
  // NOTE:  This does NOT reset SAMD board pre-scalers!
  sdi12timer.resetSDI12TimerPrescale();
}

// Begin
void SDI12::begin() {
#if defined(ESP32) || defined(ESP8266)
  // Add and remove a fake interrupt to avoid errors with gpio_install_isr_service
  attachInterrupt(digitalPinToInterrupt(_dataPin), nullptr, CHANGE);
  detachInterrupt(digitalPinToInterrupt(_dataPin));
#endif
  // setState(SDI12_HOLDING);
  setActive();
  // Set up the prescaler as needed for timers
  // This function is defined in SDI12_boards.h
  sdi12timer.configSDI12TimerPrescale();
}

void SDI12::begin(int8_t dataPin) {
  setDataPin(dataPin);
  begin();
}

// End
void SDI12::end() {
  setState(SDI12_DISABLED);
  _activeObject = nullptr;
  // Set the timer prescalers back to original values
  sdi12timer.resetSDI12TimerPrescale();
}

// Set the timeout return
void SDI12::setTimeoutValue(int16_t value) {
  TIMEOUT = value;
}

// Set the data pin for the SDI-12 instance
void SDI12::setDataPin(int8_t dataPin) {
  _dataPin = dataPin;
}

// Return the data pin for the SDI-12 instance
int8_t SDI12::getDataPin() {
  return _dataPin;
}

/* ================ Using more than one SDI-12 object ===============================*/
// a method for setting the current object as the active object
bool SDI12::setActive() {
  if (_activeObject != this) {
    setState(SDI12_HOLDING);
    _activeObject = this;
    return true;
  }
  return false;
}

// a method for checking if this object is the active object
bool SDI12::isActive() {
  return this == _activeObject;
}

/* ================ Data Line States ================================================*/
// Processor specific parity and interrupts
#if defined __AVR__
#include <avr/interrupt.h>  // interrupt handling
#include <util/parity.h>    // optimized parity bit handling
#else
// Added MJB: parity function to replace the one specific for AVR from util/parity.h
// http://graphics.stanford.edu/~seander/bithacks.html#ParityNaive
uint8_t SDI12::parity_even_bit(uint8_t v) {
  uint8_t parity = 0;
  while (v) {
    parity = !parity;
    v      = v & (v - 1);
  }
  return parity;
}
#endif

// a helper function to switch pin interrupts on or off
void SDI12::setPinInterrupts(bool enable) {
  // If we're using this library on an AVR board without the Enable Interrupts
  // dependency
  // NOTE: NOT RECOMMENDED
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && defined(SDI12_INTERNAL_PCINT)
  if (enable) {
    // Enable interrupts on the register with the pin of interest
    *digitalPinToPCICR(_dataPin) |= (1 << digitalPinToPCICRbit(_dataPin));
    // Enable interrupts on the specific pin of interest
    // The interrupt function is actually attached to the interrupt way down in
    // section 7.5
    *digitalPinToPCMSK(_dataPin) |= (1 << digitalPinToPCMSKbit(_dataPin));
  } else {
    // Disable interrupts on the specific pin of interest
    *digitalPinToPCMSK(_dataPin) &= ~(1 << digitalPinToPCMSKbit(_dataPin));
    if (!*digitalPinToPCMSK(_dataPin)) {
      // If there are no other pins on the register left with enabled interrupts,
      // disable the whole register
      *digitalPinToPCICR(_dataPin) &= ~(1 << digitalPinToPCICRbit(_dataPin));
    }
    // We don't detach the function from the interrupt for AVR processors
  }
#else
  // Use the enableInterrupt function to attach the interrupt to the pin
  // For AVR boards, this is defined in the EnableInterrupt library
  // For other boards, this is defined in SDI12.h as a rename for attachInterrupt
  if (enable) enableInterrupt(_dataPin, handleInterrupt, CHANGE);
  // Use the disableInterrupt function to detach the interrupt function from the pin
  // For AVR boards, this is defined in the EnableInterrupt library
  // For other boards, this is defined in SDI12.h as a rename for detachInterrupt
  else
    disableInterrupt(_dataPin);
#endif
}

// sets the state of the SDI-12 object.
void SDI12::setState(SDI12_STATES state) {
  switch (state) {
    case SDI12_HOLDING:
      {
        pinMode(_dataPin, INPUT);     // Turn off the pull-up resistor
        pinMode(_dataPin, OUTPUT);    // Pin mode = output
        digitalWrite(_dataPin, LOW);  // Pin state = low - marking
        setPinInterrupts(false);      // Interrupts disabled on data pin
        break;
      }
    case SDI12_TRANSMITTING:
      {
        pinMode(_dataPin, INPUT);   // Turn off the pull-up resistor
        pinMode(_dataPin, OUTPUT);  // Pin mode = output
        setPinInterrupts(false);    // Interrupts disabled on data pin
#ifdef SDI12_CHECK_PARITY
        _parityFailure = false;  // reset the parity failure flag
#endif
        break;
      }
    case SDI12_LISTENING:
      {
        digitalWrite(_dataPin, LOW);  // Pin state = low (turns off pull-up)
        pinMode(_dataPin, INPUT);     // Pin mode = input, pull-up resistor off
        interrupts();                 // Enable general interrupts
        setPinInterrupts(true);       // Enable Rx interrupts on data pin
        rxState = WAITING_FOR_START_BIT;
        break;
      }
    default:  // SDI12_DISABLED or SDI12_ENABLED
      {
        digitalWrite(_dataPin, LOW);  // Pin state = low (turns off pull-up)
        pinMode(_dataPin, INPUT);     // Pin mode = input, pull-up resistor off
        setPinInterrupts(false);      // Interrupts disabled on data pin
        break;
      }
  }
}

// forces a SDI12_HOLDING state.
void SDI12::forceHold() {
  setState(SDI12_HOLDING);
}

// forces a SDI12_LISTENING state.
void SDI12::forceListen() {
  setState(SDI12_LISTENING);
}

/* ================ Waking Up and Talking To Sensors ================================*/
// this function wakes up the entire sensor bus by sending a 12ms break followed by 8.33
// ms of marking
void SDI12::wakeSensors(int8_t extraWakeTime) {
  setState(SDI12_TRANSMITTING);
  // Universal interrupts can be on while the break and marking happen because
  // timings for break and from the recorder are not critical.
  // Interrupts on the pin are disabled for the entire transmitting state
  digitalWrite(_dataPin, HIGH);         // break is HIGH
  delayMicroseconds(lineBreak_micros);  // Required break of 12 milliseconds (12,000 µs)
  delay(extraWakeTime);                 // allow the sensors to wake
  digitalWrite(_dataPin, LOW);          // marking is LOW
  delayMicroseconds(marking_micros);  // Required marking of 8.33 milliseconds(8,333 µs)
}

// this function writes a character out on the data line
void SDI12::writeChar(uint8_t outChar) {
  uint8_t currentTxBitNum = 0;  // first bit is start bit
  uint8_t bitValue        = 1;  // start bit is HIGH (inverse parity...)

  // The tolerance on all SDI-12 commands is 0.40ms = 400µs. But... that's for between
  // commands, and we don't know how accurate all sensors are, so we probably don't want
  // to be off by more than 1/10 of that between bits.

  // Let's assume an interrupt routine can take up 1000 clock cycles. I don't know if
  // that's reasonable, but per
  // https://forum.arduino.cc/t/how-many-clock-cycles-does-digitalread-write-take/467153
  // a single digitalWrite function takes up 50 clock cycles so 20x that seems like a
  // safe buffer. Our own SDI-12 receive ISR takes up roughly 617 clock cycles on a
  // Mayfly. [Calculated using a modified version of
  // https://github.com/SRGDamia1/avrcycles.] For the a 1000 clock cycle interrupt
  // to not shift timing by more than 400µs the clock must have more than 40,000,000
  // cycles in one second (40MHz). For any board slower than 40MHz, we'll, disable _ALL_
  // interrupts during sending so timing can't be shifted. For faster boards, we
  // can probably safely leave interrupts on. Disabling interrupts can screw up build-in
  // functions like micros(), millis() and any real-time clocks, so we don't want to
  // disable them if we don't really have to.

#if F_CPU < 48000000UL
  noInterrupts();  // _ALL_ interrupts disabled
#endif

  sdi12timer_t t0 = READTIME;  // start time

  digitalWrite(
    _dataPin,
    HIGH);  // immediately get going on the start bit
            // this gives us 833µs to calculate parity and position of last high bit
  currentTxBitNum++;

  // Calculate parity, while writing the start bit
  // This takes about 24 clock cycles on an AVR board (at 8MHz, that's 3µsec)

  uint8_t parityBit = parity_even_bit(outChar);  // Calculate the parity bit
  outChar |= (parityBit << 7);  // Add parity bit to the outgoing character

  // Calculate the position of the last bit that is a 0/HIGH (ie, HIGH, not marking)
  // That bit will be the last time-critical bit.  All bits after that can be
  // sent with interrupts enabled.
  // This calculation should also finish while writing the start bit
  // This takes at least 10+13 clock cycles, and up to 10+(13*9)= 127 clock cycles (at
  // 8MHz, that's 15.875µsec)

  uint8_t lastHighBit =
    9;  // The position of the last bit that is a 0 (ie, HIGH, not marking)
  uint8_t msbMask = 0x80;  // A mask with all bits at 1
  while (msbMask & outChar) {
    lastHighBit--;
    msbMask >>= 1;
  }

  // Hold the line for the rest of the start bit duration
  // We've used up roughly 150 clock cycles messing with parity, but a bit is 833µs, so
  // we've got time.

  while (static_cast<sdi12timer_t>(READTIME - t0) < txBitWidth) {}
  t0 = READTIME;  // advance start time

  // repeat for all data bits until the last bit different from marking
  while (currentTxBitNum++ < lastHighBit) {
    bitValue = outChar & 0x01;  // get next bit in the character to send
    if (bitValue) {
      digitalWrite(_dataPin, LOW);  // set the pin state to LOW for 1's
    } else {
      digitalWrite(_dataPin, HIGH);  // set the pin state to HIGH for 0's
    }
    // Hold the line for this bit duration
    while (static_cast<sdi12timer_t>(READTIME - t0) < txBitWidth) {}
    t0 = READTIME;  // advance start time

    outChar = outChar >> 1;  // shift character to expose the following bit
  }

  // Set the line low for the all remaining 1's and the stop bit
  digitalWrite(_dataPin, LOW);

#if F_CPU < 48000000UL
  interrupts();  // Re-enable universal interrupts as soon as critical timing is past
#endif

  // Hold the line low until the end of the 10th bit
  sdi12timer_t bitTimeRemaining = txBitWidth * (10 - lastHighBit);
  while (static_cast<sdi12timer_t>(READTIME - t0) < bitTimeRemaining) {}
}

// The typical write functionality for a stream object
// This allows you to use the stream print functions to send commands out on
// the SDI-12, line, but it will not wake the sensors in advance of the command.
size_t SDI12::write(uint8_t byte) {
  setState(SDI12_TRANSMITTING);
  writeChar(byte);            // write the character/byte
  setState(SDI12_LISTENING);  // listen for reply
  return 1;                   // 1 character sent
}

// this function sends out the characters of the String cmd, one by one
void SDI12::sendCommand(String& cmd, int8_t extraWakeTime) {
  wakeSensors(extraWakeTime);  // wake up sensors
  for (int unsigned i = 0; i < cmd.length(); i++) {
    writeChar(cmd[i]);  // write each character
  }
  setState(SDI12_LISTENING);  // listen for reply
}

void SDI12::sendCommand(const char* cmd, int8_t extraWakeTime) {
  wakeSensors(extraWakeTime);  // wake up sensors
  for (int unsigned i = 0; i < strlen(cmd); i++) {
    writeChar(cmd[i]);  // write each character
  }
  setState(SDI12_LISTENING);  // listen for reply
}

void SDI12::sendCommand(FlashString cmd, int8_t extraWakeTime) {
  wakeSensors(extraWakeTime);  // wake up sensors
  for (int unsigned i = 0; i < strlen_P((PGM_P)cmd); i++) {
    // write each character
    writeChar(static_cast<char>(pgm_read_byte((const char*)cmd + i)));
  }
  setState(SDI12_LISTENING);  // listen for reply
}

// This function sets up for a response to a separate data recorder by sending out a
// marking and then sending out the characters of resp one by one (for slave-side use,
// that is, when the Arduino itself is acting as an SDI-12 device rather than a
// recorder).
void SDI12::sendResponse(String& resp, bool addCRC) {
  setState(SDI12_TRANSMITTING);       // Get ready to send data to the recorder
  digitalWrite(_dataPin, LOW);        // marking is LOW
  delayMicroseconds(marking_micros);  // 8.33 ms marking before response
  for (int unsigned i = 0; i < resp.length(); i++) {
    writeChar(resp[i]);  // write each character
  }
  // tack on the CRC if requested
  if (addCRC) {
    String crc = crcToString(calculateCRC(resp));
    for (int unsigned i = 0; i < 3; i++) {
      writeChar(crc[i]);  // write each character
    }
  }
  setState(SDI12_LISTENING);  // return to listening state
}

void SDI12::sendResponse(const char* resp, bool addCRC) {
  setState(SDI12_TRANSMITTING);       // Get ready to send data to the recorder
  digitalWrite(_dataPin, LOW);        // marking is LOW
  delayMicroseconds(marking_micros);  // 8.33 ms marking before response
  for (int unsigned i = 0; i < strlen(resp); i++) {
    writeChar(resp[i]);  // write each character
  }
  // tack on the CRC if requested
  if (addCRC) {
    String crc = crcToString(calculateCRC(resp));
    for (int unsigned i = 0; i < 3; i++) {
      writeChar(crc[i]);  // write each character
    }
  }
  setState(SDI12_LISTENING);  // return to listening state
}

void SDI12::sendResponse(FlashString resp, bool addCRC) {
  setState(SDI12_TRANSMITTING);       // Get ready to send data to the recorder
  digitalWrite(_dataPin, LOW);        // marking is LOW
  delayMicroseconds(marking_micros);  // 8.33 ms marking before response
  for (int unsigned i = 0; i < strlen_P((PGM_P)resp); i++) {
    // write each character
    writeChar(static_cast<char>(pgm_read_byte((const char*)resp + i)));
  }
  // tack on the CRC if requested
  if (addCRC) {
    String crc = crcToString(calculateCRC(resp));
    for (int unsigned i = 0; i < 3; i++) {
      writeChar(crc[i]);  // write each character
    }
  }
  setState(SDI12_LISTENING);  // return to listening state
}

/**
 * @brief The polynomial to match the CRC with; set in the SDI-12 specifications
 */
#define POLY 0xa001

uint16_t SDI12::calculateCRC(String& resp) {
  uint16_t crc = 0;

  for (uint16_t i = 0; i < resp.length(); i++) {
    crc ^= static_cast<uint16_t>(
      resp[i]);  // Set the CRC equal to the exclusive OR of the character and itself
    for (int j = 0; j < 8; j++) {  // count = 1 to 8
      if (crc & 0x0001) {          // if the least significant bit of the CRC is one
        crc >>= 1;                 // right shift the CRC one bit
        crc ^=
          POLY;  // set CRC equal to the exclusive OR of the match polynomial and itself
      } else {
        crc >>= 1;  // right shift the CRC one bit
      }
    }
  }
  return crc;
}

uint16_t SDI12::calculateCRC(const char* resp) {
  uint16_t crc = 0;

  for (size_t i = 0; i < strlen(resp); i++) {
    crc ^= static_cast<uint16_t>(
      resp[i]);  // Set the CRC equal to the exclusive OR of the character and itself
    for (int j = 0; j < 8; j++) {  // count = 1 to 8
      if (crc & 0x0001) {          // if the least significant bit of the CRC is one
        crc >>= 1;                 // right shift the CRC one bit
        crc ^= POLY;  // set CRC equal to the exclusive OR of POLY and itself
      } else {
        crc >>= 1;  // right shift the CRC one bit
      }
    }
  }
  return crc;
}

uint16_t SDI12::calculateCRC(FlashString resp) {
  uint16_t crc = 0;
  char     responsechar;

  for (size_t i = 0; i < strlen_P((PGM_P)resp); i++) {
    responsechar = (char)pgm_read_byte((const char*)resp + i);
    crc ^= static_cast<uint16_t>(responsechar);  // Set the CRC equal to the exclusive
                                                 // OR of the character and itself
    for (int j = 0; j < 8; j++) {                // count = 1 to 8
      if (crc & 0x0001) {  // if the least significant bit of the CRC is one
        crc >>= 1;         // right shift the CRC one bit
        crc ^= POLY;       // set CRC equal to the exclusive OR of POLY and itself
      } else {
        crc >>= 1;  // right shift the CRC one bit
      }
    }
  }
  return crc;
}

String SDI12::crcToString(uint16_t crc) {
  char crcStr[3] = {0};
  crcStr[0]      = (char)(0x0040 | (crc >> 12));
  crcStr[1]      = (char)(0x0040 | ((crc >> 6) & 0x003F));
  crcStr[2]      = (char)(0x0040 | (crc & 0x003F));
  return (String(crcStr[0]) + String(crcStr[1]) + String(crcStr[2]));
}

bool SDI12::verifyCRC(String& respWithCRC) {
  // trim trailing \r and \n (<CR> and <LF>)
  respWithCRC.trim();
  uint16_t nChar =
    respWithCRC.length();  // number of characters without  (readable string composed of
                           // sensor address, values separated by + and -) and the 3
                           // characters of the CRC
  String recCRC    = "";   // the CRC portion of the response
  String recString = "";   // the data portion of the response

  // extract the data portion of the string
  for (uint16_t i = 0; i < (nChar - 3); i++) recString += respWithCRC[i];

  // extract the last 3 characters that are the CRC from the full response string
  for (uint16_t i = (nChar - 3); i < nChar; i++) recCRC += respWithCRC[i];

  // calculate the CRC for the data portion
  String calcCRC = crcToString(calculateCRC(recString));

  if (recCRC == calcCRC) {
    return true;
  } else {
    return false;
  }
}

/* ================ Interrupt Service Routine =======================================*/

// 7.1 - Passes off responsibility for the interrupt to the active object.
void ISR_MEM_ACCESS SDI12::handleInterrupt() {
  if (_activeObject) _activeObject->receiveISR();
}

// 7.2 - Creates a blank slate of bits for an incoming character
void ISR_MEM_ACCESS SDI12::startChar() {
  rxState = 0x00;  // 0b00000000, got a start bit
  rxMask  = 0x01;  // 0b00000001, bit mask, lsb first
  rxValue = 0x00;  // 0b00000000, RX character to be, a blank slate
}  // startChar

// 7.3 - The actual interrupt service routine
void ISR_MEM_ACCESS SDI12::receiveISR() {
  sdi12timer_t thisBitTCNT =
    READTIME;  // time of this data transition (plus ISR latency)

  uint8_t pinLevel = digitalRead(_dataPin);  // current RX data level

  // Check if we're ready for a start bit, and if this could possibly be it.
  if (rxState == WAITING_FOR_START_BIT) {
    // If we are waiting for a start bit and the pin is low it's not a start bit, exit
    // Inverse logic start bit = HIGH
    if (pinLevel == LOW) { return; }
    // If the pin is HIGH, this should be a start bit.
    // Thus call startChar(), which zeros the timer counter, sets the rxState to 0, and
    // creates an empty character and a new mask with a 1 in the lowest place
    startChar();
  } else {
    // If we're not waiting for a start bit, it's because we're in the middle of an
    // incomplete character and therefore this change in the pin state must be from a
    // data, parity, or stop bit.

    // Check how many bit times have passed since the last change
    uint16_t rxBits = SDI12Timer::bitTimes(thisBitTCNT - prevBitTCNT);
    // Calculate how many *data+parity* bits should be left in the current character
    //      - Each character has a total of 10 bits, 1 start bit, 7 data bits, 1 parity
    // bit, and 1 stop bit
    //      - The #rxState holds record of how many of the data + parity bits we've
    // gotten (up to 8)
    //      - We have to treat the parity bit as a data bit because we don't know its
    // state
    //      - Since we're mid character, we know the start bit is past which knocks us
    // down to 9
    //      - There will always be one left over for the stop bit, which will be LOW/1
    uint8_t bitsLeft = 9 - rxState;
    // If the number of bits passed since the last transition is more than then number
    // of bits left on the character we were working on, a new character must have
    // started.
    // Because we're depending on pin **changes** here, and the stop bit at the end of a
    // character is LOW/line idle, we cannot detect the end of a stop bit.  The last
    // change we can detect from a character is the end of the last 0 bit (inverse logic
    // - 0 = HIGH).  The end of the last 0 bit **might** be the start of the (1=LOW=line
    // idle) stop bit, but it bit could actually be the end of start-bit itself - as in
    // the case of the DEL character.  (DEL = 1 HIGH start - 7 LOW (1) data bits - 1 LOW
    // (1) even parity bit - 1 LOW stop bit, last level change before line idle is the
    // end of the start bit)  Because we cannot detect the end of the stop bit, in
    // sequention characters the next change will be the next start bit and it will
    // arrive with the rxState set to the middle of the last character.  So, since we
    // cannot depend on the rxState telling us if we're WAITING_FOR_START_BIT, we have
    // to figure it out by the time passed.
    bool nextCharStarted = (rxBits > bitsLeft);

    // Check how many data+parity bits have been sent in this frame.  This will be
    // different from the rxBits if a new character has started because of the start
    // and stop bits.
    //      - If the total number of bits in this frame is more than the number of
    // data+parity bits remaining in the character, then the number of data+parity bits
    // is equal to the number of bits remaining for the character and partiy.
    //      - If the total number of bits in this frame is less than the number of data
    // bits left for the character and parity, then the number of data+parity bits
    // received in this frame is equal to the total number of bits received in this
    // frame.
    // translation:
    //    if nextCharStarted then bitsThisFrame = bitsLeft
    //                       else bitsThisFrame = rxBits
    uint8_t bitsThisFrame = nextCharStarted ? bitsLeft : rxBits;
    // Tick up the rxState by the number of data+parity bits received in the frame
    rxState += bitsThisFrame;

    // Set all the bits received between the last change and this change
    if (pinLevel == HIGH) {
      // If the current state is HIGH (and it just became so), then all bits between
      // the last change and now must have been LOW.
      // back fill previous bits with 1's (inverse logic - LOW = 1)
      while (bitsThisFrame-- > 0) {
        // for each of the bits that happened in this frame

        rxValue |= rxMask;     // Add a 1 to the LSB/right-most place of our character
                               // value from the mask
        rxMask = rxMask << 1;  // Shift the 1 in the mask up by one position
      }
      // And shift the 1 in the mask up by one more position for the current bit.
      // It's HIGH/0 now, so we don't use `|=` with the mask for this last one.
      rxMask = rxMask << 1;
    } else {
      // If the current state is LOW (and it just became so), then this bit is LOW
      // but all bits between the last change and now must have been HIGH

      // pinLevel==LOW
      // previous bits were 0's so only this bit is a 1 (inverse logic - LOW = 1)
      rxMask = rxMask << (bitsThisFrame -
                          1);  // Shift the 1 in the mask up by the number of bits past
      rxValue |= rxMask;  //  And add that shifted one to the character being created
    }

    // If this was the 8th or more bit then the character and parity are complete.
    // The stop bit may still be outstanding
    if (rxState > 7) {
#ifdef SDI12_CHECK_PARITY
      uint8_t rxParity = bitRead(rxValue, 7);  // pull out the parity bit
#endif
      rxValue &= 0x7F;        // Throw away the parity bit (and with 0b01111111)
      charToBuffer(rxValue);  // Put the finished character into the buffer
#ifdef SDI12_CHECK_PARITY
      uint8_t checkParity =
        parity_even_bit(rxValue);  // Calculate the parity bit from character w/o parity
      if (rxParity != checkParity) { _parityFailure = true; }
#endif

      // if this is LOW, or we haven't exceeded the number of bits in a
      // character (but have gotten all the data bits) then this should be a
      // stop bit and we can start looking for a new start bit.
      if ((pinLevel == LOW) || !nextCharStarted) {
        rxState =
          WAITING_FOR_START_BIT;  // reset the rx state, stop waiting for stop bit
      } else {
        // If we just switched to HIGH, or we've exceeded the total number of
        // bits in a character, then the character must have ended with 1's/LOW,
        // and this new 0/HIGH is actually the start bit of the next character.
        startChar();
      }
    }
  }
  prevBitTCNT = thisBitTCNT;  // finally remember time stamp of this change!
}

// Put a new character in the buffer
void SDI12::charToBuffer(uint8_t c) {
  // Check for a buffer overflow. If not, proceed.
  if ((_rxBufferTail + 1) % SDI12_BUFFER_SIZE == _rxBufferHead) {
    _bufferOverflow = true;
  } else {
    // Save the character, advance buffer tail.
    _rxBuffer[_rxBufferTail] = c;
    _rxBufferTail            = (_rxBufferTail + 1) % SDI12_BUFFER_SIZE;
  }
}

// Define AVR interrupts, if forcing the use of internal interrupts
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && defined(SDI12_INTERNAL_PCINT)

// Check if the various interrupt vectors are defined.  If they are the ISR is
// instructed to call handleInterrupt() when they trigger.

#if defined(PCINT0_vect)
ISR(PCINT0_vect) {
  SDI12::handleInterrupt();
}
#endif  // PCINT0_vect

#if defined(PCINT1_vect)
ISR(PCINT1_vect) {
  SDI12::handleInterrupt();
}
#endif  // PCINT1_vect

#if defined(PCINT2_vect)
ISR(PCINT2_vect) {
  SDI12::handleInterrupt();
}
#endif  // PCINT2_vect

#if defined(PCINT3_vect)
ISR(PCINT3_vect) {
  SDI12::handleInterrupt();
}
#endif  // PCINT3_vect

#endif  // SDI12_INTERNAL_PCINT
