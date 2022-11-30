#include <Arduino.h>

// pins on 74595
byte shiftRegisterClockInputPin = 2;   // SCK
byte storageRegisterClockInputPin = 3; // RCK
byte serialDataInputPin = 4;           // SI
byte outputEnablePin = 5;              // OE

// pins on dallas temperature
byte dallasTemperaturePin = 6;

// defined_later  = 7;
byte GROUND_NOT_USE = 8;

// pins for RGB
byte greenPin = 9;
byte bluePin = 10;
byte redPin = 11;

// 7segment pins
byte dinPin = 7;
byte csPin = 12;
byte clkPin = 13;

byte photoresistorPin = A3;