#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include "NFC.h"
#include <FastLED.h>
#include <math.h>
#include <WiFi.h>
#include <esp_now.h>

#define DEVICE_ID 0x01234567

#define PIN_LED 38 // ATOM Ext's PortA

#define NUM_LEDS 4
CRGB leds[NUM_LEDS];

#define LED_RED CRGB(50, 0, 0)
#define LED_GREEN CRGB(0, 50, 0)
#define LED_BLUE CRGB(0, 0, 50)
#define LED_BLACK CRGB(0, 0, 0)
#define LED_WHITE CRGB(50, 50, 50)
#define LED_SKIP CRGB(255, 255, 255) // special value to skip LED update

typedef struct struct_message {
    uint32_t device_id;
    uint32_t ntag_id;
} struct_message;

struct_message myData;
uint8_t broadcastAddress[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
    Serial.print("\r\nLast Packet Send Status:\t");
    Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

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

	// Init ESP-NOW
	WiFi.mode(WIFI_STA);
	if (esp_now_init() != ESP_OK) {
		Serial.println("Error initializing ESP-NOW");
		return;
	}
	esp_now_register_send_cb(OnDataSent);
	
	// Register peer
	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
	peerInfo.channel = 0;  
	peerInfo.encrypt = false;
	
	// Add peer        
	if (esp_now_add_peer(&peerInfo) != ESP_OK){
		Serial.println("Failed to add peer");
		return;
	}
}

int i = 0;
unsigned long count = 0;

void loop()
{
	M5.update();
	if (M5.BtnA.wasClicked()){
	}

	int Ntag_ID = readNtag(NTAG_DATA_PAGE);
	printf("Mifare uid: %s / Ntag[%d] = %lu\n", readMifare_uid().c_str(), NTAG_DATA_PAGE, Ntag_ID);
	if (Ntag_ID != 0) {
		myData.device_id = DEVICE_ID;
		myData.ntag_id = Ntag_ID;
		esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
		if (result == ESP_OK) {
			Serial.println("Sent with success");
		} else {
			Serial.println("Error sending the data");
		}
	}
	if (i == 0) showLED(LED_RED, LED_BLACK, LED_BLACK, LED_BLACK);
	else if (i == 1) showLED(LED_BLACK, LED_GREEN, LED_BLACK, LED_BLACK);
	else if (i == 2) showLED(LED_BLACK, LED_BLACK, LED_BLUE, LED_BLACK);
	else showLED(LED_BLACK, LED_BLACK, LED_BLACK, LED_WHITE);
	i = (i + 1) % 4;
	delay(500);	
}
