#!/usr/bin/env python3
"""
Generate music/title.wav, music/game.wav, and sounds/*.wav from
the audio event data originally embedded in src/audio.c.
"""

import struct, os, math, subprocess

os.makedirs("music",  exist_ok=True)
os.makedirs("sounds", exist_ok=True)

SAMPLERATE = 22050

# ---------------------------------------------------------------------------
# Event codes (same as audio.c)
EV_NOTEOFF = 0x00
EV_NOTEON  = 0x10
EV_UNDRAW  = 0x20
EV_DRAW    = 0x30
EV_BORDER  = 0x40
EV_END     = 0x50

MUS_STOP = 0
MUS_PLAY = 1

# ---------------------------------------------------------------------------
# Music score data (copied verbatim from audio.c musicScore[2][2128])
# Two tracks: [0] = title, [1] = game

BC_BLACK   = EV_BORDER | 0x0
BC_BLUE    = EV_BORDER | 0x1
BC_RED     = EV_BORDER | 0xa
BC_MAGENTA = EV_BORDER | 0x3
BC_GREEN   = EV_BORDER | 0xc
BC_CYAN    = EV_BORDER | 0x5
BC_YELLOW  = EV_BORDER | 0x6
BC_WHITE   = EV_BORDER | 0x7

MUSIC_SCORE = [
    # --- title ---
    [
        BC_BLACK,   48, 57, 12, 32, 57, 2,
        BC_GREEN,   48, 61, 12, 32, 61, 2,
        BC_YELLOW,  48, 64, 12, 32, 64, 2,

        BC_YELLOW,  48, 45, 0, 52, 64, 12, 32, 45, 0, 36, 64, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_RED,     48, 49, 0, 49, 52, 0, 51, 73, 0, 52, 76, 12, 32, 49, 0, 33, 52, 0, 35, 73, 0, 36, 76, 2,

        BC_RED,     48, 45, 0, 51, 73, 0, 52, 76, 12, 32, 45, 0, 35, 73, 0, 36, 76, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_BLACK,   48, 49, 0, 49, 52, 0, 51, 69, 0, 52, 73, 12, 32, 49, 0, 33, 52, 0, 35, 69, 0, 36, 73, 2,

        BC_BLACK,   48, 45, 0, 51, 69, 0, 52, 73, 12, 32, 45, 0, 35, 69, 0, 36, 73, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_BLACK,   48, 49, 0, 49, 52, 0, 52, 57, 12, 32, 49, 0, 33, 52, 0, 36, 57, 2,

        BC_BLACK,   48, 45, 0, 52, 57, 12, 32, 45, 0, 36, 57, 2,
        BC_GREEN,   48, 49, 0, 49, 52, 0, 52, 61, 12, 32, 49, 0, 33, 52, 0, 36, 61, 2,
        BC_YELLOW,  48, 49, 0, 49, 52, 0, 52, 64, 12, 32, 49, 0, 33, 52, 0, 36, 64, 2,

        BC_YELLOW,  48, 47, 0, 51, 62, 0, 52, 64, 12, 32, 47, 0, 35, 62, 0, 36, 64, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_RED,     48, 50, 0, 49, 52, 0, 51, 74, 0, 52, 76, 12, 32, 50, 0, 33, 52, 0, 35, 74, 0, 36, 76, 2,

        BC_RED,     48, 47, 0, 51, 74, 0, 52, 76, 12, 32, 47, 0, 35, 74, 0, 36, 76, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 0, 51, 68, 0, 52, 74, 12, 32, 50, 0, 33, 52, 0, 35, 68, 0, 36, 74, 2,

        BC_BLACK,   48, 47, 0, 51, 68, 0, 52, 74, 12, 32, 47, 0, 35, 68, 0, 36, 74, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 0, 52, 56, 12, 32, 50, 0, 33, 52, 0, 36, 56, 2,

        BC_WHITE,   48, 47, 0, 52, 56, 12, 32, 47, 0, 36, 56, 2,
        BC_RED,     48, 50, 0, 49, 52, 0, 52, 59, 12, 32, 50, 0, 33, 52, 0, 36, 59, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 0, 52, 66, 12, 32, 50, 0, 33, 52, 0, 36, 66, 2,

        BC_WHITE,   48, 44, 0, 52, 66, 12, 32, 44, 0, 36, 66, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_RED,     48, 50, 0, 49, 52, 0, 51, 74, 0, 52, 78, 12, 32, 50, 0, 33, 52, 0, 35, 74, 0, 36, 78, 2,

        BC_RED,     48, 40, 0, 51, 74, 0, 52, 78, 12, 32, 40, 0, 35, 74, 0, 36, 78, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 0, 51, 68, 0, 52, 74, 12, 32, 50, 0, 33, 52, 0, 35, 68, 0, 36, 74, 2,

        BC_BLACK,   48, 44, 0, 51, 68, 0, 52, 74, 12, 32, 44, 0, 35, 68, 0, 36, 74, 2,
        BC_BLACK,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 0, 52, 56, 12, 32, 50, 0, 33, 52, 0, 36, 56, 2,

        BC_WHITE,   48, 40, 0, 52, 56, 12, 32, 40, 0, 36, 56, 2,
        BC_RED,     48, 50, 0, 49, 52, 0, 52, 59, 12, 32, 50, 0, 33, 52, 0, 36, 59, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 0, 52, 66, 12, 32, 50, 0, 33, 52, 0, 36, 66, 2,

        BC_WHITE,   48, 45, 0, 52, 66, 12, 32, 45, 0, 36, 66, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_RED,     48, 49, 0, 49, 52, 0, 51, 73, 0, 52, 78, 12, 32, 49, 0, 33, 52, 0, 35, 73, 0, 36, 78, 2,

        BC_RED,     48, 40, 0, 51, 73, 0, 52, 78, 12, 32, 40, 0, 35, 73, 0, 36, 78, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_BLACK,   48, 49, 0, 49, 52, 0, 51, 69, 0, 52, 73, 12, 32, 49, 0, 33, 52, 0, 35, 69, 0, 36, 73, 2,

        BC_BLACK,   48, 45, 0, 51, 69, 0, 52, 73, 12, 32, 45, 0, 35, 69, 0, 36, 73, 2,
        BC_WHITE,   48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_BLACK,   48, 49, 0, 49, 52, 0, 52, 57, 12, 32, 49, 0, 33, 52, 0, 36, 57, 2,

        BC_BLACK,   48, 45, 0, 52, 57, 12, 32, 45, 0, 36, 57, 2,
        BC_GREEN,   48, 49, 0, 49, 52, 0, 52, 61, 12, 32, 49, 0, 33, 52, 0, 36, 61, 2,
        BC_YELLOW,  48, 49, 0, 49, 52, 0, 52, 64, 12, 32, 49, 0, 33, 52, 0, 36, 64, 2,

        BC_BLACK,   48, 49, 0, 52, 69, 12, 32, 49, 0, 36, 69, 2,
        BC_MAGENTA, 48, 52, 0, 49, 57, 12, 32, 52, 0, 33, 57, 2,
        BC_MAGENTA, 48, 52, 0, 49, 57, 0, 51, 76, 0, 52, 81, 12, 32, 52, 0, 33, 57, 0, 35, 76, 0, 36, 81, 2,

        BC_MAGENTA, 48, 49, 0, 51, 76, 0, 52, 81, 12, 32, 49, 0, 35, 76, 0, 36, 81, 2,
        BC_MAGENTA, 48, 52, 0, 49, 57, 12, 32, 52, 0, 33, 57, 2,
        BC_RED,     48, 52, 0, 49, 57, 0, 51, 73, 0, 52, 76, 12, 32, 52, 0, 33, 57, 0, 35, 73, 0, 36, 76, 2,

        BC_RED,     48, 49, 0, 51, 73, 0, 52, 76, 12, 32, 49, 0, 35, 73, 0, 36, 76, 2,
        BC_MAGENTA, 48, 52, 0, 49, 57, 12, 32, 52, 0, 33, 57, 2,
        BC_BLACK,   48, 52, 0, 52, 57, 12, 32, 52, 0, 36, 57, 2,

        BC_BLACK,   48, 49, 0, 52, 57, 12, 32, 49, 0, 36, 57, 2,
        BC_GREEN,   48, 52, 0, 49, 57, 0, 52, 61, 12, 32, 52, 0, 33, 57, 0, 36, 61, 2,
        BC_YELLOW,  48, 52, 0, 49, 57, 0, 52, 64, 12, 32, 52, 0, 33, 57, 0, 36, 64, 2,

        BC_BLACK,   48, 50, 0, 52, 69, 12, 32, 50, 0, 36, 69, 2,
        BC_CYAN,    48, 54, 0, 49, 57, 0, 50, 59, 12, 32, 54, 0, 33, 57, 0, 34, 59, 2,
        BC_GREEN,   48, 54, 0, 49, 57, 0, 50, 59, 0, 51, 78, 0, 52, 81, 12, 32, 54, 0, 33, 57, 0, 34, 59, 0, 35, 78, 0, 36, 81, 2,

        BC_GREEN,   48, 50, 0, 51, 78, 0, 52, 81, 12, 32, 50, 0, 35, 78, 0, 36, 81, 2,
        BC_CYAN,    48, 54, 0, 49, 57, 0, 50, 59, 12, 32, 54, 0, 33, 57, 0, 34, 59, 2,
        BC_RED,     48, 54, 0, 49, 57, 0, 50, 59, 0, 51, 74, 0, 52, 78, 12, 32, 54, 0, 33, 57, 0, 34, 59, 0, 35, 74, 0, 36, 78, 2,

        BC_RED,     48, 50, 0, 51, 74, 0, 52, 78, 12, 32, 50, 0, 35, 74, 0, 36, 78, 2,
        BC_BLACK,   48, 54, 0, 49, 57, 0, 50, 59, 12, 32, 54, 0, 33, 57, 0, 34, 59, 2,
        BC_RED,     48, 59, 12, 32, 59, 2,

        BC_RED,     48, 59, 12, 32, 59, 2,
        BC_GREEN,   48, 62, 12, 32, 62, 2,
        BC_WHITE,   48, 66, 12, 32, 66, 2,

        BC_CYAN,    48, 44, 0, 52, 66, 12, 32, 44, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 2,
        BC_WHITE,   48, 50, 0, 49, 52, 12, 32, 50, 0, 33, 52, 0, 36, 66, 2,

        BC_CYAN,    48, 40, 0, 52, 66, 12, 32, 40, 0, 36, 66, 2,
        BC_CYAN,    48, 50, 0, 49, 52, 0, 52, 63, 12, 32, 50, 0, 33, 52, 0, 36, 63, 2,
        BC_YELLOW,  48, 50, 0, 49, 52, 0, 52, 64, 12, 32, 50, 0, 33, 52, 0, 36, 64, 2,

        BC_WHITE,   48, 45, 0, 51, 69, 0, 52, 73, 12, 32, 45, 2,
        BC_RED,     48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 2,
        BC_RED,     48, 49, 0, 49, 52, 12, 32, 49, 0, 33, 52, 0, 35, 69, 0, 36, 73, 2,

        BC_WHITE,   48, 40, 0, 51, 69, 0, 52, 73, 12, 32, 40, 0, 35, 69, 0, 36, 73, 2,
        BC_BLACK,   48, 49, 0, 49, 52, 0, 52, 69, 12, 32, 49, 0, 33, 52, 0, 36, 69, 2,
        BC_GREEN,   48, 49, 0, 49, 52, 0, 52, 61, 12, 32, 49, 0, 33, 52, 0, 36, 61, 2,

        BC_GREEN,   48, 45, 0, 49, 50, 0, 50, 54, 0, 52, 61, 26, 36, 61, 2,
        BC_RED,     52, 59, 12, 32, 45, 0, 33, 50, 0, 34, 54, 0, 36, 59, 2,

        BC_WHITE,   48, 44, 0, 49, 50, 0, 50, 52, 0, 52, 66, 26, 36, 66, 2,
        BC_YELLOW,  52, 64, 12, 32, 44, 0, 33, 50, 0, 34, 52, 0, 36, 64, 2,

        BC_WHITE,   48, 45, 0, 49, 49, 0, 50, 52, 0, 52, 57, 12, 32, 45, 0, 33, 49, 0, 34, 52, 0, 36, 57, 16,
        BC_BLACK,   32, 45, 2,
        BC_BLACK,   48, 45, 0, 49, 49, 0, 50, 52, 0, 51, 57, 0, 52, 69, 12, 32, 45, 0, 33, 49, 0, 34, 52, 0, 35, 57, 0, 36, 69, 2,

        BC_WHITE,   48, 45, 0, 49, 49, 0, 50, 52, 0, 51, 57, 0, 52, 69, 12, 32, 45, 0, 33, 49, 0, 34, 52, 0, 35, 57, 0, 36, 69, 30,
        EV_END, MUS_STOP,
    ],
    # --- game ("In the Hall of the Mountain King") ---
    [
        16, 40, 0, 17, 52, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 55, 11, 1, 1, 17, 57, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 59, 11, 1, 1, 17, 55, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 59, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 58, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 58, 23, 0, 0, 1, 1,
        16, 40, 0, 17, 57, 11, 1, 1, 17, 53, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 57, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 52, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 55, 11, 1, 1, 17, 57, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 59, 11, 1, 1, 17, 55, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 59, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,

        16, 43, 0, 17, 62, 11, 1, 1, 17, 59, 11, 0, 0, 1, 1,
        16, 50, 0, 17, 55, 11, 1, 1, 17, 59, 11, 0, 0, 1, 1,
        16, 43, 0, 17, 62, 23, 0, 1, 16, 50, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 52, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 55, 11, 1, 1, 17, 57, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 59, 11, 1, 1, 17, 55, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 59, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 58, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 58, 23, 0, 0, 1, 1,
        16, 40, 0, 17, 57, 11, 1, 1, 17, 53, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 57, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 52, 11, 1, 1, 17, 54, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 55, 11, 1, 1, 17, 57, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 59, 11, 1, 1, 17, 55, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 59, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,

        16, 43, 0, 17, 62, 11, 1, 1, 17, 59, 11, 0, 0, 1, 1,
        16, 50, 0, 17, 55, 11, 1, 1, 17, 59, 11, 0, 0, 1, 1,
        16, 43, 0, 17, 62, 23, 0, 1, 16, 50, 23, 0, 0, 1, 1,

        16, 47, 0, 17, 59, 11, 1, 1, 17, 61, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 63, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 43, 0, 17, 67, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 51, 0, 17, 67, 23, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 50, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 47, 0, 17, 59, 11, 1, 1, 17, 61, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 63, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 43, 0, 17, 67, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 51, 0, 17, 67, 23, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 23, 0, 1, 16, 50, 23, 0, 0, 1, 1,

        16, 47, 0, 17, 59, 11, 1, 1, 17, 61, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 63, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 43, 0, 17, 67, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 51, 0, 17, 67, 23, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 50, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 47, 0, 17, 59, 11, 1, 1, 17, 61, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 63, 11, 1, 1, 17, 64, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 54, 0, 17, 66, 23, 0, 0, 1, 1,

        16, 43, 0, 17, 67, 11, 1, 1, 17, 63, 11, 0, 0, 1, 1,
        16, 51, 0, 17, 67, 23, 0, 0, 1, 1,
        16, 47, 0, 17, 66, 23, 0, 1, 16, 50, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 64, 11, 1, 1, 17, 66, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 67, 11, 1, 1, 17, 69, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 71, 11, 1, 1, 17, 67, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 71, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 70, 11, 1, 1, 17, 66, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 70, 23, 0, 0, 1, 1,
        16, 40, 0, 17, 69, 11, 1, 1, 17, 65, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 69, 23, 0, 0, 1, 1,

        16, 40, 0, 17, 64, 11, 1, 1, 17, 66, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 67, 11, 1, 1, 17, 69, 11, 0, 0, 1, 1,
        16, 40, 0, 17, 71, 11, 1, 1, 17, 67, 11, 0, 0, 1, 1,
        16, 47, 0, 17, 71, 11, 1, 1, 17, 76, 11, 0, 0, 1, 1,

        16, 43, 0, 17, 74, 11, 1, 1, 17, 71, 11, 0, 0, 1, 1,
        16, 50, 0, 17, 67, 11, 1, 1, 17, 71, 11, 0, 0, 1, 1,
        16, 43, 0, 17, 74, 47, 0, 0, 1, 1,
        EV_END, MUS_PLAY,
    ],
]

# ---------------------------------------------------------------------------
# SFX pitch arrays (from audio.c sfxPitch[6])
SFX_PITCH = [
    # SFX_DIE
    [84, 81, 78, 75, 72, 69, 66, 63, 60, 57, 54, 51, 48, 45, 42, 39],
    # SFX_KONG
    [84, 83, 82, 81, 80, 79, 78, 77, 76, 75, 74, 73, 72, 71, 70, 69,
     68, 67, 66, 65, 64, 63, 62, 61, 60],
    # SFX_VICTORY (one cycle)
    list(range(36, 85)),
    # SFX_AIR (226 entries, truncate trailing repeats to unique steps)
    [88]+[87]*8+[86]*8+[85]*8+[84]*8+[83]*8+[82]*8+[81]*8+[80]*8+
    [79]*8+[78]*8+[77]*8+[76]*8+[75]*8+[74]*8+[73]*8+[72]*8+
    [71]*8+[70]*8+[69]*8+[68]*8+[67]*8+[66]*8+[65]*8+[64]*8+
    [63]*8+[62]*8+[61]*8+[60]*8,
    # SFX_JUMP (used by gen_audio only for reference)
    [36, 42, 48, 54, 60, 66, 72, 78, 84],
    # SFX_GAMEOVER (empty)
    [],
]

# Jump: pitch arc from original jumpInfo table (72→88→72), durations match each stage.
SFX_JUMP_PITCH = []
for p, d in zip(
    [72, 74, 76, 78, 80, 82, 84, 86, 88, 88, 86, 84, 82, 80, 78, 76, 74, 72],
    [ 5,  5,  4,  4,  3,  3,  2,  2,  1,  1,  2,  2,  3,  3,  4,  4,  5,  5]
):
    SFX_JUMP_PITCH.extend([p] * d)

# Fall: formula was `78 - minerAir` for minerAir 3..11 = MIDI 75→67, 2 ticks per note.
SFX_FALL_PITCH = list(range(75, 66, -1))

# ---------------------------------------------------------------------------
# Parse a music event stream into per-channel note events
# Returns: list of 5 lists; each list = [(start_tick, note_idx, on)] sorted by tick
def parse_score(score):
    """Returns list of (tick, channel, note_idx, is_on)"""
    events = []
    data = list(score)
    pos = 0
    clock = 0

    while pos < len(data):
        ev = data[pos]; pos += 1
        kind = ev & 0xf0
        ch   = ev & 0x0f

        if kind == EV_BORDER:
            continue  # no time, no note

        if kind in (EV_DRAW, EV_NOTEON):
            note = data[pos]; pos += 1
            time = data[pos]; pos += 1
            events.append((clock, ch, note, True))
            clock += time
            if time == 0:
                continue

        elif kind == EV_UNDRAW:
            _skip = data[pos]; pos += 1  # note byte present only for UNDRAW
            time = data[pos]; pos += 1
            events.append((clock, ch, _skip, False))
            clock += time
            if time == 0:
                continue

        elif kind == EV_NOTEOFF:
            time = data[pos]; pos += 1
            events.append((clock, ch, 0, False))
            clock += time
            if time == 0:
                continue

        elif kind == EV_END:
            break

    return events, clock  # (events, total_ticks)

# ---------------------------------------------------------------------------
# Render music events to a stereo WAV (multi-channel square-wave mix).


def render_music_wav(path, events, total_ticks, n_channels):
    FREQ_BASE  = 8.175          # Hz at freq-table index 0
    MUSIC_VOL  = 0.08           # per-channel volume (matches C MUSICVOLUME)
    n_samples  = int((total_ticks + 60) / 60.0 * SAMPLERATE)  # +1s tail

    left  = [0.0] * n_samples
    right = [0.0] * n_samples

    # Build (start_tick, end_tick, freq_idx) per channel
    active = {}  # ch -> (start_tick, freq_idx)
    segments = []
    for (tick, ch, freq_idx, is_on) in events:
        if is_on:
            active[ch] = (tick, freq_idx)
        else:
            if ch in active:
                st, fi = active.pop(ch)
                segments.append((st, tick, fi))

    # Render each segment as a square wave, alternating L/R phase (pseudo-stereo)
    for (st, et, fi) in segments:
        freq     = FREQ_BASE * (2 ** (fi / 12.0))
        inc      = freq / SAMPLERATE
        s_start  = int(st / 60.0 * SAMPLERATE)
        s_end    = min(int(et / 60.0 * SAMPLERATE), n_samples)
        phase    = 0.0
        for s in range(s_start, s_end):
            val = MUSIC_VOL if phase < 0.5 else -MUSIC_VOL
            left[s]  += val
            right[s] -= val   # anti-phase for width (matches C channel panning)
            phase += inc
            if phase >= 1.0:
                phase -= 1.0

    # Clip, interleave, write
    pcm = []
    for i in range(n_samples):
        l = max(-32768, min(32767, int(left[i]  * 32767)))
        r = max(-32768, min(32767, int(right[i] * 32767)))
        pcm.extend([l, r])

    data = struct.pack('<' + 'h' * len(pcm), *pcm)
    with open(path, 'wb') as f:
        f.write(b'RIFF')
        f.write(struct.pack('<I', 36 + len(data)))
        f.write(b'WAVE')
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))
        f.write(struct.pack('<H', 1))              # PCM
        f.write(struct.pack('<H', 2))              # stereo
        f.write(struct.pack('<I', SAMPLERATE))
        f.write(struct.pack('<I', SAMPLERATE * 4)) # byte rate (16-bit stereo)
        f.write(struct.pack('<H', 4))              # block align
        f.write(struct.pack('<H', 16))
        f.write(b'data')
        f.write(struct.pack('<I', len(data)))
        f.write(data)
    print(f"  wrote {path}  ({n_samples} samples, {len(data)//1024} KB)")

def wav_to_ogg(wav_path):
    """Convert wav_path to an OGG Vorbis file alongside it, then remove the WAV."""
    ogg_path = os.path.splitext(wav_path)[0] + ".ogg"
    subprocess.run(
        ["ffmpeg", "-y", "-i", wav_path, "-c:a", "libvorbis", "-q:a", "8", ogg_path],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
    )
    os.remove(wav_path)
    print(f"  converted → {ogg_path}")



print("Generating music WAV files ...")
title_events, title_ticks = parse_score(MUSIC_SCORE[0])
game_events,  game_ticks  = parse_score(MUSIC_SCORE[1])
render_music_wav("music/title.wav", title_events, title_ticks, n_channels=5)
wav_to_ogg("music/title.wav")
render_music_wav("music/game.wav",  game_events,  game_ticks,  n_channels=2)
wav_to_ogg("music/game.wav")

# ---------------------------------------------------------------------------
# Generate WAV SFX files

def write_wav(path, pitch_seq, ticks_per_note=1, volume=0.25):
    """Render a pitch sequence to a WAV file (22050 Hz, 16-bit mono).
    An empty pitch_seq writes one tick of silence so the file is always valid."""
    FREQ_TABLE_BASE = 8.175  # Hz at index 0
    samples = []
    phase = 0.0
    ticks_samples = int(SAMPLERATE / 60 * ticks_per_note)
    if not pitch_seq:
        samples = [0] * ticks_samples
        pitch_seq = []  # skip the loop below

    for note_idx in pitch_seq:
        freq = FREQ_TABLE_BASE * (2 ** (note_idx / 12.0))
        inc  = freq / SAMPLERATE  # cycles per sample
        for _ in range(ticks_samples):
            val = 32767 if (phase % 1.0) < 0.5 else -32768
            samples.append(int(val * volume))
            phase += inc

    data = struct.pack('<' + 'h' * len(samples), *samples)
    with open(path, 'wb') as f:
        # RIFF header
        data_size = len(data)
        f.write(b'RIFF')
        f.write(struct.pack('<I', 36 + data_size))
        f.write(b'WAVE')
        f.write(b'fmt ')
        f.write(struct.pack('<I', 16))         # chunk size
        f.write(struct.pack('<H', 1))          # PCM
        f.write(struct.pack('<H', 1))          # mono
        f.write(struct.pack('<I', SAMPLERATE))
        f.write(struct.pack('<I', SAMPLERATE * 2))  # byte rate
        f.write(struct.pack('<H', 2))          # block align
        f.write(struct.pack('<H', 16))         # bits per sample
        f.write(b'data')
        f.write(struct.pack('<I', data_size))
        f.write(data)
    print(f"  wrote {path}  ({len(samples)} samples)")

print("Generating sounds/ ...")
for name, pitch, tpn in [
    ("die",     SFX_PITCH[0], 1),
    ("kong",    SFX_PITCH[1], 5),
    ("victory", SFX_PITCH[2], 1),
    ("gameover", [72, 69, 65, 62, 58, 55, 51, 48], 12),
    ("air",     SFX_PITCH[3], 1),
    ("jump",    SFX_JUMP_PITCH, 1),
    ("fall",    SFX_FALL_PITCH, 2),
]:
    wav_path = f"sounds/{name}.wav"
    write_wav(wav_path, pitch, ticks_per_note=tpn)
    wav_to_ogg(wav_path)

print("Done.")
