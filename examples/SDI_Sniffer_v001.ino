/*
  FILE: SDI_Sniffer_v001.ino
  AUTHOR: Muharrem ARLI
  VERSION: 0.0.1
  PURPOSE: SDI-12 Line Sniffer
  Features:
      Monitor SDI Line and get commands to Serial Monitor


  Notes:



*/
const String prog_versiyon = "v0.0.1";

/*Include Libraries*/
#include <SDI12.h>

/*Definitions*/
#define SERIAL_BAUD 9600  // The baud rate for the output serial port
#define SDI12_MASTER_PIN 7         // The pin of the Master SDI-12 data bus

// Define the SDI-12 bus
SDI12 masterSDI(SDI12_MASTER_PIN);
String masterCommand = "";

bool SDI12_Success = false;

void readSDI12Line()
{

  SDI12_Success = false;
  while (masterSDI.available()) {
    char c = masterSDI.read();
    if ((c != '\r') && (c != '\n')) {
      digitalWrite(LED_BUILTIN, c);
      masterCommand += c;
      SDI12_Success = true;
    }
    delay(5);
  }
  if (SDI12_Success) {
    Serial.print(masterCommand);
    masterCommand = "";
  }

}


void startSDI12() {
  Serial.print(" SDI-12 Sniffer Baslatiliyor... ");
  masterSDI.begin();
  masterSDI.forceListen();
  Serial.println("Tamam");
  delay(300);

}



void setup() {
  //pinMode(SDI12_MASTER_PIN,INPUT_PULLUP);
  Serial.begin(SERIAL_BAUD);
  startSDI12();
  delay(500); // allow things to settle



}

void loop()
{
  readSDI12Line();
}
