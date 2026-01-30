#pragma once

#include <Arduino.h>
#include <M5Unified.h>
#ifdef USE_SD
#include <SD.h>
#else
#include <SPIFFS.h>
#endif
#include <driver/i2s.h>
#include "libhelix-mp3/mp3dec.h"
#include <Wire.h>

// for Echo S3R
#define ES_SDA_PIN 45
#define ES_SCL_PIN 0
#define PIN_4150_CTRL 18 // amp enable (requested name: 4150_CTRL)
#define ES_LRCK_PIN 3
#define ES_BCLK_PIN 17
#define ES_DIN_PIN 48
#define ES_DOUT_PIN 4

// audio API
void configureI2S(int sampleRate);
void initAudio();
bool playMp3(const char* path);
bool playWav(const char* path);
void setVolume(uint8_t percent);
