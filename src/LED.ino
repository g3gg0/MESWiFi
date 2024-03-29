
#include <FastLED.h>

#define LED_GPIO 27
#define LED_COUNT 1

CRGB leds[LED_COUNT];

bool led_inhibit = false;

void led_setup()
{
    FastLED.addLeds<NEOPIXEL, LED_GPIO>(leds, LED_COUNT);
    for(int num = 0; num < LED_COUNT; num++)
    {
        led_set_adv(num, 0, 0, 0, false);
    }
    led_set_adv(0, 0, 0, 16, true);
}

void led_set_adv(uint8_t n, uint8_t r, uint8_t g, uint8_t b, bool commit)
{
    if(led_inhibit)
    {
        return;
    }

    leds[n % LED_COUNT] = CRGB(r, g, b);

    if (commit)
    {
        FastLED.show();
    }
}

void led_set(uint8_t n, uint8_t r, uint8_t g, uint8_t b)
{
    return led_set_adv(n, r, g, b, true);
}

void led_set_inhibit(bool state)
{
    led_inhibit = state;
}

bool led_loop()
{
    return false;
}
