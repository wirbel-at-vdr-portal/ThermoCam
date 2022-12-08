#include "UpScaler.h"
#include <math.h>
#pragma GCC optimize ("O3")

void Upscaler::SetInputImage(float* Image, uint16_t Width, uint16_t Height) {
  InputImage  = Image;
  InputWidth  = Width;
  InputHeight = Height;

  InputLastCol = InputWidth - 1;
  InputLastRow = InputHeight - 1;
}

void Upscaler::SetOutputImage(float* Image, uint16_t Width, uint16_t Height) {
  OutputImage  = Image;
  OutputWidth  = Width;
  OutputHeight = Height;

  OutputLastCol = OutputWidth - 1;
  OutputLastRow = OutputHeight - 1;
}

float CubicHermite(float A, float B, float C, float D, float t) {
  float a = -0.5f * (A - 3.0f * B  + 3.0f * C - D);
  float b =          A - 2.5f * B  + 2.0f * C - 0.5f * D;
  float c = -0.5f *  A             + 0.5f * C;
  float d = B;

  return a*t*t*t + b*t*t + c*t + d;
}

int Constrain(int V, int Min, int Max) {
  if (V < Min)
     return Min;
  if (V > Max)
     return Max;
  return V;
}

const float Upscaler::GetPixel(int x, int y) {
  int pos = (Constrain(y, 0, InputLastRow) * InputWidth) + Constrain(x, 0, InputLastCol);
  return InputImage[pos];
}

float Upscaler::SampleBicubic(float x_fraction, float y_fraction) {
  float x = x_fraction * InputWidth;
  float y = y_fraction * InputHeight;

  int xint = int(x);
  int yint = int(y);

  float remainder = x - floorf(x);

  // interpolate bi-cubically
  float col0  = CubicHermite(GetPixel(xint - 1, yint - 1), GetPixel(xint + 0, yint - 1), GetPixel(xint + 1, yint - 1), GetPixel(xint + 2, yint - 1), remainder);
  float col1  = CubicHermite(GetPixel(xint - 1, yint + 0), GetPixel(xint + 0, yint + 0), GetPixel(xint + 1, yint + 0), GetPixel(xint + 2, yint + 0), remainder);
  float col2  = CubicHermite(GetPixel(xint - 1, yint + 1), GetPixel(xint + 0, yint + 1), GetPixel(xint + 1, yint + 1), GetPixel(xint + 2, yint + 1), remainder);
  float col3  = CubicHermite(GetPixel(xint - 1, yint + 2), GetPixel(xint + 0, yint + 2), GetPixel(xint + 1, yint + 2), GetPixel(xint + 2, yint + 2), remainder);

  return CubicHermite(col0, col1, col2, col3, y - floorf(y));
}

float Upscaler::SampleBilinear(float x_fraction, float y_fraction) {
  float x = x_fraction * InputWidth;
  float y = y_fraction * InputHeight;

  int xint = int(x);
  int yint = int(y);

  float remainder = x - floorf(x);

  #define Lerp(A,B,t) (   (A) + (t)*((B) - (A))   )
  float col0 = Lerp(GetPixel(xint + 0, yint + 0), GetPixel(xint + 1, yint + 0), remainder);
  float col1 = Lerp(GetPixel(xint + 0, yint + 1), GetPixel(xint + 1, yint + 1), remainder);
  return Lerp(col0, col1, y - floorf(y));
  #undef Lerp
}

float Upscaler::SampleNearest(float x_fraction, float y_fraction) {
  int xint = int(x_fraction * InputWidth);
  int yint = int(y_fraction * InputHeight);
  return GetPixel(xint, yint);
}

void Upscaler::Resize(float (Upscaler::*Sample)(float x_fraction, float y_fraction)) {
  if (!OutputImage)
     return;

  float* CurrentRow = OutputImage;

  for(uint16_t y = 0; y < OutputHeight; y++) {
     float* NextPixel = CurrentRow;
     float y_fraction = y / float(OutputLastRow);

     for(uint16_t x = 0; x < OutputWidth; x++) {
        *NextPixel++ = (this->*Sample)(x / float(OutputLastCol), y_fraction);
        }

     CurrentRow += OutputWidth;
     }
}

void Upscaler::Resize(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h,
                      float (Upscaler::*Sample)(float x_fraction, float y_fraction)) {
  float* CurrentRow = ImgPart;

  uint16_t xmax = X+w;
  uint16_t ymax = Y+h;

  for(uint16_t y = Y; y < ymax; y++) {
     float* NextPixel = CurrentRow;
     float y_fraction = y / float(OutputLastRow);

     for(uint16_t x = X; x < xmax; x++) {
        *NextPixel++ = (this->*Sample)(x / float(OutputLastCol), y_fraction);
        }

     CurrentRow += w;
     }

}

void Upscaler::ResizeBicubic(void) {
  Resize(&Upscaler::SampleBicubic);
}

void Upscaler::ResizeBilinear(void) {
  Resize(&Upscaler::SampleBilinear);
}

void Upscaler::ResizeNearest(void) {
  Resize(&Upscaler::SampleNearest);
}

void Upscaler::ResizeBicubic(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h) {
  Resize(ImgPart, X, Y, w, h, &Upscaler::SampleBicubic);
}

void Upscaler::ResizeBilinear(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h) {
  Resize(ImgPart, X, Y, w, h, &Upscaler::SampleBilinear);
}

void Upscaler::ResizeNearest(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h) {
  Resize(ImgPart, X, Y, w, h, &Upscaler::SampleNearest);
}
