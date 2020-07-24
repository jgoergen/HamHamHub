// see instructions here for uploading the html files to spiffs info:
// http://esp8266.github.io/Arduino/versions/2.0.0/doc/filesystem.html

// esp32 wrover
// partition scheme minimal

#include <WiFi.h>
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "soc/soc.h"           // Disable brownour problems
#include "soc/rtc_cntl_reg.h"  // Disable brownour problems
#include "driver/rtc_io.h"
#include <SPIFFS.h>
#include <FS.h>
#include <ESPAsyncWebServer.h>
#include "DHTesp.h"
#include <ESPmDNS.h>
#include <Adafruit_NeoPixel.h>

#define DHT_READ_DELAY    1800 // (in seconds) 1800=30 minutes, 3600=1 hour, 43200=12 hours
#define MAX_DHT_HISTORY   144 //

#define DHT11_PIN         13
#define NEOPIXEL_PIN      15

DHTesp dht;
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(1, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

#define FILE_PHOTO "/photo.jpg"

void tempTask(void *pvParameters);
bool getTemperature();
void triggerGetTemp();
int appState = 0;
int bootPhase = 0;
boolean takeNewPhoto = false;
TaskHandle_t tempTaskHandle = NULL;
SemaphoreHandle_t baton;
ComfortState cf;
bool tasksEnabled = false;
volatile float lastTemp = 0.0f;
volatile float lastHum = 0.0f;
float temps[MAX_DHT_HISTORY];
float hums[MAX_DHT_HISTORY];
int readingIndex = 0;
int dhtReadDelayCount = 0; 

void setup()
{
  // startup delay, prevents overcurrent from preventing programming
  delay(1000);
  Serial.begin(74880);
}

void loop()
{
  switch(appState) {

    case 0:
      boot_Update();
      break;
      
    case 1: 
      Webserver_Update();
      break;
  }
}

void boot_Update() {

  switch(bootPhase) {

    case 0: 
      // init hdt11
      Serial.println("Starting Hamhamhub");
      pixels.begin();
      pixels.setPixelColor(0, pixels.Color(80,150,255));
      pixels.show();
      bootPhase = 1;
      break;

    case 1:
      if (!SPIFFS.begin(true)) {
        Serial.println("An Error has occurred while mounting SPIFFS");
        ESP.restart();
      }

      Serial.println("SPIFFS started");
      bootPhase = 2;
      break;

    case 2:
      // Turn-off the 'brownout detector'
      WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
      bootPhase = 3;
      break;

    case 3:
    {
      bootPhase = 4;
      break;
    }

    case 4:
      initDHTTask();
      bootPhase = 5;
      
    case 5:
      Webserver_Init();
      Serial.println("Webserver started");
      appState = 1;
      break;
  }
}

bool initDHTTask() 
{
  byte resultValue = 0;
  
  // Initialize temperature sensor
  dht.setup(DHT11_PIN, DHTesp::DHT11);

  delay(100);

  Serial.println("DHT's Started");

  baton = xSemaphoreCreateMutex();
  
  // Start task to get temperature
  xTaskCreatePinnedToCore(
      tempTask,                       /* Function to implement the task */
      "tempTask ",                    /* Name of the task */
      8000,                           /* Stack size in words */
      NULL,                           /* Task input parameter */
      2,                              /* Priority of the task */
      &tempTaskHandle,                /* Task handle. */
      1);                             /* Core where the task should run */

  delay(500);
 
  if (tempTaskHandle == NULL) {
    
    Serial.println("Failed to start task for temperature update");
    return false;
  }
  
  return true;
}

void tempTask(void *pvParameters) 
{
  Serial.println("tempTask loop started");
  while (1) // tempTask loop
  {
    if (dhtReadDelayCount >= DHT_READ_DELAY) {
      
      getTemperatures(true);
      dhtReadDelayCount = 0;
      
    } else {
      
      dhtReadDelayCount ++;
    }
    
    delay(1000);
  }
}

bool getTemperatures(bool logData) 
{
  Serial.print("Starting DHT Reads, idx=");
  Serial.println(readingIndex);
  
  // Reading temperature for humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (it's a very slow sensor)
  TempAndHumidity newValues = dht.getTempAndHumidity();
  
  // Check if any reads failed and exit early (to try again).
  if (dht.getStatus() != 0) {
    Serial.println("DHT11 error status: " + String(dht.getStatusString()));
    return false;
  }

  float heatIndex = dht.computeHeatIndex(newValues.temperature, newValues.humidity);
  float dewPoint = dht.computeDewPoint(newValues.temperature, newValues.humidity);
  float cr = dht.getComfortRatio(cf, newValues.temperature, newValues.humidity);

  lastTemp = newValues.temperature;
  lastHum = newValues.humidity;

  if (logData) {
    
    if (readingIndex == MAX_DHT_HISTORY) {
  
      for (byte i = 1; i < MAX_DHT_HISTORY; i ++) {
  
        temps[i - 1] = temps[i];
        hums[i - 1] = hums[i];
      }
  
      readingIndex --;
    }
    
    temps[readingIndex] = lastTemp;
    hums[readingIndex] = lastHum;
    readingIndex ++;
  }
  
  float tempInF = (lastTemp * (9.0 / 5.0)) + 32.0;
  Serial.print("Temp in f is ");
  Serial.println(tempInF);

  float r = 0.0f;
  float g = 155.0f;
  float b = 0.0f;

  if (tempInF >= 70.0f && tempInF < 76.0f) {

  } else if (tempInF < 70.0f) {

      float howCold = abs(tempInF - 70.0f);
      b = min(255.0f, howCold * 18.0f);
      g = max(0.0f, g - b);

  } else if (tempInF >= 76.0f) {

      float howHot = abs(tempInF - 76.0f);
      r = min(255.0f, howHot * 18.0f);
      g = max(0.0f, g - r);
  }

  pixels.setPixelColor(0, pixels.Color((byte)r, (byte)g, (byte)b));
  pixels.show();
  
  Serial.println("T:" + String(newValues.temperature) + " H:" + String(newValues.humidity) + " I:" + String(heatIndex) + " D:" + String(dewPoint));
  return true;
}
