#pragma once

// M5Stack Core ESP32 with ILI9341 / ILI9342C display has a
// rotated display:
#define M5STACK

// IF the blue and red are swapped on your display,
// Try ONE of these options
//  #define TFT_RGB_ORDER TFT_RGB  // Colour order Red-Green-Blue
//  #define TFT_RGB_ORDER TFT_BGR  // Colour order Blue-Green-Red



// ESP32 interface to ILI9341 / ILI9342C Display on M5 Core
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   14  // Chip select
#define TFT_DC   27  // Data Command
#define TFT_RST  33  // TFT Reset
#define TFT_BL   32  // LED backlight
#define BLK_PWM_CHANNEL 7  // TFT_BL <-> LEDC_CHANNEL_7

#define MHz(n) (n*1000000)
// Define the SPI clock frequency, this affects the graphics rendering speed.
// Too fast and the TFT driver will not keep up and display corruption appears.
// With an ILI9341 display 40MHz works OK, 80MHz sometimes fails
//
// MHz(1), MHz(5), MHz(10), MHz(16), MHz(20), MHz(27), MHz(40), MHz(80)
#define SPI_FREQUENCY     MHz(40) // 40Mhz is stable

// Optional reduced SPI frequency for reading TFT
#define SPI_READ_FREQUENCY MHz(16)



// The ESP32 has 2 free SPI ports i.e. VSPI and HSPI, the VSPI is the default.
// To use the HSPI, uncomment the line. M5Core uses VSPI.
//#define USE_HSPI_PORT

// Comment out the following #define if "SPI Transactions" do not need to be
// supported. When commented out the code size will be smaller and sketches will
// run slightly faster, so leave it commented out unless you need it!

// Transaction support is needed to work with SD library but not needed with
// TFT_SdFat Transaction support is required if other SPI devices are connected.

// Transactions are automatically enabled by the library for an ESP32 (to use
// HAL mutex) so changing it here has no effect

// #define SUPPORT_TRANSACTIONS


#define TFT_SPI_MODE SPI_MODE0

/*******************************************************************************
 * dont change below.
 ******************************************************************************/

// If the frequency is not defined, set a default
#ifndef SPI_FREQUENCY
   #define SPI_FREQUENCY MHz(20)
#endif

// If the frequency is not defined, set a default
#ifndef SPI_READ_FREQUENCY
   #define SPI_READ_FREQUENCY SPI_FREQUENCY
#endif


#ifdef USE_HSPI_PORT
   #define SPI_PORT HSPI
#else
   #define SPI_PORT VSPI
#endif

#ifndef TFT_DC
   #define DC_C  // No macro allocated so it generates no code
   #define DC_D  // No macro allocated so it generates no code
#else
#if defined(ESP8266) && (TFT_DC == 16)
   #define DC_C digitalWrite(TFT_DC, LOW)
   #define DC_D digitalWrite(TFT_DC, HIGH)
#elif defined(ESP32)
   #if defined(ESP32_PARALLEL)
      #define DC_C GPIO.out_w1tc = (1 << TFT_DC)
      #define DC_D GPIO.out_w1ts = (1 << TFT_DC)
   #else
      #if TFT_DC >= 32
          #ifdef RPI_ILI9486_DRIVER  // RPi display needs a slower DC change
              #define DC_C                                   \
                  GPIO.out1_w1ts.val = (1 << (TFT_DC - 32)); \
                  GPIO.out1_w1tc.val = (1 << (TFT_DC - 32))
              #define DC_D                                   \
                  GPIO.out1_w1tc.val = (1 << (TFT_DC - 32)); \
                  GPIO.out1_w1ts.val = (1 << (TFT_DC - 32))
          #else
              #define DC_C             \
                  GPIO.out1_w1tc.val = \
                      (1 << (TFT_DC - 32))  //;GPIO.out1_w1tc.val = (1 << (TFT_DC - 32))
              #define DC_D             \
                  GPIO.out1_w1ts.val = \
                      (1 << (TFT_DC - 32))  //;GPIO.out1_w1ts.val = (1 << (TFT_DC - 32))
          #endif
      #else
          #if TFT_DC >= 0
             #ifdef RPI_ILI9486_DRIVER  // RPi display needs a slower DC change
                #define DC_C                       \
                    GPIO.out_w1ts = (1 << TFT_DC); \
                    GPIO.out_w1tc = (1 << TFT_DC)
                #define DC_D                       \
                    GPIO.out_w1tc = (1 << TFT_DC); \
                    GPIO.out_w1ts = (1 << TFT_DC)
             #else
                #define DC_C GPIO.out_w1tc = (1 << TFT_DC)  //;GPIO.out_w1tc = (1 << TFT_DC)
                #define DC_D GPIO.out_w1ts = (1 << TFT_DC)  //;GPIO.out_w1ts = (1 << TFT_DC)
             #endif
          #else
             #define DC_C
             #define DC_D
          #endif
      #endif
   #endif
#else
   #define DC_C GPOC = dcpinmask
   #define DC_D GPOS = dcpinmask
#endif
#endif

#if defined(TFT_SPI_OVERLAP)
#undef TFT_CS
#define SPI1U_WRITE (SPIUMOSI | SPIUSSE | SPIUCSSETUP | SPIUCSHOLD)
#define SPI1U_READ  (SPIUMOSI | SPIUSSE | SPIUCSSETUP | SPIUCSHOLD | SPIUDUPLEX)
#else
#ifdef ESP8266
#define SPI1U_WRITE (SPIUMOSI | SPIUSSE)
#define SPI1U_READ  (SPIUMOSI | SPIUSSE | SPIUDUPLEX)
#endif
#endif

#ifndef TFT_CS
#define CS_L  // No macro allocated so it generates no code
#define CS_H  // No macro allocated so it generates no code
#else
#if defined(ESP8266) && (TFT_CS == 16)
#define CS_L digitalWrite(TFT_CS, LOW)
#define CS_H digitalWrite(TFT_CS, HIGH)
#elif defined(ESP32)
#if defined(ESP32_PARALLEL)
#define CS_L  // The TFT CS is set permanently low during init()
#define CS_H
#else
#if TFT_CS >= 32
#ifdef RPI_ILI9486_DRIVER  // RPi display needs a slower CS change
#define CS_L                                   \
    GPIO.out1_w1ts.val = (1 << (TFT_CS - 32)); \
    GPIO.out1_w1tc.val = (1 << (TFT_CS - 32))
#define CS_H                                   \
    GPIO.out1_w1tc.val = (1 << (TFT_CS - 32)); \
    GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))
#else
#define CS_L                                   \
    GPIO.out1_w1tc.val = (1 << (TFT_CS - 32)); \
    GPIO.out1_w1tc.val = (1 << (TFT_CS - 32))
#define CS_H             \
    GPIO.out1_w1ts.val = \
        (1 << (TFT_CS - 32))  //;GPIO.out1_w1ts.val = (1 << (TFT_CS - 32))
#endif
#else
#if TFT_CS >= 0
#ifdef RPI_ILI9486_DRIVER  // RPi display needs a slower CS change
#define CS_L                       \
    GPIO.out_w1ts = (1 << TFT_CS); \
    GPIO.out_w1tc = (1 << TFT_CS)
#define CS_H                       \
    GPIO.out_w1tc = (1 << TFT_CS); \
    GPIO.out_w1ts = (1 << TFT_CS)
#else
#define CS_L                       \
    GPIO.out_w1tc = (1 << TFT_CS); \
    GPIO.out_w1tc = (1 << TFT_CS)
#define CS_H GPIO.out_w1ts = (1 << TFT_CS)  //;GPIO.out_w1ts = (1 << TFT_CS)
#endif
#else
#define CS_L
#define CS_H
#endif
#endif
#endif
#else
#define CS_L GPOC = cspinmask
#define CS_H GPOS = cspinmask
#endif
#endif

// Use single register write for CS_L and DC_C if pins are both in range 0-31
#ifdef ESP32
   #ifdef TFT_CS
      #if (TFT_CS >= 0) && (TFT_CS < 32) && (TFT_DC >= 0) && (TFT_DC < 32)
         #ifdef RPI_ILI9486_DRIVER  // RPi display needs a slower CD and DC change
            #define CS_L_DC_C                                 \
             GPIO.out_w1tc = ((1 << TFT_CS) | (1 << TFT_DC)); \
             GPIO.out_w1tc = ((1 << TFT_CS) | (1 << TFT_DC))
         #else
            #define CS_L_DC_C                                 \
             GPIO.out_w1tc = ((1 << TFT_CS) | (1 << TFT_DC)); \
             GPIO.out_w1tc = ((1 << TFT_CS) | (1 << TFT_DC))
         #endif
      #else
         #define CS_L_DC_C \
             CS_L;         \
             DC_C
      #endif
   #else
      #define CS_L_DC_C \
          CS_L;         \
          DC_C
   #endif
#else  // ESP8266
   #define CS_L_DC_C \
       CS_L;         \
       DC_C
#endif



#ifdef ESP8266
// Concatenate two 16 bit values for the SPI 32 bit register write
#define SPI_32(H, L) ((H) << 16 | (L))
#define COL_32(H, L) ((H) << 16 | (L))
#else
#if defined(ESP32_PARALLEL) || defined(ILI9488_DRIVER)
#define SPI_32(H, L) ((H) << 16 | (L))
#else
#define SPI_32(H, L) (((H) << 8 | (H) >> 8) | (((L) << 8 | (L) >> 8) << 16))
#endif
// Swap byte order for concatenated 16 bit colors
// AB CD -> DCBA for 32 bit register write
#define COL_32(H, L) (((H) << 8 | (H) >> 8) | (((L) << 8 | (L) >> 8) << 16))
#endif

#if defined(ESP32) && defined(ESP32_PARALLEL)
// Mask for the 8 data bits to set pin directions
#define dir_mask                                                     \
    ((1 << TFT_D0) | (1 << TFT_D1) | (1 << TFT_D2) | (1 << TFT_D3) | \
     (1 << TFT_D4) | (1 << TFT_D5) | (1 << TFT_D6) | (1 << TFT_D7))

// Data bits and the write line are cleared to 0 in one step
#define clr_mask (dir_mask | (1 << TFT_WR))



// Real-time shifting alternative to above to save 1KByte RAM, 47 fps Sprite
// rendering test
/*#define set_mask(C) ((C&0x80)>>7)<<TFT_D7 | ((C&0x40)>>6)<<TFT_D6 |
((C&0x20)>>5)<<TFT_D5 | ((C&0x10)>>4)<<TFT_D4 | \
                      ((C&0x08)>>3)<<TFT_D3 | ((C&0x04)>>2)<<TFT_D2 |
((C&0x02)>>1)<<TFT_D1 | ((C&0x01)>>0)<<TFT_D0
//*/

// Write 8 bits to TFT
#define tft_Write_8(C)                    \
    GPIO.out_w1tc = clr_mask;             \
    GPIO.out_w1ts = set_mask((uint8_t)C); \
    WR_H

// Write 16 bits to TFT
#ifdef PSEUDO_8_BIT
#define tft_Write_16(C)                          \
    WR_L;                                        \
    GPIO.out_w1tc = clr_mask;                    \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 0)); \
    WR_H
#else
#define tft_Write_16(C)                          \
    GPIO.out_w1tc = clr_mask;                    \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 8)); \
    WR_H;                                        \
    GPIO.out_w1tc = clr_mask;                    \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 0)); \
    WR_H
#endif

// 16 bit write with swapped bytes
#define tft_Write_16S(C)                         \
    GPIO.out_w1tc = clr_mask;                    \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 0)); \
    WR_H;                                        \
    GPIO.out_w1tc = clr_mask;                    \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 8)); \
    WR_H

// Write 32 bits to TFT
#define tft_Write_32(C)                           \
    GPIO.out_w1tc = clr_mask;                     \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 24)); \
    WR_H;                                         \
    GPIO.out_w1tc = clr_mask;                     \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 16)); \
    WR_H;                                         \
    GPIO.out_w1tc = clr_mask;                     \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 8));  \
    WR_H;                                         \
    GPIO.out_w1tc = clr_mask;                     \
    GPIO.out_w1ts = set_mask((uint8_t)(C >> 0));  \
    WR_H

#ifdef TFT_RD
#define RD_L GPIO.out_w1tc = (1 << TFT_RD)
//#define RD_L digitalWrite(TFT_WR, LOW)
#define RD_H GPIO.out_w1ts = (1 << TFT_RD)
//#define RD_H digitalWrite(TFT_WR, HIGH)
#endif

#elif defined( \
    ILI9488_DRIVER)  // 16 bit colour converted to 3 bytes for 18 bit RGB

// Write 8 bits to TFT
#define tft_Write_8(C) spi.transfer(C)

// Convert 16 bit colour to 18 bit and write in 3 bytes
#define tft_Write_16(C)              \
    spi.transfer((C & 0xF800) >> 8); \
    spi.transfer((C & 0x07E0) >> 3); \
    spi.transfer((C & 0x001F) << 3)

// Convert swapped byte 16 bit colour to 18 bit and write in 3 bytes
#define tft_Write_16S(C)                                \
    spi.transfer(C & 0xF8);                             \
    spi.transfer((C & 0xE000) >> 11 | (C & 0x07) << 5); \
    spi.transfer((C & 0x1F00) >> 5)
// Write 32 bits to TFT
#define tft_Write_32(C) spi.write32(C)

#elif defined(RPI_ILI9486_DRIVER)

#define tft_Write_8(C) \
    spi.transfer(0);   \
    spi.transfer(C)
#define tft_Write_16(C)  spi.write16(C)
#define tft_Write_16S(C) spi.write16(C << 8 | C >> 8)
#define tft_Write_32(C)  spi.write32(C)

#elif defined ESP8266

#define tft_Write_8(C)  spi.write(C)
#define tft_Write_16(C) spi.write16(C)
#define tft_Write_32(C) spi.write32(C)

#else  // ESP32 using SPI with 16 bit color display

// ESP32 low level SPI writes for 8, 16 and 32 bit values
// to avoid the function call overhead

// Write 8 bits
#define tft_Write_8(C)                                     \
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 8 - 1);    \
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), C);               \
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);     \
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) \
        ;

// Write 16 bits with corrected endianess for 16 bit colours
#define tft_Write_16(C)                                    \
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 16 - 1);   \
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), C << 8 | C >> 8); \
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);     \
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) \
        ;

// Write 16 bits
#define tft_Write_16S(C)                                   \
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 16 - 1);   \
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), C);               \
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);     \
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) \
        ;

// Write 32 bits
#define tft_Write_32(C)                                    \
    WRITE_PERI_REG(SPI_MOSI_DLEN_REG(SPI_PORT), 32 - 1);   \
    WRITE_PERI_REG(SPI_W0_REG(SPI_PORT), C);               \
    SET_PERI_REG_MASK(SPI_CMD_REG(SPI_PORT), SPI_USR);     \
    while (READ_PERI_REG(SPI_CMD_REG(SPI_PORT)) & SPI_USR) \
        ;

#endif


#if defined(ESP32) && !defined(SUPPORT_TRANSACTIONS)
   // SUPPORT_TRANSACTIONS is mandatory for ESP32 so the hal mutex is toggled
   #define SUPPORT_TRANSACTIONS
#endif


// Make sure TFT_MISO is defined if not used to avoid an error message
#ifndef TFT_MISO
#define TFT_MISO -1
#endif
