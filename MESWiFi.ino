
#include <WiFi.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <SPIFFS.h>

bool ota_active = false;

void setup()
{
  led_setup();
  led_set(16,0,0);
  
  Serial.begin(115200);
  Serial.printf("\n\n\n");

  Serial.printf("[i] SDK:          '%s'\n", ESP.getSdkVersion());
  Serial.printf("[i] CPU Speed:    %d MHz\n", ESP.getCpuFreqMHz());
  Serial.printf("[i] Chip Id:      %06X\n", ESP.getEfuseMac());
  Serial.printf("[i] Flash Mode:   %08X\n", ESP.getFlashChipMode());
  Serial.printf("[i] Flash Size:   %08X\n", ESP.getFlashChipSize());
  Serial.printf("[i] Flash Speed:  %d MHz\n", ESP.getFlashChipSpeed() / 1000000);
  Serial.printf("[i] Heap          %d/%d\n", ESP.getFreeHeap(), ESP.getHeapSize());
  Serial.printf("[i] SPIRam        %d/%d\n", ESP.getFreePsram(), ESP.getPsramSize());
  Serial.printf("\n");
  Serial.printf("[i] Starting\n");

  Serial.printf("[i]   Setup WiFi\n");
  wifi_setup();
  Serial.printf("[i]   Setup OTA\n");
  ota_setup();
  Serial.printf("[i]   Setup Time\n");
  time_setup();
  Serial.printf("[i]   Setup SPIFFS\n");
  if(!SPIFFS.begin(true))
  {
      Serial.println("[E]   SPIFFS Mount Failed");
  }
  Serial.printf("[i]   Setup LON bus\n");
  lon_setup();
  //Serial.printf("[i]   Setup MQTT\n");
  //mqtt_setup();
  Serial.printf("[i]   Setup Relays\n");
  relays_setup();
  Serial.println("Setup done");

  relays_set(0, true);
  relays_set(1, true);
  relays_set_timed(0, 2000, false);
  relays_set_timed(1, 5000, false);
}

void loop()
{
  bool hasWork = false;

  if(!ota_active)
  {
    hasWork |= led_loop();
    hasWork |= wifi_loop();
    hasWork |= time_loop();
    hasWork |= lon_loop();
    //hasWork |= mqtt_loop();
    //hasWork |= push_loop();
    hasWork |= tempsens_loop();
    hasWork |= relays_loop();
  }
  hasWork |= ota_loop();

  if(!hasWork)
  {
    delay(100);
  }
}

