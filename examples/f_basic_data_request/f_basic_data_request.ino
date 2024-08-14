/**
 * @example{lineno} f_basic_data_request.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Ruben Kertesz <github@emnet.net> or \@rinnamon on twitter
 * @date 2/10/2016
 *
 * @brief Example F: Basic Data Request to a Single Sensor
 *
 * This is a very basic (stripped down) example where the user initiates a measurement
 * and receives the results to a terminal window without typing numerous commands into
 * the terminal.
 *
 * Edited by Ruben Kertesz for ISCO Nile 502 2/10/2016
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

/* connection information */
uint32_t serialBaud    = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin       = 7;      /*!< The pin of the SDI-12 data bus */
int8_t   powerPin      = 22; /*!< The sensor power pin (or -1 if not switching power) */
char     sensorAddress = '1'; /*!< The address of the SDI-12 sensor */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

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
  do {  // wait for a response from the serial terminal to do anything
    delay(30);
  } while (!Serial.available());
  Serial.read();  // simply hit enter in the terminal window or press send and the
                  // characters get discarded but now the rest of the loop continues

  // first command to take a measurement
  myCommand = String(sensorAddress) + "M!";
  Serial.println(myCommand);  // echo command to terminal

  mySDI12.sendCommand(myCommand);
  delay(30);  // wait a while for a response

  while (mySDI12.available()) {  // build response string
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r')) {
      sdiResponse += c;
      delay(10);  // 1 character ~ 7.5ms
    }
  }
  if (sdiResponse.length() > 1)
    Serial.println(sdiResponse);  // write the response to the screen
  mySDI12.clearBuffer();


  delay(1000);       // delay between taking reading and requesting data
  sdiResponse = "";  // clear the response string


  // next command to request data from last measurement
  myCommand = String(sensorAddress) + "D0!";
  Serial.println(myCommand);  // echo command to terminal

  mySDI12.sendCommand(myCommand);
  delay(30);  // wait a while for a response

  while (mySDI12.available()) {  // build string from response
    char c = mySDI12.read();
    if ((c != '\n') && (c != '\r')) {
      sdiResponse += c;
      delay(10);  // 1 character ~ 7.5ms
    }
  }
  if (sdiResponse.length() > 1)
    Serial.println(sdiResponse);  // write the response to the screen
  mySDI12.clearBuffer();

  // now go back to top and wait until user hits enter on terminal window
}
