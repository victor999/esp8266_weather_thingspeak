#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <NodeMcuFile.h>
#include <ThingSpeak.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

void handleRoot();
void handleNotFound();
void sendPage();
unsigned long ntpUnixTime (UDP &udp);
void readDataFromThingspeak();
bool readData(float& temp_A, float& hum_A, float& pressure_A, float& light_A, String& chNum_A, String& logApiCode_A);

const char* host = "esp8266-weather-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

#define OLED_RESET 14
Adafruit_SSD1306 display(OLED_RESET);

String g_channelNumber;
const String g_channelNumFile = "channelNum";

String g_logApiCode;
const String g_logApiCodeFile = "logApiKey";

bool g_readParamRes = false;

NodeMcuFile f;

WiFiClient thingSpeakClient;

long lastMsg = 0;

char apiCodeBuff[50] = "";

char wifiStatus[20] = "init";

int hours = 0;
int mins = 0;

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  // by default, we'll generate the high voltage from the 3.3v line internally! (neat!)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x64)

  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0,20);
  
  display.println("Starting...");
  
  display.display();

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  //reset settings - for testing
  //wifiManager.resetSettings();

  //sets timeout until configuration portal gets turned off
  //useful to make it all retry or go to sleep
  //in seconds
  wifiManager.setTimeout(300);
  
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect("AutoConnectAP")) 
  {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }

  //if you get here you have connected to the WiFi
  display.println("Connected");

  display.print(WiFi.localIP());

  display.println();

  httpServer.on ( "/", handleRoot );
  httpServer.onNotFound ( handleNotFound );

  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);

  httpServer.begin();

  MDNS.addService("http", "tcp", 80);
  
  display.println("Server started");

  display.display();

  ThingSpeak.begin(thingSpeakClient);

  //File system
  f.start();

  //Open log channel number
  bool tempRes1 = f.readFile(g_channelNumFile, g_channelNumber);

  //Open log API code
  bool tempRes2 = f.readFile(g_logApiCodeFile, g_logApiCode);

  if(tempRes1 && tempRes2)
  {
    g_readParamRes = true;
  }

  readDataFromThingspeak();
}

void loop() 
{
  httpServer.handleClient();
  
  long now = millis();
  if (now - lastMsg > 100000) 
  {
    lastMsg = now;

    readDataFromThingspeak();
  }
}

void readDataFromThingspeak()
{
  float temp = 0.0;
  float hum = 0.0;
  float pressure = 0.0;
  float light = 0.0;

  static WiFiUDP udp;
  
  if(readData(temp, hum, pressure, light, g_channelNumber, g_logApiCode))
  {
    unsigned long unixTime = ntpUnixTime(udp);
        
    if(unixTime)
    {
      hours = (unixTime % 86400) / 3600;
      mins = ((unixTime % 86400) - hours * 3600) / 60;
    }
    
    sprintf(wifiStatus, "Connection OK   %02d:%02d", hours, mins);

    display.clearDisplay();
  
    display.setTextColor(WHITE);
  
    display.setCursor(0,0);
    display.setTextSize(1);
    display.println(wifiStatus);
    
    display.setCursor(0,16);
  
    display.setTextSize(4);
    display.print((int)(temp + 0.5));
    display.print(" C");
    display.println();
    display.setTextSize(2);
    display.print((int)(pressure + 0.5));
    display.print("  ");
    display.print((int)(hum + 0.5));
    display.println("%");
   
    display.display();
  
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println(" C");
  
    Serial.print("Hum: ");
    Serial.print(hum);
    Serial.println(" %");
  
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" hPa");
  
    Serial.print("Light: ");
    Serial.print(light);
    Serial.println(" lm");
  }
  else
  {
    sprintf(wifiStatus, "Connection Error");
  }
}

bool readData(float& temp_A, float& hum_A, float& pressure_A, float& light_A, String& chNum_A, String& logApiCode_A)
{  
  if(!g_readParamRes)
    return false;

  strcpy(apiCodeBuff, logApiCode_A.c_str());

  temp_A = ThingSpeak.readFloatField(chNum_A.toInt(), 1, apiCodeBuff);
  if(!temp_A)
    return false;

  hum_A = ThingSpeak.readFloatField(chNum_A.toInt(), 2, apiCodeBuff);
  if(!hum_A)
    return false;
  
  pressure_A = ThingSpeak.readFloatField(chNum_A.toInt(), 3, apiCodeBuff);
  if(!pressure_A)
    return false;
  
  light_A = ThingSpeak.readFloatField(chNum_A.toInt(), 4, apiCodeBuff);
  
  return true;
}

/*
 * © Francesco Potortì 2013 - GPLv3 - Revision: 1.13
 *
 * Send an NTP packet and wait for the response, return the Unix time
 *
 * To lower the memory footprint, no buffers are allocated for sending
 * and receiving the NTP packets.  Four bytes of memory are allocated
 * for transmision, the rest is random garbage collected from the data
 * memory segment, and the received packet is read one byte at a time.
 * The Unix time is returned, that is, seconds from 1970-01-01T00:00.
 */
unsigned long inline ntpUnixTime (UDP &udp)
{
  static int udpInited = udp.begin(123); // open socket on arbitrary port

  const char timeServer[] = "pool.ntp.org";  // NTP server

  // Only the first four bytes of an outgoing NTP packet need to be set
  // appropriately, the rest can be whatever.
  const long ntpFirstFourBytes = 0xEC0600E3; // NTP request header

  // Fail if WiFiUdp.begin() could not init a socket
  if (! udpInited)
    return 0;

  // Clear received data from possible stray received packets
  udp.flush();

  // Send an NTP request
  if (! (udp.beginPacket(timeServer, 123) // 123 is the NTP port
   && udp.write((byte *)&ntpFirstFourBytes, 48) == 48
   && udp.endPacket()))
    return 0;       // sending request failed

  // Wait for response; check every pollIntv ms up to maxPoll times
  const int pollIntv = 150;   // poll every this many ms
  const byte maxPoll = 15;    // poll up to this many times
  int pktLen;       // received packet length
  for (byte i=0; i<maxPoll; i++) {
    if ((pktLen = udp.parsePacket()) == 48)
      break;
    delay(pollIntv);
  }
  if (pktLen != 48)
    return 0;       // no correct packet received

  // Read and discard the first useless bytes
  // Set useless to 32 for speed; set to 40 for accuracy.
  const byte useless = 40;
  for (byte i = 0; i < useless; ++i)
    udp.read();

  // Read the integer part of sending time
  unsigned long time = udp.read();  // NTP time
  for (byte i = 1; i < 4; i++)
    time = time << 8 | udp.read();

  // Round to the nearest second if we want accuracy
  // The fractionary part is the next byte divided by 256: if it is
  // greater than 500ms we round to the next second; we also account
  // for an assumed network delay of 50ms, and (0.5-0.05)*256=115;
  // additionally, we account for how much we delayed reading the packet
  // since its arrival, which we assume on average to be pollIntv/2.
  time += (udp.read() > 115 - pollIntv/8);

  // Discard the rest of the packet
  udp.flush();

  return time - 2208988800ul;   // convert NTP time to Unix time
}

void handleRoot() 
{  
  if(httpServer.hasArg("CH_NUM") || httpServer.hasArg("CODE")) 
  {
    if(httpServer.hasArg("CH_NUM"))
    {
      g_channelNumber = httpServer.arg("CH_NUM");
      Serial.print("Channel:");
      Serial.println(g_channelNumber);
      f.saveFile(g_channelNumFile, g_channelNumber);
    }
    if(httpServer.hasArg("CODE"))
    {
      g_logApiCode = httpServer.arg("CODE");
      Serial.print("API code:");
      Serial.println(g_logApiCode);
      f.saveFile(g_logApiCodeFile, g_logApiCode);
    }
    sendPage();
  }
  else 
  {
    sendPage();
  }
}

void handleNotFound() 
{
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += httpServer.uri();
  message += "\nMethod: ";
  message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += httpServer.args();
  message += "\n";

  for ( uint8_t i = 0; i < httpServer.args(); i++ ) 
  {
    message += " " + httpServer.argName ( i ) + ": " + httpServer.arg ( i ) + "\n";
  }

  httpServer.send ( 404, "text/plain", message );
}

void sendPage()
{
  char temp[1000];
  
  snprintf ( temp, 1000,

    "<html>\
      <head>\
        <title>IOT Weather Reader Webserver</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
        </head>\
        <body>\
          <h1>IOT Weather Reader Webserver</h1>\
          <BR>\
          <FORM ACTION=\"http://%s\" method=get >\
          Channel number: %s\
          <BR>\
          <INPUT TYPE=TEXT NAME=\"CH_NUM\" VALUE=\"%s\" SIZE=\"25\" MAXLENGTH=\"50\"><BR>\
          <BR>\
          Read API: %s\
          <BR>\
          <INPUT TYPE=TEXT NAME=\"CODE\" VALUE=\"%s\" SIZE=\"30\" MAXLENGTH=\"50\"><BR>\
          <BR>\
          <INPUT TYPE=SUBMIT NAME=\"submit\" VALUE=\"Apply\">\
          <BR>\
          <A HREF=\"javascript:window.location.href = 'http://%s'\">Click to refresh the page</A>\
          </FORM>\
          <BR>\
        </body>\
      </html>",
  
      WiFi.localIP().toString().c_str(),
      g_channelNumber.c_str(),
      g_channelNumber.c_str(),
      g_logApiCode.c_str(),
      g_logApiCode.c_str(),
      WiFi.localIP().toString().c_str()
    );
//  Serial.println(temp);
  httpServer.send ( 200, "text/html", temp );
}
