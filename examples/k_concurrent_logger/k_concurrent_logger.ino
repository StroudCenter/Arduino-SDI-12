/**
 * @example{lineno} k_concurrent_logger.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Sara Geleskie Damiano <sdamiano@stroudcenter.org>
 *
 * @brief Example K:  Concurrent Measurements
 *
 * This is very similar to example B - finding all attached sensors and logging data
 * from them. Unlike example B, however, which waits for each sensor to complete a
 * measurement, this asks all sensors to take measurements concurrently and then waits
 * until each is finished to query for results. This can be much faster than waiting for
 * each sensor when you have multiple sensor attached.
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

/* connection information */
uint32_t serialBaud   = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin      = 7;      /*!< The pin of the SDI-12 data bus */
int8_t   powerPin     = 22; /*!< The sensor power pin (or -1 if not switching power) */
int8_t   firstAddress = 0; /* The first address in the address space to check (0='0') */
int8_t   lastAddress = 62; /* The last address in the address space to check (62='z') */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

// keeps track of active addresses
bool isActive[64];

// keeps track of the wait time for each active addresses
uint8_t meas_time_ms[64];

// keeps track of the time each sensor was started
uint32_t millisStarted[64];

// keeps track of the time each sensor will be ready
uint32_t millisReady[64];

// keeps track of the number of results expected
uint8_t expectedResults[64];

// keeps track of the number of results returned
uint8_t returnedResults[64];

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

bool getResults(char address, int resultsExpected) {
  uint8_t resultsReceived = 0;
  uint8_t cmd_number      = 0;
  while (resultsReceived < resultsExpected && cmd_number <= 9) {
    String command = "";
    command        = "";
    command += address;
    command += "D";
    command += cmd_number;
    command += "!";  // SDI-12 command to get data [address][D][dataOption][!]
    mySDI12.sendCommand(command);

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
  returnedResults[charToDec(address)] = resultsReceived;
  return resultsReceived == resultsExpected;
}

int startConcurrentMeasurement(char i, String meas_type = "") {
  mySDI12.clearBuffer();
  String command = "";
  command += i;
  command += "C";
  command += meas_type;
  command += "!";  // SDI-12 concurrent measurement command format  [address]['C'][!]
  mySDI12.sendCommand(command);
  delay(30);

  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of
  // measurments available, 0-9]
  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();

  // find out how long we have to wait (in seconds).
  uint8_t wait = sdiResponse.substring(1, 4).toInt();

  // Set up the number of results to expect
  int numResults = sdiResponse.substring(4).toInt();

  uint8_t sensorNum = charToDec(i);  // e.g. convert '0' to 0, 'a' to 10, 'Z' to 61.
  meas_time_ms[sensorNum]  = wait;
  millisStarted[sensorNum] = millis();
  if (wait == 0) {
    millisReady[sensorNum] = millis();
  } else {
    millisReady[sensorNum] = millis() + wait * 1000;
  }
  expectedResults[sensorNum] = numResults;

  return numResults;
}

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
boolean checkActive(char i) {
  String myCommand = "";
  myCommand        = "";
  myCommand += (char)i;  // sends basic 'acknowledge' command [address][!]
  myCommand += "!";

  for (int j = 0; j < 3; j++) {  // goes through three rapid contact attempts
    mySDI12.sendCommand(myCommand);
    delay(100);
    if (mySDI12.available()) {  // If we here anything, assume we have an active sensor
      mySDI12.clearBuffer();
      return true;
    }
  }
  mySDI12.clearBuffer();
  return false;
}

/**
 * @brief gets identification information from a sensor, and prints it to the serial
 * port
 *
 * @param i a character between '0'-'9', 'a'-'z', or 'A'-'Z'.
 * @param printCommands true to print the raw output and input from the command
 */
void printInfo(char i) {
  String command = "";
  command += (char)i;
  command += "I!";
  mySDI12.sendCommand(command);
  delay(100);

  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  // allccccccccmmmmmmvvvxxx...xx<CR><LF>
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
    delay(10000L);
  }

  // Quickly Scan the Address Space
  Serial.println("Scanning all addresses, please wait...");
  Serial.println("Protocol Version, Sensor Address, Sensor Vendor, Sensor Model, "
                 "Sensor Version, Sensor ID");

  for (int8_t i = firstAddress; i <= lastAddress; i++) {
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
  Serial.println("Time Elapsed (s), Measurement 1, Measurement 2, ... etc.");
  Serial.println(
    "-------------------------------------------------------------------------------");
}

void loop() {
  // start all sensors measuring concurrently
  for (int8_t i = firstAddress; i <= lastAddress; i++) {
    char addr = decToChar(i);
    if (isActive[i]) { startConcurrentMeasurement(addr); }
  }

  // get all readings
  uint8_t numReadingsRecorded = 0;
  do {
    for (int8_t i = firstAddress; i <= lastAddress; i++) {
      char addr = decToChar(i);
      if (isActive[i] && millis() > millisReady[i] && expectedResults[i] > 0 &&
          (returnedResults[i] < expectedResults[i])) {
        Serial.print(millis() / 1000);
        Serial.print(", ");
        Serial.print(addr);
        Serial.print(", ");
        getResults(addr, expectedResults[i]);
        numReadingsRecorded++;
        Serial.println();
      }
    }
  } while (numReadingsRecorded < numSensors);

  delay(10000);  // wait ten seconds between measurement attempts.
}
