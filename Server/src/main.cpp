#include <Arduino.h>
#include <M5Unified.h>
#include <FastLED.h>
#include <unit_audioplayer.hpp>
#include <WiFi.h>
#include <esp_now.h>

// Hardware Configuraton for Prototyping:
// - ATOMS3Lite
// - AudioPlayer Unit @ ATOMS3's Grove (with uSD)

AudioPlayerUnit audioplayer;

// ESP-NOW受信コールバック関数
void OnDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len)
{
	printf("Received data (len=%d) from: %02X:%02X:%02X:%02X:%02X:%02X : ", len, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	for (int i = 0; i < len; i++)
	{
		printf("%02X ", incomingData[i]);
	}
	printf("\n");

	if (len == 8)
	{
		// 先頭4バイト: 送信元ID (リトルエンディアン)
		uint32_t senderID = (incomingData[3] << 24) | (incomingData[2] << 16) | (incomingData[1] << 8) | incomingData[0];
		// 次の4バイト: タグID (リトルエンディアン)
		uint32_t tagID = (incomingData[7] << 24) | (incomingData[6] << 16) | (incomingData[5] << 8) | incomingData[4];
		printf("Sender ID: %08x, TagID: %d\n", senderID, tagID);

		// for testing: play MP3 in uSD of tagID 

		printf("playing %d...\n", tagID);
		audioplayer.selectAudioNum(tagID);
		audioplayer.playAudio();

	}
}

void setup()
{
	auto cfg = M5.config();
	cfg.serial_baudrate = 115200;
	M5.begin(cfg);

	int8_t port_a_pin1 = -1, port_a_pin2 = -1;
	port_a_pin1 = M5.getPin(m5::pin_name_t::port_a_pin1);
	port_a_pin2 = M5.getPin(m5::pin_name_t::port_a_pin2);
	//printf("getPin: RX:%d TX:%d\n", port_a_pin1, port_a_pin2);
	while (!audioplayer.begin(&Serial1, port_a_pin1, port_a_pin2))
	{
		printf("Unit AudioPlayer is not ready, please check the connection\n");
		delay(1000);
	}
	audioplayer.setPlayMode(AUDIO_PLAYER_MODE_SINGLE_STOP);
	audioplayer.setVolume(30);

	// ESP-NOW初期化
	WiFi.mode(WIFI_STA);
	if (esp_now_init() != ESP_OK)
	{
		printf("Error initializing ESP-NOW\n");
		return;
	}
	// 受信コールバック登録
	esp_now_register_recv_cb(OnDataRecv);
}

void loop()
{
	M5.update();
	if (M5.BtnA.wasClicked())
	{
		printf("playing...\n");
		audioplayer.selectAudioNum(1);
		audioplayer.setPlayMode(AUDIO_PLAYER_MODE_SINGLE_STOP);
		audioplayer.playAudio();
	}
}
