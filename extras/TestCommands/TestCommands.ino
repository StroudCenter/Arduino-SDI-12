/**
 * @example{lineno} TestCommands.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

#ifndef SDI12_DATA_PIN
#define SDI12_DATA_PIN 7
#endif
#ifndef SDI12_POWER_PIN
#define SDI12_POWER_PIN 22
#endif

/* connection information */
#if F_CPU > 48000000L
uint32_t serialBaud = 921600; /*!< The baud rate for the output serial port */
#else
uint32_t serialBaud = 115200; /*!< The baud rate for the output serial port */
#endif
int8_t dataPin = SDI12_DATA_PIN; /*!< The pin of the SDI-12 data bus */
int8_t powerPin =
  SDI12_POWER_PIN; /*!< The sensor power pin (or -1 if not switching power) */
uint32_t     wake_delay = 10; /*!< Extra time needed for the sensor to wake (0-100ms) */
const int8_t firstAddress =
  0; /* The first address in the address space to check (0='0') */
const int8_t lastAddress =
  6; /* The last address in the address space to check (62='z') */
const int8_t commandsToTest =
  1; /*!< The number of measurement commands to test, between 1 and 11. */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

/** Define some testing specs */
const int8_t n_addresses = (lastAddress - firstAddress) + 1;

/** Error codes, if returned */
int8_t error_result_number = 7;
float  no_error_value      = 0;

/// variable that alternates output type back and forth between parsed and raw
boolean flip = 0;

String commands[] = {"", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};

// keeps track of active addresses
bool isActive[n_addresses];

// keeps track of the wait time for each active addresses
uint32_t meas_time_ms[n_addresses];

// keeps track of the time each sensor was started
uint32_t millisStarted[n_addresses];

// keeps track of the time each sensor will be ready
uint32_t millisReady[n_addresses];

// keeps track of the number of results expected
uint8_t expectedResults[n_addresses];

// keeps track of the number of results returned
uint8_t returnedResults[n_addresses];

String  prev_result[n_addresses];
String  this_result[n_addresses];
uint8_t numSensors = 0;

struct startMeasurementResult {  // Structure declaration
  String  returned_address;
  uint8_t meas_time_s;
  int     numberResults;
};

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

struct getResultsResult {  // Structure declaration
  uint8_t resultsReceived;
  uint8_t maxDataCommand;
  bool    addressMatch;
  bool    crcMatch;
  bool    errorCode;
  bool    success;
};

getResultsResult getResults(char address, int resultsExpected, bool verify_crc = false,
                            bool printCommands = true) {
  uint8_t resultsReceived = 0;
  uint8_t cmd_number      = 0;
  // The maximum number of characters that can be returned in the <values> part of the
  // response to a D command is either 35 or 75. If the D command is issued to
  // retrieve data in response to a concurrent measurement command, or in response to
  // a high-volume ASCII measurement command, the maximum is 75. The maximum is also
  // 75 in response to a continuous measurement command. Otherwise, the maximum is 35.
  int max_sdi_response = 76;
  // max chars in a unsigned 64 bit number
  int max_sdi_digits = 21;

  String compiled_response = "";

  bool success = true;

  // Create the return struct
  getResultsResult return_result;
  return_result.resultsReceived = 0;
  return_result.maxDataCommand  = 0;
  return_result.addressMatch    = true;
  return_result.crcMatch        = true;
  return_result.errorCode       = false;
  return_result.success         = true;

  while (resultsReceived < resultsExpected && cmd_number <= 9) {
    String command = "";
    command += address;
    command += "D";
    command += cmd_number;
    command += "!";  // SDI-12 command to get data [address][D][dataOption][!]
    mySDI12.sendCommand(command, wake_delay);

    // uint32_t start = millis();
    if (printCommands) {
      Serial.print(">>>");
      Serial.println(command);
    }
    char resp_buffer[max_sdi_response] = {'\0'};

    // read bytes into the char array until we get to a new line (\r\n)
    size_t bytes_read = mySDI12.readBytesUntil('\n', resp_buffer, max_sdi_response);
    // Serial.print(bytes_read);
    // Serial.println(" characters");

    size_t data_bytes_read = bytes_read - 1;  // subtract one for the /r before the /n
    String sdiResponse     = String(resp_buffer);
    compiled_response += sdiResponse;
    sdiResponse.trim();
    if (printCommands) {
      Serial.print("<<<");
      Serial.println(sdiResponse);
      // Serial.println(sdiResponse.length());
      // Serial.print("<<<");
      // Serial.println(resp_buffer);
      // Serial.println(strnlen(resp_buffer, max_sdi_response));
    }
    // read and clear anything else from the buffer
    int extra_chars = 0;
    while (mySDI12.available()) {
      Serial.write(mySDI12.read());
      extra_chars++;
    }
    if (extra_chars > 0) {
      Serial.print(extra_chars);
      Serial.println(" additional characters received.");
    }
    mySDI12.clearBuffer();

    // check the address, break if it's incorrect
    char returned_address = resp_buffer[0];
    if (returned_address != address) {
      if (printCommands) {
        Serial.println("Wrong address returned!");
        Serial.print("Expected ");
        Serial.print(String(address));
        Serial.print(" Got ");
        Serial.println(String(returned_address));
        Serial.println(String(resp_buffer));
      }
      success                    = false;
      return_result.addressMatch = false;
      break;
    }

    // check the crc, break if it's incorrect
    if (verify_crc) {
      bool crcMatch   = mySDI12.verifyCRC(sdiResponse);
      data_bytes_read = data_bytes_read - 3;
      if (crcMatch) {
        if (printCommands) { Serial.println("CRC valid"); }
      } else {
        if (printCommands) { Serial.println("CRC check failed!"); }
        return_result.crcMatch = false;
        success                = false;
        break;
      }
    }

    bool    gotResults                   = false;
    char    float_buffer[max_sdi_digits] = {'\0'};
    char*   dec_pl                       = float_buffer;
    uint8_t fb_pos                       = 0;  // start at start of buffer
    bool    finished_last_number         = false;
    // iterate through the char array and to check results
    // NOTE: start at 1 since we already looked at the address!
    for (size_t i = 1; i < data_bytes_read; i++) {
      // Get the character at position
      char c = resp_buffer[i];
      // Serial.print(i);
      // Serial.print(" of ");
      // Serial.print(data_bytes_read);
      // Serial.print(" '");
      // Serial.print(c);
      // Serial.println("'");
      // if we didn't get something number-esque or we're at the end of the buffer,
      // assume the last number finished and parse it
      //(c != '-' && (c < '0' || c > '9') && c != '.')
      if (c == '-' || (c >= '0' && c <= '9') || c == '.') {
        // if there's a number, a decimal, or a negative sign next in the
        // buffer, add it to the float buffer.
        float_buffer[fb_pos] = c;
        fb_pos++;
        float_buffer[fb_pos] = '\0';  // null terminate the buffer
        finished_last_number = false;
        // Serial.print("Added to float buffer, currently: '");
        // Serial.print(float_buffer);
        // Serial.println("'");
      } else {
        // Serial.println("Non Numeric");
        finished_last_number = true;
      }
      // if we've gotten to the end of a number or the end of the buffer, parse the
      // character
      if ((finished_last_number || i == data_bytes_read - 1) &&
          strnlen(float_buffer, max_sdi_digits) > 0) {
        float result = atof(float_buffer);
        if (printCommands) {
          Serial.print("Result ");
          Serial.print(resultsReceived);
          Serial.print(", Raw value: ");
          Serial.print(float_buffer);
          dec_pl              = strchr(float_buffer, '.');
          size_t len_post_dec = 0;
          if (dec_pl != nullptr) { len_post_dec = strnlen(dec_pl, max_sdi_digits) - 1; }
          Serial.print(", Len after decimal: ");
          Serial.print(len_post_dec);
          Serial.print(", Parsed value: ");
          Serial.println(String(result, len_post_dec));
        }
        // add how many results we have
        if (result != -9999) {
          gotResults = true;
          resultsReceived++;
        }
        // check for a failure error code at the end
        if (error_result_number >= 1) {
          if (resultsReceived == error_result_number && result != no_error_value) {
            success                 = false;
            return_result.errorCode = true;
            if (printCommands) {
              Serial.print("Got a failure code of ");
              Serial.println(String(result, strnlen(dec_pl, max_sdi_digits) - 1));
            }
          }
        }

        // empty the buffer
        float_buffer[0] = '\0';
        fb_pos          = 0;
      }
    }

    if (!gotResults) {
      if (printCommands) {
        Serial.println(("  No results received, will not continue requests!"));
      }
      break;
    }  // don't do another loop if we got nothing

    if (printCommands) {
      Serial.print("Total Results Received: ");
      Serial.print(resultsReceived);
      Serial.print(", Remaining: ");
      Serial.println(resultsExpected - resultsReceived);
    }

    cmd_number++;
  }

  mySDI12.clearBuffer();

  if (printCommands) {
    Serial.print("After ");
    Serial.print(cmd_number);
    Serial.print(" data commands got ");
    Serial.print(resultsReceived);
    Serial.print(" results of the expected ");
    Serial.print(resultsExpected);
    Serial.print(" expected. This is a ");
    Serial.println(resultsReceived == resultsExpected ? "success." : "failure.");
  }

  success &= resultsReceived == resultsExpected;
  this_result[charToDec(address)] = compiled_response;
  return_result.resultsReceived   = resultsReceived;
  return_result.maxDataCommand    = cmd_number;
  return_result.success           = success;
  return return_result;
}

bool getContinuousResults(char address, int resultsExpected,
                          bool printCommands = true) {
  uint8_t resultsReceived = 0;
  uint8_t cmd_number      = 0;
  while (resultsReceived < resultsExpected && cmd_number <= 9) {
    String command = "";
    command += address;
    command += "R";
    command += cmd_number;
    command += "!";  // SDI-12 command to get data [address][D][dataOption][!]
    mySDI12.sendCommand(command, wake_delay);
    if (printCommands) {
      Serial.print(">>>");
      Serial.println(command);
    }

    uint32_t start = millis();
    while (mySDI12.available() < 3 && (millis() - start) < 1500) {}
    if (printCommands) {
      Serial.print("<<<");
      Serial.write(mySDI12.read());  // ignore the repeated SDI12 address
    }

    while (mySDI12.available()) {
      char c = mySDI12.peek();
      if (c == '-' || (c >= '0' && c <= '9') || c == '.') {
        float result = mySDI12.parseFloat(SKIP_NONE);
        Serial.print(String(result, 10));
        if (result != -9999) { resultsReceived++; }
      } else if (c >= 0 && c != '\r' && c != '\n') {
        Serial.write(mySDI12.read());
      } else {
        mySDI12.read();
      }
      delay(10);  // 1 character ~ 7.5ms
    }
    if (printCommands) {
      Serial.print("Total Results Received: ");
      Serial.print(resultsReceived);
      Serial.print(", Remaining: ");
      Serial.println(resultsExpected - resultsReceived);
    }
    if (!resultsReceived) { break; }  // don't do another loop if we got nothing
    cmd_number++;
  }
  mySDI12.clearBuffer();

  return resultsReceived == resultsExpected;
}

startMeasurementResult startMeasurement(char address, bool is_concurrent = false,
                                        bool request_crc = false, String meas_type = "",
                                        bool printCommands = true) {
  // Create the return struct
  startMeasurementResult return_result;
  return_result.returned_address = "";
  return_result.meas_time_s      = 0;
  return_result.numberResults    = 0;

  String command = "";
  command += address;                    // All commands start with the address
  command += is_concurrent ? "C" : "M";  // C for concurrent, M for standard
  command += request_crc ? "C" : "";     // add an additional C to request a CRC
  command += meas_type;                  // Measurement type, "" or 0-9
  command += "!";                        // All commands end with "!"
  mySDI12.sendCommand(command, wake_delay);
  if (printCommands) {
    Serial.print(">>>");
    Serial.println(command);
  }

  // wait for acknowlegement with format [address][ttt (3 char, seconds)][number of
  // measurments available, 0-9]
  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  if (printCommands) {
    Serial.print("<<<");
    Serial.println(sdiResponse);
  }
  mySDI12.clearBuffer();

  // check the address, return if it's incorrect
  String returned_address = sdiResponse.substring(0, 1);
  char   ret_addr_array[2];
  returned_address.toCharArray(ret_addr_array, sizeof(ret_addr_array));
  return_result.returned_address = ret_addr_array[0];
  if (returned_address != String(address)) {
    if (printCommands) {
      Serial.println("Wrong address returned!");
      Serial.print("Expected ");
      Serial.print(String(address));
      Serial.print(" Got ");
      Serial.println(returned_address);
    }
    return return_result;
  }

  // find out how long we have to wait (in seconds).
  uint8_t meas_time_s       = sdiResponse.substring(1, 4).toInt();
  return_result.meas_time_s = meas_time_s;
  if (printCommands) {
    Serial.print("expected measurement time: ");
    Serial.print(meas_time_s);
    Serial.print(" s, ");
  }

  // Set up the number of results to expect
  int numResults              = sdiResponse.substring(4).toInt();
  return_result.numberResults = numResults;
  if (printCommands) {
    Serial.print("Number Results: ");
    Serial.println(numResults);
  }

  return return_result;
}

// This is a separate function in this example so the wait times can all be filled into
// the appropriate arrays
int startConcurrentMeasurement(char address, bool request_crc = false,
                               String meas_type = "", bool printCommands = true) {
  startMeasurementResult startResult = startMeasurement(address, true, request_crc,
                                                        meas_type, printCommands);

  uint8_t sensorNum =
    charToDec(address);  // e.g. convert '0' to 0, 'a' to 10, 'Z' to 61.
  meas_time_ms[sensorNum]  = (static_cast<uint32_t>(startResult.meas_time_s)) * 1000;
  millisStarted[sensorNum] = millis();
  if (startResult.meas_time_s == 0) {
    millisReady[sensorNum] = millis();
  } else {
    // give an extra second
    // millisReady[sensorNum] = millis() + meas_time_ms[sensorNum] + 1000;
    // subtract a second to start polling early
    millisReady[sensorNum] = millis() + meas_time_ms[sensorNum] - 1000;
  }
  expectedResults[sensorNum] = startResult.numberResults;

  return startResult.numberResults;
}

uint32_t takeMeasurement(char address, bool request_crc = false, String meas_type = "",
                         bool printCommands = true) {
  startMeasurementResult startResult = startMeasurement(address, false, request_crc,
                                                        meas_type, printCommands);
  if (startResult.numberResults == 0) { return -1; }

  uint32_t timerStart = millis();
  uint32_t measTime   = -1;
  // wait up to 1 second longer than the specified return time
  while ((millis() - timerStart) <
         (static_cast<uint32_t>(startResult.meas_time_s) + 1) * 1000) {
    if (mySDI12.available()) {
      break;
    }  // sensor can interrupt us to let us know it is done early
  }
  measTime                  = millis() - timerStart;
  String interrupt_response = mySDI12.readStringUntil('\n');
  if (printCommands) {
    Serial.print("<<<");
    Serial.println(interrupt_response);
    Serial.print("Completed after ");
    Serial.print(measTime);
    Serial.println(" ms");
  }

  // if we got results, return the measurement time, else -1
  if (getResults(address, startResult.numberResults, request_crc, printCommands)
        .success) {
    return measTime;
  }

  return -1;
}

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
bool checkActive(char address, int8_t numPings = 3, bool printCommands = true) {
  String command = "";
  command += (char)address;  // sends basic 'acknowledge' command [address][!]
  command += "!";

  for (int j = 0; j < numPings; j++) {  // goes through three rapid contact attempts
    if (printCommands) {
      Serial.print(">>>");
      Serial.println(command);
    }
    mySDI12.sendCommand(command, wake_delay);

    // the sensor should just return its address
    String sdiResponse = mySDI12.readStringUntil('\n');
    sdiResponse.trim();
    if (printCommands) {
      Serial.print("<<<");
      Serial.println(sdiResponse);
    }
    mySDI12.clearBuffer();

    // check the address, return false if it's incorrect
    String returned_address = sdiResponse.substring(0, 1);
    char   ret_addr_array[2];
    returned_address.toCharArray(ret_addr_array, sizeof(ret_addr_array));
    if (returned_address == String(address)) { return true; }
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
bool printInfo(char i, bool printCommands = true) {
  String command = "";
  command += (char)i;
  command += "I!";
  mySDI12.sendCommand(command, wake_delay);
  if (printCommands) {
    Serial.print(">>>");
    Serial.println(command);
  }
  delay(100);

  String sdiResponse = mySDI12.readStringUntil('\n');
  sdiResponse.trim();
  // allccccccccmmmmmmvvvxxx...xx<CR><LF>
  if (printCommands) {
    Serial.print("<<<");
    Serial.println(sdiResponse);
  }

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

  if (sdiResponse.length() < 3) { return false; };
  return true;
}

void setup() {
  Serial.begin(serialBaud);
  while (!Serial)
    ;

  Serial.print("Opening SDI-12 bus on pin ");
  Serial.print(String(dataPin));
  Serial.println("...");
  mySDI12.begin();
  delay(500);  // allow things to settle

  Serial.println("Timeout value: ");
  Serial.println(mySDI12.TIMEOUT);

  // Fill arrays with 0's
  for (int8_t i = firstAddress; i <= lastAddress; i++) {
    isActive[i]        = false;
    meas_time_ms[i]    = 0;
    millisStarted[i]   = 0;
    millisReady[i]     = 0;
    expectedResults[i] = 0;
    returnedResults[i] = 0;
    prev_result[i]     = "";
    this_result[i]     = "";
  }

  // Power the sensors;
  if (powerPin >= 0) {
    Serial.println("Powering up sensors with pin ");
    Serial.print(String(powerPin));
    Serial.println(", wait 30s...");
    pinMode(powerPin, OUTPUT);
    digitalWrite(powerPin, HIGH);
    delay(30000L);
  } else {
    Serial.println("Wait 5s...");
    delay(5000L);
  }

  // Quickly Scan the Address Space
  Serial.println("Scanning all addresses, please wait...");

  for (int8_t i = firstAddress; i <= lastAddress; i++) {
    char addr = decToChar(i);
    Serial.print("i: ");
    Serial.print(i);
    Serial.print(", addr: ");
    Serial.print(addr);
    Serial.print(", reversed: ");
    Serial.println(charToDec(addr));
    if (checkActive(addr, 5, true)) {
      numSensors++;
      isActive[i] = 1;
      // Serial.println(", +");
      printInfo(addr, true);
    } else {
      // Serial.println(", -");
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
  Serial.println("-------------------------------------------------------------------"
                 "------------");

  delay(1000);
}

void loop() {
  flip = !flip;  // flip the switch between concurrent and not
  // flip = 1;
  // flip           = 0;
  uint32_t start = millis();
  // Serial.print("Flip: ");
  // Serial.println(flip);

  // Fill arrays with 0's
  for (int8_t i = firstAddress; i <= lastAddress; i++) {
    meas_time_ms[i]    = 0;
    millisStarted[i]   = 0;
    millisReady[i]     = 0;
    expectedResults[i] = 0;
    returnedResults[i] = 0;
  }

  if (flip) {
    // measure one at a time
    for (int8_t i = firstAddress; i <= lastAddress; i++) {
      char addr = decToChar(i);
      if (isActive[i]) {
        for (uint8_t a = 0; a < commandsToTest; a++) {
          Serial.print("Command ");
          Serial.print(i);
          Serial.print("M");
          Serial.print(commands[a]);
          Serial.println('!');
          takeMeasurement(addr, true, commands[a], true);
        }
        // getContinuousResults(addr, 3);
        Serial.println();
      } else {
        Serial.print("Address ");
        Serial.print(addr);
        Serial.println(" is not active");
      }
    }
    Serial.print("Total Time for Individual Measurements: ");
    Serial.println(millis() - start);
  } else {
    for (uint8_t a = 0; a < commandsToTest; a++) {
      uint32_t min_wait  = 60000L;
      uint32_t max_wait  = 0;
      uint32_t for_start = millis();
      // start all sensors measuring concurrently
      for (int8_t i = firstAddress; i <= lastAddress; i++) {
        char addr = decToChar(i);
        if (isActive[i]) {
          Serial.print("Command ");
          Serial.print(i);
          Serial.print("C");
          Serial.print(commands[a]);
          Serial.println('!');
          startConcurrentMeasurement(addr, true, commands[a], true);
          if (meas_time_ms[i] < min_wait) { min_wait = meas_time_ms[i]; }
          if (meas_time_ms[i] > max_wait) { max_wait = meas_time_ms[i]; }
        } else {
          Serial.print("Address ");
          Serial.print(addr);
          Serial.println(" is not active");
        }
      }
      // min_wait = 800;
      min_wait = max(static_cast<uint32_t>(10), min_wait / 2);
      max_wait = max(static_cast<uint32_t>(1000),
                     max_wait + static_cast<uint32_t>(2000));
      Serial.print("minimum expected wait for all sensors: ");
      Serial.println(min_wait);
      Serial.print("maximum expected wait for all sensors: ");
      Serial.println(max_wait);

#if defined(TEST_PRINT_ARRAY)
      Serial.print("i,\t");
      Serial.print("addr,\t");
      Serial.print("isActive[i],\t");
      Serial.print("millis,\t");
      Serial.print("timeWaited,\t");
      Serial.print("millisReady[i],\t");
      Serial.print("expectedResults[i],\t");
      Serial.print("returnedResults[i],\t");
      Serial.print("millis() > millisReady[i],\t");
      Serial.print("expectedResults[i] > 0,\t");
      Serial.print("returnedResults[i] < expectedResults[i],\t");
      Serial.print("numSensors,\t");
      Serial.print("numReadingsRecorded,\t");
      Serial.print("maxDataCommand,\t");
      Serial.print("resultsReceived,\t");
      Serial.print("errorCode,\t");
      Serial.print("crcMatch,\t");
      Serial.print("gotGoodResults,\t");
      Serial.print("numReadingsRecorded");
      Serial.println();
#endif

      uint8_t numReadingsRecorded = 0;
      delay(min_wait);

      do {
        // get all readings
        for (int8_t i = firstAddress; i <= lastAddress; i++) {
          uint32_t timeWaited = 0;
          if (millisStarted[i] != 0) { timeWaited = millis() - millisStarted[i]; }
          if (this_result[i] != "") { prev_result[i] = this_result[i]; }

          char addr = decToChar(i);

#if defined(TEST_PRINT_ARRAY)
          Serial.print(i);
          Serial.print(",\t\"");
          Serial.print(decToChar(i));
          Serial.print("\",\t");
          Serial.print(isActive[i]);
          Serial.print(",\t");
          Serial.print(millis());
          Serial.print(",\t");
          Serial.print(timeWaited);
          Serial.print(",\t");
          Serial.print(millisReady[i]);
          Serial.print(",\t");
          Serial.print(expectedResults[i]);
          Serial.print(",\t");
          Serial.print(returnedResults[i]);
          Serial.print(",\t");
          Serial.print(millis() > millisReady[i]);
          Serial.print(",\t");
          Serial.print(expectedResults[i] > 0);
          Serial.print(",\t");
          Serial.print(returnedResults[i] < expectedResults[i]);
          Serial.print(",\t");
          Serial.print(numSensors);
          Serial.print(",\t");
          Serial.print(numReadingsRecorded);
#endif

          if (isActive[i] && (millis() > millisReady[i]) && (expectedResults[i] > 0) &&
              (returnedResults[i] < expectedResults[i])) {
#ifndef TEST_PRINT_ARRAY
            Serial.print("timeWaited: ");
            Serial.println(timeWaited);
#endif
            getResultsResult cResult = getResults(addr, expectedResults[i], true);
            returnedResults[i]       = cResult.resultsReceived;
            bool gotGoodResults      = cResult.success;
            if (gotGoodResults) {
              numReadingsRecorded++;
#ifndef TEST_PRINT_ARRAY
              Serial.print("Got results from ");
              Serial.print(numReadingsRecorded);
              Serial.print(" of ");
              Serial.print(numSensors);
              Serial.print(" sensors");
#endif
            }
#if defined(TEST_PRINT_ARRAY)
            Serial.print(",\t");
            Serial.print(cResult.maxDataCommand);
            Serial.print(",\t");
            Serial.print(cResult.resultsReceived);
            Serial.print(",\t");
            Serial.print(cResult.errorCode);
            Serial.print(",\t");
            Serial.print(cResult.crcMatch);
            Serial.print(",\t");
            Serial.print(gotGoodResults);
#endif
          }
          Serial.println();
        }

      } while (millis() - for_start < max_wait && numReadingsRecorded < numSensors);
    }
    Serial.print("Total Time for Concurrent Measurements: ");
    Serial.println(millis() - start);
  }

  Serial.println("-------------------------------------------------------------------"
                 "------------");
}
