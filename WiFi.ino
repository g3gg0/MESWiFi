
const char *ssid = "...";
const char *password = "...";
bool connecting = false;


void wifi_setup()
{
  Serial.printf("[WiFi] Connecting...\n");
  WiFi.begin(ssid, password);
  connecting = true;
  led_set(8,8,0);
}

bool wifi_loop(void)
{
  int status = WiFi.status();
  int curTime = millis();
  static int nextTime = 0;
  static int stateCounter = 0;

  if(nextTime > curTime)
  {
    return false;
  }

  /* standard refresh time */
  nextTime = curTime + 100;
  
  switch(status)
  {
    case WL_CONNECTED:
      if(connecting)
      {
        led_set(0,4,0);
        connecting = false;
        Serial.print("[WiFi] Connected, IP address: ");
        Serial.println(WiFi.localIP());
      }
      else
      {
        static int last_rssi = -1;
        int rssi = WiFi.RSSI();

        if(last_rssi != rssi)
        {
          int maxRssi = 40;
          int minRssi = 90;
          int br = minRssi + rssi;
          br = (br<0) ? 0 : (br > maxRssi) ? maxRssi : br;

          if(rssi < -80)
          {
            led_set(maxRssi-br, br, 0);
          }
          
          last_rssi = rssi;
        }
      }
      break;

    case WL_CONNECTION_LOST:
      Serial.printf("[WiFi] Connection lost\n");
      
      led_set(32,8,0);
      connecting = false;
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      nextTime = curTime + 500;
      break;

    case WL_CONNECT_FAILED:
      Serial.printf("[WiFi] Connection failed\n");
      led_set(255,0,0);
      connecting = false;
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      nextTime = curTime + 1000;
      break;

    case WL_NO_SSID_AVAIL:
      Serial.printf("[WiFi] No SSID\n");
      led_set(32,0,32);
      connecting = false;
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      nextTime = curTime + 2000;
      break;

    case WL_SCAN_COMPLETED:
      Serial.printf("[WiFi] Scan completed\n");
      connecting = false;
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      break;

    case WL_DISCONNECTED:
      if(!connecting)
      {
        Serial.printf("[WiFi] Disconnected\n");
        led_set(255,0,255);
        connecting = false;
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        break;
      }
      else
      {
        if(++stateCounter > 50)
        {
          Serial.printf("[WiFi] Timeout, aborting\n");
          led_set(255,0,255);
          connecting = false;
          WiFi.disconnect();
          WiFi.mode(WIFI_OFF);
        }
      }

    case WL_IDLE_STATUS:
      if(!connecting)
      {
        connecting = true;
        Serial.printf("[WiFi]  Idle, connecting to %s\n", ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        stateCounter = 0;
        break;
      }
      
    case WL_NO_SHIELD:
      if(!connecting)
      {
        connecting = true;
        Serial.printf("[WiFi]  Disabled (%d), connecting to %s\n", status, ssid);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid, password);
        stateCounter = 0;
        break;
      }
  }
    
  return false;
}

