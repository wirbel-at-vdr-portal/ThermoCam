#pragma once
#include <cstdint>

class Upscaler {
private:
  float*  InputImage;
  uint16_t InputWidth;
  uint16_t InputHeight;
  uint16_t InputLastCol;
  uint16_t InputLastRow;

  float*  OutputImage;
  uint16_t OutputWidth;
  uint16_t OutputHeight;
  uint16_t OutputLastCol;
  uint16_t OutputLastRow;

  const float GetPixel(int x, int y);
  float SampleBicubic (float x_fraction, float y_fraction);
  float SampleBilinear(float x_fraction, float y_fraction);
  float SampleNearest (float x_fraction, float y_fraction);

  void Resize(float (Upscaler::*Sample)(float x_fraction, float y_fraction));
  void Resize(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h,
              float (Upscaler::*Sample)(float x_fraction, float y_fraction));
public:
  Upscaler(void) {}
  ~Upscaler(void) {}

  void SetInputImage(float* Image, uint16_t Width, uint16_t Height);
  void SetOutputImage(float* Image, uint16_t Width, uint16_t Height);

  void ResizeBicubic (void);
  void ResizeBilinear(void);
  void ResizeNearest (void);

  /* if your micro controller doesn't have sufficient memory to hold the
   * complete output image, call SetOutputImage() with NULL pointer as
   * 'Image' first and after that request subparts of the output image using
   * the functions below.
   * ImgPart is the dest array of at least (w x h) float's.
   */
  void ResizeBicubic (float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h);
  void ResizeBilinear(float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h);
  void ResizeNearest (float* ImgPart, uint16_t X, uint16_t Y, uint16_t w, uint16_t h);
};
