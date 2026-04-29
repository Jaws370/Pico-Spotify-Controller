#pragma once

#include <LittleFS.h>
#include <Adafruit_ST7789.h>
#include <ArduinoJson.h>

#include "config.h"

#include <Fonts/FreeMono9pt7b.h>

class Track {
  char name[64];
  char artist[64];
  char file_name[32];
  uint32_t file_len;


  void blur_and_darken(uint16_t* buf, float luminosity = 0.4f) {
    // loop over all pixels after the BLUR_START
    for (int y = BLUR_START; y < HEIGHT; y++) {
      for (int x = 0; x < WIDTH; x++) {
        int r = 0, g = 0, b = 0, count = 0;
        // loop through all pixels in BLUR_RADIUS
        for (int off_y = -BLUR_RADIUS; off_y <= BLUR_RADIUS; off_y++) {
          for (int off_x = -BLUR_RADIUS; off_x <= BLUR_RADIUS; off_x++) {
            int curr_x = x + off_x;
            int curr_y = y + off_y;
            // if in bounds (no overflow)
            if (curr_x >= 0 && curr_x < WIDTH && curr_y >= 0 && curr_y < HEIGHT) {
              // get the pixel
              uint16_t px = buf[curr_y * WIDTH + curr_x];
              r += (px & 0xF800) >> 11;
              g += (px & 0x07E0) >> 5;
              b += (px & 0x001F);
              count++;
            }
          }
        }
        // reassign pixel with blur and add darkening effect
        buf[y * WIDTH + x] = ((int)((r / count) * luminosity) << 11) | ((int)((g / count) * luminosity) << 5) | (int)((b / count) * luminosity);
      }
    }
  }

  void print_text(Adafruit_ST7789& scr, const char* text, int16_t x, int16_t y, int16_t max_width) {
    int16_t _x, _y;
    uint16_t w, _h;
    char buf[64];
    // copies string onto buffer (without overflow)
    strncpy(buf, text, sizeof(buf));
    // sets end of the buf to 0
    buf[sizeof(buf) - 1] = '\0';

    int len = strlen(buf);
    while (len > 0) {
      scr.getTextBounds(buf, x, y, &_x, &_y, &w, &_h);
      if (w <= max_width) break;
      buf[--len] = '\0';
    }

    scr.setCursor(x, y);
    scr.print(buf);
  }


public:
  Track()
    : file_len(0) {
    name[0] = '\0';
    artist[0] = '\0';
    file_name[0] = '\0';
  }

  void receive_meta(HardwareSerial& ser, uint32_t byte_count) {
    char jsonBuf[512];
    ser.readBytes(jsonBuf, byte_count);
    jsonBuf[byte_count] = '\0';

    JsonDocument meta;
    DeserializationError err = deserializeJson(meta, jsonBuf);
    if (err) return;

    save_meta((const char*)meta["title"], (const char*)meta["artist"]);
  }

  void save_meta(const char* name, const char* artist) {
    strncpy(this->name, name, sizeof(this->name) - 1);
    strncpy(this->artist, artist, sizeof(this->artist) - 1);
    this->name[sizeof(this->name) - 1] = '\0';
    this->artist[sizeof(this->artist) - 1] = '\0';
  }

  void print_meta(Adafruit_ST7789& scr) {
    scr.setTextColor(ST77XX_WHITE);
    scr.setFont(&FreeMono9pt7b);
    print_text(scr, name, 10, BLUR_START + 18, WIDTH - 20);
    print_text(scr, artist, 10, BLUR_START + 38, WIDTH - 20);
  }

  void save_data(const char* file_name, uint8_t* data, uint32_t len) {
    strncpy(this->file_name, file_name, sizeof(this->file_name) - 1);
    this->file_name[sizeof(this->file_name) - 1] = '\0';
    this->file_len = len;
    File f = LittleFS.open(file_name, "w");
    if (!f) return;
    f.write(data, len);
    f.close();
  }

  void receive_data(HardwareSerial& ser, uint32_t byte_count, uint16_t* buf) {
    // process and receive data
    uint32_t received = 0;  // int counting how far into transfer

    // load and write all data to screen
    while (received < byte_count) {
      uint32_t to_read = min((uint32_t)4096, byte_count - received);  // gets remaining byte count
      uint32_t n = ser.readBytes((uint8_t*)buf + received, to_read);  // reads bytes to buffer
      received += n;
    }

    // save to flash
    save_data("/current.raw", (uint8_t*)buf, byte_count);
  }

  void load_data(uint8_t* buf) {
    File f = LittleFS.open(file_name, "r");
    if (!f) return;
    f.read(buf, file_len);
    f.close();
  }

  void print_data(Adafruit_ST7789& scr, uint16_t* buf) {
    load_data((uint8_t*)buf);
    blur_and_darken(buf);

    scr.startWrite();
    scr.setAddrWindow(0, 0, WIDTH, HEIGHT);
    scr.writePixels(buf, WIDTH * HEIGHT);
    scr.endWrite();
  }
};
