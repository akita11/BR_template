#include <Arduino.h>
#include "MFRC522_I2C.h"
#include "NFC.h"
#include <FastLED.h>
#include <math.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_sleep.h>

// Hardware Configuration
// - ESP32-C3-MINI or WROOM
// - RFID2 Unit @ IO2/3 (SDA/SCL)
// - LED Tape @ IO6
// Ntag: use page 5 to store ID

#define PIN_SDA 2
#define PIN_SCL 3
#define PIN_LED 7
#define PIN_USBIN 1 // divided by 2
#define PIN_VBAT  4 // divided by 2
#define PIN_SW  6

#define DEVICE_ID 0x01234567
#define NUM_LEDS 2

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

void showLED(CRGB c0, CRGB c1) {
	if (c0 != LED_SKIP) leds[0] = c0;
	if (c1 != LED_SKIP) leds[1] = c1;
	FastLED.show();
}

bool isCharging(){
	int usb_in = analogReadMilliVolts(PIN_USBIN);
	if(usb_in > 2000) return(true); // USB電圧が4V以上なら充電中とみなす
	else return(false);
}

float getBatteryVoltage()
{
	int vbat = analogReadMilliVolts(PIN_VBAT);
	return(vbat * 2 / 1000.0); // [V]
}

void checkCharging() {
	// if USB connected for charge -> enter low power mode until USB disconnected (or SW pressed)
	if (isCharging()) {
		showLED(LED_BLACK, LED_BLACK);
		nfcPowerDown();
		esp_now_deinit();       // ESP-NOW を先に停止してから WiFi を落とす
		WiFi.mode(WIFI_OFF);
		// use deep sleep during charging
		esp_sleep_enable_timer_wakeup(3000000ULL); // 3秒タイマー
		while (isCharging() && digitalRead(PIN_SW) == HIGH) {
			esp_light_sleep_start();
			showLED(CRGB(10, 10, 10), LED_BLACK); // flash LED whte during charging
			delay(1);
			showLED(LED_BLACK, LED_BLACK);
		}
		/*
		// don't use deep sleep during charging
		while (isCharging() && digitalRead(PIN_SW) == HIGH) {
			showLED(CRGB(10, 10, 10), LED_BLACK);
			delay(1);
			showLED(LED_BLACK, LED_BLACK);
			delay(3000);
		}
		*/

		// end of charging (or SW pressed), resume normal operation
		nfcBegin();
		WiFi.mode(WIFI_STA);
		if (esp_now_init() != ESP_OK) {
			printf("Error re-initializing ESP-NOW\n");
			showLED(LED_RED, LED_RED);
			return;
		}
		esp_now_register_send_cb(OnDataSent);
		if (esp_now_add_peer(&peerInfo) != ESP_OK) {
			showLED(LED_RED, LED_BLACK);
			printf("Failed to re-add peer\n");
			return;
		}
		showLED(LED_GREEN, LED_GREEN);
		delay(1000);
	}
}


void setup() {
	pinMode(PIN_SW, INPUT_PULLUP);
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
	if (digitalRead(PIN_SW) == HIGH) // SW pressed -> skip charging check
		checkCharging();

	// read NFC tag and send data via ESP-NOW
	String Ntag_uuid = readMifare_uid();
	int Ntag_ID;
	printf("%d\n", Ntag_uuid.length());
	if (Ntag_uuid.length() > 0){
		Ntag_ID = readNtag(NTAG_DATA_PAGE);
		printf("Mifare uid: %s / Ntag_ID = %lu, sending...", Ntag_uuid.c_str(), Ntag_ID);
		myData.device_id = DEVICE_ID;
		myData.ntag_id = Ntag_ID;
		showLED(LED_BLUE, LED_BLUE);
		delay(500);
		showLED(LED_GREEN, LED_GREEN);
		esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
		if (result == ESP_OK) {
			printf("OK\n");
		} else {
			printf("Error\n");
		}
	}
	delay(500);	
}
