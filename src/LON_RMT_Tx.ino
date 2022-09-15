

#include "driver/rmt.h"

#define LON_TX_RMT_CHANNEL RMT_CHANNEL_4
#define LON_TX_RMT_MEMBLOCK 4
#define LON_TX_RMT_GPIO LON_TX

#define LON_TX_RMT_ST_END (1 << (3 * LON_TX_RMT_CHANNEL))

#define LON_TX_RMT_CLK_DIV 10                                        /*!< RMT counter clock divider */
#define LON_TX_RMT_TICK_US (80000000 / LON_TX_RMT_CLK_DIV / 1000000) /*!< RMT counter value for 1 us.(Source clock is APB clock) */

#define LON_TX_RMT_TICKS(us) ((us) * (LON_TX_RMT_TICK_US))

#define LON_0_TICKS LON_TX_RMT_TICKS(13)

static intr_handle_t lon_tx_inthandle = NULL;
static rmt_config_t lon_tx_config;
volatile bool lon_tx_done = false;
volatile bool lon_tx_running = false;

static IRAM_ATTR void lon_tx_interrupt(void *arg)
{
    if (RMT.int_st.val & LON_TX_RMT_ST_END)
    {
        RMT.int_clr.val |= LON_TX_RMT_ST_END;
        digitalWrite(LON_TX_EN, LOW);
        digitalWrite(LON_TX, LOW);
        lon_tx_done = true;
        lon_tx_running = false;
    }
}

void lon_tx_setup()
{
    lon_tx_config.channel = static_cast<rmt_channel_t>(LON_TX_RMT_CHANNEL);
    lon_tx_config.gpio_num = static_cast<gpio_num_t>(LON_TX_RMT_GPIO);
    lon_tx_config.rmt_mode = RMT_MODE_TX;
    lon_tx_config.mem_block_num = LON_TX_RMT_MEMBLOCK;
    lon_tx_config.clk_div = LON_TX_RMT_CLK_DIV;
    lon_tx_config.tx_config.loop_en = false;
    lon_tx_config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    lon_tx_config.tx_config.carrier_en = false;
    lon_tx_config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    lon_tx_config.tx_config.idle_output_en = true;

    rmt_config(&lon_tx_config);

    esp_intr_alloc(ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_SHARED, lon_tx_interrupt, NULL, &lon_tx_inthandle);

    /* enable interrupt */
    RMT.int_ena.val |= LON_TX_RMT_ST_END;
}

void lon_tx(uint8_t *data, uint8_t length)
{
    rmt_item32_t *items = (rmt_item32_t *)&RMTMEM.chan[LON_TX_RMT_CHANNEL];
    uint8_t lastLevel = 1;
    uint32_t shortBit = LON_0_TICKS;

    if (length * 8 + 2 > 64 * LON_TX_RMT_MEMBLOCK)
    {
        return;
    }

    /* this will destroy parts of our buffer, so call it first */
    led_set(0, 255, 0, 0);

    for (uint32_t bitNum = 0; bitNum < length * 8; bitNum++)
    {
        uint32_t bytePos = bitNum / 8;
        uint32_t bitPos = bitNum % 8;
        uint32_t bitVal = 1 << (7 - bitPos);

        if (data[bytePos] & bitVal)
        {
            lastLevel = !lastLevel;
            items[bitNum].level0 = lastLevel;
            items[bitNum].duration0 = shortBit;
            items[bitNum].level1 = lastLevel;
            items[bitNum].duration1 = shortBit;
        }
        else
        {
            items[bitNum].level0 = !lastLevel;
            items[bitNum].duration0 = shortBit;
            items[bitNum].level1 = lastLevel;
            items[bitNum].duration1 = shortBit;
        }
    }

    /* set terminate condition */
    items[length * 8 + 0].level0 = lastLevel;
    items[length * 8 + 0].duration0 = 2 * shortBit;
    items[length * 8 + 0].level1 = lastLevel;
    items[length * 8 + 0].duration1 = 2 * shortBit;
    items[length * 8 + 1].duration0 = 0;
    items[length * 8 + 1].duration1 = 0;

    /* wait for a milisecond-window without activity */
    while (true)
    {
        int actStart = lon_rx_activity;

        delay(2);

        if (lon_rx_activity == actStart)
        {
            break;
        }
    }

    lon_tx_done = false;
    lon_tx_running = true;

    digitalWrite(LON_TX_EN, HIGH);
    RMT.conf_ch[LON_TX_RMT_CHANNEL].conf1.mem_rd_rst |= 1;
    RMT.conf_ch[LON_TX_RMT_CHANNEL].conf1.mem_owner &= 0;
    RMT.conf_ch[LON_TX_RMT_CHANNEL].conf1.tx_start |= 1;

    uint32_t startTime = millis();
    while (!lon_tx_done)
    {
        if (millis() - startTime > 100)
        {
            uint8_t type = 9;
            lon_udp_out.beginPacket(udpAddress, udpPort);
            lon_udp_out.write((const uint8_t *)&type, 1);
            lon_udp_out.endPacket();
            break;
        }
        delay(1);
    }
    led_set(0, 0, 0, 0);
}
