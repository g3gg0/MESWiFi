
const char *WarnHost = "api.pushingbox.com";
const char *warnUrlStatus = "/pushingbox?devid=%s&status=%d&msg=%s&statusMsg=%s";
const char *push_apiKey = "...";
int push_lastTime = 0;
int push_errorCode = 0;
int push_errorCodeLast = 0;

WiFiClient httpClient;

int warnRequest(const char *host, const char *path)
{
    if (!httpClient.connect(host, 80))
    {
        Serial.printf("  [E] connection to '%s' failed", host);
        return -1;
    }

    httpClient.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    httpClient.flush();
    httpClient.stop();

    return 0;
}

bool push_loop()
{
    char tmp[128];
    int curTime = millis();
    static int nextTime = 0;

    if (nextTime > curTime)
    {
        return false;
    }
    nextTime = curTime + 1000;

    if (WiFi.status() != WL_CONNECTED)
    {
        return false;
    }

    bool resend = false;

    if (push_errorCode > 0 && (curTime - push_lastTime) > 4 * 60 * 60 * 1000)
    {
        resend = true;
    }

    if ((push_errorCode != push_errorCodeLast) || resend)
    {
        const char *message = "(unbekannt)";
        const char *prefix = "FEHLER";

        switch (push_errorCode)
        {
        case -1:
        case 0:
            message = "Alles%20wieder%20gut";
            prefix = "OK";
            break;
        case 5:
            message = "Ruettelmotor";
            prefix = "DEFEKT";
            break;
        case 6:
            message = "Foerderschnecke";
            prefix = "DEFEKT";
            break;
        case 16:
            message = "Geblaese";
            prefix = "DEFEKT";
            break;
        case 76:
            message = "Kesselfuehler";
            prefix = "DEFEKT";
            break;
        case 78:
            message = "Thermocontrol%20Fuehler";
            prefix = "DEFEKT";
            break;
        case 85:
            message = "Schneckenrohr%20Fuehler";
            prefix = "DEFEKT";
            break;
        case 128:
            message = "Keine%20Flammbildung";
            prefix = "FEHLER";
            break;
        case 133:
            message = "Sicherheitsthermostat";
            prefix = "FEHLER";
            break;
        case 135:
            message = "Schneckenrohr%20Temperatur";
            prefix = "FEHLER";
            break;
        case 171:
            message = "Keine%20Flammbildung";
            prefix = "FEHLER";
            break;
        case 286:
            message = "Tuere%20offen";
            prefix = "FEHLER";
            break;
        case 381:
            message = "Keine%20Pellets";
            prefix = "FEHLER";
            break;
        case 581:
            message = "Pelletspeicher%20wird%20leer";
            prefix = "WARNUNG";
            break;
        }

        snprintf(tmp, sizeof(tmp), warnUrlStatus, push_apiKey, push_errorCode, prefix, message);
        if (warnRequest(WarnHost, tmp) == 0)
        {
            push_errorCodeLast = push_errorCode;
            push_lastTime = curTime;
        }
    }
    return false;
}

void push_set_error(int errorCode)
{
    push_errorCode = errorCode;
}
