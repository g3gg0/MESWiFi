
#include <WiFi.h>
#include <WiFiUdp.h>

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) < (b) ? (b) : (a))

const char *udpAddress = "192.168.1.255";
const int udpPort = 3333;
const int udpPortIn = 3334;

WiFiUDP lon_udp_out;
WiFiUDP lon_udp_in;

#define LON_RX 35
#define LON_TX 25
#define LON_TX_EN 26

#define LONSTATE_VERSION 0x0100

extern float tempsens_value;

volatile uint32_t lon_rx_activity = 0;

struct tm timeStruct;


uint32_t manual_set_temperature = 80;
bool manual_set_active = false;
uint32_t manual_set_next = 0;
uint32_t manual_set_end = 0;
uint32_t manual_set_duration = 1800;


typedef struct
{
    uint64_t magic1;
    uint32_t version;

    /* must not be used from stored state */
    uint32_t stat_start_valid;
    struct tm stat_start;

    /* restored from SPIFFS */
    uint32_t error;
    uint32_t rx_count;
    uint32_t crc_errors;
    uint32_t last_error_minutes;
    uint32_t burning_minutes;
    uint32_t avg_len_0;
    uint32_t avg_len_1;
    uint32_t min_len_0;
    uint32_t min_len_1;
    uint32_t max_len_0;
    uint32_t max_len_1;
    uint32_t igniteStatPos;
    uint8_t igniteStats[24 * 12];
    uint32_t ignites_24h;
    int32_t var_sel_00;
    uint32_t var_sel_10;
    uint32_t var_sel_12;
    uint32_t var_sel_72;
    uint32_t var_sel_8A;
    uint32_t var_sel_110;
    uint32_t var_nv_12;
    uint32_t var_nv_15_pmx;
    uint32_t var_nv_1B;
    uint32_t var_nv_1B_pmx;
    uint32_t var_nv_23_pmx;
    uint32_t var_nv_24_fbh;
    uint32_t var_nv_25_fbh;
    uint32_t var_nv_24_ww;
    uint32_t var_nv_25_ww;
    uint32_t var_nv_25_pmx;
    uint32_t var_nv_2A;
    uint32_t var_nv_2B;
    uint32_t var_nv_2B_last;
    uint32_t var_nv_2F;
    uint32_t var_nv_31;
    uint32_t var_nv_32;
    char var_nv_10[8];
    uint32_t var_nv_10_state;

    /* footer */
    uint64_t magic2;
} lonStat_t;

volatile lonStat_t lon_stat;

extern bool time_valid;
extern volatile bool lon_rx_running;
#define LON_BUFFER_LEN 512
uint8_t lon_buffer[LON_BUFFER_LEN];
uint32_t lon_buffer_pos = 0;
uint32_t lon_bit_duration = 0;
uint32_t lon_last_error = 0;

bool lon_save = false;

typedef struct
{
    uint8_t filled;
    uint8_t errorCode;
    uint32_t bitCount;
    uint32_t bitDuration;
    uint8_t payload[LON_BUFFER_LEN];
} lonPacket_t;

#define LON_PACKET_COUNT 8
volatile lonPacket_t lon_rx_packets[LON_PACKET_COUNT];

uint16_t crc16_table[] = {
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5,
    0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B,
    0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
    0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C,
    0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B,
    0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
    0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
    0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738,
    0xF7DF, 0xE7FE, 0xD79D, 0xC7BC, 0x48C4, 0x58E5,
    0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969,
    0xA90A, 0xB92B, 0x5AF5, 0x4AD4, 0x7AB7, 0x6A96,
    0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
    0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
    0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03,
    0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
    0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6,
    0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
    0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB,
    0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1,
    0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C,
    0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2,
    0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
    0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
    0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447,
    0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
    0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2,
    0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
    0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
    0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827,
    0x18C0, 0x08E1, 0x3882, 0x28A3, 0xCB7D, 0xDB5C,
    0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0,
    0x2AB3, 0x3A92, 0xFD2E, 0xED0F, 0xDD6C, 0xCD4D,
    0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
    0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
    0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA,
    0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

uint16_t crc16(const uint8_t *data_p, uint8_t length)
{
    uint16_t crc = 0xFFFF;

    for (int pos = 0; pos < length; pos++)
    {
        crc = crc16_table[((crc >> 8) ^ data_p[pos]) & 0xFF] ^ (crc << 8);
    }

    return crc ^ 0xFFFF;
}

void lon_queue_packet()
{
    uint16_t bitCount = lon_buffer_pos + 1;
    uint8_t byteCount = min(bitCount / 8, LON_BUFFER_LEN);

    lon_buffer_pos = 0;

    if (byteCount == 0)
    {
        // Serial.printf("lon_queue_packet: empty\n");
        return;
    }

    for (uint32_t pos = 0; pos < LON_PACKET_COUNT; pos++)
    {
        if (!lon_rx_packets[pos].filled)
        {
            for (uint32_t bytePos = 0; bytePos < byteCount; bytePos++)
            {
                lon_rx_packets[pos].payload[bytePos] = lon_buffer[bytePos];
            }

            lon_rx_packets[pos].errorCode = lon_last_error;
            lon_rx_packets[pos].bitCount = bitCount;
            lon_rx_packets[pos].bitDuration = lon_bit_duration;
            lon_rx_packets[pos].filled = 1;

            lon_stat.rx_count++;

            // Serial.printf("lon_queue_packet: queued at %d\n", pos);
            return;
        }
    }
}

void lon_parse(const uint8_t *payload, uint8_t length)
{
    uint8_t pos = 0;

    pos++;
    uint8_t npdu_pduFormat = (payload[pos] >> 4) & 3;
    uint8_t npdu_addFormat = (payload[pos] >> 2) & 3;
    uint8_t npdu_domainLength = (payload[pos] >> 0) & 3;
    uint8_t npdu_src = 0;

    uint8_t npdu_domainLengths[] = {0, 8, 24, 48};
    uint8_t npdu_addLengths[] = {24, 24, 32, 72};

    pos++;
    npdu_src = payload[pos + 1] & 0x7F;
    if (npdu_addFormat == 2 && ((payload[pos + 1] & 0x80) == 0))
    {
        /* this is type 2b */
        pos += 2;
    }
    pos += npdu_addLengths[npdu_addFormat] / 8;
    pos += npdu_domainLengths[npdu_domainLength] / 8;

    /* only handle PDUFmt APDU */
    switch (npdu_pduFormat)
    {
    /* APDU */
    case 1:
    {
        uint16_t spduType = (payload[pos] >> 4) & 0x07;
        pos++;

        switch (spduType)
        {
        case 0x02:
        {
            uint8_t apduType = payload[pos] >> 5;
            uint8_t apduCommand = payload[pos] & 0x1F;

            pos++;

            /* NM/ND response */
            if (apduType == 1)
            {
                /* Network variable fetch */
                if (apduCommand == 0x13)
                {
                    uint8_t nvIndex = payload[pos];
                    pos++;

                    switch (nvIndex)
                    {
                    /* PMX (60) NV ID 0x10 is "Display" */
                    case 0x10:
                    {
                        if (npdu_src == 60)
                        {
                            sprintf((char *)lon_stat.var_nv_10, "%c%c %d", payload[pos], payload[pos + 1], payload[pos + 3]);
                            lon_stat.var_nv_10_state = payload[pos] - 0x30;
                        }
                        break;
                    }

                    /* PMX (60) NV ID 0x12 is "Drehzahl ist" */
                    case 0x12:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_12 = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x15 is "Fördermenge Soll" */
                    case 0x15:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_15_pmx = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* UML C1 (10) NV ID 0x1B is "Boilertemperatur" */
                    /* PMX (60) NV ID 0x1B is  "Fördermenge Ist" */
                    case 0x1B:
                    {
                        if (npdu_src == 10)
                        {
                            lon_stat.var_nv_1B = (payload[pos] << 8) | payload[pos + 1];
                        }
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_1B_pmx = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x23 is "Leistung Soll" */
                    case 0x23:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_23_pmx = payload[pos];
                        }
                        break;
                    }
                    /* C1 (10/11) NV ID 0x24 is "Pumpe" in pct */
                    case 0x24:
                    {
                        if (npdu_src == 10)
                        {
                            lon_stat.var_nv_24_fbh = payload[pos];
                        }
                        if (npdu_src == 11)
                        {
                            lon_stat.var_nv_24_ww = payload[pos];
                        }
                        break;
                    }
                    /* C1 (10/11) NV ID 0x25 is "Mischventil" in level */
                    /* PMX (60) NV ID 0x25 is "Leistung" */
                    case 0x25:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_25_pmx = payload[pos];
                        }
                        if (npdu_src == 10)
                        {
                            int16_t lvl = (payload[pos] << 8) | payload[pos + 1];
                            lon_stat.var_nv_25_fbh = lvl;
                        }
                        if (npdu_src == 11)
                        {
                            int16_t lvl = (payload[pos] << 8) | payload[pos + 1];
                            lon_stat.var_nv_25_ww = lvl;
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x2A is "Betriebsstunden" */
                    case 0x2A:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_2A = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x2B is "Anheizvorgänge" */
                    case 0x2B:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_2B = (payload[pos] << 8) | payload[pos + 1];

                            if (lon_stat.var_nv_2B == (lon_stat.var_nv_2B_last + 1))
                            {
                                lon_stat.igniteStats[lon_stat.igniteStatPos]++;
                            }
                            lon_stat.var_nv_2B_last = lon_stat.var_nv_2B;
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x2F is probably "Brennraumtemperatur" */
                    case 0x2F:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_2F = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x31 is probably "Abgastemperatur" */
                    case 0x31:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_31 = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    /* PMX (60) NV ID 0x32 is probably "T_Kessel_Soll" */
                    case 0x32:
                    {
                        if (npdu_src == 60)
                        {
                            lon_stat.var_nv_32 = (payload[pos] << 8) | payload[pos + 1];
                        }
                        break;
                    }
                    }
                }
            }
            break;
        }
        }
    }

    /* APDU */
    case 3:
    {
        uint16_t destinType = (payload[pos] << 8) | payload[pos + 1];

        if ((destinType & 0xC000) != 0x8000 || pos >= length)
        {
            return;
        }
        uint16_t sel = destinType & 0x3FFF;

        switch (sel)
        {
        case 0x101:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.error = (payload[pos] << 8) | payload[pos + 1];

            if (lon_stat.error == 0xFFFF)
            {
                lon_stat.error = 0;
            }
            else
            {
                lon_stat.last_error_minutes = lon_stat.burning_minutes;
            }

            push_set_error(lon_stat.error);
            break;
        }

        case 0x00:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_00 = (int16_t)((uint16_t)((payload[pos] << 8) | payload[pos + 1]));
            break;
        }
        case 0x10:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_10 = (payload[pos] << 8) | payload[pos + 1];
            break;
        }
        case 0x12:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_12 = (payload[pos] << 8) | payload[pos + 1];
            break;
        }
        case 0x72:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_72 = (payload[pos] << 8) | payload[pos + 1];
            break;
        }
        case 0x8A:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_8A = (payload[pos] << 8) | payload[pos + 1];
            break;
        }

        case 0x110:
        {
            pos += 2;

            if (pos + 1 >= length)
            {
                return;
            }
            lon_stat.var_sel_110 = (payload[pos] << 8) | payload[pos + 1];
            break;
        }
        }

        break;
    }
    }
}

void lon_disable()
{
    digitalWrite(LON_TX, LOW);
    digitalWrite(LON_TX_EN, LOW);

    lon_rx_disable();
}

void lon_write(uint8_t *data, uint32_t length)
{
    uint8_t buffer[100];

    if (length > sizeof(buffer) - 4)
    {
        return;
    }

    memcpy(&buffer[2], data, length);

    uint16_t crc = crc16(&buffer[2], length);
    uint16_t txSize = length + 4;

    buffer[0] = 0xFF;
    buffer[1] = 0xFE;
    buffer[txSize - 2] = crc >> 8;
    buffer[txSize - 1] = crc;

    lon_tx(buffer, txSize);
}

void lon_statTime()
{
    if (!lon_stat.stat_start_valid && time_valid)
    {
        getTime((struct tm *)&lon_stat.stat_start);
        lon_stat.stat_start_valid = 1;
    }
}

void lon_initStat()
{
    File file = SPIFFS.open("/lonstate.bin", "r");
    if (!file)
    {
        Serial.println("[E] failed to open file for reading");
    }
    else
    {
        file.read((uint8_t *)&lon_stat, sizeof(lon_stat));
        file.close();
        Serial.println("[i] read status from SPIFFS");
    }

    if (lon_stat.magic1 != 0xDEADBEEF || lon_stat.magic2 != 0x55AA55AA || lon_stat.version != LONSTATE_VERSION)
    {
        Serial.println("[i] incorrect magics or version, reinit status");

        memset((void *)&lon_stat, 0x00, sizeof(lonStat_t));

        lon_stat.magic1 = 0xDEADBEEF;
        lon_stat.magic2 = 0x55AA55AA;
        lon_stat.error = 0;
        lon_stat.version = LONSTATE_VERSION;

        lon_stat.var_nv_10_state = 0x7FFFFFFF;
        lon_stat.var_nv_12 = 0x7FFFFFFF;
        lon_stat.var_nv_1B = 0x7FFFFFFF;
        lon_stat.var_nv_15_pmx = 0x7FFFFFFF;
        lon_stat.var_nv_1B_pmx = 0x7FFFFFFF;
        lon_stat.var_nv_23_pmx = 0x7FFFFFFF;
        lon_stat.var_nv_24_fbh = 0x7FFFFFFF;
        lon_stat.var_nv_24_ww = 0x7FFFFFFF;
        lon_stat.var_nv_25_fbh = 0x7FFFFFFF;
        lon_stat.var_nv_25_ww = 0x7FFFFFFF;
        lon_stat.var_nv_25_pmx = 0x7FFFFFFF;
        lon_stat.var_nv_2A = 0x7FFFFFFF;
        lon_stat.var_nv_2B = 0x7FFFFFFF;
        lon_stat.var_nv_2F = 0x7FFFFFFF;
        lon_stat.var_nv_31 = 0x7FFFFFFF;
        lon_stat.var_nv_32 = 0x7FFFFFFF;
        lon_stat.var_sel_00 = 0x7FFFFFFF;
        lon_stat.var_sel_10 = 0x7FFFFFFF;
        lon_stat.var_sel_12 = 0x7FFFFFFF;
        lon_stat.var_sel_72 = 0x7FFFFFFF;
        lon_stat.var_sel_110 = 0x7FFFFFFF;
    }

    lon_stat.stat_start_valid = 0;
    lon_statTime();

    lon_stat.rx_count = 0;
    lon_stat.crc_errors = 0;
    lon_stat.min_len_0 = 0xFFFFFFFF;
    lon_stat.avg_len_0 = 0;
    lon_stat.max_len_0 = 0;
    lon_stat.min_len_1 = 0xFFFFFFFF;
    lon_stat.avg_len_1 = 0;
    lon_stat.max_len_1 = 0;
}

void lon_setup()
{
    lon_initStat();

    pinMode(LON_RX, INPUT);
    pinMode(LON_TX, OUTPUT);
    pinMode(LON_TX_EN, OUTPUT);

    digitalWrite(LON_TX, LOW);
    digitalWrite(LON_TX_EN, LOW);

    lon_tx_setup();
    lon_rx_setup();

    lon_udp_in.begin(udpPortIn);
}

bool lon_loop()
{
    int curTime = millis();
    static int nextTime = 0;
    uint8_t type = 0;
    static int nextTimeSend = 1000;
    static int nextTimeSave = 1000;
    const uint8_t nvVarsDest[] = {60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 10, 10, 10, 11, 11};
    const uint8_t nvVars[] = {0x10, 0x12, 0x15, 0x1B, 0x23, 0x25, 0x2A, 0x2B, 0x2F, 0x31, 0x32, 0x72, 0x1B, 0x24, 0x25, 0x24, 0x25};
    static int nvVarPos = 0;
    bool hasWork = false;

    if (nextTimeSave <= curTime)
    {
        File file = SPIFFS.open("/lonstate.bin", "w");
        if (!file)
        {
            Serial.println("[E] failed to open file for writing");
        }
        else
        {
            file.write((const uint8_t *)&lon_stat, sizeof(lon_stat));
            file.close();
            Serial.println("[i] cyclic status save");
        }
        nextTimeSave = curTime + 10 * 60 * 1000;
    }

    if(manual_set_active)
    {
        if(curTime >= manual_set_next)
        {
            manual_set_next = curTime + 5000;
            uint8_t manual_cmd[] = "\x00\x35\x01\xFF\x02\x54\x80\x12\x22\x60";
            uint16_t temp = manual_set_temperature * 100;

            manual_cmd[8] = temp >> 8;
            manual_cmd[9] = temp & 0xFF;

            lon_write(manual_cmd, 10);
        }

        if(curTime >= manual_set_end)
        {
            manual_set_active = false;
            manual_set_next = 0;
            manual_set_end = 0;
        }
    }

    if (nextTimeSend <= curTime || lon_save)
    {
        uint8_t nvVarCommand[] = "\x01\x19\x01\xFF\x01\x80\x54\x0D\x73\x00";

        nvVarCommand[5] |= nvVarsDest[nvVarPos];
        nvVarCommand[9] = nvVars[nvVarPos];
        nvVarPos = (nvVarPos + 1) % sizeof(nvVars);

        /* request NV variables */
        lon_write(nvVarCommand, 10);

        nextTimeSend = curTime + 500;
        hasWork = true;
        lon_save = false;
    }
    else if (WiFi.status() == WL_CONNECTED)
    {
        uint32_t received = lon_udp_in.parsePacket();

        if (received > 0)
        {
            uint8_t buffer[100];

            lon_udp_in.read(buffer, sizeof(buffer));
            lon_write(buffer, min((uint32_t)sizeof(buffer), received));

            hasWork = true;
        }
    }

    /* initalize statistics start time ASAP */
    lon_statTime();

    /* reset 24-statistics about ignition times */
    getTime(&timeStruct);

    /* count burning minutes */
    static uint32_t lastMinute = 61;
    if (lastMinute != timeStruct.tm_min)
    {
        lastMinute = timeStruct.tm_min;
        if (lon_stat.var_nv_10[0] == '5' || lon_stat.var_nv_10[0] == '8')
        {
            lon_stat.burning_minutes++;
        }
    }

    uint32_t igniteStatPos = (timeStruct.tm_hour * 12) + (timeStruct.tm_min / 5);
    if (lon_stat.igniteStatPos != igniteStatPos)
    {
        lon_stat.igniteStatPos = igniteStatPos;
        lon_stat.igniteStats[lon_stat.igniteStatPos] = 0;
    }

    lon_rx_loop();

    if (nextTime <= curTime)
    {
        /* not threadsafe. but.. who cares... */
        lon_stat.ignites_24h = 0;
        for (int slot = 0; slot < 24 * 12; slot++)
        {
            lon_stat.ignites_24h += lon_stat.igniteStats[slot];
        }

        if (WiFi.status() == WL_CONNECTED)
        {
            type = 1;
            lon_udp_out.beginPacket(udpAddress, udpPort);
            lon_udp_out.write((const uint8_t *)&type, 1);
            lon_udp_out.write((const uint8_t *)&lon_stat.min_len_0, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.avg_len_0, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.max_len_0, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.min_len_1, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.avg_len_1, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.max_len_1, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.rx_count, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.crc_errors, 4);
            uint32_t val = ESP.getFreeHeap();
            lon_udp_out.write((const uint8_t *)&val, 4);
            val = ESP.getHeapSize();
            lon_udp_out.write((const uint8_t *)&val, 4);
            val = ESP.getFreePsram();
            lon_udp_out.write((const uint8_t *)&val, 4);
            val = ESP.getPsramSize();
            lon_udp_out.write((const uint8_t *)&val, 4);

            struct tm timeStruct;
            getStartupTime(&timeStruct);
            lon_udp_out.write((const uint8_t *)&timeStruct, sizeof(struct tm));
            lon_udp_out.write((const uint8_t *)&lon_stat.stat_start, sizeof(struct tm));

            lon_udp_out.write((const uint8_t *)&lon_stat.ignites_24h, 4);
            lon_udp_out.write((const uint8_t *)&lon_stat.igniteStats, sizeof(lon_stat.igniteStats));

            uint32_t temp = tempsens_value * 100;
            lon_udp_out.write((const uint8_t *)&temp, 4);

            lon_udp_out.endPacket();
        }
        nextTime = curTime + 10000;
    }

    for (int pos = 0; pos < LON_PACKET_COUNT; pos++)
    {
        if (lon_rx_packets[pos].filled)
        {
            const uint8_t *pkt_data = (const uint8_t *)lon_rx_packets[pos].payload;
            uint8_t pkt_len = (uint8_t)lon_rx_packets[pos].bitCount / 8;

            led_set(0, 0, 255, 0);

            uint16_t crcCalc = crc16(pkt_data, pkt_len - 2);
            uint16_t crcPkt = (((uint16_t)pkt_data[pkt_len - 2] << 8) | pkt_data[pkt_len - 1]);

            if (crcCalc != crcPkt)
            {
                lon_stat.crc_errors++;
                if (lon_rx_packets[pos].errorCode == 0)
                {
                    lon_rx_packets[pos].errorCode = 6;
                }
            }
            else
            {
                lon_parse(pkt_data, pkt_len);
            }

            type = 2;

            lon_udp_out.beginPacket(udpAddress, udpPort);
            lon_udp_out.write((const uint8_t *)&type, 1);
            lon_udp_out.write((const uint8_t *)&lon_rx_packets[pos].errorCode, 1);
            lon_udp_out.write((const uint8_t *)&lon_rx_packets[pos].bitCount, 2);
            lon_udp_out.write((const uint8_t *)&lon_rx_packets[pos].bitDuration, 2);
            lon_udp_out.write(pkt_data, pkt_len);
            lon_udp_out.endPacket();

            /*
                  Serial.printf("Received packet: (%d byte) ", pkt_len);
                  for(int pos = 0; pos < pkt_len; pos++)
                  {
                    Serial.printf(" %02X", pkt_data[pos]);
                  }
                  Serial.printf("\n");
            */

            lon_rx_packets[pos].filled = 0;
            led_set(0, 0, 0, 0);

            hasWork = true;
        }
    }

    return hasWork;
}
