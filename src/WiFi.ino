
#include <DNSServer.h>

DNSServer dnsServer;

bool connecting = false;
bool wifi_captive = false;
char wifi_error[64];
int wifi_rssi = 0;

void wifi_setup()
{
    Serial.printf("[WiFi] Connecting to '%s', password '%s'...\n", current_config.wifi_ssid, current_config.wifi_password);
    sprintf(wifi_error, "");
    WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
    connecting = true;
    led_set(1, 8, 8, 0);
}

void wifi_off()
{
    connecting = false;
    WiFi.disconnect();
    WiFi.mode(WIFI_OFF);
}

void wifi_enter_captive()
{
    wifi_off();
    WiFi.softAP(CONFIG_SOFTAPNAME);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[WiFi] Local IP: %s\n", WiFi.softAPIP().toString().c_str());

    wifi_captive = true;

    /* reset captive idle timer */
    www_activity();
}

bool wifi_loop(void)
{
    int status = WiFi.status();
    uint32_t curTime = millis();
    static uint32_t nextTime = 0;
    static uint32_t stateCounter = 0;

    if (wifi_captive)
    {
        dnsServer.processNextRequest();
        led_set(1, 0, ((millis() % 250) > 125) ? 0 : 255, 0);

        /* captive mode, but noone cares */
        if (!www_is_captive_active())
        {
            Serial.printf("[WiFi] Timeout in captive, trying known networks again\n");
            sprintf(wifi_error, "Timeout in captive, trying known networks again");
            dnsServer.stop();
            wifi_off();
            wifi_captive = false;
            stateCounter = 0;
            sprintf(wifi_error, "");
        }
        return true;
    }

    if (nextTime > curTime)
    {
        return false;
    }

    /* standard refresh time */
    nextTime = curTime + 500;

    /* when stuck at a state, disconnect */
    if (++stateCounter > 20)
    {
        Serial.printf("[WiFi] Timeout connecting\n");
        sprintf(wifi_error, "Timeout - incorrect password?");
        wifi_off();
    }

    if (strcmp(wifi_error, ""))
    {
        Serial.printf("[WiFi] Entering captive mode. Reason: '%s'\n", wifi_error);

        wifi_enter_captive();

        stateCounter = 0;
        return false;
    }

    switch (status)
    {
        case WL_CONNECTED:
            if (connecting)
            {
                led_set(1, 0, 4, 0);
                connecting = false;
                Serial.print("[WiFi] Connected, IP address: ");
                Serial.println(WiFi.localIP());
                stateCounter = 0;
                sprintf(wifi_error, "");
            }
            else
            {
                static int last_rssi = -1;
                wifi_rssi = WiFi.RSSI();

                if (last_rssi != wifi_rssi)
                {
                    float maxRssi = -70;
                    float minRssi = -90;
                    float strRatio = (wifi_rssi - minRssi) / (maxRssi - minRssi);
                    float strength = min(1, max(0, strRatio));
                    float brightness = 0.05f;
                    int r = brightness * 255.0f * (1.0f - strength);
                    int g = brightness * 255.0f * strength;

                    led_set(1, r, g, 0);

                    if(current_config.verbose & 1)
                    {
                        Serial.printf("[WiFi] RSSI %d, strength: %1.2f, r: %d, g: %d\n", wifi_rssi, strength, r, g);
                    }

                    last_rssi = wifi_rssi;
                }

                /* happy with this state, reset counter */
                stateCounter = 0;
            }
            break;

        case WL_CONNECTION_LOST:
            Serial.printf("[WiFi] Connection lost\n");
            sprintf(wifi_error, "Network found, but connection lost");
            led_set(1, 32, 8, 0);
            wifi_off();
            break;

        case WL_CONNECT_FAILED:
            Serial.printf("[WiFi] Connection failed\n");
            sprintf(wifi_error, "Network found, but connection failed");
            wifi_off();
            break;

        case WL_NO_SSID_AVAIL:
            Serial.printf("[WiFi] No SSID with that name\n");
            sprintf(wifi_error, "Network not found");
            wifi_off();
            break;

        case WL_SCAN_COMPLETED:
            Serial.printf("[WiFi] Scan completed\n");
            wifi_off();
            break;

        case WL_DISCONNECTED:
            if (!connecting)
            {
                Serial.printf("[WiFi] Disconnected\n");
                led_set(1, 255, 0, 255);
                wifi_off();
            }
            break;

        case WL_IDLE_STATUS:
            if (!connecting)
            {
                connecting = true;
                Serial.printf("[WiFi]  Idle, connect to '%s'\n", current_config.wifi_ssid);
                WiFi.mode(WIFI_STA);
                WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
            }
            else
            {
                Serial.printf("[WiFi]  Idle, connecting...\n");
            }
            break;

        case WL_NO_SHIELD:
            if (!connecting)
            {
                connecting = true;
                Serial.printf("[WiFi]  Disabled (%d), connecting to '%s'\n", status, current_config.wifi_ssid);
                WiFi.mode(WIFI_STA);
                WiFi.begin(current_config.wifi_ssid, current_config.wifi_password);
            }
            break;

        default:
            Serial.printf("[WiFi]  unknown (%d), disable\n", status);
            wifi_off();
            break;
    }

    return false;
}
