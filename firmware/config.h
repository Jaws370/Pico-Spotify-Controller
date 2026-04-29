#pragma once

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