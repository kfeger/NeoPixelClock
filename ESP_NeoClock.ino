#include <TimeLib.h>  //https://github.com/PaulStoffregen/Time
#include <Timezone.h> //https://github.com/JChristensen/Timezone
#include <TimeLord.h> //https://github.com/probonopd/TimeLord
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <Adafruit_NeoPixel.h>  //https://github.com/adafruit/Adafruit_NeoPixel
//needed for WiFi-Manager library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>  //https://github.com/tzapu/WiFiManager

//#define ESP12 1

#define NEXT_SYNC 3600  //1h zwischen Sync

//Stunden hell
#define HOUR_C   0x400000
#define HOUR_CL  0x0B0000
#define HOUR_CLL 0x030000
//Stunden dunkel
#define DARK_HOUR_C   0x0B0000
#define DARK_HOUR_CL  0x040000
#define DARK_HOUR_CLL 0x020000

//Minuten hell
#define MIN_C  0x1800
#define MIN_CL 0x0500
//Minuten dunkel
#define DARK_MIN_C  0x0400
#define DARK_MIN_CL 0x0200

//Sekunden hell
#define SEC_C      0x60
//Sekunden dunkel
#define DARK_SEC_C 0x06

//Stundenmarker hell
#define MARK_C      0x101010
//Stundenmarker dunkel
#define DARK_MARK_C 0x020202

#define HOUR_OFF 0x00ffff
#define MIN_OFF  0xff00ff
#define SEC_OFF  0xffff00

#ifdef ESP12
  int NeoPin = 14;
  #define HOSTNAME "NeoPixel12Clock"
  #define AP_NAME "NeoPixel12AP"
#else
  int NeoPin = 3; //eigentlich Rx-Pin, mal sehen...
  #define HOSTNAME "NeoPixel01Clock"
  #define AP_NAME "NeoPixel01AP"
#endif

int NeoLength = 60;
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NeoLength, NeoPin, NEO_GRB + NEO_KHZ800);

int NowSecond = 0;
int NowMinute = 0;
bool NextMinute = false;
int NowHour = 0;
bool NextHour = false;
const int Offset = 7;
uint32_t PrevColor = 0;
#define SyncPin 2

byte HourC = 64, MinC = 64, SecC = 64;
byte hourval, minuteval, secondval;
bool TimeUpdate = false;


//time_t getNtpTime();
void digitalClockDisplay();
void printDigits(int digits);
//void sendNTPpacket(IPAddress &address);

// what is our longitude (west values negative) and latitude (south values negative)
TimeLord City;

//Briesnitzr Höhe19a, Dresden
/*
float const LONGITUDE = 13.658055;
float const LATITUDE = 51.062369;
*/

//Postbrookstraße 2f, Bremerhaven (sagt Google...)

float const LONGITUDE = 8.624611;
float const LATITUDE = 53.530929;


byte Heute[6];
byte PreDay = 0;
bool SonneDa = true;
bool PreSonneDa = false;

// local time zone definition
// Central European Time (Frankfurt, Paris) from Timezone/examples/WorldClock/WorldClock.pde
TimeChangeRule CEST = {"CEST", Last, Sun, Mar, 2, 120};     //Central European Summer Time
TimeChangeRule CET = {"CET ", Last, Sun, Oct, 3, 60};       //Central European Standard Time
Timezone LTZ(CEST, CET);    // this is the Timezone object that will be used to calculate local time
TimeChangeRule *tcr;        //pointer to the time change rule, use to get the TZ abbrev

time_t utc, local;
uint32_t LastSync = 0, NextSync = 0;


//const char ssid[] = "smuldom";       //  your network SSID (name)
//const char pass[] = "Cityer str 33a";       // your network password

/* Don't hardwire the IP address or we won't get the benefits of the pool.
    Lookup the IP address for the host name instead */
//IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
IPAddress timeServerIP, LocalIP; // time.nist.gov NTP server address
const char* ntpServerName = "de.pool.ntp.org";
#define NTP_PACKET_SIZE 48  // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
boolean recalculate = 1;
String text = "";

const int timeZone = 1;     // Central European Time

// A UDP instance to let us send and receive packets over UDP
WiFiUDP udp;
unsigned int localPort = 2930;  // local port to listen for udp packets

void setup() {
  Serial.begin(115200);
  pinMode(SyncPin, OUTPUT);
  digitalWrite(SyncPin, HIGH);
  pinMode(NeoPin, OUTPUT);
  digitalWrite(NeoPin, LOW);
  Serial.println();
  Serial.println("Connecting via WiFiManager ");
  strip.begin();
  strip.show();
  SetMarkerColor(); //irgendwie lila...
  //SetMarker(1);
  delay(500);
  
  WiFi.hostname(HOSTNAME);
  WiFiManager wifiManager;
  //reset saved settings
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);
  if (!wifiManager.autoConnect(AP_NAME)) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  digitalWrite(SyncPin, LOW);
  ClearStrip();
  SetMarker(1);

  Serial.print("IP address assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(udp.localPort());
  Serial.println(F("Hole NTP-Zeit"));
  setSyncProvider(getNTPTime);
  //setTime(getNTPTime());
  if (timeStatus() != timeSet) {
    Serial.println("Uhr nicht mit NTP synchronisiert");
    while (1); //wdt provozieren
  }
  else
    Serial.println("NTP hat die Systemzeit gesetzt");
  LastSync = now();
  NextSync = now() + NEXT_SYNC;

  ClearStrip();
  SunUpDown();
  PreSonneDa = ~SonneDa;

  // Sonnenauf- und -untergang bestimmen
  City.TimeZone(1 * 60); // tell TimeLord what timezone your RTC is synchronized to. You can ignore DST
  // as long as the RTC never changes back and forth between DST and non-DST
  City.DstRules(3, 4, 10, 4, 60); //Umschaltung WZ/SZ am 4. Sonntag im ärz, zurück am 4. Sonntag im Oktober. SZ 60 min plus
  City.Position(LATITUDE, LONGITUDE); // tell TimeLord where in the world we are
}

time_t prevDisplay = 0; // when the digital clock was displayed

void loop() {
  if (now() >= NextSync) {
    setTime(getNTPTime());
    NextSync = now() + NEXT_SYNC;
  }
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      digitalClockDisplay();
      NowSecond = second();
      NowMinute = minute();
      NowHour = hour();
      if (NowHour >= 12)
        NowHour -= 12;
      SunUpDown();
      /*if (LTZ.locIsDST(now()))
        digitalWrite(SyncPin, HIGH);
      else
        digitalWrite(SyncPin, LOW);*/
      SetHands();
    }
  }
}

void digitalClockDisplay() {
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits) {
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}

