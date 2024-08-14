/**
 * @example{lineno} g_terminal_window.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Kevin M.Smith <SDI12@ethosengineering.org>
 * @date August 2013
 * @author Ruben Kertesz <github@emnet.net> or \@rinnamon on twitter
 * @date 2016
 *
 * @brief Example G: Using the Arduino as a Command Terminal for SDI-12 Sensors
 *
 * This is a simple demonstration of the SDI-12 library for Arduino.  It's purpose is to
 * allow a user to interact with an SDI-12 sensor directly, issuing commands through a
 * serial terminal window.
 *
 * Edited by Ruben Kertesz for ISCO Nile 502 2/10/2016
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

uint32_t serialBaud = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin    = 7;      /*!< The pin of the SDI-12 data bus */
int8_t   powerPin   = 22; /*!< The sensor power pin (or -1 if not switching power) */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

char   inByte      = 0;
String sdiResponse = "";
String myCommand   = "";

void setup() {
  Serial.begin(serialBaud);
  while (!Serial)
    ;

  Serial.println("Opening SDI-12 bus...");
  mySDI12.begin();
  delay(500);  // allow things to settle

  // Power the sensors;
  if (powerPin >= 0) {
    Serial.println("Powering up sensors...");
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    delay(200);
  }
}

void loop() {
  if (Serial.available()) {
    inByte = Serial.read();
    if ((inByte != '\n') &&
        (inByte != '\r')) {  // read all values entered in terminal window before enter
      myCommand += inByte;
      delay(10);  // 1 character ~ 7.5ms
    }
  }

  if (inByte == '\r') {  // once we press enter, send string to SDI sensor/probe
    inByte = 0;
    Serial.println(myCommand);
    mySDI12.sendCommand(myCommand);
    delay(30);  // wait a while for a response

    while (mySDI12.available()) {  // build a string of the response
      char c = mySDI12.read();
      if ((c != '\n') && (c != '\r')) {
        sdiResponse += c;
        delay(10);  // 1 character ~ 7.5ms
      }
    }
    if (sdiResponse.length() >= 1)
      Serial.println(sdiResponse);  // write the response to the screen

    mySDI12.clearBuffer();  // clear the line
    myCommand   = "";
    sdiResponse = "";
  }
}
