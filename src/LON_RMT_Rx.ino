
#include "driver/rmt.h"

#define LON_RX_RMT_CHANNEL_A RMT_CHANNEL_0
#define LON_RX_RMT_CHANNEL_B RMT_CHANNEL_2
#define LON_RX_RMT_MEMBLOCKS 2
#define LON_RX_RMT_GPIO LON_RX

#define LON_RX_RMT_INT_TX_END_B (1)
#define LON_RX_RMT_INT_RX_END_B (2)
#define LON_RX_RMT_INT_ERROR_B (4)
#define LON_RX_RMT_INT_THR_EVNT_B (1 << 24)

#define LON_RX_RMT_INT_TX_END(channel) (LON_RX_RMT_INT_TX_END_B << (channel * 3))
#define LON_RX_RMT_INT_RX_END(channel) (LON_RX_RMT_INT_RX_END_B << (channel * 3))
#define LON_RX_RMT_INT_ERROR(channel) (LON_RX_RMT_INT_ERROR_B << (channel * 3))
#define LON_RX_RMT_INT_THR_EVNT(channel) ((LON_RX_RMT_INT_THR_EVNT_B) << (channel))

/* the number of bits per RMT transaction */
#define LON_RX_EDGES_PER_CH (LON_RX_RMT_MEMBLOCKS * 64 * 2)

#define LON_RX_RMT_CLK_DIV 4
#define LON_RX_RMT_TICK_US (80000000 / LON_RX_RMT_CLK_DIV / 1000000)

#define LON_RX_RMT_TICKS(us) ((us) * (LON_RX_RMT_TICK_US))

/* LON with 78 kbaud, all values in microseconds */
#define LON_0_MIN LON_RX_RMT_TICKS(3)
#define LON_0_MAX LON_RX_RMT_TICKS(18)
#define LON_1_MIN LON_RX_RMT_TICKS(18)
#define LON_1_MAX LON_RX_RMT_TICKS(40)
#define LON_VIOLATION LON_RX_RMT_TICKS(100)

#define LON_RX_IDLE_MICROS 150

#define LON_ERR_NONE 0
#define LON_ERR_BIT_TOO_LONG 1
#define LON_ERR_BIT_TOO_SHORT 2
#define LON_ERR_PHASE_ERROR 3
#define LON_ERR_BUFFERSIZE 4
#define LON_ERR_NO_SYNC 5
#define LON_ERR_DONE 6

volatile bool lon_rx_running = false;
intr_handle_t lon_rx_inthandle = NULL;

#define LON_RMT_BUFFER_LEN 255 * 2 * 8
uint16_t lon_rmt_buffer[LON_RMT_BUFFER_LEN];
uint8_t lon_rmt_bitmiss[8];
uint32_t lon_rmt_buffer_pos = 0;

static uint32_t bits_overlap = 2;
static uint32_t bits_missed_on_error = 2;

static inline int IRAM_ATTR lon_rx_valid(int duration)
{
    if (duration > LON_1_MAX)
    {
        lon_last_error = LON_ERR_BIT_TOO_LONG;
        return 0;
    }

    if (duration < LON_0_MIN)
    {
        lon_last_error = LON_ERR_BIT_TOO_SHORT;
        return 0;
    }

    lon_last_error = LON_ERR_NONE;
    return 1;
}

static inline int IRAM_ATTR lon_rx_level(int duration)
{
    if (duration > LON_0_MAX)
    {
        if (lon_stat.avg_len_1 == 0)
        {
            lon_stat.avg_len_1 = duration;
        }
        lon_stat.avg_len_1 = (9 * lon_stat.avg_len_1 + duration) / 10;
        lon_stat.min_len_1 = MIN(lon_stat.min_len_1, duration);
        lon_stat.max_len_1 = MAX(lon_stat.max_len_1, duration);
        return 1;
    }

    if (lon_stat.avg_len_0 == 0)
    {
        lon_stat.avg_len_0 = duration;
    }
    lon_stat.avg_len_0 = (9 * lon_stat.avg_len_0 + duration) / 10;
    lon_stat.min_len_0 = MIN(lon_stat.min_len_0, duration);
    lon_stat.max_len_0 = MAX(lon_stat.max_len_0, duration);
    return 0;
}

static uint32_t IRAM_ATTR lon_rx_bit(uint32_t duration)
{
    static uint32_t lastZero = 0;
    static uint32_t synced = 0;
    static uint32_t syncBits = 0;

    uint32_t bitNum = 7 - (lon_buffer_pos % 8);
    uint32_t bitVal = 1 << bitNum;
    uint32_t byteNum = lon_buffer_pos / 8;
    uint32_t bitValue = 0;

    if (duration == 0xFFFFFFFF)
    {
        lastZero = 0;
        synced = 0;
        syncBits = 0;
        lon_last_error = 0;
        lon_buffer_pos = 0;
        return 0;
    }

    if (duration == 0 || duration > LON_1_MAX)
    {
        if (lastZero == 1)
        {
            lon_buffer[byteNum] &= ~bitVal;
        }
        else
        {
            lon_buffer[byteNum] |= bitVal;
        }

        lon_queue_packet();
        return LON_ERR_DONE;
    }

    if (lon_buffer_pos >= LON_BUFFER_LEN * 8)
    {
        lon_last_error = LON_ERR_BUFFERSIZE;
    }

    if (lon_last_error || !lon_rx_valid(duration))
    {
        lon_queue_packet();
        return lon_last_error;
    }

    /* handle biphase manchester code */
    if (lon_rx_level(duration) == 0)
    {
        /* zero bits consist of a BMC double-change, check for the second transition */
        if (!lastZero)
        {
            /* first transition, return */
            lastZero = 1;
            return 0;
        }

        lastZero = 0;
        bitValue = 0;
    }
    else
    {
        /* must not happen that we encounter a half BMC zero bit */
        if (lastZero)
        {
            lon_last_error = LON_ERR_PHASE_ERROR;
            lon_queue_packet();
            return lon_last_error;
        }

        bitValue = 1;
        lastZero = 0;
    }

    /* now check if we are already synced */
    if (!synced)
    {
        if (bitValue == 0)
        {
            if (syncBits < 5)
            {
                lastZero = 0;
                synced = 0;
                syncBits = 0;
                return 0;
            }
            else
            {
                synced = 1;
            }
            return 0;
        }

        syncBits++;

        if (lon_bit_duration == 0)
        {
            lon_bit_duration = duration;
        }
        lon_bit_duration = (lon_bit_duration + duration) / 2;
        return 0;
    }

    /* now we are handling the data bits */
    if (bitValue == 0)
    {
        lon_buffer[byteNum] &= ~bitVal;
    }
    else
    {
        lon_buffer[byteNum] |= bitVal;
    }

    lon_buffer_pos++;

    return 0;
}

static uint32_t IRAM_ATTR lon_rx_handle_edges()
{
    for (uint32_t pos = 0; pos < lon_rmt_buffer_pos; pos++)
    {
        if (lon_rx_bit(lon_rmt_buffer[pos] & 0x7FFF))
        {
            break;
        }
    }
    lon_rmt_buffer_pos = 0;
    return 0;
}

static void IRAM_ATTR lon_rx_setup_rmt(uint32_t channel)
{
    RMT.conf_ch[channel].conf0.val = 0x01271050;
    RMT.conf_ch[channel].conf1.val = 0x000A0020;

    RMT.conf_ch[channel].conf0.val = 0x31100002;
    RMT.conf_ch[channel].conf1.val = 0x00000F20;

    RMT.conf_ch[channel].conf0.clk_en = 0;
    RMT.conf_ch[channel].conf0.div_cnt = LON_RX_RMT_CLK_DIV;
    RMT.conf_ch[channel].conf0.mem_size = LON_RX_RMT_MEMBLOCKS;
    RMT.conf_ch[channel].conf0.carrier_en = 0;
    RMT.conf_ch[channel].conf0.carrier_out_lv = 0;
    RMT.conf_ch[channel].conf0.mem_pd = 0;
    RMT.conf_ch[channel].conf0.idle_thres = LON_VIOLATION;

    RMT.conf_ch[channel].conf1.tx_conti_mode = 0;
    RMT.conf_ch[channel].conf1.ref_cnt_rst = 0;
    RMT.conf_ch[channel].conf1.rx_filter_en = 0;
    RMT.conf_ch[channel].conf1.rx_filter_thres = 0;
    RMT.conf_ch[channel].conf1.idle_out_lv = 0;
    RMT.conf_ch[channel].conf1.idle_out_en = 1;
    RMT.conf_ch[channel].conf1.ref_always_on = 1;
    RMT.conf_ch[channel].conf1.mem_owner = 1;

    RMT.conf_ch[channel].conf1.mem_wr_rst = 1;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 1;
    RMT.conf_ch[channel].conf1.mem_wr_rst = 0;
    RMT.conf_ch[channel].conf1.mem_rd_rst = 0;

    RMT.conf_ch[channel].conf1.mem_owner = 1;
    RMT.int_clr.val = LON_RX_RMT_INT_RX_END(channel);
    RMT.int_clr.val = LON_RX_RMT_INT_ERROR(channel);
}

static void IRAM_ATTR lon_rx_stop_rmt(uint32_t channel)
{
    RMT.int_ena.val &= ~LON_RX_RMT_INT_RX_END(channel);
    RMT.int_ena.val &= ~LON_RX_RMT_INT_ERROR(channel);
    RMT.int_clr.val = LON_RX_RMT_INT_RX_END(channel);
    RMT.int_clr.val = LON_RX_RMT_INT_ERROR(channel);

    /* required for resetting a channel properly from error mode */
    RMT.conf_ch[channel].conf0.val = 0;
    RMT.conf_ch[channel].conf1.val = 0;
    lon_rx_setup_rmt(channel);
}

static void IRAM_ATTR lon_rx_start_rmt(uint32_t channel)
{
    RMT.conf_ch[channel].conf1.rx_en = 1;

    /* enable interrupts */
    RMT.int_ena.val |= LON_RX_RMT_INT_RX_END(channel);
    RMT.int_ena.val |= LON_RX_RMT_INT_ERROR(channel);
}

static uint32_t IRAM_ATTR lon_rx_queue(uint32_t channel, int lost)
{
    uint32_t rmt_items = (RMT.status_ch[channel] & 0x3FF) - (channel * 0x40);
    uint32_t entries = (rmt_items * 2) - bits_overlap + lost;
    uint32_t size = entries * sizeof(uint16_t);
    uint16_t *items = (uint16_t *)&RMTMEM.chan[channel];

    if (lon_rmt_buffer_pos + entries < LON_RMT_BUFFER_LEN)
    {
        memcpy(&lon_rmt_buffer[lon_rmt_buffer_pos], &items[bits_overlap - lost], size);
        lon_rmt_buffer_pos += entries;
    }

    lon_rx_stop_rmt(channel);

    if (rmt_items == LON_RX_RMT_MEMBLOCKS * 64)
    {
        return 1;
    }

    lon_rx_running = false;
    
    return 0;
}

static IRAM_ATTR void lon_rx_rmt_check(uint32_t channel)
{
    /* END interrupt means that a timeout occurred and we are done reading */
    if (RMT.int_st.val & LON_RX_RMT_INT_RX_END(channel))
    {
        RMT.int_clr.val = LON_RX_RMT_INT_RX_END(channel);
        lon_rx_queue(channel, lon_rmt_bitmiss[channel]);
    }

    /* ERR interrupts happen whenever a buffer overrun happened */
    if (RMT.int_st.val & LON_RX_RMT_INT_ERROR(channel))
    {
        RMT.int_clr.val = LON_RX_RMT_INT_ERROR(channel);

        /* but... when having ERR situation reset, it still generates a spurious interrupt.
           check for error flag instead of believing the interrupt */
        if (((RMT.status_ch[channel] >> 28) & 0x01) == 1)
        {
            lon_rx_queue(channel, lon_rmt_bitmiss[channel]);
        }

        /* when this RMT was in error state (was overflowed as we have more bits than the buffer can hold),
         * it goes into some error state and misses two bits next time. compensate that. ugly...
         */
        lon_rmt_bitmiss[channel] = bits_missed_on_error;
    }
}

static IRAM_ATTR void lon_rx_rmt_isr(void *arg)
{
    lon_rx_rmt_check(LON_RX_RMT_CHANNEL_A);
    lon_rx_rmt_check(LON_RX_RMT_CHANNEL_B);

    if (!lon_rx_running && lon_rmt_buffer_pos)
    {
        lon_rx_handle_edges();
    }
}

static void IRAM_ATTR lon_rx_pin_isr()
{
    /* keep track of the number of edges as we have to start RMT manually */
    static uint32_t edge_count = 0;
    static uint32_t last_edge = 0;
    uint32_t delta = micros() - last_edge;

    /* if no RX transfer is active, start a new one */
    if (delta > LON_RX_IDLE_MICROS)
    {
        if (!lon_rx_running)
        {
            edge_count = 0;
            lon_rmt_buffer_pos = 0;
            lon_rx_running = true;
            lon_rx_bit(0xFFFFFFFF);
            lon_rx_start_rmt(LON_RX_RMT_CHANNEL_A);
            memset(lon_rmt_bitmiss, 0x00, sizeof(lon_rmt_bitmiss));
        }
    }
    else if (lon_rx_running)
    {
        /* else count the edges that have appeared since then */
        edge_count++;

        /* delay a bit to make sure we dont catch the last edge sometimes */
        // delayMicroseconds(1);

        /* when a RMT buffer is getting full, start the next RMT buffer */
        if ((edge_count % (LON_RX_EDGES_PER_CH - bits_overlap)) == 0)
        {
            switch ((edge_count / (LON_RX_EDGES_PER_CH - bits_overlap)) % 2)
            {
            case 0:
                lon_rx_start_rmt(LON_RX_RMT_CHANNEL_A);
                break;

            case 1:
                lon_rx_start_rmt(LON_RX_RMT_CHANNEL_B);
                break;
            }
        }
    }

    last_edge = micros();
    lon_rx_activity++;

    return;
}

void lon_rx_disable()
{
    detachInterrupt(digitalPinToInterrupt(LON_RX_RMT_GPIO));
}

void lon_rx_enable()
{
    attachInterrupt(digitalPinToInterrupt(LON_RX_RMT_GPIO), lon_rx_pin_isr, CHANGE);
}

void lon_rx_setup()
{
    pinMode(LON_RX_RMT_GPIO, INPUT);
    pinMatrixInAttach(LON_RX_RMT_GPIO, RMT_SIG_IN0_IDX + LON_RX_RMT_CHANNEL_A, 0);
    pinMatrixInAttach(LON_RX_RMT_GPIO, RMT_SIG_IN0_IDX + LON_RX_RMT_CHANNEL_B, 0);

    lon_rx_setup_rmt(LON_RX_RMT_CHANNEL_A);
    lon_rx_setup_rmt(LON_RX_RMT_CHANNEL_B);

    esp_intr_alloc(ETS_RMT_INTR_SOURCE, ESP_INTR_FLAG_SHARED, lon_rx_rmt_isr, NULL, &lon_rx_inthandle);

    lon_rx_bit(0xFFFFFFFF);
    lon_rx_enable();
}

void lon_rx_loop()
{
}
