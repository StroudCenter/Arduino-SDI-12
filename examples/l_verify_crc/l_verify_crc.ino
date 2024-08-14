/**
 * @example{lineno} l_verify_crc.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Ruben Kertesz <github@emnet.net> or \@rinnamon on twitter
 * @date 2/10/2016
 *
 * @brief Example L: Verify CRC
 *
 * This example initiates a measurement anc checks the CRC on the returns.
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

/* connection information */
uint32_t serialBaud    = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin       = 7;      /*!< The pin of the SDI-12 data bus */
int8_t   powerPin      = 22; /*!< The sensor power pin (or -1 if not switching power) */
char     sensorAddress = '2'; /*!< The address of the SDI-12 sensor */

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

  // print out the sensor info
  String command = "";
  command += String(sensorAddress);
  command += "I!";
  mySDI12.sendCommand(command);
  Serial.print(">>>");
  Serial.println(command);
  delay(100);

  sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  // allccccccccmmmmmmvvvxxx...xx<CR><LF>
  Serial.print("<<<");
  Serial.println(sdiResponse);

  Serial.print("Address: ");
  Serial.print(sdiResponse.substring(0, 1));  // address
  Serial.print(", SDI-12 Version: ");
  Serial.print(sdiResponse.substring(1, 3).toFloat() / 10);  // SDI-12 version number
  Serial.print(", Vendor ID: ");
  Serial.print(sdiResponse.substring(3, 11));  // vendor id
  Serial.print(", Sensor Model: ");
  Serial.print(sdiResponse.substring(11, 17));  // sensor model
  Serial.print(", Sensor Version: ");
  Serial.print(sdiResponse.substring(17, 20));  // sensor version
  Serial.print(", Sensor ID: ");
  Serial.print(sdiResponse.substring(20));  // sensor id
  Serial.println();
}

void loop() {
  // first command to take a measurement
  myCommand = String(sensorAddress) + "MC!";
  Serial.print(">>>");
  Serial.println(myCommand);  // echo command to terminal

  mySDI12.sendCommand(myCommand);
  delay(5);

  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of
  // measurments available, 0-9]
  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  Serial.print("<<<");
  Serial.println(sdiResponse);
  mySDI12.clearBuffer();

  // find out how long we have to wait (in seconds).
  uint8_t meas_time_s = sdiResponse.substring(1, 4).toInt();
  Serial.print("expected measurement time: ");
  Serial.print(meas_time_s);
  Serial.print(" s, ");

  // Set up the number of results to expect
  int numResults = sdiResponse.substring(4).toInt();
  Serial.print("Number Results: ");
  Serial.println(numResults);

  // listen for measurement to finish
  unsigned long timerStart = millis();
  while ((millis() - timerStart) < (static_cast<uint32_t>(meas_time_s) + 1) * 1000) {
    if (mySDI12.available())  // sensor can interrupt us to let us know it is done early
    {
      unsigned long measTime = millis() - timerStart;
      Serial.print("<<<");
      Serial.println(mySDI12.readStringUntil('\n'));
      Serial.print("Completed after ");
      Serial.print(measTime);
      Serial.println(" ms");
      break;
    }
  }


  // next command to request data from last measurement
  myCommand = String(sensorAddress) + "D0!";
  Serial.print(">>>");
  Serial.println(myCommand);  // echo command to terminal

  mySDI12.sendCommand(myCommand);
  delay(30);  // wait a while for a response


  sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  Serial.print("<<<");
  Serial.println(sdiResponse);  // write the response to the screen
  bool crcMatch = mySDI12.verifyCRC(sdiResponse);
  if (crcMatch) {
    Serial.println("CRC matches!");
  } else {
    Serial.println("CRC check failed!");
  }
  mySDI12.clearBuffer();
}
