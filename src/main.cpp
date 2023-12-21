#include "main.h"
#include "pins.h"

OneWire oneWire(dallasTemperaturePin);
DallasTemperature dallasTemperature(&oneWire);
DeviceAddress dallasThermometerAddress;

byte i = 1;

RTC_DS3231 clock = RTC_DS3231();

byte rgbColors[3] = {255, 0, 0};
const byte RED_INDEX = 0;
const byte GREEN_INDEX = 1;
const byte BLUE_INDEX = 2;

byte stage = 0;
unsigned long previousColorUpdate = 0;
const int colorUpdateInterval = 3000;
unsigned long previousTimerUpdate = 0;
const int timerUpdateInterval = 200;
const byte MAX_GREEN_VALUE = 100;

const int minLightLevel = 550;
const int maxLightLevel = 800;

LedControl_SW_SPI sevenSegment;
bool resetSevenSegmentOnNextUpdate = false;

// contructor to select https://github.com/olikraus/u8g2/wiki/u8g2setupcpp#sh1106-128x64_noname-1
U8G2_SH1106_128X64_NONAME_1_HW_I2C display(U8G2_R0);
//U8G2_SH1106_128X64_NONAME_2_HW_I2C display(U8G2_R0);
//U8G2_SH1106_128X64_NONAME_F_HW_I2C display(U8G2_R0);
//U8G2_SH1106_128X64_NONAME_1_2ND_HW_I2C display(U8G2_R0);
//U8G2_SH1106_128X64_NONAME_2_2ND_HW_I2C display(U8G2_R0);
//U8G2_SH1106_128X64_NONAME_F_2ND_HW_I2C display(U8G2_R0);

void setup()
{
  pinMode(serialDataInputPin, OUTPUT);
  pinMode(storageRegisterClockInputPin, OUTPUT);
  pinMode(shiftRegisterClockInputPin, OUTPUT);
  pinMode(outputEnablePin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonResetSevenSegmentPin, INPUT_PULLUP);
  pinMode(brightnessPotentiometerPin, INPUT);

  Wire.begin(0x68);
  clock.begin(&Wire);
  Serial.begin(9600);

  pinMode(redPin, OUTPUT);
  pinMode(greenPin, OUTPUT);
  pinMode(bluePin, OUTPUT);

  dallasTemperature.begin();
  dallasTemperature.getAddress(dallasThermometerAddress, 0);

  sevenSegment.begin(dinPin, clkPin, csPin);
  sevenSegment.shutdown(0, false);
  sevenSegment.clearDisplay(0);

  for (int i = 0; i < 8; i++)
  {
    if (i == 3)
    {
      i = 5;
    }

    sevenSegment.setChar(0, i, '-', false);
  }
  //clock.adjust(DateTime(2023, 11, 28, 16, 58, 0));
  display.begin();
}

void loop()
{
  refreshBrightness();
  int readLightLevel = analogRead(photoresistorPin);
  int currentLightLevelScaled = readLightLevel - minLightLevel;
  int maxLightLevelScaled = maxLightLevel - minLightLevel;
  byte rgbColorsAdjustedLight[3];

  if (readLightLevel > maxLightLevel)
  {
    // FULL POWER
    writeToRegister(0xFF);

    for (byte i = 0; i < 3; i++)
    {
      rgbColorsAdjustedLight[i] = rgbColors[i];
    }
  }
  else if (readLightLevel < minLightLevel)
  {
    // TURN OFF
    writeToRegister(B10000000);

    rgbColorsAdjustedLight[RED_INDEX] = rgbColorsAdjustedLight[GREEN_INDEX] = rgbColorsAdjustedLight[BLUE_INDEX] = 0;
  }
  else
  {
    // adjust
    byte newLight = map(currentLightLevelScaled, 0, maxLightLevelScaled, 0, 255);

    for (byte i = 0; i < 3; i++)
    {
      rgbColorsAdjustedLight[i] = map(rgbColors[i], 0, 255, 0, newLight);
    }

    byte lightLevelIndicator = map(currentLightLevelScaled, 0, maxLightLevelScaled, 5, 0);
    writeToRegister(B11000000 | (B00111111 >> lightLevelIndicator));
  }

  analogWrite(redPin, rgbColorsAdjustedLight[RED_INDEX]);
  analogWrite(greenPin, map(rgbColorsAdjustedLight[GREEN_INDEX], 0, 255, 0, MAX_GREEN_VALUE));
  analogWrite(bluePin, rgbColorsAdjustedLight[BLUE_INDEX]);

  digitalWrite(LED_BUILTIN, resetSevenSegmentOnNextUpdate);
  if (!resetSevenSegmentOnNextUpdate && (digitalRead(buttonResetSevenSegmentPin) == LOW))
  {
    resetSevenSegmentOnNextUpdate = true;
    sevenSegment.shutdown(0, true);
  }

  unsigned long now = millis();
  if (now > previousColorUpdate + colorUpdateInterval)
  {
    if (resetSevenSegmentOnNextUpdate)
    {
      resetSevenSegmentOnNextUpdate = false;
      sevenSegment.shutdown(0, false);
    }

    previousColorUpdate = now;
    updateColors();

    dallasTemperature.requestTemperatures();
    updateTemperaturesOnSevenSegment(clock.getTemperature(), dallasTemperature.getTempC(dallasThermometerAddress));

    showDate("DATETIME: ", clock.now());
    Serial.println(clock.getTemperature());
    Serial.println(dallasTemperature.getTempC(dallasThermometerAddress));
  }

  if (now > previousTimerUpdate + timerUpdateInterval)
  {
    previousTimerUpdate = now;
    char firstLine[30];
    char secondLine[30];
    DateTime now = clock.now();
    sprintf(firstLine, "%02d:%02d:%02d", now.hour(), now.minute(), now.second()); 
    sprintf(secondLine, "%02d/%02d/%02d", now.day(), now.month(), now.year()); 

    display.firstPage();
    do {
      display.setFont(u8g2_font_ncenB18_tr);
      display.drawStr(0,18, firstLine);
      display.setFont(u8g2_font_ncenB08_tr);
      display.drawStr(0,32, secondLine);
    } while ( display.nextPage() );
  }
}

void updateColors()
{
  switch (stage)
  {
  case 0:
    rgbColors[RED_INDEX]--;
    rgbColors[GREEN_INDEX]++;
    break;
  case 1:
    rgbColors[GREEN_INDEX]--;
    rgbColors[BLUE_INDEX]++;
    break;
  case 2:
    rgbColors[BLUE_INDEX]--;
    rgbColors[RED_INDEX]++;
    break;
  }

  byte numberOfEdgePoints = (rgbColors[RED_INDEX] == 0) + (rgbColors[GREEN_INDEX] == 0) + (rgbColors[BLUE_INDEX] == 0);
  if (numberOfEdgePoints == 2)
  {
    stage++;
  }
  if (stage == 3)
    stage = 0;

  Serial.print("Stage: ");
  Serial.print(stage);
  Serial.print("\tRED: ");
  Serial.print(rgbColors[RED_INDEX]);
  Serial.print("\tGREEN: ");
  Serial.print(rgbColors[GREEN_INDEX]);
  Serial.print("[MAPPED: ");
  Serial.print(map(rgbColors[GREEN_INDEX], 0, 255, 0, MAX_GREEN_VALUE));
  Serial.print("]\tBLUE: ");
  Serial.println(rgbColors[BLUE_INDEX]);
}

void writeToRegister(byte number)
{
  digitalWrite(storageRegisterClockInputPin, LOW);
  shiftOut(serialDataInputPin, shiftRegisterClockInputPin, LSBFIRST, number);
  digitalWrite(storageRegisterClockInputPin, HIGH);
}

void showDate(const char *txt, const DateTime &dt)
{
  Serial.print(txt);
  Serial.print(' ');
  Serial.print(dt.year(), DEC);
  Serial.print('/');
  Serial.print(dt.month(), DEC);
  Serial.print('/');
  Serial.print(dt.day(), DEC);
  Serial.print(' ');
  Serial.print(dt.hour(), DEC);
  Serial.print(':');
  Serial.print(dt.minute(), DEC);
  Serial.print(':');
  Serial.print(dt.second(), DEC);

  Serial.print(" = ");
  Serial.print(dt.unixtime());
  Serial.print("s / ");
  Serial.print(dt.unixtime() / 86400L);
  Serial.print("d since 1970");

  Serial.println();
}

void updateTemperaturesOnSevenSegment(float leftThermometer, float rightThermometer)
{
  // sevenSegment.clearDisplay(0);
  sevenSegmentInternalUpdateTemperature(leftThermometer, 7);
  sevenSegmentInternalUpdateTemperature(rightThermometer, 2);
  sevenSegment.setChar(0, 3, ' ', false);
  sevenSegment.setChar(0, 4, ' ', false);
}

void sevenSegmentInternalUpdateTemperature(float value, byte startIndex)
{
  byte digits = (byte)value;
  int valueTenTimes = (value * 10);
  byte decimals = valueTenTimes % 10;
  sevenSegment.setDigit(0, startIndex, digits / 10, false);
  sevenSegment.setDigit(0, startIndex - 1, digits % 10, true);
  sevenSegment.setDigit(0, startIndex - 2, decimals, false);
}

const int minBrightnessPotentiometerValue = 900;
const int maxBrightnessPotentiometerValue = 100;

const int minBrightnessSevenSegmentValue = 0;
const int maxBrightnessSevenSegmentValue = 15;

const int minBrightnessLedArrayValue = 254;
const int maxBrightnessLedArrayValue = 50;

byte currentSevenSegmentBrightnessLevel = 255;
byte currentLedArrayBrightnessLevel = 255;

void refreshBrightness()
{
  int potentiometer = analogRead(brightnessPotentiometerPin);
  if (potentiometer < maxBrightnessPotentiometerValue) potentiometer = maxBrightnessPotentiometerValue;
  if (potentiometer > minBrightnessPotentiometerValue) potentiometer = minBrightnessPotentiometerValue;

  byte newSevenSegmentBrightnessLevel = map(
    potentiometer, 
    minBrightnessPotentiometerValue, 
    maxBrightnessPotentiometerValue,
    minBrightnessSevenSegmentValue, 
    maxBrightnessSevenSegmentValue);

  byte newLedArrayBrightnessLevel = map(
    potentiometer, 
    minBrightnessPotentiometerValue, 
    maxBrightnessPotentiometerValue,
    minBrightnessLedArrayValue, 
    maxBrightnessLedArrayValue);

    if (newSevenSegmentBrightnessLevel != currentSevenSegmentBrightnessLevel)
    {
      currentSevenSegmentBrightnessLevel = newSevenSegmentBrightnessLevel;
      applyNewBrightnessToSevenSegment();
    }

    if (newLedArrayBrightnessLevel != currentLedArrayBrightnessLevel)
    {
      currentLedArrayBrightnessLevel = newLedArrayBrightnessLevel;
      applyNewBrightnessToLedArray();
    }
}

void applyNewBrightnessToSevenSegment()
{
  sevenSegment.setIntensity(0, currentSevenSegmentBrightnessLevel);
}

void applyNewBrightnessToLedArray()
{
  analogWrite(outputEnablePin, currentLedArrayBrightnessLevel);
}