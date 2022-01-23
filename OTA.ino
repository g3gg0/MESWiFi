
#include <ArduinoOTA.h>

WiFiServer ota_knock(80);

void ota_setup()
{
  ArduinoOTA.setHostname("MES-WiFi-v1-4");

  ArduinoOTA
    .onStart([]() {
      String type;
      
      led_set(255,0,0);
      lon_disable();
      ota_active = true;
    })
    .onEnd([]() {
      lon_rx_enable();
      ota_active = false;
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      led_set(255 - (progress / (total / 255)), 0, (progress / (total / 255)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ota_knock.begin();
}

bool ota_loop()
{
  int curTime = millis();
  static int offTime = 0;
  static bool ota_allowed = false;

  if(ota_allowed && !ota_active)
  {
    if(offTime < curTime)
    {
      ota_allowed = false;
      Serial.println("[OTA] Disabled");
      ArduinoOTA.end();
    }
    else
    {
      ArduinoOTA.handle();
    }
  }
  
  WiFiClient client = ota_knock.available();
 
  if(client)
  {
    if(client.connected()) 
    {
      if(!ota_allowed)
      {
        ota_allowed = true;
        offTime = curTime + 10*60000;
        Serial.println("[OTA] Enabled");
        client.println("[OTA] Enabled");
        ArduinoOTA.begin();
        lon_save = true;
      }
      else
      {
        offTime = curTime + 60000;
        Serial.println("[OTA] Already enabled");
        client.println("[OTA] Already enabled");
      }
      client.stop();
    }
  }

  return ota_active;
}

