unit Unit1;

{$mode objfpc}{$H+}

interface

uses
  Classes, SysUtils, Forms, Controls, Graphics, Dialogs, StdCtrls, ExtCtrls,
  Buttons, upscalerp, SpinEx;

type

  { TForm1 }

  TForm1 = class(TForm)
    Button1: TButton;
    Button2: TButton;
    CheckBox1: TCheckBox;
    ComboBox1: TComboBox;
    GroupBox1: TGroupBox;
    Image: TImage;
    GroupBox_Scale: TRadioGroup;
    Label1: TLabel;
    Label2: TLabel;
    OpenDialog: TOpenDialog;
    RadioButton1: TRadioButton;
    RadioButton2: TRadioButton;
    RadioButton3: TRadioButton;
    SaveDialog: TSaveDialog;
    SpinEditEx1: TSpinEditEx;
    SpinEditEx2: TSpinEditEx;
    procedure Button1Click(Sender: TObject);
    procedure Button2Click(Sender: TObject);
    procedure CheckBox1Change(Sender: TObject);
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
  private

  public

  end;

var
  Form1: TForm1;
  PNG: TPortableNetworkGraphic;

function map(value:float; inMin:float; inMax:float; outMin:float; outMax:float):integer;
procedure setRGB(dest:puint8; src:uint16);

implementation
{$R *.lfm}

uses IronBow;

var
  data: array[0..767] of float; {24x32 - 1}
  upscaled: array of float;
  rgb: array of uint8;
  OutW,OutH:uint16;
  tmin,tmax:float;
  Scaler:Upscaler;


procedure TForm1.Button1Click(Sender: TObject);
var
  i,j:integer;
  p:puint8;
  px:uint16;
  bitmap:TBitmap;
  pxcolor:TColor;
  automin,automax:integer;
  lastCursor:TCursor;
  TDF: file of uint32;
  u32: uint32;
  pf: pFloat;
  path:string;
begin
  OpenDialog.Options    := [ofEnableSizing,ofFileMustExist,ofPathMustExist,ofViewDetail];
  OpenDialog.DefaultExt := '.tdf';
  OpenDialog.Filter     := 'Temperature Data File(*.tdf)|*.tdf';
  if not OpenDialog.Execute then
     exit;
  path:=OpenDialog.FileName;

  Assignfile(TDF, path);
  Reset(TDF);
  i:=0;
  pf := addr(u32);
  while not EOF(TDF) do
     begin
     read(TDF, u32);
     if (i = 0) and (u32 <> $00464454) {'T','D','F','\0'} then
        begin
        ShowMessage('Invalid Marker 0x' + IntToHex(u32,4));
        exit;
        end
     else if (i = 1) and (u32 <> $00180020) {w = 32, h = 24} then
        begin
        ShowMessage('Invalid size info 0x' + IntToHex(u32,4));
        exit;
        end;
     inc(i);
     if i<3 then {header}
        continue;

     data[i-3] := pf^;
     end;
  CloseFile(TDF);


  lastCursor := Form1.Cursor;
  Form1.Cursor := crHourGlass;
  Image.Canvas.Clear;
  Image.Refresh;
  Scaler.SetInputImage(addr(data[0]), 32, 24);

  if ComboBox1.ItemIndex = 0 then
     begin
     OutW := 320;
     OutH := 240;
     end
  else if ComboBox1.ItemIndex = 1 then
     begin
     OutW := 640;
     OutH := 480;
     end
  else if ComboBox1.ItemIndex = 2 then
     begin
     OutW := 1280;
     OutH := 960;
     end;

  setlength(upscaled, OutW*OutH);
  Scaler.SetOutputImage(addr(upscaled[0]), OutW, OutH);

  if RadioButton1.Checked then
     Scaler.ResizeNearest()
  else if RadioButton2.Checked then
     Scaler.ResizeBilinear()
  else
     Scaler.ResizeBicubic();

  if CheckBox1.Checked then
     begin
     automin:=1000;
     automax:=-1000;
     for i:=0 to (sizeof(data) div sizeof(data[0]))-1 do
        begin
        if data[i] > automax then automax:=round(data[i]+0.5);
        if data[i] < automin then automin:=round(data[i]-0.5);
        end;
     while(abs(automax mod 5) <> 0) do inc(automax);
     while(abs(automin mod 5) <> 0) do dec(automin);
     SpinEditEx1.Value := automin;
     SpinEditEx2.Value := automax;
     end;

  tmin := SpinEditEx1.Value;
  tmax := SpinEditEx2.Value;

  setlength(rgb, 3*OutW*OutH);
  p := addr(rgb[0]);
  i := 0;
  while i < OutW*OutH do
     begin
     px := Ironbow2048[map(upscaled[i], tmin, tmax, 0, 2047)];
     inc(i);

     setRGB(p, px);
     inc(p,3);
     end;

  Bitmap := TBitmap.Create;
  Bitmap.PixelFormat := pf24bit;
  Bitmap.Width := OutW;
  Bitmap.Height := OutH;
  p := addr(rgb[0]);
  for j := 0 to Bitmap.Height-1 do
     begin
     for i := 0 to Bitmap.Width-1 do
        begin
        pxcolor := (p)^ or (p+1)^ shl 8 or (p+2)^ shl 16;
        inc(p,3);
        Bitmap.Canvas.Pixels[i,j] := pxcolor;
        end;
     end;

  PNG.Assign(Bitmap);
  image.Picture.Bitmap:=bitmap;
  image.Refresh;
  Bitmap.Free;
  Form1.Cursor := lastCursor;
end;

procedure TForm1.Button2Click(Sender: TObject);
var path:string;
begin
  SaveDialog.Options    := [ofoverwriteprompt,ofpathmustexist,ofExtensionDifferent];
  SaveDialog.FileName   := 'data.png';
  SaveDialog.DefaultExt := '.png';
  SaveDialog.Filter     := 'Portable Network Graphic(*.png)|*.png';

  if not SaveDialog.Execute then exit;

  path:=SaveDialog.FileName;
  PNG.SaveToFile(path);
end;

procedure TForm1.CheckBox1Change(Sender: TObject);
begin
  SpinEditEx1.Enabled := not CheckBox1.Checked;
  SpinEditEx2.Enabled := not CheckBox1.Checked;
end;

procedure TForm1.FormCreate(Sender: TObject);
begin
  Scaler := Upscaler.Create;
  OutW := 640;
  OutH := 480;
  tmin := 10;
  tmax := 60;
  PNG := TPortableNetworkGraphic.Create;
  Image.Canvas.Clear;
end;

procedure TForm1.FormDestroy(Sender: TObject);
begin
  GroupBox_Scale.Free;
  PNG.Free;
end;

function map(value:float; inMin:float; inMax:float; outMin:float; outMax:float):integer;
var
  f:float;
begin
  f := (value - inMin);
  f /= (inMax - inMin);
  f *= (outMax - outMin);
  f += outMin;
  result := round(f);

  if result > outMax then
     result := trunc(outMax)
  else if result < outMin then
     result := round(outMin+0.5);
end;

procedure setRGB(dest:puint8; src:uint16);
var
  r,GroupBox_Scale,b:puint8;
begin
  r := dest;
  GroupBox_Scale := dest+1;
  b := dest+2;

  b^ := (src and $1F) shl 3; // 5bits Blue
  src := src shr 5;

  GroupBox_Scale^ := (src and $3F) shl 2; // 6Bits Green
  src := src shr 6;

  r^ := (src and $1F) shl 3; // 5Bits Red
end;


end.

