#include <Arduino.h>
#include <M5Unified.h>
#include "MFRC522_I2C.h"
#include <FastLED.h>
#include <SPI.h>
#include <driver/i2s.h>
#include <math.h>

#define USE_ATOM_TF

#ifdef USE_ATOM_TF
#include <SD.h>
#endif

#define PIN_LED 38 // ATOM Ext's PortA
// for Echo S3R (+ Atomic TF)
#include <Wire.h>
#define PIN_MOSI 6
#define PIN_SCK  7
#define PIN_MISO 8
#define PIN_CS   5 // dummy
#define ES_LRCK_PIN 3
#define ES_BCLK_PIN 17
#define ES_DIN_PIN 48
#define ES_DOUT_PIN 4
// ES8311 I2C pins
#define ES_SDA_PIN 45
#define ES_SCL_PIN 0
#define PIN_4150_CTRL 18 // amp enable (requested name: 4150_CTRL)

#include "libhelix-mp3/mp3dec.h"

// Simple MP3 -> I2S player using libhelix mp3dec
int currentI2sRate = 44100;

static void configureI2S(int sampleRate){
	// reinstall I2S driver with requested sample rate (keeps MCLK disabled)
	i2s_driver_uninstall(I2S_NUM_0);
	i2s_config_t i2s_cfg = {};
	i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
	i2s_cfg.sample_rate = sampleRate;
	i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
	i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
	i2s_cfg.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S);
	i2s_cfg.intr_alloc_flags = 0;
	i2s_cfg.dma_buf_count = 8;
	i2s_cfg.dma_buf_len = 1024;
	i2s_cfg.use_apll = false;
	i2s_cfg.tx_desc_auto_clear = true;
	i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
	i2s_pin_config_t pin_cfg = {};
	pin_cfg.bck_io_num = ES_BCLK_PIN;
	pin_cfg.ws_io_num = ES_LRCK_PIN;
	pin_cfg.data_out_num = ES_DIN_PIN;
	pin_cfg.data_in_num = ES_DOUT_PIN;
	pin_cfg.mck_io_num = I2S_PIN_NO_CHANGE;
	i2s_set_pin(I2S_NUM_0, &pin_cfg);
	i2s_zero_dma_buffer(I2S_NUM_0);
}

bool playMp3(const char* path){
	File f = SD.open(path);
	if (!f) { printf("playMp3: open failed %s\n", path); return false; }

	uint32_t fsize = f.size();
	printf("playMp3: opening %s size=%u\n", path, (unsigned)fsize);

	HMP3Decoder dec = MP3InitDecoder();
	if (!dec) { printf("playMp3: decoder init failed\n"); f.close(); return false; }

	const int bufSize = 4096;
	uint8_t *buf = (uint8_t*)malloc(bufSize);
	if (!buf){ printf("playMp3: alloc fail\n"); MP3FreeDecoder(dec); f.close(); return false; }

	int bytesRead = f.read(buf, bufSize);
	// show first bytes for quick inspection (ID3 or sync)
	if (bytesRead > 8) {
		printf("playMp3: header bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
			   buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7]);
	}
	printf("playMp3: initial read %d bytes\n", bytesRead);
	unsigned char *readPtr = buf;
	int bytesLeft = bytesRead;
	int16_t *pcmOut = (int16_t*)malloc(1152 * 2 * sizeof(int16_t));
	if (!pcmOut) { printf("playMp3: pcmOut alloc fail\n"); free(buf); MP3FreeDecoder(dec); f.close(); return false; }
	MP3FrameInfo info;

	int frameCount = 0;
	while (bytesRead > 0){
		int offset = MP3FindSyncWord((unsigned char*)readPtr, bytesLeft);
		if (offset < 0){
			// need more data
			// move remaining to start
			if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
			int r = f.read(buf + bytesLeft, bufSize - bytesLeft);
			printf("playMp3: need more data, bytesLeft=%d read_more=%d\n", bytesLeft, r);
			if (r <= 0) break;
			bytesRead = bytesLeft + r;
			readPtr = buf;
			bytesLeft = bytesRead;
			continue;
		}
		readPtr += offset;
		bytesLeft -= offset;

		int err = MP3Decode(dec, &readPtr, &bytesLeft, pcmOut, 0);
		if (err == ERR_MP3_NONE){
			frameCount++;
			MP3GetLastFrameInfo(dec, &info);

			if (frameCount <= 5) {
				printf("playMp3: frame %d samprate=%d nChans=%d outputSamps=%d\n",
					   frameCount, info.samprate, info.nChans, info.outputSamps);
			}

			// If I2S sample rate mismatches MP3, reconfigure I2S
			if (info.samprate > 0 && info.samprate != currentI2sRate){
				configureI2S(info.samprate);
				currentI2sRate = info.samprate;
			}

				int outSamples = info.outputSamps; // number of 16-bit samples per channel or total depending on decoder
			if (info.nChans == 1){
				// expand mono -> stereo
				int samples = outSamples;
				int16_t *st = (int16_t*)malloc(samples * 2 * sizeof(int16_t));
				if (st){
					for (int i = 0; i < samples; ++i){
						int16_t s = pcmOut[i];
						st[i*2] = s;
						st[i*2+1] = s;
					}
					size_t written = 0;
							i2s_write(I2S_NUM_0, st, samples * 2 * sizeof(int16_t), &written, portMAX_DELAY);
							if (frameCount <= 5) printf("playMp3: i2s wrote %u bytes (mono->stereo)\n", (unsigned)written);
					free(st);
				}
			} else {
				// stereo or multi-channel: write as provided
					size_t written = 0;
						i2s_write(I2S_NUM_0, pcmOut, outSamples * sizeof(int16_t), &written, portMAX_DELAY);
						if (frameCount <= 5) printf("playMp3: i2s wrote %u bytes (stereo)\n", (unsigned)written);
			}
		} else if (err == ERR_MP3_INDATA_UNDERFLOW){
			printf("playMp3: decode underflow, bytesLeft=%d\n", bytesLeft);
			// refill buffer
			if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
			int r = f.read(buf + bytesLeft, bufSize - bytesLeft);
			printf("playMp3: refill read_more=%d\n", r);
			if (r <= 0) break;
			bytesRead = bytesLeft + r;
			readPtr = buf;
			bytesLeft = bytesRead;
		} else {
			printf("playMp3: decode error %d\n", err);
			// try to recover by searching next sync in remaining buffer
			if (bytesLeft > 1){
				int skip = MP3FindSyncWord((unsigned char*)readPtr + 1, bytesLeft - 1);
				if (skip >= 0){
					readPtr += skip + 1;
					bytesLeft -= skip + 1;
					continue;
				}
			}
			// otherwise refill and continue
			if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
			int r = f.read(buf + bytesLeft, bufSize - bytesLeft);
			if (r <= 0) {
				break;
			}
			bytesRead = bytesLeft + r;
			readPtr = buf;
			bytesLeft = bytesRead;
		}
	}

	free(buf);
	if (pcmOut) free(pcmOut);
	MP3FreeDecoder(dec);
	printf("playMp3: finished\n");
	f.close();
	return true;
}

MFRC522 mfrc522(0x28);
#define NTAG_DATA_PAGE 5

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

static uint32_t read_le_u32(File &f){
	uint8_t b[4];
	if (f.read(b,4) != 4) return 0;
	return (uint32_t)b[0] | ((uint32_t)b[1]<<8) | ((uint32_t)b[2]<<16) | ((uint32_t)b[3]<<24);
}

static uint16_t read_le_u16(File &f){
	uint8_t b[2];
	if (f.read(b,2) != 2) return 0;
	return (uint16_t)b[0] | ((uint16_t)b[1]<<8);
}

bool playWav(const char* path){
	File f = SD.open(path);
	if (!f){
		printf("playWav: open failed %s\n", path);
		return false;
	}
	// rewind to start for normal parsing (ensure parsing from file start)
	f.seek(0);
	// check RIFF
	char id[5] = {0};
	if (f.read((uint8_t*)id,4) != 4 || strncmp(id, "RIFF", 4) != 0){
		printf("playWav: not RIFF\n"); f.close(); return false;
	}
	// skip file size
	f.seek(f.position() + 4);
	// check WAVE
	if (f.read((uint8_t*)id,4) != 4 || strncmp(id, "WAVE", 4) != 0){
		printf("playWav: not WAVE\n"); f.close(); return false;
	}

	uint16_t audioFormat = 0;
	uint16_t numChannels = 0;
	uint32_t sampleRate = 0;
	uint16_t bitsPerSample = 0;
	uint32_t dataLen = 0;
	uint32_t dataPos = 0;

	// parse chunks
	while (f.position() + 8 <= f.size()){
		char chunkId[5] = {0};
		if (f.read((uint8_t*)chunkId,4) != 4) break;
		uint32_t chunkSize = read_le_u32(f);
		if (strncmp(chunkId, "fmt ",4) == 0){
			audioFormat = read_le_u16(f);
			numChannels = read_le_u16(f);
			sampleRate = read_le_u32(f);
			// skip byte rate and block align
			f.seek(f.position() + 6);
			bitsPerSample = read_le_u16(f);
			// skip remaining of fmt chunk if any
			if (chunkSize > 16) f.seek(f.position() + (chunkSize - 16));
		}
		else if (strncmp(chunkId, "data",4) == 0){
			dataLen = chunkSize;
			dataPos = f.position();
			break; // start of data
		}
		else{
			// skip unknown chunk
			f.seek(f.position() + chunkSize);
		}
	}

	if (dataPos == 0 || dataLen == 0){
		printf("playWav: no data chunk\n"); f.close(); return false;
	}
	if (audioFormat != 1){
		printf("playWav: unsupported audio format %d\n", audioFormat); f.close(); return false;
	}
	if (bitsPerSample != 16){
		printf("playWav: unsupported bitsPerSample %d\n", bitsPerSample); f.close(); return false;
	}

	// Note: sample rate is set when I2S was initialized (fixed to 44100).
	// Changing runtime clock varies between IDF versions; skip calling i2s_set_clk here.

	printf("playWav: format=%d channels=%d sampleRate=%d bitsPerSample=%d dataLen=%d\n",
		audioFormat, numChannels, sampleRate, bitsPerSample, dataLen);
	// play
	const size_t BUF_SAMPLES = 1024;
	const size_t READ_BYTES = BUF_SAMPLES * (bitsPerSample/8) * numChannels;
	uint8_t *readBuf = (uint8_t*)malloc(READ_BYTES);
	if (!readBuf){ printf("playWav: alloc fail\n"); f.close(); return false; }

	// temp buffer for stereo output when input is mono
	uint8_t *outBuf = nullptr;
	size_t outBytes = READ_BYTES;
	if (numChannels == 1){
		outBytes = READ_BYTES * 2;
		outBuf = (uint8_t*)malloc(outBytes);
		if (!outBuf){ free(readBuf); f.close(); printf("playWav: alloc out fail\n"); return false; }
	}

	f.seek(dataPos);
	uint32_t remaining = dataLen;
	while (remaining > 0){
		size_t toRead = (remaining > READ_BYTES) ? READ_BYTES : remaining;
		size_t r = f.read(readBuf, toRead);
		if (r == 0) break;
			if (numChannels == 1){
			// duplicate samples to stereo
			int16_t *src = (int16_t*)readBuf;
			int16_t *dst = (int16_t*)outBuf;
			size_t samples = r / 2; // number of 16-bit samples (mono)
			for (size_t i=0;i<samples;i++){
				int16_t s = src[i];
				dst[i*2] = s;
				dst[i*2+1] = s;
			}
				size_t written = 0;
				i2s_write(I2S_NUM_0, outBuf, samples * 4, &written, portMAX_DELAY);
			remaining -= r;
		} else {
					size_t written = 0;
				i2s_write(I2S_NUM_0, readBuf, r, &written, portMAX_DELAY);
			remaining -= r;
		}
	}

	if (outBuf) free(outBuf);
	free(readBuf);
	f.close();
	return true;
}

// Set ES8311 volume (0-100%). Writes DAC/LDAC/RDAC volume registers.
void setEs8311Volume(uint8_t percent){
	if (percent > 100) percent = 100;
	const uint8_t ES_ADDR = 0x18;
	uint8_t val = (uint8_t)((uint32_t)percent * 255 / 100);
	m5gfx::i2c::i2c_temporary_switcher_t tmp(1, ES_SDA_PIN, ES_SCL_PIN);
	// Write main DAC volume register and read back to verify
	auto do_write_and_read = [&](uint8_t reg){
		bool ok = M5.In_I2C.writeRegister(ES_ADDR, reg, &val, 1, 100000);
		if (!ok) {
			printf("setEs8311Volume: i2c write reg 0x%02X failed\n", reg);
			return;
		}
		printf("setEs8311Volume: i2c write reg 0x%02X ok\n", reg);
		uint8_t rb = 0;
		bool rok = M5.In_I2C.readRegister(ES_ADDR, reg, &rb, 1, 100000);
		if (rok) printf("setEs8311Volume: read reg 0x%02X = 0x%02X\n", reg, rb);
		else printf("setEs8311Volume: read reg 0x%02X failed\n", reg);
	};

	// Write DAC volume and LDAC/RDAC (no startup readback logging)
	{
		bool ok;
		ok = M5.In_I2C.writeRegister(ES_ADDR, 0x32, &val, 1, 100000);
		if (!ok) printf("setEs8311Volume: i2c write reg 0x32 failed\n");
		ok = M5.In_I2C.writeRegister(ES_ADDR, 0x26, &val, 1, 100000);
		if (!ok) printf("setEs8311Volume: i2c write reg 0x26 failed\n");
		ok = M5.In_I2C.writeRegister(ES_ADDR, 0x27, &val, 1, 100000);
		if (!ok) printf("setEs8311Volume: i2c write reg 0x27 failed\n");
	}
	tmp.restore();
}

void setup() {
	M5.begin();
	Wire.begin(2, 1); // ATOMS3 Lite/EchoS3R Grove
	FastLED.addLeds<NEOPIXEL, PIN_LED>(leds, NUM_LEDS); // ATOMS3 Ext.'s PortB (black)
	// clear all LEDs
	for (int i = 0; i < NUM_LEDS; i++) leds[i] = LED_BLACK; FastLED.show();

	mfrc522.PCD_Init(); // Init RFID2 Unit

//	for (int i = 0; i < 3; i++){printf("%d\n", i); delay(500);}
#ifdef USE_ATOM_TF
	// Initialize SPI (SCK, MISO, MOSI)
	SPI.begin(PIN_SCK, PIN_MISO, PIN_MOSI);
	// SD card on this module has CS fixed LOW; try to initialize without controlling a CS pin.
	// The SD library's overloads vary between versions; pass an invalid ssPin (0xFF) then SPI.
	if (!SD.begin(PIN_CS, SPI, 25000000)) {
		printf("SD init failed\n");
	}
	else {
		printf("SD initialized\n");
	}
#endif
	// initialize I2S for ES8311
	{
		i2s_config_t i2s_cfg = {};
		i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
		i2s_cfg.sample_rate = 44100;
		i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
		i2s_cfg	.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
		i2s_cfg.communication_format = (i2s_comm_format_t)(I2S_COMM_FORMAT_I2S);
		i2s_cfg.intr_alloc_flags = 0;
		i2s_cfg.dma_buf_count = 4;
		i2s_cfg.dma_buf_len = 512;
		i2s_cfg.use_apll = false;
		i2s_cfg.tx_desc_auto_clear = true;
		i2s_driver_install(I2S_NUM_0, &i2s_cfg, 0, NULL);
		i2s_pin_config_t pin_cfg = {};
		pin_cfg.bck_io_num = ES_BCLK_PIN;
		pin_cfg.ws_io_num = ES_LRCK_PIN;
		// Do not drive/provide external MCLK from the MCU; use BCLK (SCLK) as the codec clock source.
		// Explicitly leave MCLK pin unchanged so no external MCLK is used by the driver.
		// MCU is I2S TX (master) -> codec should receive data on its DIN pin.
		// Map MCU data_out to codec DSDIN, and MCU data_in to codec DSDOUT.
		pin_cfg.data_out_num = ES_DIN_PIN;
		pin_cfg.data_in_num = ES_DOUT_PIN;
		pin_cfg.mck_io_num = I2S_PIN_NO_CHANGE;
		i2s_set_pin(I2S_NUM_0, &pin_cfg);
		i2s_zero_dma_buffer(I2S_NUM_0);
	}

	// initialize ES8311 via I2C (power up / unmute / DAC enable)
	{
		const uint8_t ES_ADDR = 0x18;
		// Adjusted default to the Variant A settings that produced reliable loud output
		// - set moderate DAC/LDAC/RDAC (0x80) instead of extremes to avoid pops
		// - keep HP drive enabled (0x13) and leave DAC muted (0x25=0x00) until fade+unmute
		static constexpr const uint8_t es_enabled_bulk[] = {
			2, 0x00, 0x80,  // RESET/ CSM POWER ON
			2, 0x01, 0xB5,  // CLOCK_MANAGER/ MCLK=BCLK
			2, 0x02, 0x18,  // CLOCK_MANAGER/ MULT_PRE=3
			2, 0x0D, 0x01,  // SYSTEM/ Power up analog circuitry
			2, 0x12, 0x00,  // SYSTEM/ power-up DAC
			2, 0x13, 0x10,  // SYSTEM/ Enable output to HP drive
			2, 0x08, 0x00,  // I2S mode: slave(0) or master(0x01) depending on board (try 0)
			2, 0x23, 0x18,  // I2S format (16bit)
			2, 0x24, 0x00,  // I2S MCLK ratio (128)
			2, 0x25, 0x00,  // DAC muted at init (unmute later after fade)
			2, 0x26, 0x80,  // LDACVOL (mid)
			2, 0x27, 0x80,  // RDACVOL (mid)
			2, 0x28, 0x08,  // enable digital click free power up/down
			2, 0x32, 0x80,  // DAC volume (mid)
			2, 0x37, 0x08,  // Bypass DAC equalizer
			2, 0x38, 0x00,
			2, 0x39, 0xB8,
			2, 0x42, 0xB8,
			2, 0x43, 0x08,
			0
		};
		m5gfx::i2c::i2c_temporary_switcher_t backup_i2c_setting(1, ES_SDA_PIN, ES_SCL_PIN);
		const uint8_t *p = es_enabled_bulk;
		while (*p) {
			uint8_t len = *p++;
			uint8_t reg = *p++;
			bool ok = M5.In_I2C.writeRegister(ES_ADDR, reg, p, len - 1, 100000);
			if (!ok) {
				printf("ES8311: i2c write reg 0x%02X failed\n", reg);
			}
			p += len - 1;
			delay(5);
		}
		backup_i2c_setting.restore();
		// enable amp output (match M5Unified behavior)
		pinMode(PIN_4150_CTRL, OUTPUT);
		digitalWrite(PIN_4150_CTRL, HIGH);

		// Start muted, set minimal volume, then fade up and unmute
		setEs8311Volume(80);
/*
		{
			const uint8_t ES_ADDR_LOCAL = 0x18;
			m5gfx::i2c::i2c_temporary_switcher_t tmp2(1, ES_SDA_PIN, ES_SCL_PIN);
			uint8_t rb = 0;
			if (M5.In_I2C.readRegister(ES_ADDR_LOCAL, 0x00, &rb, 1, 100000)) printf("ES8311: reg 0x00 = 0x%02X\n", rb);
			else printf("ES8311: read reg 0x00 failed\n");
			if (M5.In_I2C.readRegister(ES_ADDR_LOCAL, 0x01, &rb, 1, 100000)) printf("ES8311: reg 0x01 = 0x%02X\n", rb);
			else printf("ES8311: read reg 0x01 failed\n");
			tmp2.restore();
		}
*/
		i2s_zero_dma_buffer(I2S_NUM_0);
	}

}

int i = 0;
unsigned long count = 0;

void loop()
{
	M5.update();
	if (M5.BtnA.wasClicked()){
		// Scan SD root for first .wav file and play it
#ifdef USE_ATOM_TF
//		if (SD.exists("/01.wav")){
		if (SD.exists("/02.mp3")){
			printf("playing...\n");
//			if (!playWav("/01.wav")) printf("failed\n");
			if (!playMp3("/002.mp3")) printf("failed\n");
		}
		else{
			printf("not Found\n");
		}
#else
		printf("SD playback not enabled (USE_ATOM_TF undefined)\n");
#endif
	}

/*
	printf("Mifare uid: %s / Ntag[%d] = %lu\n", readMifare_uid().c_str(), NTAG_DATA_PAGE, readNtag(NTAG_DATA_PAGE));
	if (i == 0) showLED(LED_RED, LED_BLACK, LED_BLACK, LED_BLACK);
	else if (i == 1) showLED(LED_BLACK, LED_GREEN, LED_BLACK, LED_BLACK);
	else if (i == 2) showLED(LED_BLACK, LED_BLACK, LED_BLUE, LED_BLACK);
	else showLED(LED_BLACK, LED_BLACK, LED_BLACK, LED_WHITE);
	i = (i + 1) % 4;
	delay(500);	
*/
}
