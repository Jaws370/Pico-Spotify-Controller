#include <Arduino.h>
#include <Adafruit_ST7789.h>
#include <LittleFS.h>
#include <SPI.h>

#include "config.h"
#include "track.h"

Adafruit_ST7789 scr = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static Track t;
static uint16_t imgBuf[WIDTH * HEIGHT];

void setup() {
  set_sys_clock_khz(250000, true);
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
    delay(10);
    if (Serial.available() > 0 && Serial.read() == PY_READY_SIG) break;
  }

  Serial.write(FULL_READY_SIG);  // fully ready signal
  Serial.flush();
}

void loop() {
  // wait for python ready signal for next data transfer
  while (true) {
    delay(10);
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
    // receive art
    t.receive_data(Serial, byte_count, imgBuf);
    // print art
    t.print_data(scr, imgBuf);

  } else if (dt_sig[0] == 'T' && dt_sig[1] == 'R') {
    // case json track data transfer
    // send ack
    Serial.write(ACK);
    Serial.flush();
    // receive meta
    t.receive_meta(Serial, byte_count);
    // print meta
    t.print_meta(scr);
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