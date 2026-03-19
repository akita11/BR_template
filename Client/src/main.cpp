#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include "NFC.h"
#include <FastLED.h>
#include <math.h>
#include <WiFi.h>
#include <esp_now.h>

// Hardware Configuration for Prototyping:
// - ESP32-C3-MINI or WROOM
// - RFID2 Unit @ IO2/3 (SDA/SCL)
// - LED Tape @ IO4
// Ntag: use page 5 to store ID

#define PIN_SDA 2
#define PIN_SCL 3
#define PIN_LED 4

#define DEVICE_ID 0x01234567
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
//    printf("\r\nLast Packet Send Status:\t");
//    printf(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success\n" : "Delivery Fail\n");
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
	Wire.begin(PIN_SDA, PIN_SCL); // 

	FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, NUM_LEDS);
	// clear all LEDs
	for (int i = 0; i < NUM_LEDS; i++) leds[i] = LED_BLACK; FastLED.show();

	nfcBegin(); // Init RFID/NFC unit

	// Init ESP-NOW
	WiFi.mode(WIFI_STA);
	if (esp_now_init() != ESP_OK) {
		printf("Error initializing ESP-NOW\n");
		return;
	}
	esp_now_register_send_cb(OnDataSent);
	
	// Register peer
	memcpy(peerInfo.peer_addr, broadcastAddress, 6);
	peerInfo.channel = 0;  
	peerInfo.encrypt = false;
	
	// Add peer        
	if (esp_now_add_peer(&peerInfo) != ESP_OK){
		printf("Failed to add peer\n");
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
	String Ntag_uuid = readMifare_uid();
	int Ntag_ID;
	printf("%d\n", Ntag_uuid.length());
	if (Ntag_uuid.length() > 0){
		Ntag_ID = readNtag(NTAG_DATA_PAGE);
		printf("Mifare uid: %s / Ntag_ID = %lu, sending...", Ntag_uuid.c_str(), Ntag_ID);
		myData.device_id = DEVICE_ID;
		myData.ntag_id = Ntag_ID;
		esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
		if (result == ESP_OK) {
			printf("OK\n");
		} else {
			printf("Error\n");
		}
	}
	if (i == 0) showLED(LED_RED, LED_BLACK, LED_BLACK, LED_BLACK);
	else if (i == 1) showLED(LED_BLACK, LED_GREEN, LED_BLACK, LED_BLACK);
	else if (i == 2) showLED(LED_BLACK, LED_BLACK, LED_BLUE, LED_BLACK);
	else showLED(LED_BLACK, LED_BLACK, LED_BLACK, LED_WHITE);
	i = (i + 1) % 4;
	delay(500);	
}
