/**
 * @example{lineno} d_simple_logger.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Kevin M.Smith <SDI12@ethosengineering.org>
 * @date August 2013
 *
 * @brief Example D: Check all Addresses for Active Sensors and Log Data
 *
 * This is a simple demonstration of the SDI-12 library for Arduino.
 *
 * It discovers the address of all sensors active on a single bus and takes measurements
 * from them.
 *
 * Every SDI-12 device is different in the time it takes to take a
 * measurement, and the amount of data it returns. This sketch will not serve every
 * sensor type, but it will likely be helpful in getting you started.
 *
 * Each sensor should have a unique address already - if not, multiple sensors may
 * respond simultaenously to the same request and the output will not be readable by the
 * Arduino.
 *
 * To address a sensor, please see Example B: b_address_change.ino
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

uint32_t serialBaud   = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin      = 7;      /*!< The pin of the SDI-12 data bus */
int8_t   powerPin     = 22; /*!< The sensor power pin (or -1 if not switching power) */
uint32_t wake_delay   = 0;  /*!< Extra time needed for the sensor to wake (0-100ms) */
int8_t   firstAddress = 0; /* The first address in the address space to check (0='0') */
int8_t   lastAddress = 62; /* The last address in the address space to check (62='z') */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

// keeps track of active addresses
bool isActive[64] = {
  0,
};

uint8_t numSensors = 0;

/**
 * @brief converts allowable address characters ('0'-'9', 'a'-'z', 'A'-'Z') to a
 * decimal number between 0 and 61 (inclusive) to cover the 62 possible
 * addresses.
 */
byte charToDec(char i) {
  if ((i >= '0') && (i <= '9')) return i - '0';
  if ((i >= 'a') && (i <= 'z')) return i - 'a' + 10;
  if ((i >= 'A') && (i <= 'Z'))
    return i - 'A' + 36;
  else
    return i;
}

/**
 * @brief maps a decimal number between 0 and 61 (inclusive) to allowable
 * address characters '0'-'9', 'a'-'z', 'A'-'Z',
 *
 * THIS METHOD IS UNUSED IN THIS EXAMPLE, BUT IT MAY BE HELPFUL.
 */
char decToChar(byte i) {
  if (i < 10) return i + '0';
  if ((i >= 10) && (i < 36)) return i + 'a' - 10;
  if ((i >= 36) && (i <= 62))
    return i + 'A' - 36;
  else
    return i;
}

/**
 * @brief gets identification information from a sensor, and prints it to the serial
 * port
 *
 * @param i a character between '0'-'9', 'a'-'z', or 'A'-'Z'.
 */
void printInfo(char i) {
  String command = "";
  command += (char)i;
  command += "I!";
  mySDI12.sendCommand(command, wake_delay);
  delay(100);

  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  // allccccccccmmmmmmvvvxxx...xx<CR><LF>
  Serial.print(sdiResponse.substring(0, 1));  // address
  Serial.print(", ");
  Serial.print(sdiResponse.substring(1, 3).toFloat() / 10);  // SDI-12 version number
  Serial.print(", ");
  Serial.print(sdiResponse.substring(3, 11));  // vendor id
  Serial.print(", ");
  Serial.print(sdiResponse.substring(11, 17));  // sensor model
  Serial.print(", ");
  Serial.print(sdiResponse.substring(17, 20));  // sensor version
  Serial.print(", ");
  Serial.print(sdiResponse.substring(20));  // sensor id
  Serial.print(", ");
}

bool getResults(char i, int resultsExpected) {
  uint8_t resultsReceived = 0;
  uint8_t cmd_number      = 0;
  while (resultsReceived < resultsExpected && cmd_number <= 9) {
    String command = "";
    // in this example we will only take the 'DO' measurement
    command = "";
    command += i;
    command += "D";
    command += cmd_number;
    command += "!";  // SDI-12 command to get data [address][D][dataOption][!]
    mySDI12.sendCommand(command, wake_delay);

    uint32_t start = millis();
    while (mySDI12.available() < 3 && (millis() - start) < 1500) {}
    mySDI12.read();           // ignore the repeated SDI12 address
    char c = mySDI12.peek();  // check if there's a '+' and toss if so
    if (c == '+') { mySDI12.read(); }

    while (mySDI12.available()) {
      char c = mySDI12.peek();
      if (c == '-' || (c >= '0' && c <= '9') || c == '.') {
        float result = mySDI12.parseFloat(SKIP_NONE);
        Serial.print(String(result, 10));
        if (result != -9999) { resultsReceived++; }
      } else if (c == '+') {
        mySDI12.read();
        Serial.print(", ");
      } else {
        mySDI12.read();
      }
      delay(10);  // 1 character ~ 7.5ms
    }
    if (resultsReceived < resultsExpected) { Serial.print(", "); }
    cmd_number++;
  }
  mySDI12.clearBuffer();

  return resultsReceived == resultsExpected;
}

bool takeMeasurement(char i, String meas_type = "") {
  mySDI12.clearBuffer();
  String command = "";
  command += i;
  command += "M";
  command += meas_type;
  command += "!";  // SDI-12 measurement command format  [address]['M'][!]
  mySDI12.sendCommand(command, wake_delay);
  delay(100);

  Serial.print(command);
  Serial.print(", ");

  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of
  // measurments available, 0-9]
  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();

  String addr = sdiResponse.substring(0, 1);
  Serial.print(addr);
  Serial.print(", ");

  // find out how long we have to wait (in seconds).
  uint8_t wait = sdiResponse.substring(1, 4).toInt();
  Serial.print(wait);
  Serial.print(", ");

  // Set up the number of results to expect
  int numResults = sdiResponse.substring(4).toInt();
  Serial.print(numResults);
  Serial.print(", ");

  unsigned long timerStart = millis();
  while ((millis() - timerStart) < (1000UL * (wait + 1))) {
    if (mySDI12.available())  // sensor can interrupt us to let us know it is done early
    {
      Serial.print(millis() - timerStart);
      Serial.print(", ");
      mySDI12.clearBuffer();
      break;
    }
  }
  // Wait for anything else and clear it out
  delay(30);
  mySDI12.clearBuffer();

  if (numResults > 0) { return getResults(i, numResults); }

  return true;
}

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
boolean checkActive(char i) {
  String myCommand = "";
  myCommand        = "";
  myCommand += (char)i;  // sends basic 'acknowledge' command [address][!]
  myCommand += "!";

  for (int j = 0; j < 3; j++) {  // goes through three rapid contact attempts
    mySDI12.sendCommand(myCommand, wake_delay);
    delay(100);
    if (mySDI12.available()) {  // If we here anything, assume we have an active sensor
      mySDI12.clearBuffer();
      return true;
    }
  }
  mySDI12.clearBuffer();
  return false;
}

void setup() {
  Serial.begin(serialBaud);
  while (!Serial)
    ;

  Serial.println("Opening SDI-12 bus...");
  mySDI12.begin();
  delay(500);  // allow things to settle

  Serial.println("Timeout value: ");
  Serial.println(mySDI12.TIMEOUT);

  // Power the sensors;
  if (powerPin >= 0) {
    Serial.println("Powering up sensors, wait...");
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    delay(15000L);
    // delay(200);
  }

  // Quickly Scan the Address Space
  Serial.println("Scanning all addresses, please wait...");
  Serial.println("Sensor Address, Protocol Version, Sensor Vendor, Sensor Model, "
                 "Sensor Version, Sensor ID");

  for (byte i = firstAddress; i < lastAddress; i++) {
    char addr = decToChar(i);
    if (checkActive(addr)) {
      numSensors++;
      isActive[i] = 1;
      printInfo(addr);
      Serial.println();
    }
  }
  Serial.print("Total number of sensors found:  ");
  Serial.println(numSensors);

  if (numSensors == 0) {
    Serial.println(
      "No sensors found, please check connections and restart the Arduino.");
    while (true) { delay(10); }  // do nothing forever
  }

  Serial.println();
  Serial.println("Time Elapsed (s), Measurement Type, Sensor Address, Est Measurement "
                 "Time (s), Number Measurements, "
                 "Real Measurement Time (ms), Measurement 1, Measurement 2, ... etc.");
  Serial.println(
    "-------------------------------------------------------------------------------");
}

void loop() {
  String commands[] = {"", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
  for (uint8_t a = 0; a < 1; a++) {
    // measure one at a time
    for (byte i = firstAddress; i < lastAddress; i++) {
      char addr = decToChar(i);
      if (isActive[i]) {
        // Serial.print(millis() / 1000);
        Serial.print(millis());
        Serial.print(", ");
        takeMeasurement(addr, commands[a]);
        Serial.println();
      }
    }
  }

  delay(10000L);  // wait ten seconds between measurement attempts.
}
