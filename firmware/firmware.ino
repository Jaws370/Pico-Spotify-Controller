#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <SPI.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include <Fonts/FreeMono9pt7b.h>

// display
#define TFT_CS 22
#define TFT_DC 21
#define TFT_RST 20
#define TFT_SPI_HZ 62500000
#define WIDTH 240
#define HEIGHT 240

// blur
#define BLUR_START ((HEIGHT / 5) * 4)
#define BLUR_RADIUS 3

// buttons
#define ENC_CL 10
#define ENC_DT 11
#define ENC_SW 12
#define BTN_1 13
#define BTN_2 14
#define BTN_3 15

// buffer
#define IMG_BUF_SIZE (WIDTH * HEIGHT)

// protocol
#define SERIAL_BAUD 2000000
#define UPDATE_REQ 0x04
#define READY_SIG 0x05
#define PY_READY_SIG 0x06
#define FULL_READY_SIG 0x07
#define ACK 0x08
#define CMD_SIG 0x09

// commands
#define CMD_PREV 0x01
#define CMD_PLAY_PAUSE 0x02
#define CMD_NEXT 0x03
#define CMD_VOL_UP 0x04
#define CMD_VOL_DOWN 0x05
#define CMD_MUTE 0x06

Adafruit_ST7789 scr = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static uint16_t imgBuf[IMG_BUF_SIZE];
static JsonDocument doc;

void save_to_flash(const char* filename, uint8_t* data, uint32_t len) {
  File f = LittleFS.open(filename, "w");
  if (!f) { return; }
  f.write(data, len);
  f.close();
}

void blur_and_darken(uint16_t* buf, float luminosity = 0.4f) {
  // create temp buf for accurate blurring
  const uint16_t* temp_buf = buf;
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
            uint16_t px = temp_buf[curr_y * WIDTH + curr_x];
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

void setup() {
  Serial.begin(SERIAL_BAUD);
  LittleFS.begin();

  // init screen settings
  scr.init(WIDTH, HEIGHT);       // scr size
  scr.setSPISpeed(TFT_SPI_HZ);   // transfer speeds
  scr.setRotation(3);            // scr rotation
  scr.fillScreen(ST77XX_BLACK);  // black background

  // send out pico ready signal until python ready
  while (true) {
    Serial.write(READY_SIG);
    Serial.flush();
    delay(100);
    if (Serial.available() > 0 && Serial.read() == PY_READY_SIG) break;
  }

  Serial.write(FULL_READY_SIG);  // fully ready signal
  Serial.flush();
}

void loop() {
  // wait for python ready signal for next data transfer
  while (true) {
    delay(100);
    if (Serial.available() > 0 && Serial.read() == PY_READY_SIG) break;
  }

  // get transfer length
  uint32_t byte_count;
  Serial.readBytes((uint8_t*)&byte_count, 4);

  // get signal for data transfer type
  uint8_t dt_sig[2];
  Serial.readBytes(dt_sig, 2);

  // do data transfer ops
  if (dt_sig[0] == 'I' && dt_sig[1] == 'M') {
    // case image transfer
    // send ack
    Serial.write(ACK);
    Serial.flush();

    // process and receive data
    uint32_t received = 0;  // int counting how far into transfer

    // load and write all data to screen
    while (received < byte_count) {
      uint32_t to_read = min((uint32_t)4096, byte_count - received);        // gets remaining byte count
      uint32_t n = Serial.readBytes((uint8_t*)imgBuf + received, to_read);  // reads bytes to buffer
      received += n;
    }

    // save to flash
    save_to_flash("/current.raw", (uint8_t*)imgBuf, byte_count);

    // add blur
    blur_and_darken(imgBuf);

    // prep screen
    scr.startWrite();
    scr.setAddrWindow(0, 0, WIDTH, HEIGHT);  // left corner at 0, 0 and fully editable screen
    scr.writePixels(imgBuf, WIDTH * HEIGHT);
    scr.endWrite();

  } else if (dt_sig[0] == 'T' && dt_sig[1] == 'R') {
    // case json track data transfer
    // send ack
    Serial.write(ACK);
    Serial.flush();

    // receive into temp buffer
    char jsonBuf[512];
    Serial.readBytes(jsonBuf, byte_count);
    jsonBuf[byte_count] = '\0';

    // parse
    DeserializationError err = deserializeJson(doc, jsonBuf);
    if (err) return;

    // print text to screen
    scr.setTextColor(ST77XX_WHITE);
    scr.setFont(&FreeMono9pt7b);
    print_text(scr, doc["title"], 10, BLUR_START + 18, WIDTH - 20);
    print_text(scr, doc["artist"], 10, BLUR_START + 38, WIDTH - 20);
  } else {
    // case data transfer signal was malformed
    Serial.read();
    return;
  }
}

void setup1() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(ENC_CL, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  pinMode(BTN_1, INPUT_PULLUP);
  pinMode(BTN_2, INPUT_PULLUP);
  pinMode(BTN_3, INPUT_PULLUP);
}

void loop1() {
  static uint32_t last_press = 0;
  static uint32_t last_enc = 0;
  static bool last_cl = HIGH;
  const uint32_t DEBOUNCE_MS = 200;
  const uint32_t ENC_DEBOUNCE_MS = 50;
  uint32_t now = millis();

  // buttons
  if (now - last_press > DEBOUNCE_MS) {
    if (digitalReadFast(BTN_1) == LOW) {
      digitalWriteFast(LED_BUILTIN, HIGH);
      last_press = now;
    } else if (digitalReadFast(BTN_2) == LOW) {
      digitalWriteFast(LED_BUILTIN, HIGH);
      last_press = now;
    } else if (digitalReadFast(BTN_3) == LOW) {
      digitalWriteFast(LED_BUILTIN, HIGH);
      last_press = now;
    } else if (digitalReadFast(ENC_SW) == LOW) {
      digitalWriteFast(LED_BUILTIN, HIGH);
      last_press = now;
    }
  }

  // encoder
  bool cl = digitalReadFast(ENC_CL);
  if (cl != last_cl && cl == LOW) {
    bool dt = digitalReadFast(ENC_DT);  // read DT immediately
    if (now - last_enc > ENC_DEBOUNCE_MS) {
      if (dt == HIGH) {
        digitalWriteFast(LED_BUILTIN, HIGH);  // clockwise
      } else {
        digitalWriteFast(LED_BUILTIN, LOW);  // counter-clockwise
      }
      last_enc = now;
    }
  }
  last_cl = cl;

  delay(1);
}