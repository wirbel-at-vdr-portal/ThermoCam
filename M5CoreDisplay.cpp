/*******************************************************************************
 * M5CoreDisplay, a ILI934x driver for M5 Core BASIC.
 ******************************************************************************/

#include <pgmspace.h>
#include <SPI.h>
#include "soc/spi_reg.h"
#include "M5CoreSetup.h"
#include "M5CoreDisplay.h"
#include "glcdfont.c"
#pragma GCC optimize ("O2")

/*******************************************************************************
 * global vars
 *******************************************************************************/
#ifdef USE_HSPI_PORT
   SPIClass spi = SPIClass(HSPI);
#else
   SPIClass &spi = SPI;  // default VSPI port
#endif

/*******************************************************************************
 * forward decls
 *******************************************************************************/
void writeBlock(uint16_t color, uint32_t repeat);
inline GFXglyph* pgm_read_glyph_ptr(const GFXfont* gfxFont, uint8_t c);
inline uint8_t* pgm_read_bitmap_ptr(const GFXfont* gfxFont);
template <typename T> static inline void swap_coord(T &a, T &b) {
  T t = a;  a = b; b = t;
}





/*******************************************************************************
 * class M5CoreDisplay
 *******************************************************************************/


/*******************************************************************************
 * constructor.
 *******************************************************************************/
M5CoreDisplay::M5CoreDisplay(int16_t w, int16_t h) {
  digitalWrite(TFT_CS, HIGH);  // Chip select high (inactive)
  pinMode(TFT_CS, OUTPUT);

  digitalWrite(TFT_DC, HIGH);  // Data/Command high = data mode
  pinMode(TFT_DC, OUTPUT);

  digitalWrite(TFT_RST, HIGH);  // Set high, do not share pin with another SPI device
  pinMode(TFT_RST, OUTPUT);

  needs_init = true;
  addr_row = addr_col = 0xFFFF;
  inTransaction = false;
  locked        = true;  // ESP32 transaction mutex lock flags

  WIDTH  = _width  = w;  // Set by specific xxxxx_Defines.h file or by users sketch
  HEIGHT = _height = h;  // Set by specific xxxxx_Defines.h file or by users sketch
  rotation = 1;
  cursor_y = cursor_x = 0;
  textsize_x = textsize_y = 1;
  textcolor = 0xFFFF, textbgcolor = 0;
  wrap = true;
  _cp437  = false;

  brightness = 80;
}

/*******************************************************************************
 * begin(), reset and init TFT registers
 *******************************************************************************/
void M5CoreDisplay::begin(void) {
  if (needs_init) {
     spi.begin(TFT_SCLK, TFT_MISO, TFT_MOSI, -1);

     inTransaction = false;
     locked        = true;

     // Set to output once again in case D6 (MISO) is used for CS
     digitalWrite(TFT_CS, HIGH);  // Chip select high (inactive)
     pinMode(TFT_CS, OUTPUT);

     // Set to output once again in case D6 (MISO) is used for DC
     digitalWrite(TFT_DC, HIGH);  // Data/Command high = data mode
     pinMode(TFT_DC, OUTPUT);
     needs_init = false;
     spi_end();
     }

  // Toggle RST low to reset
  spi_begin();

  pinMode(TFT_RST, INPUT_PULLDOWN);
  delay(1);
  bool lcd_version = digitalRead(TFT_RST);

  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(5);
  digitalWrite(TFT_RST, LOW);
  delay(20);
  digitalWrite(TFT_RST, HIGH);

  spi_end();
  delay(150);  // Wait for reset to complete

  spi_begin();

  // IC specific init code
  if (lcd_version)
     init9342();
  else
     init9341();

  spi_end();

  // Init the back-light LED PWM
  ledcSetup(BLK_PWM_CHANNEL, 44100, 8);
  ledcAttachPin(TFT_BL, BLK_PWM_CHANNEL);
  ledcWrite(BLK_PWM_CHANNEL, brightness);

  setRotation(rotation);
  fillScreen(BLACK);
}

/*******************************************************************************
 * drawPixel(), push a single pixel at an arbitrary position
 *******************************************************************************/
void M5CoreDisplay::drawPixel(int32_t x, int32_t y, uint32_t color) {
  // range check
  if ((x < 0) || (y < 0) || (x >= _width) || (y >= _height))
     return;

  spi_begin();
  DC_C;

  // No need to send x if it has not changed (speeds things up)
  if (addr_col != x) {
     tft_Write_8(TFT_CASET);
     DC_D;
     tft_Write_32(SPI_32(x, x));
     DC_C;
     addr_col = x;
     }

  // No need to send y if it has not changed (speeds things up)
  if (addr_row != y) {
     tft_Write_8(TFT_PASET);
     DC_D;
     tft_Write_32(SPI_32(y, y));
     DC_C;
     addr_row = y;
     }

  tft_Write_8(TFT_RAMWR);
  DC_D;
  tft_Write_16(color);
  spi_end();
}

/*******************************************************************************
 * startWrite(), begin transaction with CS low, must be followed by endWrite
 *******************************************************************************/
void M5CoreDisplay::startWrite(void) {
  spi_begin();
  inTransaction = true;
}

/*******************************************************************************
 * startWrite(), begin transaction with CS low, must be followed by endWrite
 *******************************************************************************/
void M5CoreDisplay::writePixel(int32_t x, int32_t y, uint32_t color) {
  DC_C;

  // No need to send x if it has not changed (speeds things up)
  if (addr_col != x) {
     tft_Write_8(TFT_CASET);
     DC_D;
     tft_Write_32(SPI_32(x, x));
     DC_C;
     addr_col = x;
     }

  // No need to send y if it has not changed (speeds things up)
  if (addr_row != y) {
     tft_Write_8(TFT_PASET);
     DC_D;
     tft_Write_32(SPI_32(y, y));
     DC_C;
     addr_row = y;
     }

  tft_Write_8(TFT_RAMWR);
  DC_D;
  tft_Write_16(color);
}

/*******************************************************************************
 * writeFillRect(), duplicate of fillRect()
 *******************************************************************************/
void M5CoreDisplay::writeFillRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t color) {
  fillRect(x, y, w, h, color);
}

/*******************************************************************************
 * writeFastVLine(), as drawFastVLine, but no startWrite and endWrite.
 * writeFastHLine(), as drawFastHLine, but no startWrite and endWrite.
 *******************************************************************************/
void M5CoreDisplay::writeFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
  setWindow(x, y, x, y + h - 1);
  writeBlock(color, h);
}

void M5CoreDisplay::writeFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
  setWindow(x, y, x + w - 1, y);
  writeBlock(color, w);
}

void M5CoreDisplay::writeLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
  boolean steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
     swap_coord(x0, y0);
     swap_coord(x1, y1);
     }

  if (x0 > x1) {
     swap_coord(x0, x1);
     swap_coord(y0, y1);
     }

  int32_t dx = x1 - x0, dy = abs(y1 - y0);
  int32_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

  if (y0 < y1) ystep = 1;

  // Split into steep and not steep for FastH/V separation
  if (steep) {
     for(; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
           err += dx;
           if (dlen == 1)
              drawPixel(y0, xs, color);
           else
              drawFastVLine(y0, xs, dlen, color);
           dlen = 0;
           y0 += ystep;
           xs = x0 + 1;
           }
        }
     if (dlen) drawFastVLine(y0, xs, dlen, color);
     }
  else {
     for(; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
           err += dx;
           if (dlen == 1)
              drawPixel(xs, y0, color);
           else
              drawFastHLine(xs, y0, dlen, color);
           dlen = 0;
           y0 += ystep;
           xs = x0 + 1;
           }
        }
     if (dlen) drawFastHLine(xs, y0, dlen, color);
     }
}

/*******************************************************************************
 * startWrite(), end transaction with CS high
 *******************************************************************************/
void M5CoreDisplay::endWrite(void) {
  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * setRotation, rotate the screen orientation m = 0-3 or 4-7 for BMP drawing
 ******************************************************************************/
void M5CoreDisplay::setRotation(uint8_t m) {
  spi_begin();
  rotate934x(m); // IC specific rotation code
  delayMicroseconds(10);
  spi_end();
  addr_row = addr_col = 0xFFFF;
}

/*******************************************************************************
 * invertDisplay, invert the display colours
 ******************************************************************************/
void M5CoreDisplay::invertDisplay(boolean i) {
  spi_begin();
  // Send the command twice as otherwise it does not always work!
  writecommand(i ? TFT_INVON : TFT_INVOFF);
  writecommand(i ? TFT_INVON : TFT_INVOFF);
  spi_end();
}

/*******************************************************************************
 * sleep, shutdown the display
 ******************************************************************************/
void M5CoreDisplay::sleep(void) {
  spi_begin();
  writecommand(ILI9341_SLPIN);  // Software reset
  spi_end();
  setBrightness(0);
}

/*******************************************************************************
 * wakeup, wakeup the display
 ******************************************************************************/
void M5CoreDisplay::wakeup(void) {
  setBrightness(brightness);
  spi_begin();
  writecommand(ILI9341_SLPOUT);
  spi_end();
}

/*******************************************************************************
 * setBrightness, set the backlight LEDS brightness
 ******************************************************************************/
void M5CoreDisplay::setBrightness(uint8_t value) {
  brightness = value;
  ledcWrite(BLK_PWM_CHANNEL, brightness);
}

/*******************************************************************************
 * drawFastVLine, draw a vertical line
 ******************************************************************************/
void M5CoreDisplay::drawFastVLine(int32_t x, int32_t y, int32_t h, uint32_t color) {
  // Clipping
  if ((x < 0) || (x >= _width) || (y >= _height))
     return;

  if (y < 0) {
     h += y;
     y = 0;
     }

  if ((y + h) > _height)
     h = _height - y;

  if (h < 1)
     return;

  spi_begin();
  setWindow(x, y, x, y + h - 1);
  writeBlock(color, h);
  spi_end();
}

/*******************************************************************************
 * drawFastHLine, draw a horizontal line
 ******************************************************************************/
void M5CoreDisplay::drawFastHLine(int32_t x, int32_t y, int32_t w, uint32_t color) {
  // Clipping
  if ((y < 0) || (x >= _width) || (y >= _height))
     return;

  if (x < 0) {
     w += x;
     x = 0;
     }

  if ((x + w) > _width)
     w = _width - x;

  if (w < 1)
     return;

  spi_begin();
  setWindow(x, y, x + w - 1, y);
  writeBlock(color, w);
  spi_end();
}

/*******************************************************************************
 * fillRect, draw a filled rectangle
 ******************************************************************************/
void M5CoreDisplay::fillRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  // Clipping
  if ((x >= _width) || (y >= _height))
     return;

  if (x < 0) {
     w += x;
     x = 0;
     }

  if (y < 0) {
     h += y;
     y = 0;
     }

  if ((x + w) > _width)
     w = _width - x;

  if ((y + h) > _height)
     h = _height - y;

  if ((w < 1) || (h < 1))
     return;

  spi_begin();
  setWindow(x, y, x + w - 1, y + h - 1);
  writeBlock(color, w * h);
  spi_end();
}

/*******************************************************************************
 * fillScreen, fill the screen with a colour
 ******************************************************************************/
void M5CoreDisplay::fillScreen(uint32_t color) {
  spi_begin();
  setWindow(0, 0, _width - 1, _height - 1);
  writeBlock(color, _width * _height);
  spi_end();
}

/*******************************************************************************
 * drawLine, draw a line between two arbitrary points
 ******************************************************************************/
void M5CoreDisplay::drawLine(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t color) {
  inTransaction = true;
  boolean steep = abs(y1 - y0) > abs(x1 - x0);
  if (steep) {
     swap_coord(x0, y0);
     swap_coord(x1, y1);
     }

  if (x0 > x1) {
     swap_coord(x0, x1);
     swap_coord(y0, y1);
     }

  int32_t dx = x1 - x0, dy = abs(y1 - y0);
  int32_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

  if (y0 < y1)
     ystep = 1;

  // Split into steep and not steep for FastH/V separation
  if (steep) {
     for(; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
           err += dx;
           if (dlen == 1)
              drawPixel(y0, xs, color);
           else
              drawFastVLine(y0, xs, dlen, color);
           dlen = 0;
           y0 += ystep;
           xs = x0 + 1;
           }
        }
      if (dlen)
         drawFastVLine(y0, xs, dlen, color);
      }
  else {
     for(; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
           err += dx;
           if (dlen == 1)
              drawPixel(xs, y0, color);
           else
              drawFastHLine(xs, y0, dlen, color);
           dlen = 0;
           y0 += ystep;
           xs = x0 + 1;
           }
        }
     if (dlen)
        drawFastHLine(xs, y0, dlen, color);
     }
  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * drawRect, draw a rectangle outline
 ******************************************************************************/
void M5CoreDisplay::drawRect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t color) {
  spi_begin();
  inTransaction = true;

  // Clipping
  if ((x >= _width) || (y >= _height))
     return;

  if (x < 0) {
     w += x;
     x = 0;
     }

  if (y < 0) {
     h += y;
     y = 0;
     }

  if ((x + w) > _width)
     w = _width - x;

  if ((y + h) > _height)
     h = _height - y;

  if ((w < 1) || (h < 1))
     return;

  writeFastHLine(x, y, w, color);
  writeFastHLine(x, y + h - 1, w, color);
  // Avoid drawing corner pixels twice
  writeFastVLine(x, y + 1, h - 2, color);
  writeFastVLine(x + w - 1, y + 1, h - 2, color);
  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * drawCircle, draw a circle outline
 ******************************************************************************/
void M5CoreDisplay::drawCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
  int32_t x  = 0;
  int32_t dx = 1;
  int32_t dy = r + r;
  int32_t p  = -(r >> 1);

  inTransaction = true;

  // These are ordered to minimise coordinate changes in x or y
  // drawPixel can then send fewer bounding box commands
  drawPixel(x0 + r, y0, color);
  drawPixel(x0 - r, y0, color);
  drawPixel(x0, y0 - r, color);
  drawPixel(x0, y0 + r, color);

  while(x < r) {
     if (p >= 0) {
        dy -= 2;
        p -= dy;
        r--;
        }

     dx += 2;
     p += dx;
     x++;

     // These are ordered to minimise coordinate changes in x or y
     // drawPixel can then send fewer bounding box commands
     drawPixel(x0 + x, y0 + r, color);
     drawPixel(x0 - x, y0 + r, color);
     drawPixel(x0 - x, y0 - r, color);
     drawPixel(x0 + x, y0 - r, color);

     drawPixel(x0 + r, y0 + x, color);
     drawPixel(x0 - r, y0 + x, color);
     drawPixel(x0 - r, y0 - x, color);
     drawPixel(x0 + r, y0 - x, color);
     }

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * drawCircleHelper, support function for rounded rectangles
 ******************************************************************************/
void M5CoreDisplay::drawCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t cornername, uint32_t color) {
  int32_t f     = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -2 * r;
  int32_t x     = 0;

  while(x < r) {
     if (f >= 0) {
        r--;
        ddF_y += 2;
        f += ddF_y;
        }
     x++;
     ddF_x += 2;
     f += ddF_x;
     if (cornername & 0x4) {
        drawPixel(x0 + x, y0 + r, color);
        drawPixel(x0 + r, y0 + x, color);
        }
     if (cornername & 0x2) {
        drawPixel(x0 + x, y0 - r, color);
        drawPixel(x0 + r, y0 - x, color);
        }
     if (cornername & 0x8) {
        drawPixel(x0 - r, y0 + x, color);
        drawPixel(x0 - x, y0 + r, color);
        }
     if (cornername & 0x1) {
        drawPixel(x0 - r, y0 - x, color);
        drawPixel(x0 - x, y0 - r, color);
        }
     }
}

/*******************************************************************************
 * fillCircle, draw a filled circle
 ******************************************************************************/
void M5CoreDisplay::fillCircle(int32_t x0, int32_t y0, int32_t r, uint32_t color) {
  int32_t x  = 0;
  int32_t dx = 1;
  int32_t dy = r + r;
  int32_t p  = -(r >> 1);

  inTransaction = true;
  drawFastHLine(x0 - r, y0, dy + 1, color);

  while (x < r) {
     if (p >= 0) {
        dy -= 2;
        p -= dy;
        r--;
        }

     dx += 2;
     p += dx;
     x++;

     drawFastHLine(x0 - r, y0 + x, 2 * r + 1, color);
     drawFastHLine(x0 - r, y0 - x, 2 * r + 1, color);
     drawFastHLine(x0 - x, y0 + r, 2 * x + 1, color);
     drawFastHLine(x0 - x, y0 - r, 2 * x + 1, color);
     }

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * fillCircleHelper, support function for filled rounded rectangles
 ******************************************************************************/
void M5CoreDisplay::fillCircleHelper(int32_t x0, int32_t y0, int32_t r, uint8_t cornername, int32_t delta, uint32_t color) {
  int32_t f     = 1 - r;
  int32_t ddF_x = 1;
  int32_t ddF_y = -r - r;
  int32_t y     = 0;

  delta++;
  while(y < r) {
     if (f >= 0) {
        r--;
        ddF_y += 2;
        f += ddF_y;
        }
     y++;
     // x++;
     ddF_x += 2;
     f += ddF_x;

     if (cornername & 0x1) {
        drawFastHLine(x0 - r, y0 + y, r + r + delta, color);
        drawFastHLine(x0 - y, y0 + r, y + y + delta, color);
        }
     if (cornername & 0x2) {
        drawFastHLine(x0 - r, y0 - y, r + r + delta, color);  // 11995, 1090
        drawFastHLine(x0 - y, y0 - r, y + y + delta, color);
        }
     }
}

/*******************************************************************************
 * drawTriangle, draw a triangle outline
 ******************************************************************************/
void M5CoreDisplay::drawTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
  startWrite();
  writeLine(x0, y0, x1, y1, color);
  writeLine(x1, y1, x2, y2, color);
  writeLine(x2, y2, x0, y0, color);
  endWrite();  
}

/*******************************************************************************
 * fillTriangle, draw a filled triangle outline
 ******************************************************************************/
void M5CoreDisplay::fillTriangle(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2, uint32_t color) {
  int32_t a, b, y, last;

  // Sort coordinates by Y order (y2 >= y1 >= y0)
  if (y0 > y1) {
     swap_coord(y0, y1);
     swap_coord(x0, x1);
     }
  if (y1 > y2) {
     swap_coord(y2, y1);
     swap_coord(x2, x1);
     }
  if (y0 > y1) {
     swap_coord(y0, y1);
     swap_coord(x0, x1);
     }

  startWrite();

  if (y0 == y2) {  // Handle awkward all-on-same-line case as its own thing
     a = b = x0;
     if (x1 < a)
        a = x1;
     else if (x1 > b)
        b = x1;
     if (x2 < a)
        a = x2;
     else if (x2 > b)
        b = x2;
     writeFastHLine(a, y0, b - a + 1, color);
     return;
     }

  int32_t dx01 = x1 - x0, dy01 = y1 - y0, dx02 = x2 - x0, dy02 = y2 - y0,
          dx12 = x2 - x1, dy12 = y2 - y1, sa = 0, sb = 0;

  /* For upper part of triangle, find scanline crossings for segments
   * 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
   * is included here (and second loop will be skipped, avoiding a /0
   * error there), otherwise scanline y1 is skipped here and handled
   * in the second loop...which also avoids a /0 error here if y0=y1
   * (flat-topped triangle). */
  if (y1 == y2)
     last = y1;  // Include y1 scanline
  else
     last = y1 - 1;  // Skip it

  for(y = y0; y <= last; y++) {
     a = x0 + sa / dy01;
     b = x0 + sb / dy02;
     sa += dx01;
     sb += dx02;

     if (a > b) swap_coord(a, b);
     writeFastHLine(a, y, b - a + 1, color);
     }

  /* For lower part of triangle, find scanline crossings for segments
   * 0-2 and 1-2.  This loop is skipped if y1=y2. */
  sa = dx12 * (y - y1);
  sb = dx02 * (y - y0);
  for(; y <= y2; y++) {
     a = x1 + sa / dy12;
     b = x0 + sb / dy02;
     sa += dx12;
     sb += dx02;

     if (a > b)
        swap_coord(a, b);
     writeFastHLine(a, y, b - a + 1, color);
     }

  endWrite();
}

/*******************************************************************************
 * drawRoundRect, draw a rounded corner rectangle outline
 ******************************************************************************/
void M5CoreDisplay::drawRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
  inTransaction = true;

  // smarter version
  drawFastHLine(x + r, y, w - r - r, color);          // Top
  drawFastHLine(x + r, y + h - 1, w - r - r, color);  // Bottom
  drawFastVLine(x, y + r, h - r - r, color);          // Left
  drawFastVLine(x + w - 1, y + r, h - r - r, color);  // Right
  // draw four corners
  drawCircleHelper(x + r, y + r, r, 1, color);
  drawCircleHelper(x + w - r - 1, y + r, r, 2, color);
  drawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
  drawCircleHelper(x + r, y + h - r - 1, r, 8, color);

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * fillRoundRect, draw a filled rounded corner rectangle
 ******************************************************************************/
void M5CoreDisplay::fillRoundRect(int32_t x, int32_t y, int32_t w, int32_t h, int32_t r, uint32_t color) {
  inTransaction = true;

  // smarter version
  fillRect(x, y + r, w, h - r - r, color);

  // draw four corners
  fillCircleHelper(x + r, y + h - r - 1, r, 1, w - r - r - 1, color);
  fillCircleHelper(x + r, y + r, r, 2, w - r - r - 1, color);

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * drawBitmap, draw an image stored in an array on the TFT
 ******************************************************************************/
void M5CoreDisplay::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t b = 0;

  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        b <<= 1;
      else
        b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      if (b & 0x80)
        writePixel(x + i, y, color);
    }
  }
  endWrite();
}

void M5CoreDisplay::drawBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t b = 0;

  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        b <<= 1;
      else
        b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      writePixel(x + i, y, (b & 0x80) ? color : bg);
    }
  }
  endWrite();
}

void M5CoreDisplay::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t b = 0;

  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        b <<= 1;
      else
        b = bitmap[j * byteWidth + i / 8];
      if (b & 0x80)
        writePixel(x + i, y, color);
    }
  }
  endWrite();
}

void M5CoreDisplay::drawBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h, uint16_t color, uint16_t bg) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t b = 0;

  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        b <<= 1;
      else
        b = bitmap[j * byteWidth + i / 8];
      writePixel(x + i, y, (b & 0x80) ? color : bg);
    }
  }
  endWrite();
}

/*******************************************************************************
 * drawXBitmap, draw an image stored in an XBM array on the TFT
 ******************************************************************************/
void M5CoreDisplay::drawXBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h, uint16_t color) {
  int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
  uint8_t b = 0;

  startWrite();
  for (int16_t j = 0; j < h; j++, y++) {
    for (int16_t i = 0; i < w; i++) {
      if (i & 7)
        b >>= 1;
      else
        b = pgm_read_byte(&bitmap[j * byteWidth + i / 8]);
      // Nearly identical to drawBitmap(), only the bit order
      // is reversed here (left-to-right = LSB to MSB):
      if (b & 0x01)
        writePixel(x + i, y, color);
    }
  }
  endWrite();
}

/*******************************************************************************
 * drawGrayscaleBitmap, draw a RAM-resident 8-bit image (grayscale) with a 1-bit
 * mask
 ******************************************************************************/
void M5CoreDisplay::drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], int16_t w, int16_t h) {
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        writePixel(x + i, y, (uint8_t)pgm_read_byte(&bitmap[j * w + i]));
        }
     }
  endWrite();
}

void M5CoreDisplay::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap, int16_t w, int16_t h) {
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        writePixel(x + i, y, bitmap[j * w + i]);
        }
     }
  endWrite();
}

void M5CoreDisplay::drawGrayscaleBitmap(int16_t x, int16_t y, const uint8_t bitmap[], const uint8_t mask[], int16_t w, int16_t h) {
  int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
  uint8_t b = 0;
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        if (i & 7)
           b <<= 1;
        else
           b = pgm_read_byte(&mask[j * bw + i / 8]);
        if (b & 0x80) {
           writePixel(x + i, y, (uint8_t)pgm_read_byte(&bitmap[j * w + i]));
           }
        }
     }
  endWrite();
}

void M5CoreDisplay::drawGrayscaleBitmap(int16_t x, int16_t y, uint8_t *bitmap, uint8_t *mask, int16_t w, int16_t h) {
  int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
  uint8_t b = 0;
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        if (i & 7)
           b <<= 1;
        else
           b = mask[j * bw + i / 8];
        if (b & 0x80) {
           writePixel(x + i, y, bitmap[j * w + i]);
           }
        }
     }
  endWrite();
}

/*******************************************************************************
 * drawGrayscaleBitmap, draw a PROGMEM-resident 16-bit image (RGB 5/6/5)
 ******************************************************************************/
void M5CoreDisplay::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], int16_t w, int16_t h) {
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        writePixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
        }
     }
  endWrite();
}

void M5CoreDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, int16_t w, int16_t h) {
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        writePixel(x + i, y, bitmap[j * w + i]);
        }
     }
  endWrite();
}

void M5CoreDisplay::drawRGBBitmap(int16_t x, int16_t y, const uint16_t bitmap[], const uint8_t mask[], int16_t w, int16_t h) {
  int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
  uint8_t b = 0;
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        if (i & 7)
           b <<= 1;
        else
           b = pgm_read_byte(&mask[j * bw + i / 8]);
        if (b & 0x80) {
           writePixel(x + i, y, pgm_read_word(&bitmap[j * w + i]));
           }
        }
     }
  endWrite();
}

void M5CoreDisplay::drawRGBBitmap(int16_t x, int16_t y, uint16_t *bitmap, uint8_t *mask, int16_t w, int16_t h) {
  int16_t bw = (w + 7) / 8; // Bitmask scanline pad = whole byte
  uint8_t b = 0;
  startWrite();
  for(int16_t j = 0; j < h; j++, y++) {
     for(int16_t i = 0; i < w; i++) {
        if (i & 7)
           b <<= 1;
        else
           b = mask[j * bw + i / 8];
        if (b & 0x80) {
           writePixel(x + i, y, bitmap[j * w + i]);
           }
        }
     }
  endWrite();
}

/*******************************************************************************
 * drawChar, draw a single char
 ******************************************************************************/
void M5CoreDisplay::drawChar(int32_t x, int32_t y, unsigned char c, uint32_t color, uint32_t bg, uint8_t size) {
  drawChar(x, y, c, color, bg, size, size);
}

void M5CoreDisplay::drawChar(int32_t x, int32_t y, unsigned char c, uint32_t color, uint32_t bg, uint8_t size_x, uint8_t size_y) {
  if (!gfxFont) { // 'Classic' built-in font

    if ((x >= _width) ||              // Clip right
        (y >= _height) ||             // Clip bottom
        ((x + 6 * size_x - 1) < 0) || // Clip left
        ((y + 8 * size_y - 1) < 0))   // Clip top
      return;

    if (!_cp437 && (c >= 176))
      c++; // Handle 'classic' charset behavior

    startWrite();
    for (int8_t i = 0; i < 5; i++) { // Char bitmap = 5 columns
      uint8_t line = pgm_read_byte(&font[c * 5 + i]);
      for (int8_t j = 0; j < 8; j++, line >>= 1) {
        if (line & 1) {
          if (size_x == 1 && size_y == 1)
            writePixel(x + i, y + j, color);
          else
            writeFillRect(x + i * size_x, y + j * size_y, size_x, size_y,
                          color);
        } else if (bg != color) {
          if (size_x == 1 && size_y == 1)
            writePixel(x + i, y + j, bg);
          else
            writeFillRect(x + i * size_x, y + j * size_y, size_x, size_y, bg);
        }
      }
    }
    if (bg != color) { // If opaque, draw vertical line for last column
      if (size_x == 1 && size_y == 1)
        writeFastVLine(x + 5, y, 8, bg);
      else
        writeFillRect(x + 5 * size_x, y, size_x, 8 * size_y, bg);
    }
    endWrite();

  } else { // Custom font

    // Character is assumed previously filtered by write() to eliminate
    // newlines, returns, non-printable characters, etc.  Calling
    // drawChar() directly with 'bad' characters of font may cause mayhem!

    c -= (uint8_t)pgm_read_byte(&gfxFont->first);
    GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c);
    uint8_t *bitmap = pgm_read_bitmap_ptr(gfxFont);

    uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
    uint8_t w = pgm_read_byte(&glyph->width), h = pgm_read_byte(&glyph->height);
    int8_t xo = pgm_read_byte(&glyph->xOffset),
           yo = pgm_read_byte(&glyph->yOffset);
    uint8_t xx, yy, bits = 0, bit = 0;
    int16_t xo16 = 0, yo16 = 0;

    if (size_x > 1 || size_y > 1) {
      xo16 = xo;
      yo16 = yo;
    }

    // Todo: Add character clipping here

    // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
    // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
    // has typically been used with the 'classic' font to overwrite old
    // screen contents with new data.  This ONLY works because the
    // characters are a uniform size; it's not a sensible thing to do with
    // proportionally-spaced fonts with glyphs of varying sizes (and that
    // may overlap).  To replace previously-drawn text when using a custom
    // font, use the getTextBounds() function to determine the smallest
    // rectangle encompassing a string, erase the area with fillRect(),
    // then draw new text.  This WILL infortunately 'blink' the text, but
    // is unavoidable.  Drawing 'background' pixels will NOT fix this,
    // only creates a new set of problems.  Have an idea to work around
    // this (a canvas object type for MCUs that can afford the RAM and
    // displays supporting setAddrWindow() and pushColors()), but haven't
    // implemented this yet.

    startWrite();
    for (yy = 0; yy < h; yy++) {
      for (xx = 0; xx < w; xx++) {
        if (!(bit++ & 7)) {
          bits = pgm_read_byte(&bitmap[bo++]);
        }
        if (bits & 0x80) {
          if (size_x == 1 && size_y == 1) {
            writePixel(x + xo + xx, y + yo + yy, color);
          } else {
            writeFillRect(x + (xo16 + xx) * size_x, y + (yo16 + yy) * size_y,
                          size_x, size_y, color);
          }
        }
        bits <<= 1;
      }
    }
    endWrite();

  } // End classic vs custom font
}

/*******************************************************************************
 * getTextBounds, determine size of a string with current font/size
 ******************************************************************************/
void M5CoreDisplay::getTextBounds(const char* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  uint8_t c; // Current character
  int16_t minx = 0x7FFF, miny = 0x7FFF, maxx = -1, maxy = -1; // Bound rect
  // Bound rect is intentionally initialized inverted, so 1st char sets it

  *x1 = x; // Initial position is value passed in
  *y1 = y;
  *w = *h = 0; // Initial size is zero

  while((c = *str++)) {
     // charBounds() modifies x/y to advance for each character,
     // and min/max x/y are updated to incrementally build bounding rect.
     charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);
     }

  if (maxx >= minx) {      // If legit string bounds were found...
     *x1 = minx;           // Update x1 to least X coord,
     *w = maxx - minx + 1; // And w to bound rect width
     }
  if (maxy >= miny) {      // Same for height
     *y1 = miny;
     *h = maxy - miny + 1;
     }
}

void M5CoreDisplay::getTextBounds(const String& str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  if (str.length() != 0)
     getTextBounds(const_cast<char *>(str.c_str()), x, y, x1, y1, w, h);
}

void M5CoreDisplay::getTextBounds(const __FlashStringHelper* str, int16_t x, int16_t y, int16_t* x1, int16_t* y1, uint16_t* w, uint16_t* h) {
  uint8_t* s = (uint8_t*) str, c;

  *x1 = x;
  *y1 = y;
  *w = *h = 0;

  int16_t minx = _width, miny = _height, maxx = -1, maxy = -1;

  while((c = pgm_read_byte(s++)))
     charBounds(c, &x, &y, &minx, &miny, &maxx, &maxy);

  if (maxx >= minx) {
     *x1 = minx;
     *w = maxx - minx + 1;
     }
  if (maxy >= miny) {
     *y1 = miny;
     *h = maxy - miny + 1;
     }
}

/*******************************************************************************
 * charBounds, helper for getTextBounds
 ******************************************************************************/
void M5CoreDisplay::charBounds(unsigned char c, int16_t* x, int16_t* y, int16_t* minx, int16_t* miny, int16_t* maxx, int16_t* maxy) {
  if (gfxFont) {
     if (c == '\n') { // Newline?
        *x = 0;        // Reset x to zero, advance y by one line
        *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        }
     else if (c != '\r') { // Not a carriage return; is normal char
        uint8_t first = pgm_read_byte(&gfxFont->first);
        uint8_t last  = pgm_read_byte(&gfxFont->last);
        if ((c >= first) && (c <= last)) { // Char present in this font?
           GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);
           uint8_t gw = pgm_read_byte(&glyph->width);
           uint8_t gh = pgm_read_byte(&glyph->height);
           uint8_t xa = pgm_read_byte(&glyph->xAdvance);
           int8_t xo = pgm_read_byte(&glyph->xOffset);
           int8_t yo = pgm_read_byte(&glyph->yOffset);
           if (wrap && ((*x + (((int16_t)xo + gw) * textsize_x)) > _width)) {
              *x = 0; // Reset x to zero, advance y by one line
              *y += textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
              }
           int16_t tsx = (int16_t)textsize_x;
           int16_t tsy = (int16_t)textsize_y;
           int16_t x1 = *x + xo * tsx;
           int16_t y1 = *y + yo * tsy;
           int16_t x2 = x1 + gw * tsx - 1;
           int16_t y2 = y1 + gh * tsy - 1;
           if (x1 < *minx)
              *minx = x1;
           if (y1 < *miny)
              *miny = y1;
           if (x2 > *maxx)
              *maxx = x2;
           if (y2 > *maxy)
              *maxy = y2;
           *x += xa * tsx;
           }
        }

     }
  else { // Default font
     if (c == '\n') {        // Newline?
        *x = 0;               // Reset x to zero,
        *y += textsize_y * 8; // advance y one line
        // min/max x/y unchanged -- that waits for next 'normal' character
        }
     else if (c != '\r') { // Normal char; ignore carriage returns
        if (wrap && ((*x + textsize_x * 6) > _width)) { // Off right?
           *x = 0;                                       // Reset x to zero,
           *y += textsize_y * 8;                         // advance y one line
           }
        int x2 = *x + textsize_x * 6 - 1; // Lower-right pixel of char
        int y2 = *y + textsize_y * 8 - 1;
        if (x2 > *maxx)
           *maxx = x2; // Track max x, y
        if (y2 > *maxy)
           *maxy = y2;
        if (*x < *minx)
           *minx = *x; // Track min x, y
        if (*y < *miny)
          *miny = *y;
        *x += textsize_x * 6; // Advance x one char
        }
     }
}

/*******************************************************************************
 * write, print one byte/character of data, used to support print()
 ******************************************************************************/
size_t M5CoreDisplay::write(uint8_t c) {
  if (!gfxFont) {
     // 'Classic' built-in font
     if (c == '\n') {              // Newline?
        cursor_x = 0;               // Reset x to zero,
        cursor_y += textsize_y * 8; // advance y one line
        }
     else if (c != '\r') {       // Ignore carriage returns
        if (wrap && ((cursor_x + textsize_x * 6) > _width)) { // Off right?
           cursor_x = 0;                                       // Reset x to zero,
           cursor_y += textsize_y * 8; // advance y one line
           }
        drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
        cursor_x += textsize_x * 6; // Advance x one char
        }
     }
  else {
     // Custom font
     if (c == '\n') {
        cursor_x = 0;
        cursor_y += (int16_t)textsize_y * (uint8_t)pgm_read_byte(&gfxFont->yAdvance);
        }
     else if (c != '\r') {
        uint8_t first = pgm_read_byte(&gfxFont->first);
        if ((c >= first) && (c <= (uint8_t)pgm_read_byte(&gfxFont->last))) {
           GFXglyph *glyph = pgm_read_glyph_ptr(gfxFont, c - first);
           uint8_t w = pgm_read_byte(&glyph->width), h = pgm_read_byte(&glyph->height);
           if ((w > 0) && (h > 0)) { // Is there an associated bitmap?
              int16_t xo = (int8_t)pgm_read_byte(&glyph->xOffset); // sic
              if (wrap && ((cursor_x + textsize_x * (xo + w)) > _width)) {
                 cursor_x = 0;
                 cursor_y += (int16_t) textsize_y * (uint8_t) pgm_read_byte(&gfxFont->yAdvance);
                 }
              drawChar(cursor_x, cursor_y, c, textcolor, textbgcolor, textsize_x, textsize_y);
              }
           cursor_x += (uint8_t) pgm_read_byte(&glyph->xAdvance) * (int16_t) textsize_x;
           }
        }
     }
  return 1;
}

/*******************************************************************************
 * setTextSize, set the text size multiplier
 ******************************************************************************/
void M5CoreDisplay::setTextSize(uint8_t s) {
  setTextSize(s, s);
}

void M5CoreDisplay::setTextSize(uint8_t s_x, uint8_t s_y) {
  textsize_x = (s_x > 0) ? s_x : 1;
  textsize_y = (s_y > 0) ? s_y : 1;
}

/*******************************************************************************
 * setTextWrap, wrap long text around next line
 ******************************************************************************/
void M5CoreDisplay::setTextWrap(bool On) {
  wrap = On;
}

/*******************************************************************************
 * setFont, set display font
 ******************************************************************************/
void M5CoreDisplay::setFont(const GFXfont* f) {
  if (f) {          // Font struct pointer passed in?
     if (!gfxFont) { // And no current font struct?
        // Switching from classic to new font behavior.
        // Move cursor pos down 6 pixels so it's on baseline.
        cursor_y += 6;
        }
     }
  else if (gfxFont) { // NULL passed.  Current font struct defined?
     // Switching from new to classic font behavior.
     // Move cursor pos up 6 pixels so it's at top-left of char.
     cursor_y -= 6;
     }
  gfxFont = (GFXfont*) f;
}

/*******************************************************************************
 * setCursor, set text cursor location
 ******************************************************************************/
void M5CoreDisplay::setCursor(int16_t x, int16_t y) {
  cursor_x = x;
  cursor_y = y;
}

/*******************************************************************************
 * setTextColor, set text color
 ******************************************************************************/
void M5CoreDisplay::setTextColor(uint16_t c) {
  textcolor = textbgcolor = c;
}

void M5CoreDisplay::setTextColor(uint16_t c, uint16_t bg) {
  textcolor   = c;
  textbgcolor = bg;
}

/*******************************************************************************
 * cp437, enable CP 437
 ******************************************************************************/
void M5CoreDisplay::cp437(bool On) {
  _cp437 = On;
}

/*******************************************************************************
 * width, height, returns width/height of display.
 ******************************************************************************/
int16_t M5CoreDisplay::width(void) {
  return _width;
}

int16_t M5CoreDisplay::height(void) {
  return _height;
}

/*******************************************************************************
 * returns current rotation.
 ******************************************************************************/
uint8_t M5CoreDisplay::getRotation(void) const {
  return rotation;
}

/*******************************************************************************
 * returns current cursor position.
 ******************************************************************************/
int16_t M5CoreDisplay::getCursorX(void) const {
  return cursor_x;
}

int16_t M5CoreDisplay::getCursorY(void) const {
  return cursor_y;
}

/*******************************************************************************
 * drawEllipse, draw a ellipse outline
 ******************************************************************************/
void M5CoreDisplay::drawEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
  if ((rx < 2) || (ry < 2))
     return;

  int32_t x, y;
  int32_t rx2 = rx * rx;
  int32_t ry2 = ry * ry;
  int32_t fx2 = 4 * rx2;
  int32_t fy2 = 4 * ry2;
  int32_t s;

  inTransaction = true;

  for(x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++) {
     // These are ordered to minimise coordinate changes in x or y
     // drawPixel can then send fewer bounding box commands
     drawPixel(x0 + x, y0 + y, color);
     drawPixel(x0 - x, y0 + y, color);
     drawPixel(x0 - x, y0 - y, color);
     drawPixel(x0 + x, y0 - y, color);
     if (s >= 0) {
        s += fx2 * (1 - y);
        y--;
        }
     s += ry2 * ((4 * x) + 6);
     }

  for(x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++) {
     // These are ordered to minimise coordinate changes in x or y
     // drawPixel can then send fewer bounding box commands
     drawPixel(x0 + x, y0 + y, color);
     drawPixel(x0 - x, y0 + y, color);
     drawPixel(x0 - x, y0 - y, color);
     drawPixel(x0 + x, y0 - y, color);
     if (s >= 0) {
        s += fy2 * (1 - x);
        x--;
        }
     s += rx2 * ((4 * y) + 6);
     }

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * fillEllipse, draw a filled ellipse
 ******************************************************************************/
void M5CoreDisplay::fillEllipse(int16_t x0, int16_t y0, int32_t rx, int32_t ry, uint16_t color) {
  if ((rx < 2) || (ry < 2))
     return;
  int32_t x, y;
  int32_t rx2 = rx * rx;
  int32_t ry2 = ry * ry;
  int32_t fx2 = 4 * rx2;
  int32_t fy2 = 4 * ry2;
  int32_t s;

  inTransaction = true;
  for(x=0, y=ry, s=2*ry2+rx2*(1-2*ry); ry2*x <= rx2*y; x++) {
     drawFastHLine(x0 - x, y0 - y, x + x + 1, color);
     drawFastHLine(x0 - x, y0 + y, x + x + 1, color);
     if (s >= 0) {
        s += fx2 * (1 - y);
        y--;
        }
     s += ry2 * ((4 * x) + 6);
     }

  for(x=rx, y=0, s=2*rx2+ry2*(1-2*rx); rx2*y <= ry2*x; y++) {
        drawFastHLine(x0 - x, y0 - y, x + x + 1, color);
        drawFastHLine(x0 - x, y0 + y, x + x + 1, color);
        if (s >= 0) {
           s += fy2 * (1 - x);
           x--;
           }
        s += rx2 * ((4 * y) + 6);
        }

  inTransaction = false;
  spi_end();
}

/*******************************************************************************
 * setAddrWindow, define an area to receive a stream of pixels
 ******************************************************************************/
// Chip select is high at the end of this function
void M5CoreDisplay::setAddrWindow(int32_t x0, int32_t y0, int32_t w, int32_t h) {
  spi_begin();
  setWindow(x0, y0, x0 + w - 1, y0 + h - 1);
  spi_end();
}

/*******************************************************************************
 * setWindow, define an area to receive a stream of pixels
 ******************************************************************************/
// Chip select stays low, call spi_begin first. Use setAddrWindow() from
// sketches
void M5CoreDisplay::setWindow(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  addr_col = 0xFFFF;
  addr_row = 0xFFFF;

  DC_C;
  tft_Write_8(TFT_CASET);
  DC_D;
  tft_Write_32(SPI_32(x0, x1));
  DC_C;
  // Row addr set
  tft_Write_8(TFT_PASET);
  DC_D;
  tft_Write_32(SPI_32(y0, y1));
  DC_C;
  // write to RAM
  tft_Write_8(TFT_RAMWR);
  DC_D;
}

/*******************************************************************************
 * spi_begin, internal SPI communication.
 ******************************************************************************/
inline void M5CoreDisplay::spi_begin(void) {
  if (locked) {
     locked = false;
     spi.beginTransaction(
     SPISettings(SPI_FREQUENCY, MSBFIRST, TFT_SPI_MODE));
     CS_L;
     }
}

/*******************************************************************************
 * spi_end, internal SPI communication.
 ******************************************************************************/
void M5CoreDisplay::spi_end(void) {
  if (!inTransaction) {
     if (!locked) {
        locked = true;
        CS_H;
        spi.endTransaction();
        }
     }
}

/*******************************************************************************
 * writecommand, send an 8 bit command to the TFT
 ******************************************************************************/
void M5CoreDisplay::writecommand(uint8_t c) {
  spi_begin();
  DC_C;
  tft_Write_8(c);
  DC_D;
  spi_end();
}

/*******************************************************************************
 * writedata, send a 8 bit data value to the TFT
 ******************************************************************************/
void M5CoreDisplay::writedata(uint8_t d) {
  spi_begin();
  DC_D;
  tft_Write_8(d);
  CS_L;
  spi_end();
}

/*******************************************************************************
 * pushColor, send a single pixel
 ******************************************************************************/
void M5CoreDisplay::pushColor(uint16_t color) {
  spi_begin();
  tft_Write_16(color);
  spi_end();
}

/*******************************************************************************
 * pushColor, send a block of equal pixels
 ******************************************************************************/
void M5CoreDisplay::pushColor(uint16_t color, uint32_t len) {
  spi_begin();
  writeBlock(color, len);
  spi_end();
}

/*******************************************************************************
 * pushColors, send an array of 16 bit pixels
 ******************************************************************************/
void M5CoreDisplay::pushColors(uint8_t *data, uint32_t len) {
  spi_begin();
  if (SPI_FREQUENCY > 40000000) {
      while(len >= 64) {
         spi.writePattern(data, 64, 1);
         data += 64;
         len -= 64;
         }
      if (len) spi.writePattern(data, len, 1);
      }
  else
     spi.writeBytes(data, len);
  spi_end();
}

/*******************************************************************************
 * pushColors, send an array of 16 bit pixels
 ******************************************************************************/
void M5CoreDisplay::pushColors(uint16_t *data, uint32_t len, bool swap) {
  spi_begin();
  if (swap)
     spi.writePixels(data, len << 1);
  else
     spi.writeBytes((uint8_t*) data, len << 1);
  spi_end();
}

/*******************************************************************************
 * writeBlock, write a block of pixels of the same colour
 ******************************************************************************/
void writeBlock(uint16_t color, uint32_t repeat) {
  uint32_t color32 = COL_32(color, color);

  if (repeat > 31) {  // Revert legacy toggle buffer change
     WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 511);
     while (repeat > 31) {
        while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) ;
           WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W1_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W2_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W3_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W4_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W5_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W6_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W7_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W8_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W9_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W10_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W11_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W12_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W13_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W14_REG(SPI_PORT), color32);
           WRITE_PERI_REG(SPI_W15_REG(SPI_PORT), color32);
           SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
           repeat -= 32;
           }
        while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) ;
        }

    if (repeat) {
       // Revert toggle buffer change
       WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), (repeat << 4) - 1);
       for(uint32_t i = 0; i <= (repeat >> 1); i++)
          WRITE_PERI_REG((SPI_W0_REG(SPI_PORT) + (i << 2)), color32);
       SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);
       while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) ;
       }
}

inline GFXglyph* pgm_read_glyph_ptr(const GFXfont* gfxFont, uint8_t c) {
  return gfxFont->glyph + c;
}

inline uint8_t* pgm_read_bitmap_ptr(const GFXfont* gfxFont) {
  return gfxFont->bitmap;
}

/*******************************************************************************
 * below IC specific command and data sequences.
 ******************************************************************************/
#define WCMD(x)  writecommand(x)
#define WDATA(x) writedata(x)

void M5CoreDisplay::init9341(void) {
  WCMD(0XEF); WDATA(0X03); WDATA(0X80); WDATA(0X02);
  WCMD(0XCF); WDATA(0X00); WDATA(0XC1); WDATA(0X30);
  WCMD(0XED); WDATA(0X64); WDATA(0X03); WDATA(0X12); WDATA(0X81);
  WCMD(0XE8); WDATA(0X85); WDATA(0X00); WDATA(0X78);
  WCMD(0XCB); WDATA(0X39); WDATA(0X2C); WDATA(0X00); WDATA(0X34); WDATA(0X02);
  WCMD(0xF7); WDATA(0x20);
  WCMD(0xEA); WDATA(0x00); WDATA(0x00);
  WCMD(ILI9341_PWCTR1); WDATA(0x23);               // Power control VRH[5:0]
  WCMD(ILI9341_PWCTR2); WDATA(0x10);               // Power control SAP[2:0];BT[3:0]
  WCMD(ILI9341_VMCTR1); WDATA(0x3e); WDATA(0x28);  // VCM control
  WCMD(ILI9341_VMCTR2);  WDATA(0x86);              // VCM control2

  WCMD(ILI9341_MADCTL);  // Memory Access Control
  #ifdef M5STACK
      WDATA(TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);  // Rotation 0 (portrait mode)
  #else
      WDATA(TFT_MAD_MX |              TFT_MAD_COLOR_ORDER);  // Rotation 0 (portrait mode)
  #endif

  WCMD(ILI9341_PIXFMT);  WDATA(0x55);
  WCMD(ILI9341_FRMCTR1); WDATA(0x00); WDATA(0x13);  // 0x18 79Hz, 0x1B default 70Hz, 0x13 100Hz
  WCMD(ILI9341_DFUNCTR); WDATA(0x08); WDATA(0x82); WDATA(0x27); // Display Function Control
  WCMD(0xF2); WDATA(0x00); // 3Gamma Function Disable
  WCMD(ILI9341_GAMMASET); WDATA(0x01); // Gamma curve selected
  WCMD(ILI9341_GMCTRP1);  // Set Gamma
    WDATA(0x0F); WDATA(0x31); WDATA(0x2B); WDATA(0x0C); WDATA(0x0E);
    WDATA(0x08); WDATA(0x4E); WDATA(0xF1); WDATA(0x37); WDATA(0x07);
    WDATA(0x10); WDATA(0x03); WDATA(0x0E); WDATA(0x09); WDATA(0x00);

  WCMD(ILI9341_GMCTRN1);  // Set Gamma
    WDATA(0x00); WDATA(0x0E); WDATA(0x14); WDATA(0x03); WDATA(0x11);
    WDATA(0x07); WDATA(0x31); WDATA(0xC1); WDATA(0x48); WDATA(0x08);
    WDATA(0x0F); WDATA(0x0C); WDATA(0x31); WDATA(0x36); WDATA(0x0F);

  WCMD(ILI9341_SLPOUT);  // Exit Sleep

  spi_end();
  delay(120);
  spi_begin();

  WCMD(ILI9341_DISPON);  // Display on
}

void M5CoreDisplay::init9342(void) {
  WCMD(0xC8); WDATA(0xFF); WDATA(0x93); WDATA(0x42);
  WCMD(ILI9341_PWCTR1); WDATA(0x12); WDATA(0x12);
  WCMD(ILI9341_PWCTR2); WDATA(0x03);
  WCMD(0xB0); WDATA(0xE0);
  WCMD(0xF6); WDATA(0x00); WDATA(0x01); WDATA(0x01);
  WCMD(ILI9341_MADCTL);  // Memory Access Control
  #ifdef M5STACK
     WDATA(TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);  // Rotation 0 (portrait mode)
  #else
     WDATA(TFT_MAD_MX |              TFT_MAD_COLOR_ORDER);  // Rotation 0 (portrait mode)
  #endif
  WCMD(ILI9341_PIXFMT); WDATA(0x55);
  WCMD(ILI9341_DFUNCTR); WDATA(0x08); WDATA(0x82); WDATA(0x27); // Display Function Control
  WCMD(ILI9341_GMCTRP1); WDATA(0x00); WDATA(0x0C); WDATA(0x11);
  WDATA(0x04); WDATA(0x11); WDATA(0x08); WDATA(0x37); WDATA(0x89);
  WDATA(0x4C); WDATA(0x06); WDATA(0x0C); WDATA(0x0A); WDATA(0x2E);
  WDATA(0x34); WDATA(0x0F);  // Set Gamma
  WCMD(ILI9341_GMCTRN1); WDATA(0x00); WDATA(0x0B); WDATA(0x11);
  WDATA(0x05); WDATA(0x13); WDATA(0x09); WDATA(0x33); WDATA(0x67);
  WDATA(0x48); WDATA(0x07); WDATA(0x0E); WDATA(0x0B); WDATA(0x2E);
  WDATA(0x33); WDATA(0x0F); // Set Gamma
  WCMD(ILI9341_SLPOUT);  // Exit Sleep

  spi_end();
  delay(120);
  spi_begin();
  WCMD(ILI9341_DISPON);  // Display on

  WCMD(TFT_INVON);
}

void M5CoreDisplay::rotate934x(uint8_t Rotation) {
  WCMD(TFT_MADCTL);

  #ifdef M5STACK
  switch (Rotation % 8) {
     case 0:
        WDATA(TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 1:
        WDATA(TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     case 2:
        WDATA(TFT_MAD_MX | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 3:
         WDATA(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     // These next rotations are for bottom up BMP drawing
     case 4:
        WDATA(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 5:
        WDATA(TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     case 6:
        WDATA(TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 7:
        WDATA(TFT_MAD_MX | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
  }
  #else
  switch (Rotation % 8) {
     case 0:
        WDATA(TFT_MAD_MX | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 1:
        WDATA(TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     case 2:
        WDATA(TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 3:
        WDATA(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     // These next rotations are for bottom up BMP drawing
     case 4:
        WDATA(TFT_MAD_MX | TFT_MAD_MY | TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 5:
        WDATA(TFT_MAD_MV | TFT_MAD_MX | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
     case 6:
        WDATA(TFT_MAD_COLOR_ORDER);
        _width  = WIDTH;
        _height = HEIGHT;
        break;
     case 7:
        WDATA(TFT_MAD_MY | TFT_MAD_MV | TFT_MAD_COLOR_ORDER);
        _width  = HEIGHT;
        _height = WIDTH;
        break;
  }
  #endif
}

#undef WCMD
#undef WDATA
