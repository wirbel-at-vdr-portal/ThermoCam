unit Upscalerp;

{$mode ObjFPC}{$H+}

interface

uses
  Classes, SysUtils, Math;


type

pFloat = pSingle;
 Float = Single;

TSample = function(x_fraction:float; y_fraction:float):float of object;

Upscaler = class
private
  InputImage:pFloat;
  InputWidth:uint16;
  InputHeight:uint16;
  InputLastCol:uint16;
  InputLastRow:uint16;

  OutputImage:pFloat;
  OutputWidth:uint16;
  OutputHeight:uint16;
  OutputLastCol:uint16;
  OutputLastRow:uint16;

  bicubic:TSample;
  bilinear:TSample;
  nearest:TSample;

  function GetPixel(x:integer; y:integer):float;
  function SampleBicubic (x_fraction:float; y_fraction:float):float;
  function SampleBilinear(x_fraction:float; y_fraction:float):float;
  function SampleNearest (x_fraction:float; y_fraction:float):float;

  procedure Resize(Sample:TSample);
  procedure Resize(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16; Sample:TSample);
public
  constructor Create;

  procedure SetInputImage (Image:pFloat; Width:uint16; Height:uint16);
  procedure SetOutputImage(Image:pFloat; Width:uint16; Height:uint16);

  procedure ResizeBicubic;
  procedure ResizeBilinear;
  procedure ResizeNearest;

  (* if your micro controller doesn't have sufficient memory to hold the
   * complete output image, call SetOutputImage() with NULL pointer as
   * 'Image' first and after that request subparts of the output image using
   * the functions below.
   * ImgPart is the dest array of at least (w x h) float's.
   *)
  procedure ResizeBicubic (ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);
  procedure ResizeBilinear(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);
  procedure ResizeNearest (ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);

  end;


implementation

constructor Upscaler.Create;
begin
  bicubic  := addr(SampleBicubic);
  bilinear := addr(SampleBilinear);
  nearest  := addr(SampleNearest);
end;


procedure Upscaler.SetInputImage(Image:pFloat; Width:uint16; Height:uint16);
begin
  InputImage  := Image;
  InputWidth  := Width;
  InputHeight := Height;

  InputLastCol := InputWidth - 1;
  InputLastRow := InputHeight - 1;
end;


procedure Upscaler.SetOutputImage(Image:pFloat; Width:uint16; Height:uint16);
begin
  OutputImage  := Image;
  OutputWidth  := Width;
  OutputHeight := Height;

  OutputLastCol := OutputWidth - 1;
  OutputLastRow := OutputHeight - 1;
end;


function CubicHermite(A:float; B:float; C:float; D:float; t:float):float;
var
  fa,fb,fc,fd:float;
begin
  fa := -0.5 * (A - 3.0 * B  + 3.0 * C - D);
  fb :=         A - 2.5 * B  + 2.0 * C - 0.5 * D;
  fc := -0.5 *  A            + 0.5 * C;
  fd := B;

  result := fa*t*t*t + fb*t*t + fc*t + fd;
end;


function Constrain(V:integer; Min:integer; Max:integer):integer;
begin
  if (V < Min) then
     result := Min
  else if (V > Max) then
     result := Max
  else
     result := V;
end;


function Upscaler.GetPixel(x:integer; y:integer):float;
var xy:integer;
begin
  xy := (Constrain(y, 0, InputLastRow) * InputWidth) + Constrain(x, 0, InputLastCol);
  result := InputImage[xy];
end;


function Upscaler.SampleBicubic(x_fraction:float; y_fraction:float):float;
var
  x,y:float;
  xint,yint:integer;
  remainder:float;
  col0,col1,col2,col3:float;
begin
  x := x_fraction * InputWidth;
  y := y_fraction * InputHeight;

  xint := trunc(x);
  yint := trunc(y);

  remainder := x - floor(x);

  // interpolate bi-cubically
  col0 := CubicHermite(GetPixel(xint - 1, yint - 1), GetPixel(xint + 0, yint - 1), GetPixel(xint + 1, yint - 1), GetPixel(xint + 2, yint - 1), remainder);
  col1 := CubicHermite(GetPixel(xint - 1, yint + 0), GetPixel(xint + 0, yint + 0), GetPixel(xint + 1, yint + 0), GetPixel(xint + 2, yint + 0), remainder);
  col2 := CubicHermite(GetPixel(xint - 1, yint + 1), GetPixel(xint + 0, yint + 1), GetPixel(xint + 1, yint + 1), GetPixel(xint + 2, yint + 1), remainder);
  col3 := CubicHermite(GetPixel(xint - 1, yint + 2), GetPixel(xint + 0, yint + 2), GetPixel(xint + 1, yint + 2), GetPixel(xint + 2, yint + 2), remainder);

  result := CubicHermite(col0, col1, col2, col3, y - floor(y));
end;


function Lerp(A:float; B:float; t:float):float;
begin
  result := (A) + (t)*((B) - (A));
end;


function Upscaler.SampleBilinear(x_fraction:float; y_fraction:float):float;
var
  x,y:float;
  xint,yint:integer;
  remainder:float;
  col0,col1:float;
begin
  x := x_fraction * InputWidth;
  y := y_fraction * InputHeight;

  xint := trunc(x);
  yint := trunc(y);

  remainder := x - floor(x);

  col0 := Lerp(GetPixel(xint + 0, yint + 0), GetPixel(xint + 1, yint + 0), remainder);
  col1 := Lerp(GetPixel(xint + 0, yint + 1), GetPixel(xint + 1, yint + 1), remainder);
  result := Lerp(col0, col1, y - floor(y));
end;


function Upscaler.SampleNearest(x_fraction:float; y_fraction:float):float;
var
  xint,yint:integer;
begin
  xint := trunc(x_fraction * InputWidth);
  yint := trunc(y_fraction * InputHeight);
  result := GetPixel(xint, yint);
end;


procedure Upscaler.Resize(Sample:TSample);
var CurrentRow,NextPixel:pFloat;
    x,y:uint16;
    y_fraction:float;
begin
  if (OutputImage = NIL) then
     exit;

  CurrentRow := OutputImage;

  for y := 0 to OutputHeight-1 do
     begin
     NextPixel := CurrentRow;
     y_fraction := y / float(OutputLastRow);

     for x := 0 to OutputWidth-1 do
        begin
        NextPixel^ := Sample(x / float(OutputLastCol), y_fraction);
        inc(NextPixel);
        end;

     CurrentRow += OutputWidth;
     end;
end;


procedure Upscaler.Resize(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16; Sample:TSample);
var
  CurrentRow,NextPixel:pFloat;
  xmax,ymax:uint16;
  iX,iY:uint16;
  y_fraction:float;
begin
  CurrentRow := ImgPart;

  xmax := X+w;
  ymax := Y+h;

  for iY := Y to ymax-1 do
     begin
     NextPixel := CurrentRow;
     y_fraction := iY / float(OutputLastRow);

     for iX := X to xmax-1 do
        begin
        NextPixel^ := Sample(x / float(OutputLastCol), y_fraction);
        inc(NextPixel);
        end;

     CurrentRow += w;
     end;
end;


procedure Upscaler.ResizeBicubic;
begin
  Resize(bicubic);
end;


procedure Upscaler.ResizeBilinear;
begin
  Resize(bilinear);
end;


procedure Upscaler.ResizeNearest;
begin
  Resize(nearest);
end;


procedure Upscaler.ResizeBicubic(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);
begin
  Resize(ImgPart, X, Y, w, h, bicubic);
end;


procedure Upscaler.ResizeBilinear(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);
begin
  Resize(ImgPart, X, Y, w, h, bilinear);
end;


procedure Upscaler.ResizeNearest(ImgPart:pFloat; X:uint16; Y:uint16; w:uint16; h:uint16);
begin
  Resize(ImgPart, X, Y, w, h, nearest);
end;

end.
