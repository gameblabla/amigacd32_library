#include "cd32.h"
#include "audio_paula.h"

struct AudioPCM pc;
struct ImageVSprite img; // Only used for the non-Akiko version

// Whenever to use AKIKO or the OS's own Blitter functions
// On an Amiga without the Akiko, it will revert back to slower CPU-based C2P
// I may add a CPU fallback for 680/40/60 Amigas out there
// (or even Amiga CD32's with 50mhz 68030s)
#define AKIKO 1
// Whenever we should playback the video upon bootup
#define VIDEO_PLAYBACK 0

void Check_input()
{
	uint32_t state0;
	state0 = ReadJoyPort(0);
	
	if ((state0 & JPF_BUTTON_RED) == JPF_BUTTON_RED)
	{
#ifndef MOD_PLAYER
		Stop_CDDA();
#endif
		Stop_PCM(&pc);
	}
	
	if ((state0 & JPF_BUTTON_BLUE) == JPF_BUTTON_BLUE)
	{
#ifndef MOD_PLAYER
		Play_CD_Track(2, LOOP_CDDA);
#endif
		Play_PCM(&pc, -1);
	}
	
/*
	if ((state0 & JPF_JOY_UP)==JPF_JOY_UP) memset(gfxbuf, 0, vwidth*vheight);
	else if ((state0 & JPF_JOY_DOWN)==JPF_JOY_DOWN) memset(gfxbuf, 1, vwidth*vheight);

	if ((state0 & JPF_JOY_LEFT)==JPF_JOY_LEFT) memset(gfxbuf, 2, vwidth*vheight);
	else if ((state0 & JPF_JOY_LEFT)==JPF_JOY_LEFT) memset(gfxbuf, 3, vwidth*vheight);
	
	if ((state0 & JPF_BUTTON_RED)==JPF_BUTTON_RED) memset(gfxbuf, 4, vwidth*vheight);
	if ((state0 & JPF_BUTTON_BLUE)==JPF_BUTTON_BLUE) memset(gfxbuf, 5, vwidth*vheight);
	if ((state0 & JPF_BUTTON_YELLOW)==JPF_BUTTON_YELLOW) memset(gfxbuf, 6, vwidth*vheight);
	if ((state0 & JPF_BUTTON_GREEN)==JPF_BUTTON_GREEN) memset(gfxbuf, 7, vwidth*vheight);
	
	if ((state0 & JPB_BUTTON_FORWARD)==JPB_BUTTON_FORWARD) memset(gfxbuf, 8, vwidth*vheight);
	if ((state0 & JPB_BUTTON_REVERSE)==JPB_BUTTON_REVERSE) memset(gfxbuf, 9, vwidth*vheight);
	
	if ((state0 & JPB_BUTTON_PLAY)==JPB_BUTTON_PLAY) memset(gfxbuf, 10, vwidth*vheight);
*/

}


int main()
{
	Init_Video(320,256,320,256, AKIKO);
	Init_Audio();
	Init_CD();
	
#if VIDEO_PLAYBACK == 1
	CDPLAYER_PlayVideo(1, "vid.cdxl", 220, 1);
	Init_Video(320,256,320,256, AKIKO); /* For whatever reason, this is needed */
#endif

#if AKIKO == 1
	// Simply export a 256 colors image with GIMP as RAW and it will output RAW/PAL files
    LoadFile_tobuffer("image.raw", gfxbuf);
	LoadPalette_fromfile_RAW("image.pal");
#else
	//amigeconv -f bitplane -d 8 -l amiga8.png amiga8.raw
	// Frames must be non interleaved unlike CDTV version (for speed reasons)
	LoadImage_native("amiga8.raw", &img, 320, 256);
	//amigeconv -f palette -p loadrgb32 amiga8.png amiga8.pal
	LoadPalette_fromfile_LoadRGB("amiga8.pal");
#endif

	Load_PCM("sound.raw", &pc, 11025, 0);
	
#ifdef MOD_PLAYER
	Load_MOD("mymod.mod");
	Play_MOD();
#endif
	BOOL success = Play_CD_Track(2, LOOP_CDDA);

	while(1)
	{
		CDDA_Loop_check();
		Check_input();
		
#if AKIKO == 1
		Draw_Video_Akiko();
#else
		DrawImage_native(img);
#endif
		Update_Video();
	}
	
	Clean_PCM(&pc);
	Close_CD();
	
	return 0;
}
