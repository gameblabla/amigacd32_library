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

#include <proto/graphics.h>

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

#include <intuition/intuition.h>
#include <intuition/screens.h>
#include <workbench/startup.h>
#include <workbench/workbench.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <exec/types.h>
#include <devices/audio.h>
#include <devices/cd.h>
#include <devices/trackdisk.h>
#include <dos/dos.h>
#include <libraries/dos.h>
#include <inline/dos.h>
#include <dos/dosextens.h>
#include <exec/types.h>
#include <clib/exec_protos.h>
#include <inline/intuition.h>
#include <inline/graphics.h>
#include <graphics/gfxbase.h>
#include <graphics/gfx.h>
#include <inline/dos.h>

extern struct DosLibrary *DOSBase;

#define OLDER_NDK 1

#define SINGLE_BUFFER 1
#define LOOP_CDDA 1
#define NOLOOP_CDDA 0

#ifdef DEBUG
#define INTERNAL_LIB_LOG printf
#else
// Empty stub for release builds
#define INTERNAL_LIB_LOG(...) 
#endif

// Whenever of buffers to draw to : 3 is triple buffering, 2 is double buffering, 1 is single buffering
#define BUFFERS 1

struct ImageVSprite
{
	int16_t x;
	int16_t y;
	uint16_t width;
	uint16_t height;
	uint8_t frames;
	struct BitMap *imgdata;
};

#if BUFFERS == 1
#define FINAL_BITMAP mywindow->RPort->BitMap
#else
#define FINAL_BITMAP offScreenBitmap[bufferSwap]
#endif


/* Needed for macros */
extern struct BitMap *offScreenBitmap[BUFFERS];
extern uint8_t bufferSwap;
extern struct Window *mywindow;

#define my_malloc(x) AllocMem(x, 0)

/*
 * Creates the display that you are going to draw the graphics to.
 * Needed for game_setpalette and Video_UpdateScreen
*/

extern int Init_Video(uint16_t w, uint16_t h, uint16_t depth);

/*
 * Creates the display that you are going to draw the graphics to.
 * numberofcolors is the numberofcolors the palette has (It should be 256 by default).
 * If the buffer is 8bpp but the palette only has 64 entries, adjust accordingly.
*/

extern  void SetPalette_Video(const uint8_t *palette, uint16_t numberofcolors);

extern ULONG LoadImage_native(const char* fname, struct ImageVSprite* buffer, uint16_t width, uint16_t height);

//extern void DrawImage_native(struct ImageVSprite buffer);
#define DrawImage_native(a) \
	BltBitMap(a.imgdata, 0, 0, FINAL_BITMAP, a.x, a.y, a.width, a.height, 0xC0, 0xFF, NULL);


/*
 * Flips the screen.
 * 
 * Note that for CDDA music looping, this is also required to be called in a looping function.
 * 
*/
#if BUFFERS == 1
#define UpdateScreen_Video() \
	WaitBlit(); \
	WaitTOF();
#else
extern inline void UpdateScreen_Video();
#endif

/*
 * Load a palette from file and applies it to the system palette.
 * 256 colors are expected. The palette file should be in the Game/ folder.
*/
extern void LoadPalette_fromfile(const char* fname);

/*
 * Loads a file (The file should be in the Game/ folder) and uploads it to the selected buffer.
 * Use this for say, loading an image file to the screen buffer for instance.
 * That said, it can also be used for other things too. (For example loading a text file)
*/
extern ULONG LoadFile_tobuffer(const char* fname, uint8_t* buffer);


/* CDTV/CDXL stuff */

extern void CDPLAYER_PlayVideo(LONG NumPlays, char* fname, uint32_t speed, uint8_t InputStopPlayback);


#endif
