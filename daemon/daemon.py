import spotipy
from spotipy.oauth2 import SpotifyOAuth
from dotenv import load_dotenv
import os
import pyautogui
# use import threading to run spotipy in the background

load_dotenv(r"C:\Development\Pico-Spotify-Controller\daemon\creds.env")

scope = (
    "user-library-read "
    "user-read-currently-playing "
    "user-read-playback-state "
    "user-modify-playback-state"
)

sp = spotipy.Spotify(auth_manager=SpotifyOAuth(
    client_id=os.getenv("SPOTIFY_CLIENT_ID"),
    client_secret=os.getenv("SPOTIFY_CLIENT_SECRET"),
    redirect_uri=os.getenv("SPOTIFY_REDIRECT_URI"),
    scope=scope,
    cache_path=r"C:\Development\Pico-Spotify-Controller\daemon\.spotify_cache"
))

def get_current_track():
	track = sp.current_user_playing_track()
	if not track or not track['item']:
		return None
	item = track['item']
	return {
		"title": item['name'],
		"artist": ", ".join(a['name'] for a in item['artists']),
		"album": item['album']['name'],
		"uri": item['uri'],
		"art_url": item['album']['images'][0]['url'],
        "is_playing": track['is_playing'],
        "progress_ms": track['progress_ms'],
        "duration_ms": item['duration_ms']
    }


track = get_current_track()
print(track['title'] + ", " + track['artist'])

pyautogui.press("playpause")
