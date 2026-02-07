#pragma once

#include <Arduino.h>
#include "MFRC522_I2C.h"

#define NTAG_DATA_PAGE 5

void nfcBegin();
String readMifare_uid();
unsigned long readNtag(byte page);
bool writeNtag(byte page, unsigned long data);
