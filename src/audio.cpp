#include "audio.h"
#include <M5Unified.h>
#include <SPI.h>
#include <Wire.h>
#include <driver/i2s.h>

// Keep decoder include already in header

int currentI2sRate = 44100;

// reinstall I2S driver with requested sample rate (keeps MCLK disabled)
void configureI2S(int sampleRate){
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

// Initialize ES8311 codec (power up / mute / set volumes)
void initAudio(){
    // initialize I2S for ES8311 (moved here from setup)
    {
        i2s_config_t i2s_cfg = {};
        i2s_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
        i2s_cfg.sample_rate = 44100;
        i2s_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
        i2s_cfg.channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT;
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
        pin_cfg.data_out_num = ES_DIN_PIN;
        pin_cfg.data_in_num = ES_DOUT_PIN;
        pin_cfg.mck_io_num = I2S_PIN_NO_CHANGE;
        i2s_set_pin(I2S_NUM_0, &pin_cfg);
        i2s_zero_dma_buffer(I2S_NUM_0);
    }


    const uint8_t ES_ADDR = 0x18;
    // Adjusted default to the Variant A settings that produced reliable loud output
    static constexpr const uint8_t es_enabled_bulk[] = {
        2, 0x00, 0x80,
        2, 0x01, 0xB5,
        2, 0x02, 0x18,
        2, 0x0D, 0x01,
        2, 0x12, 0x00,
        2, 0x13, 0x10,
        2, 0x08, 0x00,
        2, 0x23, 0x18,
        2, 0x24, 0x00,
        2, 0x25, 0x00,
        2, 0x26, 0x80,
        2, 0x27, 0x80,
        2, 0x28, 0x08,
        2, 0x32, 0x80,
        2, 0x37, 0x08,
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

    i2s_zero_dma_buffer(I2S_NUM_0);
}

// Set ES8311 volume (0-100%). Writes DAC/LDAC/RDAC volume registers.
void setVolume(uint8_t percent){
    if (percent > 100) percent = 100;
    const uint8_t ES_ADDR = 0x18;
    uint8_t val = (uint8_t)((uint32_t)percent * 255 / 100);
    m5gfx::i2c::i2c_temporary_switcher_t tmp(1, ES_SDA_PIN, ES_SCL_PIN);

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

bool playWav(const char* path){
    // open file from SD or SPIFFS depending on build
    File f;
#ifdef USE_ATOM_TF
    f = SD.open(path);
#else
    f = SPIFFS.open(path);
#endif
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

bool playMp3(const char* path){
    // open file from SD or SPIFFS depending on build
    File f;
#ifdef USE_ATOM_TF
    f = SD.open(path);
#else
    f = SPIFFS.open(path);
#endif
    if (!f) { printf("playMp3: open failed %s\n", path); return false; }

    uint32_t fsize = f.size();
    

    HMP3Decoder dec = MP3InitDecoder();
    if (!dec) { printf("playMp3: decoder init failed\n"); f.close(); return false; }


    // Stream MP3 input reads with a larger refill buffer
    const int bufSize = 4096;
    uint8_t *buf = (uint8_t*)malloc(bufSize);
    if (!buf){ printf("playMp3: alloc fail\n"); MP3FreeDecoder(dec); f.close(); return false; }

    int bytesRead = f.read(buf, bufSize);
    if (bytesRead <= 0) { printf("playMp3: read failed\n"); free(buf); MP3FreeDecoder(dec); f.close(); return false; }
    // show first bytes for quick inspection (ID3 or sync)
    
    unsigned char *readPtr = buf;
    int bytesLeft = bytesRead;
    // If file starts with ID3v2 tag, skip the tag entirely (uses synchsafe size in bytes 6-9)
    if (bytesRead >= 10 && buf[0] == 'I' && buf[1] == 'D' && buf[2] == '3'){
        uint32_t id3size = ((uint32_t)(buf[6] & 0x7F) << 21) | ((uint32_t)(buf[7] & 0x7F) << 14) |
                           ((uint32_t)(buf[8] & 0x7F) << 7) | (uint32_t)(buf[9] & 0x7F);
        uint32_t skip = id3size + 10U;
        // sanity clamp: don't skip more than half the file or an arbitrary cap
        if (skip > fsize/2 || skip > 65536) {
            skip = 10;
        }
        if (skip >= fsize) {
            printf("playMp3: ID3 skip beyond EOF\n"); free(buf); MP3FreeDecoder(dec); f.close(); return false;
        }
        
        // seek to after the tag and refill buffer
        f.seek((uint32_t)skip);
        bytesRead = f.read(buf, bufSize);
        if (bytesRead <= 0) { free(buf); MP3FreeDecoder(dec); f.close(); return false; }
        readPtr = buf;
        bytesLeft = bytesRead;
    }
    // allocate PCM output buffer and a reusable stereo expansion buffer
    const int PCM_SAMPLES_ALLOC = 1152 * 2; // decoder may output up to this many 16-bit samples
    int16_t *pcmOut = (int16_t*)malloc(PCM_SAMPLES_ALLOC * sizeof(int16_t));
    if (!pcmOut) { free(buf); MP3FreeDecoder(dec); f.close(); return false; }
    int16_t *stereoBuf = (int16_t*)malloc(PCM_SAMPLES_ALLOC * 2 * sizeof(int16_t));
    if (!stereoBuf) { free(pcmOut); free(buf); MP3FreeDecoder(dec); f.close(); return false; }
    MP3FrameInfo info;

    bool eof = false;
    int frameCount = 0;
    int decodeErrCount = 0;
    const int DECODE_ERR_LIMIT = 16;
    while (true){
        int offset = MP3FindSyncWord((unsigned char*)readPtr, bytesLeft);
        if (offset < 0){
            // need more data; refill unless EOF
            if (eof) break;
            if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
            // clamp bytesLeft
            if (bytesLeft < 0) bytesLeft = 0;
            if (bytesLeft > bufSize) bytesLeft = bufSize;
            int avail = bufSize - bytesLeft;
            if (avail <= 0) { bytesLeft = 0; avail = bufSize; }
            int r = f.read(buf + bytesLeft, avail);
            if (r <= 0) {
                eof = true;
                if (bytesLeft == 0) break;
            } else {
                bytesLeft += r;
            }
            readPtr = buf;
            continue;
        }
        readPtr += offset;
        bytesLeft -= offset;

        int err = MP3Decode(dec, &readPtr, &bytesLeft, pcmOut, 0);
        if (err == ERR_MP3_NONE){
            frameCount++;
            MP3GetLastFrameInfo(dec, &info);

            if (info.samprate == 0 || info.nChans == 0 || info.outputSamps == 0) {
                // treat as decode error and try to resync
                decodeErrCount++;
                if (decodeErrCount > DECODE_ERR_LIMIT) break;
                // search next sync
                if (bytesLeft > 1) {
                    int skip = MP3FindSyncWord((unsigned char*)readPtr + 1, bytesLeft - 1);
                    if (skip >= 0) {
                        readPtr += skip + 1;
                        bytesLeft -= skip + 1;
                        continue;
                    }
                }
                // otherwise attempt refill
                if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
                int r2 = f.read(buf + bytesLeft, bufSize - bytesLeft);
                if (r2 > 0) { bytesLeft += r2; readPtr = buf; continue; }
                break;
            }

            // If I2S sample rate mismatches MP3, reconfigure I2S
            if (info.samprate > 0 && info.samprate != currentI2sRate){
                configureI2S(info.samprate);
                currentI2sRate = info.samprate;
            }
            int outSamples = info.outputSamps; // number of 16-bit samples (total)
            if (outSamples <= 0 || info.nChans <= 0) {
                // unexpected; count as decode error and try to continue
                decodeErrCount++;
                if (decodeErrCount > DECODE_ERR_LIMIT) break;
                continue;
            }
            decodeErrCount = 0;
            if (info.nChans == 1){
                // expand mono -> stereo using preallocated buffer
                int samples = outSamples; // samples per channel or total depending on decoder; assume total here
                if (samples > PCM_SAMPLES_ALLOC) samples = PCM_SAMPLES_ALLOC;
                for (int i = 0; i < samples; ++i){
                    int16_t s = pcmOut[i];
                    stereoBuf[i*2] = s;
                    stereoBuf[i*2+1] = s;
                }
                size_t written = 0;
                i2s_write(I2S_NUM_0, stereoBuf, samples * 2 * sizeof(int16_t), &written, portMAX_DELAY);
                
            } else {
                // stereo: write as provided (outSamples is total number of 16-bit samples)
                if (outSamples > PCM_SAMPLES_ALLOC) outSamples = PCM_SAMPLES_ALLOC;
                size_t written = 0;
                i2s_write(I2S_NUM_0, pcmOut, outSamples * sizeof(int16_t), &written, portMAX_DELAY);
                
            }
        } else if (err == ERR_MP3_INDATA_UNDERFLOW){
            // decoder requests more input; refill buffer (EOF-aware)
            if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
            if (bytesLeft < 0) bytesLeft = 0;
            if (bytesLeft > bufSize) bytesLeft = bufSize;
            int avail = bufSize - bytesLeft;
            if (avail <= 0) { bytesLeft = 0; avail = bufSize; }
            int r = f.read(buf + bytesLeft, avail);
            if (r <= 0) {
                eof = true;
                if (bytesLeft == 0) break;
            } else {
                bytesLeft += r;
            }
            readPtr = buf;
            continue;
        } else {
            
            decodeErrCount++;
            if (decodeErrCount > DECODE_ERR_LIMIT) break;
            // if EOF and not enough data, stop
            if (eof && bytesLeft < 4) break;
            // try to find explicit 0xFF Ex sync in remaining buffer
            int s = -1;
            for (int i = 0; i + 1 < bytesLeft; ++i){
                if (readPtr[i] == 0xFF && ((readPtr[i+1] & 0xE0) == 0xE0)) { s = i; break; }
            }
            if (s >= 0){
                readPtr += s;
                bytesLeft -= s;
                continue;
            }
            // otherwise refill and continue
            if (bytesLeft > 0) memmove(buf, readPtr, bytesLeft);
            if (bytesLeft < 0) bytesLeft = 0;
            if (bytesLeft > bufSize) bytesLeft = bufSize;
            int avail2 = bufSize - bytesLeft;
            if (avail2 <= 0) { bytesLeft = 0; avail2 = bufSize; }
            int r = f.read(buf + bytesLeft, avail2);
            if (r <= 0) {
                eof = true;
                if (bytesLeft == 0) break;
                // if we have some leftover, allow loop to attempt decode once more
            } else {
                bytesRead = bytesLeft + r;
                readPtr = buf;
                bytesLeft = bytesRead;
            }
        }
    }

    free(buf);
    if (pcmOut) free(pcmOut);
    if (stereoBuf) free(stereoBuf);
    MP3FreeDecoder(dec);
    printf("playMp3: finished\n");
    f.close();
    return true;
}
