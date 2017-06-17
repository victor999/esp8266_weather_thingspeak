#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <NodeMcuFile.h>
#include <ThingSpeak.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_TSL2561_U.h>
#include <ESP8266mDNS.h>
#include <ESP8266HTTPUpdateServer.h>

void sendData(float temp_A, float hum_A, float pressure_A, float light_A, String& chNum_A, String& logApiCode_A);
void handleRoot();
void handleNotFound();
void sendPage();

const char* host = "esp8266-weather-webupdate";
const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "admin";

ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

String channelNumber;
const String channelNumFile = "channelNum";

String logApiCode;
const String logApiCodeFile = "logApiKey";

bool readParamRes = false;

NodeMcuFile f;

WiFiClient thingSpeakClient;

Adafruit_BME280 bme; // I2C
Adafruit_TSL2561_Unified tsl = Adafruit_TSL2561_Unified(TSL2561_ADDR_FLOAT, 12345);

long lastMsg = 0;

char apiCodeBuff[50] = "";

void setup() 
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!bme.begin()) 
  {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
    while (1);
  }
  else
  {
    Serial.println("bme OK");
  }

  /* Initialise TSL2561 */
  if(!tsl.begin())
  {
    /* There was a problem detecting the TSL2561 ... check your connections */
    Serial.print("Ooops, no TSL2561 detected ... Check your wiring or I2C ADDR!");
    while(1);
  }
  else
  {
    Serial.println("tsl OK");
  }

   /* Setup the sensor gain and integration time */
  configureSensor();
  
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
  Serial.println("Connected");

  Serial.print(WiFi.localIP());

  Serial.println();

  httpServer.on ( "/", handleRoot );
  httpServer.onNotFound ( handleNotFound );

  MDNS.begin(host);

  httpUpdater.setup(&httpServer, update_path, update_username, update_password);

  httpServer.begin();

  MDNS.addService("http", "tcp", 80);

  Serial.println("Server started");

  ThingSpeak.begin(thingSpeakClient);

  //File system
  f.start();

  //Open log channel number
  bool tempRes1 = f.readFile(channelNumFile, channelNumber);

  //Open log API code
  bool tempRes2 = f.readFile(logApiCodeFile, logApiCode);

  if(tempRes1 && tempRes2)
  {
    readParamRes = true;
  }
}

void loop() 
{
  httpServer.handleClient();
  
  long now = millis();
  if (now - lastMsg > 100000) 
  {
    lastMsg = now;
    
    float temp = bme.readTemperature();
    float hum = bme.readHumidity();
    float pressure = bme.readPressure() / 100.0F;

    /* Get a new sensor event */ 
    sensors_event_t event;
    tsl.getEvent(&event);
   
    /* Display the results (light is measured in lux) */
    if (event.light)
    {
      Serial.print(event.light); Serial.println(" lux");
    }
    
    sendData(temp, hum, pressure, event.light, channelNumber, logApiCode);
  }

}

void sendData(float temp_A, float hum_A, float pressure_A, float light_A, String& chNum_A, String& logApiCode_A)
{
  if(!readParamRes)
    return;

  Serial.println(temp_A);
  Serial.println(hum_A);
  Serial.println(pressure_A);

  strcpy(apiCodeBuff, logApiCode_A.c_str());

  ThingSpeak.setField(1, temp_A);
  ThingSpeak.setField(2, hum_A);
  ThingSpeak.setField(3, pressure_A);
  ThingSpeak.setField(4, light_A);

  int stat = ThingSpeak.writeFields(chNum_A.toInt(), apiCodeBuff);

  Serial.print("write status:");
  Serial.println(stat);
}

void handleRoot() 
{  
  if(httpServer.hasArg("CH_NUM") || httpServer.hasArg("CODE")) 
  {
    if(httpServer.hasArg("CH_NUM"))
    {
      channelNumber = httpServer.arg("CH_NUM");
      Serial.print("Channel:");
      Serial.println(channelNumber);
      f.saveFile(channelNumFile, channelNumber);
    }
    if(httpServer.hasArg("CODE"))
    {
      logApiCode = httpServer.arg("CODE");
      Serial.print("API code:");
      Serial.println(logApiCode);
      f.saveFile(logApiCodeFile, logApiCode);
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
        <title>IOT Weather Webserver</title>\
        <style>\
          body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
        </style>\
        </head>\
        <body>\
          <h1>IOT Weather Webserver</h1>\
          <BR>\
          <FORM ACTION=\"http://%s\" method=get >\
          Channel number: %s\
          <BR>\
          <INPUT TYPE=TEXT NAME=\"CH_NUM\" VALUE=\"%s\" SIZE=\"25\" MAXLENGTH=\"50\"><BR>\
          <BR>\
          Write API: %s\
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
      channelNumber.c_str(),
      channelNumber.c_str(),
      logApiCode.c_str(),
      logApiCode.c_str(),
      WiFi.localIP().toString().c_str()
    );
//  Serial.println(temp);
  httpServer.send ( 200, "text/html", temp );
}

void displaySensorDetails(void)
{
  sensor_t sensor;
  tsl.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" lux");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" lux");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" lux");  
  Serial.println("------------------------------------");
  Serial.println("");
  delay(500);
}

/**************************************************************************/
/*
    Configures the gain and integration time for the TSL2561
*/
/**************************************************************************/
void configureSensor(void)
{
  /* You can also manually set the gain or enable auto-gain support */
  // tsl.setGain(TSL2561_GAIN_1X);      /* No gain ... use in bright light to avoid sensor saturation */
  // tsl.setGain(TSL2561_GAIN_16X);     /* 16x gain ... use in low light to boost sensitivity */
  tsl.enableAutoRange(true);            /* Auto-gain ... switches automatically between 1x and 16x */
  
  /* Changing the integration time gives you better sensor resolution (402ms = 16-bit data) */
  tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_13MS);      /* fast but low resolution */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_101MS);  /* medium resolution and speed   */
  // tsl.setIntegrationTime(TSL2561_INTEGRATIONTIME_402MS);  /* 16-bit data but slowest conversions */

  /* Update these values depending on what you've set above! */  
  Serial.println("------------------------------------");
  Serial.print  ("Gain:         "); Serial.println("Auto");
  Serial.print  ("Timing:       "); Serial.println("13 ms");
  Serial.println("------------------------------------");
}


