/**
 * @example{lineno} TestSensorTiming.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 * @date March 2024
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
uint32_t serialBaud = 115200;         /*!< The baud rate for the output serial port */
int8_t   dataPin    = SDI12_DATA_PIN; /*!< The pin of the SDI-12 data bus */
int8_t   powerPin =
  SDI12_POWER_PIN;          /*!< The sensor power pin (or -1 if not switching power) */
uint32_t wake_delay    = 0; /*!< Extra time needed for the sensor to wake (0-100ms) */
char     sensorAddress = '0'; /*!< The address of the SDI-12 sensor */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

/** Define some testing specs */

/** Error codes, if returned */
int8_t error_result_number = 7;
float  no_error_value      = 0;

/** Testing turning off power */
bool    testPowerOff    = false;
int32_t min_power_delay = 100L;       /*!< The min time to test wake after power on. */
int32_t max_power_delay = 180000L;    /*!< The max time to test wake after power on. */
int32_t increment_power_delay = 100L; /*!< The time to lengthen waits between reps. */
int32_t power_off_time        = 60000L; /*!< The time to power off between tests. */
/** NOTE: set the power off time to be something similar to what you will be using the
 * the real world! Some sensors take longer to warm up if they've been off for a while.
 */

/** Testing the length of the break */
bool    testBreak      = true;
int32_t min_wake_delay = 0;   /*!< The min time to test wake after a line break. */
int32_t max_wake_delay = 100; /*!< The max time to test wake (should be <=100). */
int32_t increment_wake = 5;   /*!< The time to lengthen waits between reps. */

/** set some initial values */
int32_t power_delay = min_power_delay;
int32_t wake_delay  = min_wake_delay;

int32_t  total_meas_time = 0;
int32_t  total_meas_made = 0;
uint32_t max_meas_time   = 0;

struct startMeasurementResult {  // Structure declaration
  String  returned_address;
  uint8_t meas_time_s;
  int     numberResults;
};

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
  return_result.resultsReceived = resultsReceived;
  return_result.maxDataCommand  = cmd_number;
  return_result.success         = success;
  return return_result;
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

  // Power the sensors;
  if (powerPin >= 0 && !testPowerOff) {
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
}

void loop() {
  bool got_good_results = true;
  int  checks_at_time   = 0;
  while (got_good_results && checks_at_time < 25) {
    Serial.print("Repeat attempt ");
    Serial.print(checks_at_time);
    Serial.print(" with power on warm-up time of ");
    Serial.println(power_delay);

    // Power down the sensors;
    if (powerPin >= 0 && testPowerOff) {
      Serial.print("Powering down sensors, wait ");
      Serial.print(power_off_time);
      Serial.println(" ms");
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
      delay(power_off_time);
    }

    // Power up the sensors;
    if (powerPin >= 0 && testPowerOff) {
      Serial.print("Powering up sensors, wait ");
      Serial.print(power_delay);
      Serial.println(" ms");
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, HIGH);
      delay(power_delay);
      mySDI12.clearBuffer();
    }

    // checkActive(sensorAddress, true);

    uint32_t this_meas_time   = takeMeasurement(sensorAddress, true, "", true);
    bool     this_result_good = this_meas_time != static_cast<uint32_t>(-1);

    if (this_result_good) {
      total_meas_time += this_meas_time;
      total_meas_made++;
      if (this_meas_time > max_meas_time) { max_meas_time = this_meas_time; }
      Serial.print("Warm-up Time: ");
      Serial.print(power_delay);
      Serial.print(", This measurement time: ");
      Serial.print(this_meas_time);
      Serial.print(", Mean measurement time: ");
      Serial.print(total_meas_time / total_meas_made);
      Serial.print(", Max measurement time: ");
      Serial.println(max_meas_time);
    }

    got_good_results &= this_result_good;
    checks_at_time++;

    if (!got_good_results) {
      Serial.print("Time check failed after ");
      Serial.print(checks_at_time);
      Serial.println(" reps");
      total_meas_time = 0;
      total_meas_made = 0;
      max_meas_time   = 0;
    }
  }

  // if we got a good result 25x at this warm-up, keep testing how long the
  // measurements take
  while (got_good_results) {
    uint32_t this_meas_time = takeMeasurement(sensorAddress, true, "", false);
    if (this_meas_time != static_cast<uint32_t>(-1)) {
      total_meas_time += this_meas_time;
      total_meas_made++;
      if (this_meas_time > max_meas_time) { max_meas_time = this_meas_time; }
      Serial.print("Warm-up Time: ");
      Serial.print(power_delay);
      Serial.print(", This measurement time: ");
      Serial.print(this_meas_time);
      Serial.print(", Mean measurement time: ");
      Serial.print(total_meas_time / total_meas_made);
      Serial.print(", Max measurement time: ");
      Serial.println(max_meas_time);
    }
  }


  Serial.println("-------------------------------------------------------------------"
                 "------------");
  if (power_delay > max_power_delay) {
    power_delay = min_power_delay;
  } else {
    power_delay = power_delay + increment_power_delay;
  }
}
