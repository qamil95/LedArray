#include <Arduino.h>
#include "RTClib.h"
#include <Wire.h>
#include <Adafruit_SPIDevice.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LedControl_SW_SPI.h>

void writeToRegister(byte number);
void showDate(const char *txt, const DateTime &dt);
void updateColors();
void updateTemperaturesOnSevenSegment(float leftThermometer, float rightThermometer);
void sevenSegmentInternalUpdateTemperature(float value, byte decimals);