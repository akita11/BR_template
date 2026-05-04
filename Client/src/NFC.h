#pragma once

#include <Arduino.h>
#include "MFRC522_I2C.h"

#define NTAG_DATA_PAGE 5

void nfcBegin();
void nfcPowerDown();
String readMifare_uid();
unsigned long readNtag(byte page);
bool writeNtag(byte page, unsigned long data);
