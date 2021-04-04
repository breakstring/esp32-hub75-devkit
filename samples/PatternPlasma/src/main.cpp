/**
 * @brief Demo code
 * @version 0.1
 * @date 2021-03-29
 * 
 * @copyright Copyright (c) 2021
 * 
 * Portions of this code are adapted from https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/blob/master/examples/2_PatternPlasma/2_PatternPlasma.ino
 */

#include <Arduino.h>
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <FastLED.h>

// Hub75 pins config
#define R1_PIN 25
#define G1_PIN 26
#define B1_PIN 27
#define R2_PIN 14
#define G2_PIN 17
#define B2_PIN 13
#define A_PIN 32
#define B_PIN 33
#define C_PIN 21
#define D_PIN 15
#define LAT_PIN 4
#define OE_PIN 22
#define CLK_PIN 16
#define E_PIN -1 // set to 12 for Hub75E

/********** CONFIG *************/
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32   	// Panel height of 64 will required PIN_E to be defined.
#define PANELS_NUMBER 1 	// Number of chained panels, if just a single panel, obviously set to 1


#define PANE_WIDTH PANEL_WIDTH * PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;
uint16_t time_counter = 0, cycles = 0, fps = 0;
unsigned long fps_timer;

CRGB currentColor;
CRGBPalette16 palettes[] = {HeatColors_p, LavaColors_p, RainbowColors_p, RainbowStripeColors_p, CloudColors_p};
CRGBPalette16 currentPalette = palettes[0];


CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
  return ColorFromPalette(currentPalette, index, brightness, blendType);
}

void setup() {
  Serial.begin(115200);
  
  Serial.println(F("*****************************************************"));
  Serial.println(F("*        ESP32-HUB75-MatrixPanel-I2S-DMA DEMO       *"));
  Serial.println(F("*****************************************************"));

  /*
    The configuration for MatrixPanel_I2S_DMA object is held in HUB75_I2S_CFG structure,
    pls refer to the lib header file for full details.
    All options has it's predefined default values. So we can create a new structure and redefine only the options we need 
    // those are the defaults
    mxconfig.mx_width = 64;                   // physical width of a single matrix panel module (in pixels, usually it is always 64 ;) )
    mxconfig.mx_height = 32;                  // physical height of a single matrix panel module (in pixels, usually amost always it is either 32 or 64)
    mxconfig.chain_length = 1;                // number of chained panels regardless of the topology, default 1 - a single matrix module
    mxconfig.gpio.r1 = R1;                    // pin mappings
    mxconfig.gpio.g1 = G1;
    mxconfig.gpio.b1 = B1;                    // etc
    mxconfig.driver = HUB75_I2S_CFG::SHIFT;   // shift reg driver, default is plain shift register
    mxconfig.double_buff = false;             // use double buffer (twice amount of RAM required)
    mxconfig.i2sspeed = HUB75_I2S_CFG::HZ_10M;// I2S clock speed, better leave as-is unless you want to experiment
  */

  /*
    For example we have two 64x64 panels chained, so we need to customize our setup like this
  */
  HUB75_I2S_CFG mxconfig;
  mxconfig.mx_height = PANEL_HEIGHT;      // we have 64 pix heigh panels
  mxconfig.chain_length = PANELS_NUMBER;  // we have 2 panels chained
  mxconfig.gpio.r1 = R1_PIN;
  mxconfig.gpio.g1 = G1_PIN;
  mxconfig.gpio.b1 = B1_PIN;
  mxconfig.gpio.r2 = R2_PIN;
  mxconfig.gpio.g2 = G2_PIN;
  mxconfig.gpio.b2 = B2_PIN;
  mxconfig.gpio.a = A_PIN;
  mxconfig.gpio.b = B_PIN;
  mxconfig.gpio.c = C_PIN;
  mxconfig.gpio.d = D_PIN;
  mxconfig.gpio.e = E_PIN;                // we MUST assign pin e to some free pin on a board to drive 64 pix height panels with 1/32 scan
  mxconfig.gpio.lat = LAT_PIN;
  mxconfig.gpio.oe = OE_PIN;
  mxconfig.gpio.clk = CLK_PIN;
  
  //mxconfig.driver = HUB75_I2S_CFG::FM6126A;     // in case that we use panels based on FM6126A chip, we can change that

  /*
    //Another way of creating config structure
    //Custom pin mapping for all pins
    HUB75_I2S_CFG::i2s_pins _pins={R1, G1, BL1, R2, G2, BL2, CH_A, CH_B, CH_C, CH_D, CH_E, LAT, OE, CLK};
    HUB75_I2S_CFG mxconfig(
                            64,   // width
                            64,   // height
                             4,   // chain length
                         _pins,   // pin mapping
      HUB75_I2S_CFG::FM6126A      // driver chip
    );
  */


  // OK, now we can create our matrix object
  dma_display = new MatrixPanel_I2S_DMA(mxconfig);

  // let's adjust default brightness to about 50%
  dma_display->setBrightness8(127);    // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if( not dma_display->begin() )
      Serial.println("****** !KABOOM! I2S memory allocation failed ***********");
 
  // well, hope we are OK, let's draw some colors first :)
  Serial.println("Fill screen: RED");
  dma_display->fillScreenRGB888(255, 0, 0);
  delay(1000);

  Serial.println("Fill screen: GREEN");
  dma_display->fillScreenRGB888(0, 255, 0);
  delay(1000);

  Serial.println("Fill screen: BLUE");
  dma_display->fillScreenRGB888(0, 0, 255);
  delay(1000);

  Serial.println("Fill screen: Neutral White");
  dma_display->fillScreenRGB888(64, 64, 64);
  delay(1000);

  Serial.println("Fill screen: black");
  dma_display->fillScreenRGB888(0, 0, 0);
  delay(1000);


  // Set current FastLED palette
  currentPalette = RainbowColors_p;
  Serial.println("Starting plasma effect...");
  fps_timer = millis();
}

void loop() {
    for (int x = 0; x < PANE_WIDTH; x++) {
            for (int y = 0; y <  PANE_HEIGHT; y++) {
                int16_t v = 0;
                uint8_t wibble = sin8(time_counter);
                v += sin16(x * wibble * 3 + time_counter);
                v += cos16(y * (128 - wibble)  + time_counter);
                v += sin16(y * x * cos8(-time_counter) / 8);

                currentColor = ColorFromPalette(currentPalette, (v >> 8) + 127); //, brightness, currentBlendType);
                dma_display->drawPixelRGB888(x, y, currentColor.r, currentColor.g, currentColor.b);
            }
    }

    ++time_counter;
    ++cycles;
    ++fps;

    if (cycles >= 1024) {
        time_counter = 0;
        cycles = 0;
        currentPalette = palettes[random(0,sizeof(palettes)/sizeof(palettes[0]))];
    }

    // print FPS rate every 5 seconds
    // Note: this is NOT a matrix refresh rate, it's the number of data frames being drawn to the DMA buffer per second
    if (fps_timer + 5000 < millis()){
      Serial.printf_P(PSTR("Effect fps: %d\n"), fps/5);
      fps_timer = millis();
      fps = 0;
    }

}