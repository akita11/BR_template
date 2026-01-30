#include "NFC.h"
#include <Arduino.h>
#include <M5Unified.h>

MFRC522 mfrc522(0x28);

void nfcBegin(){
    mfrc522.PCD_Init();
}

String readMifare_uid(){
    String id = "";
    if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
        // no card
    }
    else{
        for (byte i = 0; i < mfrc522.uid.size; i++) {
            id += String(mfrc522.uid.uidByte[i], HEX);
        }
    }
    return(id);
}

unsigned long readNtag(byte page) {
    byte buffer[18];
    byte bufferSize = sizeof(buffer);
    if (mfrc522.MIFARE_Read(page, buffer, &bufferSize) != MFRC522::STATUS_OK) return 0;
    return ((unsigned long)buffer[3] << 24) | ((unsigned long)buffer[2] << 16) | ((unsigned long)buffer[1] << 8) | buffer[0];
}

bool writeNtag(byte page, unsigned long data) {
    byte buffer[4];
    buffer[0] = data & 0xFF;
    buffer[1] = (data >> 8) & 0xFF;
    buffer[2] = (data >> 16) & 0xFF;
    buffer[3] = (data >> 24) & 0xFF;
    return mfrc522.MIFARE_Ultralight_Write(page, buffer, 4) == MFRC522::STATUS_OK;
}
