/**
 * @example{lineno} TestWarmUp.ino
 * @copyright Stroud Water Research Center
 * @license This example is published under the BSD-3 license.
 * @author Sara Damiano <sdamiano@stroudcenter.org>
 * @date March 2021
 */
#if (defined(__AVR__) || defined(ARDUINO_ARCH_AVR)) && not defined(SDI12_INTERNAL_PCINT)
#include <EnableInterrupt.h>
#endif
#include <SDI12.h>

/* connection information */
uint32_t serialBaud    = 115200; /*!< The baud rate for the output serial port */
int8_t   dataPin       = 7;      /*!< The pin of the SDI-12 data bus */
char     sensorAddress = '0';    /*!< The address of the SDI-12 sensor */
int8_t   powerPin      = 22; /*!< The sensor power pin (or -1 if not switching power) */

/** Define the SDI-12 bus */
SDI12 mySDI12(dataPin);

/** Define some testing specs */

/** Error codes, if returned */
int8_t error_result_number = 7;
float  no_error_value      = 0;

/** Testing turning off power */
int32_t min_power_delay = 100L;   /*!< The min time to test wake after power on. */
int32_t max_power_delay = 10000L; /*!< The max time to test wake after power on. */
int32_t increment_power = 100;    /*!< The time to lengthen waits between reps. */

/** Testing the length of the break */
int32_t min_wake_delay = 0;   /*!< The min time to test wake after a line break. */
int32_t max_wake_delay = 100; /*!< The max time to test wake (should be <=100). */
int32_t increment_wake = 5;   /*!< The time to lengthen waits between reps. */

/** set some initial values */
int32_t power_delay = min_power_delay;
int32_t wake_delay  = min_wake_delay;

// this checks for activity at a particular address
// expects a char, '0'-'9', 'a'-'z', or 'A'-'Z'
bool checkActive(char address, int8_t numPings = 3, bool printCommands = false) {
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

  Serial.println("Opening SDI-12 bus...");
  mySDI12.begin();
  delay(500);  // allow things to settle

  Serial.println("Timeout value: ");
  Serial.println(mySDI12.TIMEOUT);
}

void loop() {
  while (wake_delay <= max_wake_delay) {
    // Power the sensors;
    if (powerPin >= 0) {
      Serial.println("Powering down sensors...");
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, LOW);
      delay(300000L);
    }

    // Power the sensors;
    if (powerPin >= 0) {
      Serial.println("Powering up sensors...");
      pinMode(powerPin, OUTPUT);
      digitalWrite(powerPin, HIGH);
      delay(power_delay);
      mySDI12.clearBuffer();
    }

    if (checkActive(sensorAddress, 1, true)) {
      Serial.print("Got some response after ");
      Serial.print(power_delay);
      Serial.print("ms after power with ");
      Serial.print(wake_delay);
      Serial.println("ms with wake delay");
      if (printInfo(sensorAddress, true)) {
        // if we got sensor info, stop
        Serial.println("Looks good.  Stopping.");
        while (1)
          ;
      } else {
        Serial.println("Sensor info not valid!");
      }
    } else {
      Serial.print("No response after ");
      Serial.print(power_delay);
      Serial.print("ms after power with ");
      Serial.print(wake_delay);
      Serial.println("ms with wake delay");
    }
    Serial.println("-------------------------------------------------------------------"
                   "------------");
  }
  power_delay = power_delay + increment_power;
  if (power_delay > max_power_delay) {
    Serial.println("FINISHED!!");
    while (1) {}
  }
}
