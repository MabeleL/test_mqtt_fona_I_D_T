

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
#define FONA_TX  3   // FONA serial TX pin (pin 3 for shield).
#define FONA_RST 4   // FONA reset pin (pin 4 for shield)
#define LED 10

SoftwareSerial fonaSS = SoftwareSerial(FONA_TX, FONA_RX);

Adafruit_FONA fona = Adafruit_FONA(FONA_RST);

/************************* Broker Setup *********************************/
/*#define AIO_SERVER      "lab-i.tech"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""
#define AIO_KEY         ""
#define DEVICE_ID "GreenMarine_1"
*/
/************ Global State (you don't need to change this!) ******************/

/**************************Public Broker**************************/
#define AIO_SERVER      "broker.mqttdashboard.com"
#define AIO_SERVERPORT  1883
#define AIO_USERNAME    ""
#define AIO_KEY         ""
#define DEVICE_ID "GreenMarine_1"

/*****************Global State II*****************************/

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
Adafruit_MQTT_Publish assetPub = Adafruit_MQTT_Publish(&mqtt, "feeds/" AIO_USERNAME "/gms/" DEVICE_ID);

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

// Calculate distance between two points
float distanceCoordinates(float flat1, float flon1, float flat2, float flon2) {

  // Variables
  float dist_calc=0;
  float dist_calc2=0;
  float diflat=0;
  float diflon=0;

  // Calculations
  diflat  = radians(flat2-flat1);
  flat1 = radians(flat1);
  flat2 = radians(flat2);
  diflon = radians((flon2)-(flon1));

  dist_calc = (sin(diflat/2.0)*sin(diflat/2.0));
  dist_calc2 = cos(flat1);
  dist_calc2*=cos(flat2);
  dist_calc2*=sin(diflon/2.0);
  dist_calc2*=sin(diflon/2.0);
  dist_calc +=dist_calc2;

  dist_calc=(2*atan2(sqrt(dist_calc),sqrt(1.0-dist_calc)));

  dist_calc*=6371000.0; //Converting to meters

  return dist_calc;
}

void printFloat(float value, int places) {
  // this is used to cast digits
  int digit;
  float tens = 0.1;
  int tenscount = 0;
  int i;
  float tempfloat = value;

    // make sure we round properly. this could use pow from <math.h>, but doesn't seem worth the import
  // if this rounding step isn't here, the value  54.321 prints as 54.3209

  // calculate rounding term d:   0.5/pow(10,places)
  float d = 0.5;
  if (value < 0)
    d *= -1.0;
  // divide by ten for each decimal place
  for (i = 0; i < places; i++)
    d/= 10.0;
  // this small addition, combined with truncation will round our values properly
  tempfloat +=  d;

  // first get value tens to be the large power of ten less than value
  // tenscount isn't necessary but it would be useful if you wanted to know after this how many chars the number will take

  if (value < 0)
    tempfloat *= -1.0;
  while ((tens * 10.0) <= tempfloat) {
    tens *= 10.0;
    tenscount += 1;
  }


  // write out the negative if needed
  if (value < 0)
    Serial.print('-');

  if (tenscount == 0)
    Serial.print(0, DEC);

  for (i=0; i< tenscount; i++) {
    digit = (int) (tempfloat/tens);
    Serial.print(digit, DEC);
    tempfloat = tempfloat - ((float)digit * tens);
    tens /= 10.0;
  }

  // if no places after decimal, stop now and return
  if (places <= 0)
    return;

  // otherwise, write the point and continue on
  Serial.print('.');

  // now write out each decimal place by shifting digits one by one into the ones place and writing the truncated value
  for (i = 0; i < places; i++) {
    tempfloat *= 10.0;
    digit = (int) tempfloat;
    Serial.print(digit,DEC);
    // once written, subtract off that digit
    tempfloat = tempfloat - (float) digit;
  }
}

void setup() {
  while (!Serial);
  // Watchdog is optional!
  //Watchdog.enable(8000);
  Serial.begin(115200);
  Serial.println(F("Adafruit FONA MQTT demo"));
  pinMode(TrigPin,OUTPUT);
  pinMode(EchoPin,INPUT);
  Serial.println(F("Geofencing with Adafruit IO & FONA808"));

  // Initialise the FONA module
  while (! FONAconnect(F(FONA_APN), F(FONA_USERNAME), F(FONA_PASSWORD))) {
    Serial.println("Retrying FONA");
  }

  Serial.println(F("Connected to Cellular!"));

  // Enable GPS.
  fona.enableGPS(true);

  // Initial GPS read
  bool gpsFix = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);
  initialLatitude = latitude;
  initialLongitude = longitude;

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
  int distance_sonar = pulseIn(EchoPin, HIGH,26000); // Read in times pulse
  distance_sonar= distance_sonar/58;
  Serial.print(distance_sonar);
  Serial.println("   cm");
  delay(50);// Wait 50mS before next ranging

  // Grab a GPS reading.
 float latitude, longitude, speed_kph, heading, altitude;
 bool gpsFix = fona.getGPS(&latitude, &longitude, &speed_kph, &heading, &altitude);

 Serial.print("Latitude: ");
 printFloat(latitude, 5);
 Serial.println("");

 Serial.print("Longitude: ");
 printFloat(longitude, 5);
 Serial.println("");

 // Calculate distance between new & old coordinates
 float distance = distanceCoordinates(latitude, longitude, initialLatitude, initialLongitude);

 Serial.print("Distance: ");
 printFloat(distance, 5);
 Serial.println("");
 // Set alarm on?
  if (distance > maxDistance) {
    Serial.println("The track has veered off");
  }

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

  StaticJsonBuffer<100> jsonBuffer;
  JsonObject& payload = jsonBuffer.createObject();
  payload["tracker_id"] = DEVICE_ID;
  payload["temperature"] = temp_transmit;
  payload["current"] = I;
  payload["fuel_level"] = distance_sonar;
  payload["latitude"] = latitude;
  payload["longitude"] = longitude;
  payload["geo-distance"] = distance;

  String sPayload = "";
  payload.printTo(sPayload);
  const char* cPayload = &sPayload[0u];

  // Now we can publish stuff!
 Serial.print(F("\nPublishing "));
 Serial.print(cPayload);
 Serial.print("...");


  if(!assetPub.publish(cPayload)) {
  Serial.println(F("Failed"));
  } else {
  Serial.println(F("OK!"));
  }

  // ping the server to keep the mqtt connection alive
if(! mqtt.ping()) {
  Console.println(F("MQTT Ping failed."));
}
  Watchdog.reset();
  delay(2000);

 //wait for 5s
  delay(5000);

}

