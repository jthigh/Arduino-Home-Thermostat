/*
  Copyright (c) 03/04/2016

    By Nitrof

  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/


#include <SD.h>
#include <SPI.h>
#include <Dhcp.h>
#include <Dns.h>
#include <Ethernet2.h>
//#include <Ethernet.h> //to use ethernet shield 1, mute ethernet2.h and unmute ethernet.h
#include <EthernetClient.h>
#include <EthernetServer.h>
#include <EthernetUdp2.h>
//#include <EthernetUdp.h> //to use ethernet shield 1, mute EthernetUdp2.h and unmute EthernetUdp.h
#include <TimeLib.h>
#include <weeklyAlarm.h>
#include <DueTimer.h>
#include <TimeOut.h>
//#include <currentSwitch.h>  //not used in this controler
//#include <Bounce2.h> //not used in this controler
#include <uHTTP.h>
#include <ArduinoJson.h>
#include <snippets.h>
#include <RTD10K.h>
#include <IOctrl.h>
#include <ADC_SEQR.h>
#include <Streaming.h>
#define REQ_BUF_SZ   60   // size of buffer used to capture HTTP requests

#define vRef 3.3   //set reference voltage to 3.3 or 5.0V
#define RESO 12    //set resolution to 10 or 12 for arduino DUE
#define SDC_PIN 4 // pin for sd card
#define default_page "index.htm"  //set the default page to load on request

void watchdogSetup (void) {
  watchdogEnable(4095); //enable watchdog to maximum time 0xFFF
}


//currentSwitch workProof; not use in this controler
Backup bckup;
Snippets sn;

byte mac[] = {0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF};
IPAddress ip(192, 168, 0, 110);
uHTTP *uHTTPserver = new uHTTP(80);
EthernetClient response;
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets
EthernetServer server(80);

Adc_Seqr adc;


RTD10k RTDRead(RESO); //library to read 10k RTD input
//    Per channel declare section Struct array
const byte numChannel = 10;
RTDinChannels inChannelID[numChannel]; //input channel obj
SSRoutput outChannelID[numChannel]; //ouput channel obj

//global Regulator variables
byte K = 80; // set process gain 1 to 10
float vs = 0.5; // set sustain value in degres
float smm = 5; // treshold in pourcent
//float calibOffset[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; //calibration of Shield rRef resistor

//interval instance
Interval timerMainRegulator;
Interval timerWeeklyAlarm;

//programable alarm section
byte numSetpoint = 10; //set number of setpoint save in conjuction with outchannel
byte numAlarm = 10; // set the number of alarm
WeeklyAlarm SPAlarm(numAlarm);//initiane 10 alarm
float alarmMem[10][10];  // vector 1 = numAlarm, vector 2 = setpoint

uHTTP_request getContainer[] = {    //resquest index : (ID, callback function)
  {"ajax_inputs", writeJSONResponse},
  {"ajax_alarms", writeJSON_Alarm_Response},
  {"configs", writeJSONConfigResponse}
};

uHTTP_request putContainer[] = {    //resquest index : (ID, callback function)
  {"channels", parseJSONInputs},
  {"alarms", parseJSONalarms},
  {"switch", parseJSONswitch},
  {"switchAlarms", parseJSONswitchAlarms},
  {"configs", parseJSONConfigs}
};


//function prototype
void setupSdCard();
void setupEthernet();
void setupTime();
void regulator_inputs();
void regulator_outputs();
void checkWekklyAlarm();
void RTDSetup();
void setupOutput();
void restore();

//-----------------------------------------------------------------

void setup() {
  Serial.begin(115200);

  Serial.println("startup");
  setupSdCard();
  setupEthernet();
  setupTime();
  //Timer3.attachInterrupt(regulator_inputs).setFrequency(0.1).start(); // read inputs every 10 sec //-->move into interval methode
  timerMainRegulator.interval(sc(10),regulator_inputs);// read inputs every 10 sec
  Timer4.attachInterrupt(regulator_outputs).setFrequency(10).start(); //outputs regulator controler at 10 Hz
  timerWeeklyAlarm.interval(mn(1),checkWekklyAlarm); //check weekly alarm each minute
  RTDSetup();
  setupOutput();
  setupWeeklyAlarm();
  restore(); //restoring data from sd card
  WDT_Restart (WDT); //reset the watchdog timer
  delay(1000);
}

//-----------------------------------------------------------
//-----------------------------------------------------------
void webServ();

void loop() {
  //printTime();
  webServ();
  Interval::handler();
  //SPAlarm.handler(); // move into interval methode
  WDT_Restart (WDT); //reset the watchdog timer
  delay(1);
}

//-----------------------------------------------------------


