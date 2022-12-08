#include <Arduino.h>
#include <math.h>
#include <SPI.h>
#include <Wire.h>
#include <EEPROM.h>
#include "FS.h"
#include "SD.h"
#include "M5CoreDisplay.h"
#include "UpScaler.h"
#include "IronBow.h"
#include "MLX90640_API.h"
#pragma GCC optimize ("O3")

auto LCD = M5CoreDisplay();
auto Scaler = Upscaler();
long long t1, t2;
float temps[32*24];

#define SCALE_X 300
#define SCALE_Y 220
#define MINTEMP 20
#define MAXTEMP 50

void PrintTmin(void);
void PrintTmax(void);
void PrintEmissivity(void);
void ColorBar(void);
void CrossHair(int x, int y, float v);
void Store(void);
void Restore(void);
void SaveToSD(void);

paramsMLX90640 sensorCal;
float tmin = 20.0f, tmax = 60.0f;

#define NumEmissivities 9
const float emissivities[NumEmissivities] = {0.98f       ,0.95f              , 0.10f              , 0.65f              , 0.59            , 0.93f   , 0.94f   , 0.87f , 0.90f  };
const char* emNames[NumEmissivities]      = {"Human Skin","Water,Paint,Cloth","polished Aluminium","anodized Aluminium","Stainless Steel","Plastic","Ceramic","Glass","Rubber"};
uint8_t     emIndex = 1;

float crossv;
int UpdateEEprom = -1;

void setup() {
  Wire.begin();
  EEPROM.begin(6);
  Serial.begin(115200);
  Wire.setClock(800000);
  delay(1000); // otherwise ESP32 screws up serial console.
  LCD.begin();

  Restore();
  PrintTmin();
  PrintTmax();
  PrintEmissivity();
  LCD.setCursor(150,230);
  LCD.print("to SD");

  Scaler.SetInputImage(temps, 32, 24);
  Scaler.SetOutputImage(NULL, SCALE_X, SCALE_Y);

  uint16_t sensorEeprom[832];
  MLX90640_DumpEE(0x33, sensorEeprom);
  MLX90640_ExtractParameters(sensorEeprom, &sensorCal);

    // 0 – 0.5Hz
    // 1 – 1Hz
    // 2 – 2Hz
    // 3 – 4Hz
    // 4 – 8Hz
    // 5 – 16Hz
    // 6 – 32Hz
    // 7 – 64Hz
  MLX90640_SetRefreshRate(0x33, 4);
    // 0 16 bit
    // 1 17 bit
    // 2 18 bit
    // 3 19 bit
  MLX90640_SetResolution(0x33, 3);

  pinMode(37, INPUT_PULLUP);
  pinMode(38, INPUT_PULLUP);
  pinMode(39, INPUT_PULLUP);
}



const uint16_t PARTW = 300;
const uint16_t PARTH = 20;
const uint16_t PARTSZ = PARTW * PARTH;

float part[PARTSZ]; /* upscaled temp samples */


float slope;
float inMin;

void SetSlope(float iMin, float iMax, float oMin, float oMax) {
  slope = (oMax - oMin) / (iMax - iMin);
  inMin = iMin;
}

int Map(float f) {
  return (f-inMin)*slope;
}



void loop() {

  t1 = millis();
  bool LeftButton   = digitalRead(39) == 0;
  bool MiddleButton = digitalRead(38) == 0;
  bool RightButton  = digitalRead(37) == 0;

  if (LeftButton and not RightButton) {
     tmin = tmin <= 290.0f? tmin + 5.0f : -40.0f;
     PrintTmin();
     UpdateEEprom = 120;
     }
  if (RightButton and not LeftButton) {
     tmax = tmax <= 295.0f? tmax + 5.0f : tmin + 5.0f;
     PrintTmax();
     UpdateEEprom = 120;
     }
  if (RightButton and LeftButton) {
     emIndex++;
     if (emIndex >= NumEmissivities)
        emIndex = 0;
     PrintEmissivity();
     LCD.fillRect(0, 0, 300, 220, 0);
     LCD.setCursor(100,80);
     LCD.print("emissivity:");
     LCD.setCursor(100,100);
     LCD.print(emNames[emIndex]);
     LCD.setCursor(100,120);
     LCD.print(emissivities[emIndex],2);
     UpdateEEprom = 120;
     delay(5000);
     }
  if (MiddleButton)
     SaveToSD();

  if (UpdateEEprom >= 0) {
     if (UpdateEEprom-- == 0)
        Store();
     }

  for(int i=0; i<2; i++) {
     uint16_t RAMdata[834];
     MLX90640_GetFrameData(0x33, RAMdata);

     //float vdd = MLX90640_GetVdd(RAMdata, &sensorCal);
     float Ta  = MLX90640_GetTa(RAMdata, &sensorCal);
     float tr  = Ta - 8.0f;
     
     MLX90640_CalculateTo(RAMdata, &sensorCal, emissivities[emIndex], tr, temps);

     int interleave = MLX90640_GetCurMode(0x33);
     MLX90640_BadPixelsCorrection((&sensorCal)->brokenPixels, temps, interleave, &sensorCal);
     }

  // flip image in x (sensor mounted at backside)
  float* offset = temps;
  for(int col=0; col<24; col++) {
     float* px1 = offset;
     float* px2 = offset + 31;        
     for(; px1 < px2; px1++,px2--) {
        float t = *px1;
        *px1 = *px2;
        *px2 = t;
        }
     offset+=32;
     }

  crossv =(temps[15 * 32 + 11] +
           temps[15 * 32 + 12] +
           temps[16 * 32 + 11] +
           temps[16 * 32 + 12])*0.25f;
  CrossHair(150,110,crossv);

  SetSlope(tmin-1.0f,tmax+1.0f,0,2047);

  for(int y=0; y<SCALE_Y; y+=PARTH)  {
     for(int x=0; x<SCALE_X; x+=PARTW) {
        LCD.setAddrWindow(x, y, PARTW, PARTH);
        Scaler.ResizeBicubic(part, x, y, PARTW, PARTH);

        uint16_t* tmp = (uint16_t*) part;
        for(int i=0; i<PARTSZ; i++) {
           int idx = constrain(Map(part[i]), 0, 2047);
           tmp[i] = Ironbow2048[idx];
           }
        LCD.pushColors(tmp, PARTSZ);
        }
     }
  t2 = millis();
  Serial.println(t2-t1); // 1277ms. 1100ms sleep -> 177ms

}

void CrossHair(int x, int y, float v) {
  LCD.drawFastHLine(x-5, y, 10, 0xFFFF);
  LCD.drawFastVLine(x, y-5, 10, 0xFFFF);
  LCD.setCursor(x+10,y+10);
  LCD.print(v,1);  
}

void ColorBar(void) {
  int x = 303;
  int y = 15;

  for(int i=0; i<190; i++) {
     int idx = constrain(map(189-i,0,190,0,2047), 0, 2047);
     LCD.drawFastHLine(x, y+i, 10/*x+10, y+i*/, Ironbow2048[idx]);
     }

  LCD.setCursor(303,0);
  LCD.print(tmax,0);
  LCD.print("  ");

  LCD.setCursor(303,210);
  LCD.print(tmin,0);
  LCD.print("  ");
}

void PrintTmin(void) {
  LCD.setCursor( 60,230);
  LCD.print(tmin,0);
  LCD.print("  ");
  ColorBar();
}

void PrintTmax(void) {
  LCD.setCursor(250,230);
  LCD.print(tmax,0);
  LCD.print("  ");
  ColorBar();
}

void PrintEmissivity(void) {
  LCD.setCursor(295,230);
  LCD.print(emissivities[emIndex],2);
}

void Store(void) {
  EEPROM.writeUShort(0,(uint16_t)tmin);
  EEPROM.writeUShort(2,(uint16_t)tmax);
  EEPROM.writeByte(4,(uint8_t)emIndex);
  EEPROM.commit();
}

void Restore(void) {
  tmin = EEPROM.readUShort(0);
  tmax = EEPROM.readUShort(2);
  emIndex = EEPROM.readByte(4) % NumEmissivities;
}

void SaveToSD(void) {
  LCD.fillRect(0, 0, 300, 220, BLACK); 
  if (!SD.begin(4, SPI, 40000000)) {
     LCD.fillRect(0, 0, 300, 220, RED);
     LCD.setCursor(100,100);
     LCD.print("Invalid SD card.");
     delay(3000);
     return;
     }

  uint8_t row = 1;

  LCD.setCursor(10,10*row++);
  switch(SD.cardType()) {
     case CARD_NONE:
        LCD.fillRect(0, 0, 300, 220, RED);
        LCD.setCursor(100,100);
        LCD.print("No SD card.");
        delay(3000);
        return;
     case CARD_MMC:
        LCD.print("found MMC card.");
        break;
     case CARD_SD:
        LCD.print("found SD card.");
        break;
     case CARD_SDHC:
        LCD.print("found SDHC card.");
        break;
     default:
        LCD.print("found card with unknown type.");
     }

  LCD.setCursor(10,10*row++);
  //uint64_t cardSize = (SD.cardSize() / (1024 * 1024 * 1024));
  LCD.printf("card size %lluGB", SD.cardSize() / 1073741824LLU);

  if (!SD.exists("/thermal") and SD.mkdir("/thermal")) {
     LCD.setCursor(10,10*row++);
     LCD.print("created dir /thermal");
     }

  File fd = SD.open("/thermal/state", FILE_READ);
  uint32_t num = 0;
  if (fd) {
     fd.read((uint8_t*)&num,4);
     fd.close();
     }
  fd = SD.open("/thermal/state", FILE_WRITE);
  if (fd) {
     uint32_t n1 = num+1;
     fd.write((uint8_t*)&n1,4);
     fd.close();
     }

  char filename[20];
  sprintf(filename, "/thermal/%04u.tdf", num);
  LCD.setCursor(10,10*row++);
  LCD.printf("creating %s", filename);

  fd = SD.open(filename, FILE_WRITE);
  if (fd) {
     uint16_t u;
     float f;

     LCD.setCursor(10,10*row++);
     LCD.print("writing file - pls wait");

     const char* marker = "TDF";
     fd.write((const uint8_t*) marker,4);

     u = 32; fd.write((uint8_t*)&u,2);
     u = 24; fd.write((uint8_t*)&u,2);

     for(u=0; u<32*24; u++) {
        f = temps[u];
        fd.write((uint8_t*)&f,4);
        }

     LCD.fillRect(0, 0, 300, 220, GREEN);
     LCD.setCursor(100,100);
     LCD.printf("DONE: %s",filename);
     fd.close();
     }
  else {
     LCD.fillRect(0, 0, 300, 220, RED);
     LCD.setCursor(100,100);
     LCD.print("Error writing file.");
     }
  SD.end();
  delay(3000);
}
