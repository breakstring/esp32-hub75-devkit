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
#include <AnimatedGIF.h>
#include <SPI.h>
#include <SD.h>
#define SD_CS 5
#define SD_SCK 18
#define SD_MISO 19
#define SD_MOSI 23

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
#define E_PIN -1 // set to 0/2/12 for Hub75E

/********** CONFIG *************/
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 32 // Panel height of 64 will required PIN_E to be defined.
#define PANELS_NUMBER 1 // Number of chained panels, if just a single panel, obviously set to 1

#define PANE_WIDTH PANEL_WIDTH *PANELS_NUMBER
#define PANE_HEIGHT PANEL_HEIGHT

// placeholder for the matrix object
MatrixPanel_I2S_DMA *dma_display = nullptr;

AnimatedGIF gif;
int x_offset, y_offset;
File f;

// Draw a line of image directly on the LED Matrix
void GIFDraw(GIFDRAW *pDraw)
{
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[320];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth > MATRIX_WIDTH)
    iWidth = MATRIX_WIDTH;

  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; // current line

  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2) // restore to background color
  {
    for (x = 0; x < iWidth; x++)
    {
      if (s[x] == pDraw->ucTransparent)
        s[x] = pDraw->ucBackground;
    }
    pDraw->ucHasTransparency = 0;
  }
  // Apply the new pixels to the main image
  if (pDraw->ucHasTransparency) // if transparency used
  {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + pDraw->iWidth;
    x = 0;
    iCount = 0; // count non-transparent pixels
    while (x < pDraw->iWidth)
    {
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent) // done, stop
        {
          s--; // back up to treat it like transparent
        }
        else // opaque
        {
          *d++ = usPalette[c];
          iCount++;
        }
      }           // while looking for opaque pixels
      if (iCount) // any opaque pixels?
      {
        for (int xOffset = 0; xOffset < iCount; xOffset++)
        {
          dma_display->drawPixelRGB565(x + xOffset, y, usTemp[xOffset]);
        }
        x += iCount;
        iCount = 0;
      }
      // no, look for a run of transparent pixels
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd)
      {
        c = *s++;
        if (c == ucTransparent)
          iCount++;
        else
          s--;
      }
      if (iCount)
      {
        x += iCount; // skip these
        iCount = 0;
      }
    }
  }
  else // does not have transparency
  {
    s = pDraw->pPixels;
    // Translate the 8-bit pixels through the RGB565 palette (already byte reversed)
    for (x = 0; x < pDraw->iWidth; x++)
    {
      dma_display->drawPixelRGB565(x, y, usPalette[*s++]);
    }
  }
} /* GIFDraw() */

void *GIFOpenFile(const char *fname, int32_t *pSize)
{
  f = SD.open(fname);
  if (f)
  {
    *pSize = f.size();
    return (void *)&f;
  }
  return NULL;
} /* GIFOpenFile() */

void GIFCloseFile(void *pHandle)
{
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
    f->close();
} /* GIFCloseFile() */

int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  // Note: If you read a file all the way to the last byte, seek() stops working
  if ((pFile->iSize - pFile->iPos) < iLen)
    iBytesRead = pFile->iSize - pFile->iPos - 1; // <-- ugly work-around
  if (iBytesRead <= 0)
    return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
} /* GIFReadFile() */

int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition)
{
  int i = micros();
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  i = micros() - i;
  //  Serial.printf("Seek time = %d us\n", i);
  return pFile->iPos;
} /* GIFSeekFile() */

unsigned long start_tick = 0;

void ShowGIF(char *name)
{
  start_tick = millis();

  if (gif.open(name, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw))
  {
    x_offset = (MATRIX_WIDTH - gif.getCanvasWidth()) / 2;
    if (x_offset < 0)
      x_offset = 0;
    y_offset = (MATRIX_HEIGHT - gif.getCanvasHeight()) / 2;
    if (y_offset < 0)
      y_offset = 0;
    Serial.printf("Successfully opened GIF; Canvas size = %d x %d\n", gif.getCanvasWidth(), gif.getCanvasHeight());
    Serial.flush();
    while (gif.playFrame(true, NULL))
    {
      if ((millis() - start_tick) > 8000)
      { // we'll get bored after about 8 seconds of the same looping gif
        break;
      }
    }
    gif.close();
  }

} /* ShowGIF() */

SPIClass spi = SPIClass(VSPI);

void setup()
{
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
  mxconfig.mx_height = PANEL_HEIGHT;     // we have 64 pix heigh panels
  mxconfig.chain_length = PANELS_NUMBER; // we have 2 panels chained
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
  mxconfig.gpio.e = E_PIN; // we MUST assign pin e to some free pin on a board to drive 64 pix height panels with 1/32 scan
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

  // let's adjust default brightness to about 75%
  dma_display->setBrightness8(192); // range is 0-255, 0 - 0%, 255 - 100%

  // Allocate memory and start DMA display
  if (not dma_display->begin())
    Serial.println("****** !KABOOM! I2S memory allocation failed ***********");

  spi.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  Serial.print("Try to mount SD Card.");
  while (!SD.begin(SD_CS, spi))
  {
    Serial.print(".");
  };

  Serial.println("SD Card mounted with success");
#if ESP32
  uint8_t cardType = SD.cardType();

  if (cardType == CARD_NONE)
  {
    Serial.println("No SD card attached");
    return;
  }

  Serial.print("SD Card Type: ");
  if (cardType == CARD_MMC)
  {
    Serial.println("MMC");
  }
  else if (cardType == CARD_SD)
  {
    Serial.println("SDSC");
  }
  else if (cardType == CARD_SDHC)
  {
    Serial.println("SDHC");
  }
  else
  {
    Serial.println("UNKNOWN");
  }

  uint64_t cardSize = SD.cardSize();
  int cardSizeInMB = cardSize / (1024 * 1024);

  Serial.printf("Card size: %u MB \n", cardSizeInMB);

  uint64_t bytesAvailable = SD.totalBytes();
  int spaceAvailableInMB = bytesAvailable / (1024 * 1024);

  Serial.printf("Space available: %u MB \n", spaceAvailableInMB);

  uint64_t spaceUsed = SD.usedBytes();
  Serial.printf("Space used: %u bytes", spaceUsed);

#endif

  gif.begin(LITTLE_ENDIAN_PIXELS);
}

void loop()
{


  /************** Fill with random color  *************************/
  dma_display->fillScreen(dma_display->color565(random(256), random(256), random(256)));
  delay(2000);
  dma_display->clearScreen();

  /****************** Draw text *******************/
  dma_display->setTextColor(dma_display->color565(random(256), random(256), random(256)));
  dma_display->setCursor(0,0);
  dma_display->setTextSize(1);
  dma_display->setTextWrap(true);
  dma_display->println("Hellow, \nWorld!");
  delay(2000);
  dma_display->clearScreen();

  /******************* Show Gif *****************/
  // Please put the gif files in TF card
  ShowGIF("/2077.gif");
  delay(200);
  ShowGIF("/hundouluo.gif");
  delay(200);
  ShowGIF("/pac.gif");
  delay(200);

}