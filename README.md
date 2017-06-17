# esp8266_weather_thingspeak

Weather station with ESP8266 and Thingspeak.
Project info: https://hackaday.io/project/19324-weather-station

Consists of 2 parts: 
-Weather station, sending the data from BME280 and TLS2561 to Thingspeak
-Receiver, displaying the data on the LCD

Libraries:
https://github.com/adafruit/Adafruit_SSD1306

(modify H file: enable #define SSD1306_128_64 and disable #define SSD1306_128_32)

https://github.com/adafruit/Adafruit-GFX-Library

https://github.com/adafruit/Adafruit_BME280_Library

https://github.com/adafruit/Adafruit_TSL2561

https://github.com/tzapu/WiFiManager