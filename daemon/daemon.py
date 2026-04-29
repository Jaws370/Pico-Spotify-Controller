import spotipy
from spotipy.oauth2 import SpotifyOAuth
from dotenv import load_dotenv
import cv2
import requests
import os
import numpy as np
import time
import serial
import struct
import json

load_dotenv(r"C:\Development\Pico-Spotify-Controller\daemon\creds.env")

# spotipy init
scope = (
    "user-library-read "
    "user-read-currently-playing "
    "user-read-playback-state "
)

sp = spotipy.Spotify(auth_manager=SpotifyOAuth(
    client_id=os.getenv("SPOTIFY_CLIENT_ID"),
    client_secret=os.getenv("SPOTIFY_CLIENT_SECRET"),
    redirect_uri=os.getenv("SPOTIFY_REDIRECT_URI"),
    scope=scope,
    cache_path=r"C:\Development\Pico-Spotify-Controller\daemon\.spotify_cache"
))

# get current track data
def fetch_current_track() -> dict:
    # fetch current track data
    track = sp.current_user_playing_track()

    # make sure track not empty
    if not track or not track['item']:
        return None
    
    item = track['item']
    
    # dict return format
    return {
        "title": item['name'].split('(')[0].strip(),
        "artist": item['artists'][0]['name'],
        "album": item['album']['name'],
        "uri": item['uri'],
        "art_url": item['album']['images'][0]['url'],
        "is_playing": track['is_playing'],
        "progress_ms": track['progress_ms'],
        "duration_ms": item['duration_ms']
    }

def fetch_album_art(art_url: str) -> np.ndarray:
    # get the data from the url
    response = requests.get(art_url)
    # load data onto np array
    arr = np.frombuffer(response.content, np.uint8)
    # decode array data
    img = cv2.imdecode(arr, cv2.IMREAD_COLOR)
    img = cv2.resize(img, (240, 240), interpolation=cv2.INTER_LANCZOS4)
    return img

def img_to_raw(img: np.ndarray) -> bytes:
    # convert to rgb565
    rgb565 = cv2.cvtColor(img, cv2.COLOR_BGR2BGR565)
    return rgb565.tobytes()

def tr_to_bytes(current_track: dict) -> bytes:
    return json.dumps(current_track).encode('utf-8')

def push_data(ser: serial.Serial, dt_sig: bytes, data: bytes):
    # check for valid dt_sig
    if (len(dt_sig) != 2):
        raise RuntimeError(f"Invalid data signal, got: {dt_sig}")
    
    # construct data header packet
    packet = b"\x06" + struct.pack("<I2s", len(data), dt_sig)
    # push header packet
    ser.write(packet)

    # wait for ACKnowledgement
    ack = ser.read(1)
    if (ack != b"\x08"):
        raise RuntimeError(f"No ACK, got: {ack!r}")
    
    start = time.time()

    # push data
    ser.write(data)
    ser.flush()

    elapsed = time.time() - start
    print(f"Sent {len(data)} bytes in {elapsed:.2f}s")

def connect(port: str, baud: int = 2000000) -> serial.Serial:
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = baud
    ser.timeout = 10
    ser.dtr = True
    ser.rts = True
    ser.open()
    
    # check for pico presense on port
    print("Waiting for Pico...")
    while True:
        byte = ser.read(1)
        #check for pico connection
        if byte == b"\x05":
            ser.write(b"\x06")
            while ser.read(1) != b"\x07":
                time.sleep(0.1)
            break
        # if pico already connected
        if byte == b"":
            ser.write(b"\x06")
            break

    ser.reset_input_buffer()
    time.sleep(0.2)
    print("Pico ready")
    return ser

def run(port: str, baud: int = 2000000):
    ser = connect(port, baud)
    last_uri = None

    while True:
        try:
            cs = fetch_current_track()
            if cs and cs['uri'] != last_uri:
                print(f"Now playing: {cs['title']} by {cs['artist']}")

                img = fetch_album_art(cs['art_url'])
                raw = img_to_raw(img)
                push_data(ser, b'IM', raw)

                track_json = tr_to_bytes(cs)
                push_data(ser, b'TR', track_json)

                last_uri = cs['uri']
        except Exception as e:
            print(f"Error: {e}")
            ser.close()
            time.sleep(2)
            ser = connect(port, baud)
            last_uri = None

        time.sleep(1.1)

if __name__ == "__main__":
    run("COM5", 2000000)
