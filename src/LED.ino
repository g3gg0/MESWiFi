
#include "driver/rmt.h"

#define LED_CHANNEL RMT_CHANNEL_7
#define LED_MEMBLOCK 1
#define LED_GPIO 27

#define LED_RMT_CLK_DIV 1                                          /*!< RMT counter clock divider */
#define LED_RMT_TICK_100NS (80000000 / LED_RMT_CLK_DIV / 10000000) /*!< RMT counter value for 1 ns.(Source clock is APB clock) */

static rmt_config_t led_rmt_config;

void led_setup()
{
    pinMode(LED_GPIO, OUTPUT);
    digitalWrite(LED_GPIO, LOW);

    led_rmt_config.channel = static_cast<rmt_channel_t>(LED_CHANNEL);
    led_rmt_config.gpio_num = static_cast<gpio_num_t>(LED_GPIO);
    led_rmt_config.rmt_mode = RMT_MODE_TX;
    led_rmt_config.mem_block_num = LED_MEMBLOCK;
    led_rmt_config.clk_div = LED_RMT_CLK_DIV;
    led_rmt_config.tx_config.loop_en = false;
    led_rmt_config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
    led_rmt_config.tx_config.carrier_en = false;
    led_rmt_config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    led_rmt_config.tx_config.idle_output_en = true;

    rmt_config(&led_rmt_config);
}

void led_tx(uint8_t *data, uint8_t length)
{
    rmt_item32_t *items = (rmt_item32_t *)&RMTMEM.chan[LED_CHANNEL];
    uint8_t totalBit = 12 * LED_RMT_TICK_100NS;
    uint8_t oneBit = 8 * LED_RMT_TICK_100NS;
    uint8_t zeroBit = 4 * LED_RMT_TICK_100NS;

    for (uint32_t bitNum = 0; bitNum < length * 8; bitNum++)
    {
        uint8_t bytePos = bitNum / 8;
        uint8_t bitPos = bitNum % 8;
        uint8_t bitVal = 1 << (7 - bitPos);

        if (data[bytePos] & bitVal)
        {
            items[bitNum].level0 = 1;
            items[bitNum].duration0 = oneBit;
            items[bitNum].level1 = 0;
            items[bitNum].duration1 = totalBit - oneBit;
        }
        else
        {
            items[bitNum].level0 = 1;
            items[bitNum].duration0 = zeroBit;
            items[bitNum].level1 = 0;
            items[bitNum].duration1 = totalBit - zeroBit;
        }
    }

    /* set terminate condition */
    items[length * 8 + 1].duration0 = 0;
    items[length * 8 + 1].duration1 = 0;

    RMT.conf_ch[LED_CHANNEL].conf1.mem_rd_rst |= 1;
    RMT.conf_ch[LED_CHANNEL].conf1.mem_owner &= 0;
    RMT.conf_ch[LED_CHANNEL].conf1.tx_start |= 1;

    delay(1);
}

void led_set(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t buffer[3];

    buffer[0] = g;
    buffer[1] = r;
    buffer[2] = b;

    led_tx(buffer, 3);
}

bool led_loop()
{
    return false;
}
