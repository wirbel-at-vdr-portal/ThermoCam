/*******************************************************************************
 * M5CoreDisplay, a ILI934x driver for M5 Core BASIC.
 ******************************************************************************/
#ifndef _M5CoreDisplay_h_
#define _M5CoreDisplay_h_

#include <Arduino.h>
#include <Print.h>       // class Print
//#include "M5CoreSetup.h"
#include "ILI934x.h"     // TFT_WIDTH, TFT_HEIGHT
#include "gfxfont.h"     // GFXfont


/*******************************************************************************
 * class M5CoreDisplay
 ******************************************************************************/

class M5CoreDisplay : public Print {
friend class M5Display;
private:
  int16_t WIDTH, HEIGHT;         // hw display width/height - never changes
  int16_t _width, _height;       // display width and height, as per rotation
  int16_t cursor_x, cursor_y;    // current cursor pos
  uint16_t textcolor, textbgcolor;
  uint8_t textsize_x,textsize_y; // text size multiplier
  uint8_t rotation;              // display rotation (0-3)
  bool wrap;                     // wrap text at right edge of display
  bool _cp437;                   // use codepage CP437
  GFXfont* gfxFont;              // Pointer to special font

  bool needs_init;               // true, if begin was not yet called.
  int32_t addr_row, addr_col;    // cache for drawPixel x,y
  bool inTransaction;            // between startWrite and endWrite
  bool locked;                   // Transaction and mutex lock flags for ESP32

  uint8_t brightness;            // backlight LED brightness

  inline void spi_begin()               __attribute__((always_inline));
  void spi_end();
  void writecommand(uint8_t c);
  void writedata(uint8_t d);

  void init9341(void);
  void init9342(void);
  void rotate934x(uint8_t Rotation);

protected:
  void charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy);


public:
  M5CoreDisplay(int16_t w = TFT_WIDTH, int16_t h = TFT_HEIGHT);

  // needs to be called once.
  void begin(void);

  void drawPixel(int32_t x, int32_t y, uint32_t color);

  // internal: CORE DRAW API
  void startWrite(void)                                                           __attribute__((always_inline));  // begin SPI transaction
  void writePixel(int32_t x, int32_t y, uint32_t color)                           __attribute__((always_inline));
  void writeFillRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color)   __attribute__((always_inline));
  void writeFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color)            __attribute__((always_inline));
  void writeFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color)            __attribute__((always_inline));
  void writeLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color)  __attribute__((always_inline));
  void endWrite(void)                                                             __attribute__((always_inline));  // end SPI transaction

  // CONTROL API
  void setRotation(uint8_t r);
  void invertDisplay(bool i);

  // CONTROLS FOR M5 Core Display
  void sleep(void);
  void wakeup(void);
  void setBrightness(uint8_t value);

  // BASIC DRAW API
  void drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color);
  void drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color);
  void fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);
  void fillScreen(uint32_t color);
  void drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color);
  void drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color);

  void drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
  void drawCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t cornername, uint32_t color);
  void fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color);
  void fillCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t cornername, int32_t delta, uint32_t color);
  void drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
  void fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color);
  void drawRoundRect(int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t radius, uint32_t color);
  void fillRoundRect(int32_t x0, int32_t y0, int32_t w, int32_t h, int32_t radius, uint32_t color);

  void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);
  void drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg);
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color);
  void drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg);

  void drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color);

  void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h);
  void drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap       , int16_t w, int16_t h);
  void drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], const uint8_t mask[], int16_t w, int16_t h);
  void drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap       , uint8_t *mask       , int16_t w, int16_t h);

  void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h);
  void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap       , int16_t w, int16_t h);
  void drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], const uint8_t mask[], int16_t w, int16_t h);
  void drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap       , uint8_t *mask       , int16_t w, int16_t h);

  void drawChar(int32_t x, int32_t y, unsigned char c, uint32_t color, uint32_t bg, uint8_t size);
  void drawChar(int32_t x, int32_t y, unsigned char c, uint32_t color, uint32_t bg, uint8_t size_x, uint8_t size_y);

  void getTextBounds(const char* string,           int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
  void getTextBounds(const __FlashStringHelper* s, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);
  void getTextBounds(const String& str,            int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h);

  void setTextSize(uint8_t size);
  void setTextSize(uint8_t sx, uint8_t sy);

  void setFont(const GFXfont* f = NULL);

  void setCursor(int16_t x, int16_t y);

  void setTextColor(uint16_t c);
  void setTextColor(uint16_t c, uint16_t bg);

  void setTextWrap(bool On);

  void cp437(bool On = true);

  size_t write(uint8_t);

  int16_t width(void);
  int16_t height(void);

  uint8_t getRotation(void) const;

  int16_t getCursorX(void) const;
  int16_t getCursorY(void) const;

  //----------------------------------------------------------------------------
  void drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color);
  void fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color);

  void setAddrWindow(int32_t xs, int32_t ys, int32_t w, int32_t h);
  void setWindow(int32_t xs, int32_t ys, int32_t xe, int32_t ye);

  /* write pixels after setAddrWindow() call */
  void pushColor(uint16_t color);                                  // send a single pixel
  void pushColor(uint16_t color, uint32_t len);                    // send a a block of equal pixels
  void pushColors(uint8_t* data, uint32_t len);                    // send an array of 16 bit pixels
  void pushColors(uint16_t* data, uint32_t len, bool swap = true); // send an array of 16 bit pixels

};

#endif /* #ifndef _M5CoreDisplay_h_ */
