#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include <FastLED.h>

#define PIN_LED 7 // ATOM Ext's PortB

MFRC522 mfrc522(0x28);

#define NUM_LEDS 15
CRGB leds[NUM_LEDS];

#define LED_RED CRGB(50, 0, 0)
#define LED_GREEN CRGB(0, 50, 0)
#define LED_BLUE CRGB(0, 0, 50)
#define LED_BLACK CRGB(0, 0, 0)
#define LED_WHITE CRGB(50, 50, 50)
#define LED_SKIP CRGB(255, 255, 255) // special value to skip LED update

void showLED(CRGB c0, CRGB c1, CRGB c2, CRGB c3) {
	if (c0 != LED_SKIP) leds[0] = c0;
	if (c1 != LED_SKIP) leds[1] = c1;
	if (c2 != LED_SKIP) leds[2] = c2;
	if (c3 != LED_SKIP) leds[3] = c3;
	FastLED.show();
}

String readMifare_uid(){
	String id = "";
	if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
		//printf("no card\n");
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

void setup() {
	M5.begin();
	Wire.begin(2, 1); // ATOMS3 Lite's Grove
	FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, NUM_LEDS); // ATOMS3 Ext.'s PortB (black)
	mfrc522.PCD_Init(); // Init RFID2 Unit

	// clear all LEDs
	for (int i = 0; i < NUM_LEDS; i++) leds[i] = LED_BLACK; FastLED.show();
}

int i = 0;

void loop()
{
	printf("Mifare uid: %s\n", readMifare_uid().c_str());
	if (i == 0) showLED(LED_RED, LED_BLACK, LED_BLACK, LED_BLACK);
	else if (i == 1) showLED(LED_BLACK, LED_GREEN, LED_BLACK, LED_BLACK);
	else if (i == 2) showLED(LED_BLACK, LED_BLACK, LED_BLUE, LED_BLACK);
	else showLED(LED_BLACK, LED_BLACK, LED_BLACK, LED_WHITE);
	i = (i + 1) % 4;
	delay(500);	
}
