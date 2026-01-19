#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include <FastLED.h>

#define PIN_LED 7 // ATOM Ext's PortB

MFRC522 mfrc522(0x28);

#define NUM_LEDS 15
CRGB leds[NUM_LEDS];

void showLED(CRGB c0, CRGB c1, CRGB c2, CRGB c3) {
  leds[0] = c0;
  leds[1] = c1;
  leds[2] = c2;
  leds[3] = c3;
  FastLED.show();
}

String getCardID(){
	String id = "";
	if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
		//printf("no card\n");
	}
	else{
		for (byte i = 0; i < mfrc522.uid.size; i++) {
			id += String(mfrc522.uid.uidByte[i], HEX);
			//printf("%02x ", mfrc522.uid.uidByte[i]);
		}
		//printf("\n");
	}
//	printf("card ID: %s (%d)\n", id.c_str(), id.length());
	return(id);
}

void setup() {
	M5.begin();
	Wire.begin(2, 1); // ATOMS3 Lite's Grove
	FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, NUM_LEDS); // ATOMS3 Ext.'s PortB (black)
	mfrc522.PCD_Init(); // Init RFID2 Unit
	for (int i = 0; i < NUM_LEDS; i++) leds[i] = CRGB::Black; FastLED.show();
}

int i = 0;
void loop()
{
	printf("getCardID: %s\n", getCardID().c_str());
	if (i == 0) showLED(CRGB::Red, 0, 0, 0);
	else if (i == 1) showLED(0, CRGB::Green, 0, 0);
	else if (i == 2) showLED(0, 0, CRGB::Blue, 0);
	else showLED(0, 0, 0, CRGB::White);
	i = (i + 1) % 4;
	delay(500);	
}
