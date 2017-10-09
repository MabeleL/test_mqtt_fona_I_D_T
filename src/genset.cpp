
//Leonard Mabele

// Libraries
#include "Adafruit_FONA.h"
#include <SoftwareSerial.h>
#include "Arduino.h"
#include <Process.h>
#include <ArduinoJson.h>
#include "ACS712.h"
#include "Adafruit_SleepyDog.h"
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_FONA.h"
#include "Adafruit_MQTT_Client.h"
#define LOCATION_FEED_NAME   "location"

//token for Arduino
//#define TOKEN "a93b2f07-dcf1-4555-a9e6-f250db84590e"


//Transmitting data in Json
//creating the json manually
#define JSONSTART (String)"{\"token\":\"a93b2f07-dcf1-4555-a9e6-f250db84590e\",\"C101\":\""
#define JSONEND (String)"\"}"



/************************* WiFi Access Point *********************************/
#define FONA_APN       "safaricom"
#define FONA_USERNAME  ""
#define FONA_PASSWORD  ""

// Alarm pins
const int ledPin = 10;

// Size of the geo fence (in meters)
const float maxDistance = 100;

//FONA Configuration
#define FONA_RX  2   // FONA serial RX pin (pin 2 for shield).
#define FONA_TX  9   // FONA serial TX pin (pin 3 for shield).
#define FONA_RST 4   // FONA reset pin (pin 4 for shield)
#define LED 10

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);


/************************* Broker Setup *********************************/
/*#define AIO_SERVER      "m12.cloudmqtt.com"
#define AIO_SERVERPORT  11659
#define AIO_USERNAME    "olafauer"
#define AIO_KEY         "MAjitPJhwNJc"
*/

/************************* Adafruit.io Setup *********************************/
/*
#define AIO_SERVER      "io.adafruit.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    "Mabele_L"
#define AIO_KEY         "5e7974e4b1be4189bde9bda267a371f7"
*/
/************************* Adafruit.io Setup *********************************/


#define AIO_SERVER      "lab-i.tech"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""
#define AIO_KEY         ""



/************ Global State (you don't need to change this!) ******************/

// Setup the FONA MQTT class by passing in the FONA class and MQTT server and login details.
Adafruit_MQTT_FONA mqtt(&fona, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);

// You don't need to change anything below this line!
#define halt(s) { Serial.println(F( s )); while(1);  }

// FONAconnect is a helper function that sets up the FONA and connects to
// the GPRS network. See the fonahelper.cpp tab above for the source!
boolean FONAconnect(const __FlashStringHelper *apn, const __FlashStringHelper *username, const __FlashStringHelper *password);

/****************************** Feeds ***************************************/
// Setup a feed called 'photocell' for publishing.
// Notice MQTT paths for AIO follow the form: <username>/feeds/<feedname>
Adafruit_MQTT_Publish current_feed= Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/current");
Adafruit_MQTT_Publish temperature_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/temperature");
Adafruit_MQTT_Publish oil_level = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/oil_level");
Adafruit_MQTT_Publish location_feed = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds//location_feed");

// Setup a feed called 'onoff' for subscribing to changes.
Adafruit_MQTT_Subscribe onoffbutton = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/onoff");

/*************************** Sketch Code ************************************/



// Feeds
#define LOCATION_FEED_NAME  "location"  // Name of the AIO feed to log regular location updates.
#define MAX_TX_FAILURES      3  // Maximum number of publish failures in a row before resetting the whole sketch.

//Adafruit_MQTT_FONA mqtt(&fona, MQTT_SERVER, AIO_SERVERPORT, MQTT_USERNAME, MQTT_PASSWORD);

uint8_t txFailures = 0;       // Count of how many publish failures have occured in a row.

#define pinTemp A0
ACS712 sensor(ACS712_05B, A1);
// Latitude & longitude for distance measurement
float latitude, longitude, speed_kph, heading, altitude;
float initialLatitude;
float initialLongitude;


float tempC;
float temp_transmit;
const int time = 1000;
#define TrigPin 8
#define EchoPin 7

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
    Serial.println(mqtt.connectErrorString(ret));
    Serial.println("Retrying MQTT connection in 5 seconds...");
    mqtt.disconnect();
    delay(5000);  // wait 5 seconds
  }
  Serial.println("MQTT Connected!");
}


// Log temperature
void logTemperatureData(uint32_t temp_transmit,Adafruit_MQTT_Publish& publishFeed) {

  // Publish
  Serial.print(F("Publishing temperature Data: "));
  Serial.println(temp_transmit);
  if (!publishFeed.publish(temp_transmit)) {
    Serial.println(F("Publish failed!"));
    txFailures++;
  }
  else {
    Serial.println(F("Publish succeeded!"));
    txFailures = 0;
  }

}

//Log current
void logCurrentData(uint32_t I, Adafruit_MQTT_Publish& publishFeed) {
  Serial.println(I);
  if (!publishFeed.publish(I)) {
    Serial.println(F("Publish failed!"));
    txFailures++;
  }
  else {
    Serial.println(F("Publish succeeded!"));
    txFailures = 0;
  }
}

//Log oil_level
void logOilLevelData(uint32_t distance, Adafruit_MQTT_Publish& publishFeed) {
  Serial.println(distance);
  if (!publishFeed.publish(distance)) {
    Serial.println(F("Publish failed!"));
    txFailures++;
  }
  else {
    Serial.println(F("Publish succeeded!"));
    txFailures = 0;
  }
}

void setup() {
  while (!Serial);

  // Watchdog is optional!
  //Watchdog.enable(8000);

  Serial.begin(115200);
  Serial.println(F("Adafruit FONA MQTT demo"));


// subscription
  mqtt.subscribe(&onoffbutton);
  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();

  // Initialise the FONA module
  while (! FONAconnect(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD))) {
    Serial.println("Retrying FONA");
  }

  Serial.println(F("Connected to Cellular!"));

  Watchdog.reset();
  delay(5000);  // wait a few seconds to stabilize connection
  Watchdog.reset();
}

void loop() {
  digitalWrite(TrigPin, LOW); // Set the trigger pin to low for 2uS
  delayMicroseconds(2);
  digitalWrite(TrigPin, HIGH); // Send a 10uS high to trigger ranging
  delayMicroseconds(10);
  digitalWrite(TrigPin, LOW); // Send pin low again
  int distance = pulseIn(EchoPin, HIGH,26000); // Read in times pulse
  distance= distance/58;
  Serial.print(distance);
  Serial.println("   cm");
  delay(50);// Wait 50mS before next ranging

  // Watchdog reset at start of loop--make sure everything below takes less than 8 seconds in normal operation!
  Watchdog.reset();

  // Ensure the connection to the MQTT server is alive (this will make the first
MQTT_connect();
  // Measure sensor data
  tempC = analogRead(pinTemp);
  float tempCelsius = (tempC/1024.0)*5000;
  temp_transmit = tempCelsius/10;
  Serial.print("temperature: ");
  Serial.println(temp_transmit);
  // Read current from sensor
  float I = sensor.getCurrentDC();
  // Send it to serial
  Serial.println(String("I = ") + I + " A");
  delay(time);

  Watchdog.reset();


  //publish current
  logCurrentData(I, current_feed);
  //publish temperature
  logTemperatureData(temp_transmit, temperature_feed);
  //publish oil_level
  logOilLevelData(distance, oil_level);
  Watchdog.reset();
  delay(2000);

 //wait for 5s
  delay(5000);

}
