/**
 * @file SDI12.h
 * @copyright Stroud Water Research Center
 * @license This library is published under the BSD-3 license.
 * @date August 2013
 * @author Kevin M.Smith <SDI12@ethosengineering.org>
 *
 * @brief This file contains the main class for the SDI-12 implementation.
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
/**
 * @page specifications Notes on SDI-12, Specification v1.4
 *
 * @tableofcontents
 *
 * @section overview Overview
 *
 * SDI-12 is a communications protocol that uses a single data wire to communicate with
 * up to 62 uniquely addressed sensors.  So long as each sensor supports SDI-12, mixed
 * sensor types can appear on the same data bus.  Each address is a single character.
 * The valid ranges are 0-9, a-z, and A-Z. Only the datalogger can initiate
 * communications on the data bus.
 *
 * It does so by pulling the data line into a 5v state for at least 12 milliseconds to
 * wake up all the sensors, before returning the line into a 0v state for 8 milliseconds
 * announce an outgoing command.  The command contains both the action to be taken, and
 * the address of the device who should respond.  If there is a sensor on the bus with
 * that address, it is responsible for responding to the command.  Sensors should ignore
 * commands that were not issued to them, and should return to a sleep state until the
 * datalogger again issues the wakeup sequence.
 *
 * @section connection_details Connection Details
 *
 * **Physical Connections:**
 *   - 1 data line (0v - 5.5v)
 *   - 1 12v power line (9.6v - 16v)
 *   - 1 ground line
 *
 * **Baud Rate:**
 *   - 1200 bits per second
 *
 * **Data Frame Format:**
 *   - 10 bits per data frame
 *   - 1 start bit
 *   - 7 data bits (least significant bit first)
 *   - 1 even parity bit
 *   - 1 stop bit
 *
 * Data Line:  SDI-12 communication uses a single bi-directional data line with
 * three-state, inverse logic.
 *
 * | LINE CONDITION | BINARY STATE | VOLTAGE RANGE     |
 * |----------------|--------------|-------------------|
 * | marking        |      1       | -0.5 to 1.0 volts |
 * | spacing        |      0       | 3.5 to 5.5 volts  |
 * | transition     |  undefined   | 1.0 to 3.5 volts  |
 *
 * While a series of bits is being transmitted on the dataline, the voltage level on
 * that line might look something like this:
 *
 * @code
 *       _____       _____       _____       _____       _____     spacing
 * 5v   |     |     |     |     |     |     |     |     |     |
 *      |  0  |  1  |  0  |  1  |  0  |  1  |  0  |  1  |  0  | transition
 * Ov___|     |_____|     |_____|     |_____|     |_____|     |___ marking
 * @endcode
 *
 * @note Although the specification gives these voltages, some manufacturers chose to
 * implement SDI-12 at other logic levels - ie, with spacing voltages lower or higher
 * than the specified ~5V.
 *
 * For more information, and for a list of commands and responses, please see
 * SDI-12.org, official site of the SDI-12 Support Group.
 */
/*** ==================== Code Organization ======================
 * - Includes, Defines, & Variable Declarations
 * - Buffer Setup
 * - Reading from the SDI-12 Buffer
 * - Constructor, Destructor, Begins, and Setters
 * - Using more than one SDI-12 object, isActive() and setActive()
 * - Setting Proper Data Line States
 * - Waking up and Talking to the Sensors
 * - Interrupt Service Routine (getting the data into the buffer)
 */


#ifndef SRC_SDI12_H_
#define SRC_SDI12_H_

//  Import Required Libraries

#include <inttypes.h>      // integer types library
#include <Arduino.h>       // Arduino core library
#include <Stream.h>        // Arduino Stream library
#include "SDI12_boards.h"  //  Include timer information

/// Helper for strings stored in flash
typedef const __FlashStringHelper* FlashString;

/// a char not found in a valid ASCII numeric field
#define NO_IGNORE_CHAR '\x01'

#ifndef SDI12_IGNORE_PARITY
/**
 * @brief Check the value of the parity bit on reception
 */
#define SDI12_CHECK_PARITY
#endif

#ifndef SDI12_WAKE_DELAY
/**
 * @brief The amount of additional time in milliseconds that the sensor takes to wake
 * before being ready to receive a command.  Default is 0ms - meaning the sensor is
 * ready for a command by the end of the 12ms break.  Per protocol, the wake time must
 * be less than 100 ms.
 */
#define SDI12_WAKE_DELAY 0
#endif

#ifndef SDI12_BUFFER_SIZE
/**
 * @brief The buffer size for incoming SDI-12 data.
 *
 * All responses should be less than 81 characters:
 * - address is a single (1) character
 * - values has a maximum value of 75 characters
 * - CRC is 3 characters
 * - CR is a single character
 * - LF is a single character
 */
#define SDI12_BUFFER_SIZE 81
#endif

#ifndef SDI12_YIELD_MS
/**
 * @brief The time to delay, in milliseconds, to allow the buffer to fill before
 * returning the value from the buffer.
 *
 * This may be needed for faster processors to account for the slow baud rate of SDI-12.
 * Without this, the available() function may return 0 while we're in the middle of
 * reading a character.
 *
 * There are 8.33 ms/character, so we delay by 8ms for fast processors to allow one
 * character to finish.
 */
#if F_CPU >= 48000000L
#define SDI12_YIELD_MS 8
#else
#define SDI12_YIELD_MS 0
#endif
#endif

#ifndef SDI12_YIELD
/**
 * @brief A delay function to allow the buffer to fill before returning the value from
 * the buffer.
 *
 * This may be needed for faster processors to account for the slow baud rate of SDI-12.
 */
#define SDI12_YIELD() \
  { delay(SDI12_YIELD_MS); }
#endif

#if defined(ESP32) || defined(ESP8266)
/**
 * @brief This enumeration provides the lookahead options for parseInt(), parseFloat().
 *
 * The rules set out here are used until either the first valid character is found or a
 * time out occurs due to lack of input.
 *
 * This enum is part of the Stream parent class, but is missing from the ESP8266 and
 * ESP32 cores.
 */
enum LookaheadMode {
  /** All invalid characters are ignored. */
  SKIP_ALL,
  /** Nothing is skipped, and the stream is not touched unless the first waiting
     character is valid. */
  SKIP_NONE,
  /** Only tabs, spaces, line feeds & carriage returns are skipped.*/
  SKIP_WHITESPACE
};
#endif  // defined(ESP32) || defined(ESP8266)


#if not defined(SDI12_INTERNAL_PCINT) && not defined(SDI12_EXTERNAL_PCINT)
/**
 * @brief Define to use internal interrupt handing.
 *
 * On AVR boards, comment this out to use built-in pin-change interrupt handling rather
 * than using
 *
 * @note There's an extra check to make sure SDI12_EXTERNAL_PCINT isn't defined twice.
 */
#define SDI12_EXTERNAL_PCINT
#endif

#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
// Unless we're forcing use of internal interrupts, use EnableInterrupt for AVR boards
#define LIBCALL_ENABLEINTERRUPT  // To prevent compiler/linker crashes
#include <EnableInterrupt.h>     // To handle external and pin change interrupts

#elif not defined(__AVR__) && not defined(ARDUINO_ARCH_AVR)
// For compatibility with non AVR boards, we need these macros
#define enableInterrupt(pin, userFunc, mode) \
  attachInterrupt(digitalPinToInterrupt(pin), userFunc, mode)
#define disableInterrupt(pin) detachInterrupt(digitalPinToInterrupt(pin))
#endif

/**
 * @brief The main class for SDI 12 instances
 */
class SDI12 : public Stream {
  /**
   * @anchor sdi12_statics
   * @name Static member variables
   *
   * @brief These are constants that apply to all SDI-12 instances.
   */
  /**@{*/
 private:
  /**
   * @brief static pointer to active SDI12 instance
   */
  static SDI12* _activeObject;
  /**
   * @brief The SDI12Timer instance to use for checking bit reception times.
   */
  static SDI12Timer sdi12timer;
  /**
   * @brief The size of a bit in microseconds
   *
   * 1200 baud = 1200 bits/second ~ 833.333 µs/bit
   */
  static const uint16_t bitWidth_micros;
  /**
   * @brief The required "break" before sending commands, >= 12ms
   *
   */
  static const uint16_t lineBreak_micros;
  /**
   * @brief The required mark before a command or response, >= 8.33ms
   */
  static const uint16_t marking_micros;

  /**
   * @brief the width of a single bit in "ticks" of the cpu clock.
   */
  static const sdi12timer_t txBitWidth;
  /**
   * @brief A mask for the #rxState while waiting for a start bit; 0b11111111
   */
  static const uint8_t WAITING_FOR_START_BIT;

  /**
   * @brief Stores the time of the previous RX transition in micros
   */
  static sdi12timer_t prevBitTCNT;
  /**
   * @brief Tracks how many bits are accounted for on an incoming character.
   *
   * - if 0: indicates that we got a start bit
   * - if >0: indicates the number of bits received
   *
   * 0 - got start bit
   * 1 - got data bit 0
   * 2 - got data bit 1
   * 3 - got data bit 2
   * 4 - got data bit 3
   * 5 - got data bit 4
   * 6 - got data bit 5
   * 7 - got data bit 6
   * 8 - got data bit 7 (parity)
   * 9 - got stop bit
   * 255 - waiting for next start bit
   */
  static uint8_t rxState;
  /**
   * @brief a bit mask for building a received character
   *
   * The mask has a single bit set, in the place of the active bit based on the
   * #rxState.
   */
  static uint8_t rxMask;
  /**
   * @brief the value of the character being built
   */
  static uint8_t rxValue;
  /**@}*/


  /**
   * @anchor sdi12_buffer
   * @name Buffer Setup
   *
   * @brief Creating a circular buffer for incoming data.
   *
   * The buffer is used to store characters from the SDI-12 data line.  Characters are
   * read into the buffer when an interrupt is received on the data line. The buffer
   * uses a circular implementation with pointers to both the head and the tail. All
   * SDI-12 instances share the same buffer.
   *
   * The default buffer size is the maximum length of a response to a normal SDI-12
   * command, which is 81 characters:
   * - address is a single (1) character
   * - values has a maximum value of 75 characters
   * - CRC is 3 characters
   * - CR is a single character
   * - LF is a single character
   *
   * For more information on circular buffers:
   * http://en.wikipedia.org/wiki/Circular_buffer
   */
  /**@{*/
 private:
  /**
   * @brief A single incoming character buffer for ALL SDI-12 objects (Rx buffer)
   *
   * Increasing the buffer size will use more RAM. If you exceed 256 characters, be sure
   * to change the data type of the index to support the larger range of addresses.  To
   * adjust the size of the buffer, change the value of `SDI12_BUFFER_SIZE` in the
   * header file.
   */
  static uint8_t _rxBuffer[SDI12_BUFFER_SIZE];
  /**
   * @brief Index of buffer head. (unsigned 8-bit integer, can map from 0-255)
   */
  static volatile uint8_t _rxBufferTail;
  /**
   * @brief Index of buffer tail. (unsigned 8-bit integer, can map from 0-255)
   */
  static volatile uint8_t _rxBufferHead;
  /**
   * @brief The buffer overflow status
   */
  bool _bufferOverflow = false;
  /**@}*/


  /**
   * @anchor reading_buffer
   * @name Reading from the SDI-12 Buffer
   *
   * @brief These functions are for reading incoming data stored in the SDI-12 buffer.
   *
   * @see <a href="class_s_d_i12.html#buffer-setup">Buffer Setup</a>
   *
   * @note peakNextDigit(), parseInt() and parseFloat() are fully implemented in the
   * parent Stream class but we don't want to them use as they are inherited.  Although
   * they are not virtual and cannot be overridden, recreating them here hides the
   * stream default versions to allow for a custom timeout return value.  The default
   * value for the Stream class is to return 0.  This makes distinguishing timeouts from
   * true zero readings impossible. Therefore the default value has been set to -9999 in
   * the being function.  The value returned by a timeout (SDI12::TIMEOUT) is a public
   * variable and can be changed dynamically within a program by calling
   * `mySDI12.TIMEOUT = (int) newValue` or using SDI12::setTimeoutValue(int16_t value).
   *
   */
  /**@{*/
 public:
  /**
   * @brief Return the number of bytes available in the Rx buffer
   *
   * @return The number of characters in the buffer
   *
   * available() is a public function that returns the number of characters available in
   * the Rx buffer.
   *
   * To understand how:
   * `_rxBufferTail + SDI12_BUFFER_SIZE - _rxBufferHead) % SDI12_BUFFER_SIZE;`
   * accomplishes this task, we will use a few examples.
   *
   * To start take the buffer below that has `SDI12_BUFFER_SIZE = 10`. The message
   * "abc" has been wrapped around (circular buffer).
   *
   * @code{.cpp}
   *     _rxBufferTail = 1 // points to the '-' after c
   *     _rxBufferHead = 8 // points to 'a'
   * @endcode
   *
   * [ c ] [ - ] [ - ] [ - ] [ - ] [ - ] [ - ] [ - ]  [ a ] [ b ]
   *
   * The number of available characters is (1 + 10 - 8) % 10 = 3
   *
   * The '%' or modulo operator finds the remainder of division of one number by
   * another. In integer arithmetic 3 / 10 = 0, but has a remainder of 3.  We can only
   * get the remainder by using the the modulo '%'. 3 % 10 = 3.  This next case
   * demonstrates more clearly why the modulo is used.
   *
   * @code{.cpp}
   *     _rxBufferTail = 4 // points to the '-' after c
   *     _rxBufferHead = 1 // points to 'a'
   * @endcode
   *
   * [ a ] [ b ] [ c ] [ - ] [ - ] [ - ] [ - ] [ - ]  [ - ] [ - ]
   *
   * The number of available characters is (4 + 10 - 1) % 10 = 3
   *
   * If we did not use the modulo we would get either ( 4 + 10 - 1 ) = 13 characters or
   * ( 4 + 10 - 1 ) / 10 = 1 character. Obviously neither is correct.
   *
   * If there has been a buffer overflow, available() will return -1.
   */
  int available() override;
  /**
   * @brief Reveal next byte in the Rx buffer without consuming it.
   *
   * @return The next byte in the character buffer.
   *
   * peek() is a public function that allows the user to look at the character that is
   * at the head of the buffer. Unlike read() it does not consume the character (i.e.
   * the index addressed by _rxBufferHead is not changed). peek() returns -1 if there
   * are no characters to show.
   */
  int peek() override;
  /**
   * @brief Clear the Rx buffer by setting the head and tail pointers to the same value.
   *
   * clearBuffer() is a public function that clears the buffers contents by setting the
   * index for both head and tail back to zero.
   */
  void clearBuffer();
  /**
   * @brief Return next byte in the Rx buffer, consuming it
   *
   * @return The next byte in the character buffer.
   *
   * read() returns the character at the current head in the buffer after incrementing
   * the index of the buffer head. This action 'consumes' the character, meaning it can
   * not be read from the buffer again. If you would rather see the character, but leave
   * the index to head intact, you should use peek();
   */
  int read() override;

  /**
   * @brief Wait for sending to finish - because no TX buffering, does nothing
   */
  void flush() override {}

  /**
   * @brief Return the first valid (long) integer value from the current position.
   *
   * The value of lookahead determines how parseInt looks ahead in the stream.
   * See LookaheadMode enumeration at the top of the file.
   * Lookahead is terminated by the first character that is not a valid part of an
   * integer.
   * Once parsing commences, 'ignore' will be skipped in the stream.
   *
   * @param lookahead the mode to use to look ahead in the
   * stream, default is LookaheadMode::SKIP_ALL
   * @param ignore a character to ignore in the stream, default is '\\x01'
   * @return The first valid integer in the stream
   *
   * @note This function _hides_ the Stream class function to allow a custom value to be
   * returned on timeout.  It cannot overwrite the Stream function because it is not
   * virtual.
   * @see @ref SDI12::LookaheadMode
   */
  long parseInt(LookaheadMode lookahead = SKIP_ALL, char ignore = NO_IGNORE_CHAR);

  /**
   * @brief Return the first valid float value from the current position.
   *
   * The value of lookahead determines how parseInt looks ahead in the stream.
   * See LookaheadMode enumeration at the top of the file.
   * Lookahead is terminated by the first character that is not a valid part of an
   * integer.
   * Once parsing commences, 'ignore' will be skipped in the stream.
   *
   * @param lookahead the mode to use to look ahead in the
   * stream, default is LookaheadMode::SKIP_ALL
   * @param ignore a character to ignore in the stream, default is '\\x01'
   * @return The first valid float in the stream
   *
   * @note This function _hides_ the Stream class function to allow a custom value to be
   * returned on timeout.  It cannot overwrite the Stream function because it is not
   * virtual.
   * @see @ref SDI12::LookaheadMode
   */
  float parseFloat(LookaheadMode lookahead = SKIP_ALL, char ignore = NO_IGNORE_CHAR);

 protected:
  /**
   * @brief Return the next numeric digit in the stream or -1 if timeout
   *
   * @param lookahead the mode to use to look ahead in the
   * stream
   * @param detectDecimal True to accept a decimal point ('.') as part of a
   * number
   * @return The next numeric digit in the stream
   */
  int peekNextDigit(LookaheadMode lookahead, bool detectDecimal);
  /**@}*/


  /**
   * @anchor ctor
   * @name Constructor, Destructor, Begins, and Setters
   *
   * @brief These functions set up the SDI-12 object and prepare it for use.
   */
  /**@{*/
 private:
  /**
   * @brief reference to the data pin
   */
  int8_t _dataPin = -1;

 public:
  /**
   * @brief Construct a new SDI12 instance with no data pin set.
   *
   * Before using the SDI-12 instance, the data pin must be set with
   * SDI12::setDataPin(dataPin) or SDI12::begin(dataPin). This empty constructor is
   * provided for easier integration with other Arduino libraries.
   *
   * When the constructor is called it resets the buffer overflow status to FALSE.
   */
  SDI12();
  /**
   * @brief Construct a new SDI12 with the data pin set
   *
   * @param dataPin The data pin's digital pin number
   *
   * When the constructor is called it resets the buffer overflow status to FALSE and
   * assigns the pin number "dataPin" to the private variable "_dataPin".
   */
  explicit SDI12(int8_t dataPin);
  /**
   * @brief Destroy the SDI12 object.
   *
   * When the destructor is called, it's main task is to disable any interrupts that had
   * been previously assigned to the pin, so that the pin will behave as expected when
   * used for other purposes. This is achieved by putting the SDI-12 object in the
   * SDI12_DISABLED state.  After disabling interrupts, the pointer to the current
   * active SDI-12 instance is set to null if it had pointed to the destroyed object.
   * Finally, for AVR board, the timer prescaler is set back to whatever it had been
   * prior to creating the SDI-12 object.
   */
  ~SDI12();
  /**
   * @brief Begin the SDI-12 object.
   *
   * This is called to begin the functionality of the SDI-12 object.  It sets the object
   * as the active object, sets the stream timeout to 150ms to match SDI-12 specs, sets
   * the timeout return value to SDI12::TIMEOUT, and configures the timer prescaler.
   */
  void begin();
  /**
   * @brief Set the SDI12::_datapin and begin the SDI-12 object.
   *
   * @copydetails SDI12::begin()
   * If the SDI-12 instance is created using the empty constuctor, this must be used
   * to set the data pin.
   *
   * @param dataPin The data pin's digital pin number
   */
  void begin(int8_t dataPin);
  /**
   * @brief Disable the SDI-12 object (but do not destroy it).
   *
   * Set the SDI-12 state to disabled, set the pointer to the current active instance
   * to null, and then, for AVR boards, unset the timer prescaler.
   *
   * This can be called to temporarily cease all functionality of the SDI-12 object. It
   * is not as harsh as destroying the object with the destructor, as it will maintain
   * the memory buffer.
   */
  void end();
  /**
   * @brief The value to return if a parse or read times out with no return from the
   * sensor.
   *
   * The timeout return for an Arduino stream object when no character is available in
   * the Rx buffer is "0."  For enviromental sensors (the typical SDI-12 users) 0 is a
   * common result value.  To better distinguish between a timeout because of no
   * sensor response and a true zero return, the timeout should be set to some value
   * that is NOT a possible return from that sensor.  If the timeout is not set, -9999
   * is used.
   */
  int16_t TIMEOUT;
  /**
   * @brief Set the value to return if a parse int or parse float times out with no
   * return from the sensor.
   *
   * The "standard" timeout return for an Arduino stream object when no character is
   * available in the Rx buffer is "0."  For enviromental sensors (the typical SDI-12
   * users) 0 is a common result value.  To better distinguish between a timeout because
   * of no sensor response and a true zero return, the timeout should be set to some
   * value that is NOT a possible return from that sensor.  If the timeout is not set,
   * -9999 is used.
   *
   * @param value the value to return on timeout
   */
  void setTimeoutValue(int16_t value);
  /**
   * @brief Get the data pin for the current SDI-12 instance
   *
   * @return The data pin number
   */
  int8_t getDataPin();
  /**
   * @brief Set the data pin for the current SDI-12 instance
   *
   * @param dataPin  The data pin's digital pin number
   */
  void setDataPin(int8_t dataPin);
#ifdef SDI12_CHECK_PARITY
  bool _parityFailure;
#endif
  /**@}*/


  /**
   * @anchor multiple_objects
   * @name Using more than one SDI-12 Object
   *
   * @brief Functions needed for multiple instances of the SDI12 class.
   *
   * This library is allows for multiple instances of itself running on the same or
   * different pins.  SDI-12 can support up to 62 sensors on a single pin/bus, so it is
   * not necessary to use an instance for each sensor.
   *
   * Because we are using pin change interrupts there can only be one active object at a
   * time (since this is the only reliable way to determine which pin the interrupt
   * occurred on).  The active object is the only object that will respond properly to
   * interrupts.  However promoting another instance to Active status does not
   * automatically remove the interrupts on the other pin. For proper behavior it is
   * recommended to use this pattern:
   *
   * @code{.cpp}
   *     mySDI12.forceHold();
   *     myOtherSDI12.setActive();
   * @endcode
   *
   * @note
   * - Promoting an object into the Active state will set it as `SDI12_HOLDING`.
   * - Calling mySDI12.begin() will assert mySDI12 as the new active object, until
   * another instance calls myOtherSDI12.begin() or myOtherSDI12.setActive().
   * - Calling mySDI12.end() does NOT hand-off active status to another SDI-12 instance.
   * - You can check on the active object by calling mySDI12.isActive(), which will
   * return a boolean value TRUE if active or FALSE if inactive.
   */
  /**@{*/
 public:
  /**
   * @brief Set this instance as the active SDI-12 instance
   *
   * @return True indicates that the current SDI-12
   * instance was not formerly the active one and now is.  False indicates that the
   * current SDI-12 instance *is already the active one* and the state was not changed.
   *
   * A method for setting the current object as the active object; returns TRUE if
   * the object was not formerly the active object and now is.
   * - Promoting an inactive to the active instance will start it in the SDI12_HOLDING
   * state and return TRUE.
   * - Otherwise, if the object is currently the active instance, it will remain
   * unchanged and return FALSE.
   */
  bool setActive();

  /**
   * @brief Check if this instance is active
   *
   * @return True indicates that the curren SDI-12
   * instace is the active one.
   *
   * isActive() is a method for checking if the object is the active object.  Returns
   * true if the object is currently the active object, false otherwise.
   */
  bool isActive();
  /**@}*/


  /**
   * @anchor line_states
   * @name Data Line States
   *
   * @brief Functions for maintaining the proper data line state.
   *
   * The Arduino is responsible for managing communication with the sensors.  Since all
   * the data transfer happens on the same line, the state of the data line is very
   * important.
   *
   * @section line_state_spec Specifications
   *
   * Per the SDI-12 specification, the voltage ranges for SDI-12 are:
   *
   * - When the pin is in the SDI12_HOLDING state, it is holding the line LOW so that
   * interference does not unintentionally wake the sensors up.  The interrupt is
   * disabled for the dataPin, because we are not expecting any SDI-12 traffic.
   * - In the SDI12_TRANSMITTING state, we would like exclusive control of the Arduino,
   * so we shut off all interrupts, and vary the voltage of the dataPin in order to wake
   * up and send commands to the sensor.
   * - In the SDI12_LISTENING state, we are waiting for a sensor to respond, so we drop
   * the voltage level to LOW and relinquish control (INPUT).
   * - If we would like to disable all SDI-12 functionality, then we set the system to
   * the SDI12_DISABLED state, removing the interrupt associated with the dataPin.  For
   * predictability, we set the pin to a LOW level high impedance state (INPUT).
   *
   * @section line_state_table As a Table
   *
   * Summarized in a table:
   *
   * | State               | Interrupts       | Pin Mode   | Pin Level |
   * |---------------------|------------------|------------|-----------|
   * | SDI12_DISABLED      | Pin Disable      | INPUT      | ---       |
   * | SDI12_ENABLED       | Pin Disable      | INPUT      | ---       |
   * | SDI12_HOLDING       | Pin Disable      | OUTPUT     | LOW       |
   * | SDI12_TRANSMITTING  | All/Pin Disable  | OUTPUT     | VARYING   |
   * | SDI12_LISTENING     | All Enable       | INPUT      | ---       |
   *
   *
   * @section line_state_seq Sequencing
   *
   * Generally, this flow of line states is acceptable:
   *
   * `HOLDING --> TRANSMITTING --> LISTENING --> TRANSMITTING --> LISTENING`
   *
   * If you have interference, you should force a hold, using forceHold().
   * The flow would then be:
   *
   * `HOLDING --> TRANSMITTING --> LISTENING -->` done reading, forceHold() `--->
   * HOLDING`
   *
   * @see For a detailed explanation of interrupts see @ref interrupts_page
   */
  /**@{*/
 private:
  /**
   * @brief The various SDI-12 line states.
   */
  typedef enum SDI12_STATES {
    /** SDI-12 is disabled, pin mode INPUT, interrupts disabled for the pin */
    SDI12_DISABLED,
    /** SDI-12 is enabled, pin mode INPUT, interrupts disabled for the pin */
    SDI12_ENABLED,
    /** The line is being held LOW, pin mode OUTPUT, interrupts disabled for the pin */
    SDI12_HOLDING,
    /** Data is being transmitted by the SDI-12 master, pin mode OUTPUT, interrupts
       disabled for the pin */
    SDI12_TRANSMITTING,
    /** The SDI-12 master is listening for a response from the slave, pin mode INPUT,
       interrupts enabled for the pin */
    SDI12_LISTENING
  } SDI12_STATES;

#ifndef __AVR__
  /**
   * @brief Calculate the parity value for a character using even parity.
   *
   * @param v **uint8_t (char)** the character to calculate the parity of
   * @return The input character with the 8th bit set
   * to the even parity value for that character
   *
   * Sets up parity and interrupts for different processor types - that is, imports the
   * interrupts and parity for the AVR processors where they exist.
   *
   * This function is defined in the Arduino core for AVR processors, but must be
   * defined here for SAMD and ESP cores.
   */
  static uint8_t parity_even_bit(uint8_t v);
#endif

  /**
   * @brief Set the pin interrupts to be on (enabled) or off (disabled)
   *
   * @param enable True to enable pin interrupts
   *
   * A private helper function to turn pin interupts on or off
   */
  void setPinInterrupts(bool enable);
  /**
   * @brief Set the the state of the SDI12 object[s]
   *
   * @param state The state the SDI-12 object should be set to, from
   * the SDI12_STATES enum.
   *
   * This is a private function, and only used internally.
   */
  void setState(SDI12_STATES state);

 public:
  /**
   * @brief Set line state to SDI12_HOLDING
   *
   * A public function which forces the line into a "holding" state. This is generally
   * unneeded, but for deployments where interference is an issue, it should be used
   * after all expected bytes have been returned from the sensor.
   */
  void forceHold();
  /**
   * @brief Set line state to SDI12_LISTENING
   *
   * A public function which forces the line into a "listening" state.  This may be
   * needed for implementing a slave-side device, which should relinquish control of the
   * data line when not transmitting.
   */
  void forceListen();
  /**@}*/


  /**
   * @anchor communication
   * @name Waking Up and Talking To Sensors
   *
   * @brief These functions are needed to communicate with SDI-12 sensors (slaves) or an
   * SDI-12 datalogger (master).
   */
  /**@{*/
 private:
  /**
   * @brief Used to wake up the SDI12 bus.
   *
   * @param extraWakeTime The amount of additional time in milliseconds that the sensor
   * takes to wake before being ready to receive a command.  Default is 0ms - meaning
   * the sensor is ready for a command by the end of the 12ms break.  Should be lower
   * than 100.
   *
   * Wakes up all the sensors on the bus.  Set the SDI-12 state to transmitting, hold
   * the data line high for the required break of 12 milliseconds plus any needed
   * additional delay to allow the sensor to wake, then hold the line low for the
   * required marking of 8.33 milliseconds.
   *
   * The SDI-12 protocol requires a pulse of HIGH voltage for at least 12 milliseconds
   * (the break) followed immediately by a pulse of LOW voltage for at least 8.33, but
   * not more than 100, milliseconds. Setting the SDI-12 object into the
   * SDI12_TRANSMITTING allows us to assert control of the line without triggering any
   * interrupts.
   *
   * Per specifications:
   * > • A data recorder transmits a break by setting the data line to spacing for at
   * > least 12 milliseconds.
   * >
   * > • The sensor will not recognize a break condition for a continuous spacing time
   * > of less than 6.5 milliseconds and will always recognize a break when the line is
   * > continuously spacing for more than 12 milliseconds.
   *
   * > • Upon receiving a break, a sensor must detect 8.33 milliseconds of marking on
   * > the data line before it looks for an address.
   * >
   * > • A sensor must wake up from a low-power standby mode and be capable of detecting
   * > a start bit from a valid command within 100 milliseconds after detecting a break
   * >
   * > • Sensors must return to a low-power standby mode after receiving an invalid
   * > address or after detecting a marking state on the data line for 100 milliseconds.
   * > (Tolerance:    +0.40 milliseconds.)
   */
  void wakeSensors(int8_t extraWakeTime = 0);
  /**
   * @brief Used to send a character out on the data line
   *
   * @param out **uint8_t (char)** the character to write
   *
   * This function writes a character out to the data line.  SDI-12 specifies the
   * general transmission format of a single character as:
   * - 10 bits per data frame
   *     - 1 start bit
   *     - 7 data bits (least significant bit first)
   *     - 1 even parity bit
   *     - 1 stop bit
   *
   * Recall that we are using inverse logic, so HIGH represents 0, and LOW represents
   * a 1.
   */
  void writeChar(uint8_t out);

 public:
  /**
   * @brief Write out a byte on the SDI-12 line
   *
   * @param byte The character to write
   * @return The number of characters written
   *
   * Sets the state to transmitting, writes a character, and then sets the state back to
   * listening.  This function must be implemented as part of the Arduino Stream
   * instance, but is *NOT* intenteded to be used for SDI-12 objects.  Instead, use the
   * SDI12::sendCommand() or SDI12::sendResponse() functions.
   */
  virtual size_t write(uint8_t byte);

  /**
   * @brief Send a command out on the data line, acting as a datalogger (master)
   *
   * @param cmd the command to send
   *
   * A publicly accessible function that sends a break to wake sensors and sends out a
   * command byte by byte on the data line.
   *
   * @param extraWakeTime The amount of additional time in milliseconds that the sensor
   * takes to wake before being ready to receive a command.  Default is 0ms - meaning
   * the sensor is ready for a command by the end of the 12ms break.  Per protocol, the
   * wake time must be less than 100 ms.
   */
  void sendCommand(String& cmd, int8_t extraWakeTime = SDI12_WAKE_DELAY);
  /// @copydoc SDI12::sendCommand(String&, int8_t)
  void sendCommand(const char* cmd, int8_t extraWakeTime = SDI12_WAKE_DELAY);
  /// @copydoc SDI12::sendCommand(String&, int8_t)
  void sendCommand(FlashString cmd, int8_t extraWakeTime = SDI12_WAKE_DELAY);

  /**
   * @brief Calculates the 16-bit Cyclic Redundancy Check (CRC) for an SDI-12 message.
   *
   * @param resp The message to calculate the CRC for.
   * @return *uint16_t* The calculated CRC
   */
  uint16_t calculateCRC(String& resp);
  /// @copydoc SDI12::calculateCRC(String&)
  uint16_t calculateCRC(const char* resp);
  /// @copydoc SDI12::calculateCRC(String&)
  uint16_t calculateCRC(FlashString resp);

  /**
   * @brief Converts a numeric 16-bit CRC to an ASCII String.
   *
   * From the SDI-12 Specifications:
   *
   *     The 16 bit CRC is encoded as three ASCII characters
   *     using the following algorithm:
   *         1st character = 0x40 OR (CRC shifted right 12 bits)
   *         2nd character = 0x40 OR ((CRC shifted right 6 bits) AND 0x3F)
   *         3rd character = 0x40 OR (CRC AND 0x3F)
   *
   * @param crc The 16-bit CRC
   * @return *String* An ASCII string for the CRC
   */
  String crcToString(uint16_t crc);

  /**
   * @brief Verifies that the CRC returned at the end of an SDI-12 message matches that
   * of the content of the message.
   *
   * @param respWithCRC The full SDI-12 message, including the CRC at the end.
   * @return True if the CRC matches and the message is valid, false if the CRC doesn't
   * match and the message could be retried.
   */
  bool verifyCRC(String& respWithCRC);

  /**
   * @brief Send a response out on the data line (for slave use)
   *
   * @param resp the response to send
   * @param addCRC True to append a CRC to the outgoing response
   *
   * A publicly accessible function that sends out an 8.33 ms marking and a response
   * byte by byte on the data line.  This is needed if the Arduino is acting as an
   * SDI-12 device itself, not as a recorder for another SDI-12 device.
   */
  void sendResponse(String& resp, bool addCRC = false);
  /// @copydoc SDI12::sendResponse(String& resp, bool addCRC)
  void sendResponse(const char* resp, bool addCRC = false);
  /// @copydoc SDI12::sendResponse(String& resp, bool addCRC)
  void sendResponse(FlashString resp, bool addCRC = false);
  ///@}

  /**
   * @anchor interrupt_fxns
   * @name Interrupt Service Routine
   *
   * @brief Functions for handling interrupts - responding to changes on the data line
   * and converting them to characters in the Rx buffer.
   *
   * @see For a detailed explanation of interrupts see @ref interrupts_page
   */
  /**@{*/
 private:
  /**
   * @brief Creates a blank slate for a new incoming character
   */
  void startChar();
  /**
   * @brief The interrupt service routine (ISR) - the function responding to changes in
   * rx line state.
   *
   * This function checks which direction the change of the interrupt was and then uses
   * that to populate the bits of the character. Unlike SoftwareSerial which listens for
   * a start bit and then halts all program and other ISR execution until the end of the
   * character, this library grabs the time of the interrupt, does some quick math, and
   * lets the processor move on.  The logic of creating a character this way is harder
   * for a person to follow, but it pays off because we're not tieing up the processor
   * in an ISR that lasts for 8.33ms for each character. [10 bits @ 1200 bits/s] For a
   * person, that 8.33ms is trivial, but for even a "slow" 8MHz processor, that's over
   * 60,000 ticks sitting idle per character.
   */
  void receiveISR();
  /**
   * @brief Put a finished character into the SDI12 buffer
   *
   * @param c **uint8_t (char)** the character to add to the buffer
   */
  void charToBuffer(uint8_t c);

 public:
  /**
   * @brief Intermediary used by the ISR - passes off responsibility for the interrupt
   * to the active object.
   *
   * On espressif boards (ESP8266 and ESP32), the ISR must be stored in IRAM
   */
  static void handleInterrupt();
  /**@}*/
};

#endif  // SRC_SDI12_H_
