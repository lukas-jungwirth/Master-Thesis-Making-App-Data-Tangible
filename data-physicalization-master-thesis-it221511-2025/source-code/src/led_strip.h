#ifndef LED_STRIP_H
#define LED_STRIP_H

#include <FastLED.h>

#define LED_PIN1 15
#define LED_PIN2 7
#define NUM_LEDS 6
#define BRIGHTNESS 50
#define LED_TYPE WS2812
#define COLOR_ORDER GRB

CRGB lights[2][NUM_LEDS];

struct RGBStruct
{
  int red;
  int green;
  int blue;

  RGBStruct(int r = 0, int g = 0, int b = 0) : red(r), green(g), blue(b) {}
};

RGBStruct hexToRGB(const std::string &hex)
{
  // Check if the string is properly formatted
  if (hex.size() != 7 || hex[0] != '#')
  {
    throw std::invalid_argument("Invalid hex color format.");
  }

  // Parse the hexadecimal values
  int red = std::stoi(hex.substr(1, 2), nullptr, 16);
  int green = std::stoi(hex.substr(3, 2), nullptr, 16);
  int blue = std::stoi(hex.substr(5, 2), nullptr, 16);

  return RGBStruct{red, green, blue};
}

void initLEDStrip(ColumnsStruct &columns)
{
    FastLED.setBrightness(BRIGHTNESS);
    for (size_t i = 0; i < sizeof(columns); i++)
    {
        if (i == 0)
        {
            FastLED.addLeds<LED_TYPE, LED_PIN1, COLOR_ORDER>(lights[0], NUM_LEDS);
        }
        else if (i == 1)
        {
            FastLED.addLeds<LED_TYPE, LED_PIN2, COLOR_ORDER>(lights[1], NUM_LEDS);
        }
    }
}

void setSolidColor(int stripIndex, CRGB color)
{
    for (int i = 0; i < NUM_LEDS; i++)
    {
        lights[stripIndex][i] = color;
    }
    FastLED.show();
}

void fadeAnimation(int stripIndex, CRGB color)
{
    for (int i = 0; i < NUM_LEDS + 3; i++)
    {
        if (i < NUM_LEDS)
        {
            lights[stripIndex][i] = color;
        }
        if (i > 0 && i - 1 < NUM_LEDS)
        {
            lights[stripIndex][i - 1].nscale8(128);
        }
        if (i > 1 && i - 2 < NUM_LEDS)
        {
            lights[stripIndex][i - 2].nscale8(64);
        }
        if (i > 2 && i - 3 < NUM_LEDS)
        {
            lights[stripIndex][i - 3] = CRGB::Black; 
        }

        FastLED.show();
        delay(100);
    }
}

void blinkLedStrip(int stripIndex, CRGB color)
{
    for (int i = 0; i < 3; i++)
    {
        setSolidColor(stripIndex, color);
        delay(500);
        setSolidColor(stripIndex, CRGB::Black);
        delay(500);
    }
}

void turnOffLEDStrips()
{
    for (int i = 0; i < 2; i++)
    {
        setSolidColor(i, CRGB::Black);
    }
    FastLED.show();
}

CRGB getColorFromStore(int index)
{
    String settings = getSettings();
    JsonDocument settingsDoc;
    deserializeJson(settingsDoc, settings);

    JsonArray settingsArray = settingsDoc.as<JsonArray>();
    if (index < 0 || index >= settingsArray.size())
    {
        return CRGB::Black;
    }

    JsonObject setting = settingsArray[index];

    const char *color = setting["color"];
    RGBStruct rgb = hexToRGB(color);

    CRGB fastLedColor = CRGB(rgb.red, rgb.green, rgb.blue);
    return fastLedColor;
}

#endif // LED_STRIP_H