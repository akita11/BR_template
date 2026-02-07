#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <unit_audioplayer.hpp>
#include <WiFi.h>
#include <esp_now.h>

AudioPlayerUnit audioplayer;

// ESP-NOW受信コールバック関数
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    printf("Received data from: %02X:%02X:%02X:%02X:%02X:%02X\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    printf("Data length: %d\n", len);
    printf("Data: ");
    for (int i = 0; i < len; i++) {
        printf("%02X ", incomingData[i]);
    }
    printf("\n");
}

void setup() {
    auto cfg = M5.config();
    cfg.serial_baudrate = 115200;
    M5.begin(cfg);
    M5.Power.setExtOutput(true);
    M5.Display.setTextSize(2);
    M5.Lcd.setTextFont(&fonts::efontCN_12);
    M5.Display.drawString("Unit AudioPlayer Test V0.1", 0, 0);

    int8_t port_a_pin1 = -1, port_a_pin2 = -1;
    port_a_pin1 = M5.getPin(m5::pin_name_t::port_a_pin1);
    port_a_pin2 = M5.getPin(m5::pin_name_t::port_a_pin2);
   printf("getPin: RX:%d TX:%d\n", port_a_pin1, port_a_pin2);
    while (!audioplayer.begin(&Serial1, port_a_pin1, port_a_pin2)) {
        printf("Unit AudioPlayer is not ready, please check the connection\n");
        delay(1000);
    }
    printf("Unit AudioPlayer is ready\n");
    audioplayer.setVolume(30);

    // ESP-NOW初期化
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        printf("Error initializing ESP-NOW\n");
        return;
    }
    printf("ESP-NOW initialized\n");

    // 受信コールバック登録
    esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
	M5.update();
	if (M5.BtnA.wasClicked()){
		printf("playing...\n");
    audioplayer.selectAudioNum(1);
    audioplayer.setPlayMode(AUDIO_PLAYER_MODE_SINGLE_STOP);
    audioplayer.playAudio();
	}
}
