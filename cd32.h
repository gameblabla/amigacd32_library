/**
  @file cd32.h

  This is a small library for the Amiga CD32 that interfaces with AmigaOS 3.1.

  by gameblabla 2023

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/

#ifndef CD32_H
#define CD32_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/lowlevel_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <graphics/gfxbase.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/icon.h>
#include <pragmas/intuition_pragmas.h>
#include <pragmas/graphics_pragmas.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <workbench/icon.h>
#include <exec/types.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/exec.h>
#include <exec/types.h>
#include <devices/audio.h>
#include <devices/cd.h>
#include <devices/trackdisk.h>
#include <dos/dos.h>
#include <libraries/lowlevel.h>


#define LOOP_CDDA 1
#define NOLOOP_CDDA 0

#ifdef DEBUG
#define LOG printf
#else
// Empty stub for release builds
#define LOG(...) ((void)0)
#endif

// For holding a sample into memory
struct AudioPCM {
  BYTE* pcmData;
  uint8_t assigned_channel;
  uint16_t frequency;
  uint16_t sampleCycles;
  int16_t duration;
  ULONG size;
};

extern uint8_t *gfxbuf;

extern uint16_t vwidth;
extern uint16_t vheight;

/*
 * Creates the display that you are going to draw the graphics to.
 * Needed for game_setpalette and Video_UpdateScreen
*/

extern int Init_Video();

/*
 * Creates the display that you are going to draw the graphics to.
*/

extern void SetPalette_Video(const uint8_t *palette);

/*
 * Flips the screen.
 * 
 * Note that for CDDA music looping, this is also required to be called in a looping function.
 * 
*/
extern void UpdateScreen_Video();

/*
 * Call this to use the PCM related functions.
 * Otherwise they won't work.
 * */
extern int Init_Audio();

/*
 * Load a file relative to the directory of the executable.
 * (as long as the provided system files have not been tampered with)
 * 
 * repeat is as it says, the number of times it will be repeated.
 * If this is set to -1, it will loop constantly.
 * Keep in mind that you are being memory limited by the amount of RAM that you may load for PCM samples.
 * You should also look into the CDDA functions if you want to play music on the Amiga CD32 instead.
 * 
 * frequency is the Audio frequency of the audio files.
 * I believe the limit is 22050 or so on the Amiga 3.1 OS.
 * PCM format is Signed 8-bits MONO
 *
 * 
 */
extern void Load_PCM(char* filename, struct AudioPCM* pcm, int16_t repeat, uint16_t frequency, uint8_t assigned_channel);

/* Plays the Loaded PCM audio file that was previously loaded with Load_PCM. */
extern void Play_PCM(struct AudioPCM* pcm);

/* This will stop the PCM channel that is playing the selected PCM. */
extern void Stop_PCM(struct AudioPCM* pcm);

/* Frees the memory that was allocated to it (the buffer inside the structure that is). */
extern void Clean_PCM(struct AudioPCM* pcm);

// CDDA

/*
 * You need to call this before you can use any of the CD functions below.
*/
extern int Init_CD();

/* 
 * Use this upon exit or if you don't need CD stuff anymore.
 * Note that this may crash upon it being called.
 * */
extern void Close_CD();

/*
 * You need to stop playing a CDDA track before using this function otherwise nothing will happen.
 * Just call Stop_CDDA() before using this. (Note that it's not required to do this upon the first time being used)
 * If you set loop to 1, it will play back the CDDA track again upon it being done.
 * 
 * LONG trackNum is what you would expect.
 * First track is usually the game/app data and second track is usually what is the first audio on the disc.
 * There are exceptions of course. Sometimes the data is on track 2 and it is possible to have music on track 1.
 * Or the game is being played from a floppy, in which case, it's possible to play all of the tracks from 1 to whatever.
 * This case scenario is typically not supported though but make sure your cue file reflects it.
 * 
 * Looping support needs VW_UpdateScreen() to be called in a loop.
*/
extern BOOL Play_CD_Track(LONG trackNum, uint8_t loop);

/*
 * Stops the current CDDA track being played.
 * If nothing is being played then nothing will happen.
*/
extern void Stop_CDDA();


#endif
