#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include "NFC.h"
#include <FastLED.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <math.h>
#include "audio.h"

//#define USE_SD // use microSD for wav/mp3 storage

#ifdef USE_SD
#include <SD.h>
#define PIN_MOSI 6
#define PIN_SCK  7
#define PIN_MISO 8
#define PIN_CS   5 // dummy
#endif
#include <SPIFFS.h>

#define PIN_LED 38 // ATOM Ext's PortA

#include "libhelix-mp3/mp3dec.h"

#define NUM_LEDS 4
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

void setup() {
	M5.begin();
	Wire.begin(2, 1); // ATOMS3Lite/choS3R Grove

	FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, NUM_LEDS); // ATOMS3 Ext.'s PortB (black)
	// clear all LEDs
	for (int i = 0; i < NUM_LEDS; i++) leds[i] = LED_BLACK; FastLED.show();

	nfcBegin(); // Init RFID/NFC unit

#ifdef USE_SD
	// Initialize SPI (SCK, MISO, MOSI)
	SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
	if (!SD.begin(PIN_CS, SPI, 25000000)) printf("SD init failed\n");
#else
	if (!SPIFFS.begin(true)) printf("SPIFFS mount failed\n");
#endif
	initAudio();
	setVolume(80);

}

int i = 0;
unsigned long count = 0;

void loop()
{
	M5.update();
	if (M5.BtnA.wasClicked()){
		printf("playing...\n");
//		if (!playWav("/01.wav")) printf("failed\n");
		if (!playMp3("/02.mp3")) printf("failed\n");
	}

	printf("Mifare uid: %s / Ntag[%d] = %lu\n", readMifare_uid().c_str(), NTAG_DATA_PAGE, readNtag(NTAG_DATA_PAGE));
	if (i == 0) showLED(LED_RED, LED_BLACK, LED_BLACK, LED_BLACK);
	else if (i == 1) showLED(LED_BLACK, LED_GREEN, LED_BLACK, LED_BLACK);
	else if (i == 2) showLED(LED_BLACK, LED_BLACK, LED_BLUE, LED_BLACK);
	else showLED(LED_BLACK, LED_BLACK, LED_BLACK, LED_WHITE);
	i = (i + 1) % 4;
	delay(500);	
}
