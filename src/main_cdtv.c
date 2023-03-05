#include "cdtv.h"
#include "audio_paula.h"

struct AudioPCM pc;
struct ImageVSprite img;

// Whenever we should playback the video upon bootup
#define VIDEO_PLAYBACK 0

void Check_input()
{
	uint32_t state0;
	state0 = ReadJoyPort(0);
	
	if ((state0 & JPF_BUTTON_RED) == JPF_BUTTON_RED)
	{
		Stop_CDDA();
		Stop_PCM(&pc);
	}
	
	if ((state0 & JPF_BUTTON_BLUE) == JPF_BUTTON_BLUE)
	{
		Play_PCM(&pc, -1);
		Play_CD_Track(2, LOOP_CDDA);
	}
}

int main()
{
	int i = 0;

	Init_Video(320,256,5);
	Init_Audio();
	Init_CD();
	
#if VIDEO_PLAYBACK == 1
	CDPLAYER_PlayVideo(1, "vid.cdxl", 220, 1);
	Init_Video(320,256,320,256); /* For whatever reason, this is needed */
#endif

	// -d 5 is the bitdepth (user selectable). 5 is 32 colors, 4 is 16 colors.
	//amigeconv -f bitplane -d 5 myimage.png amiga.raw
	LoadImage_native("amiga.raw", &img, 320, 256);
	//amigeconv -f palette -p loadrgb4 myimage.png amiga.pal
	LoadPalette_fromfile("amiga.pal");

	Load_PCM("sound.raw", &pc, 11025, 0);
	
#ifdef MOD_PLAYER
	Load_MOD("mymod.mod");
	Play_MOD();
#endif

	Play_CD_Track(2, LOOP_CDDA);

	while(1)
	{
		CDDA_Loop_check();
		Check_input();
		
		DrawImage_native(img);
		UpdateScreen_Video();
	}
	
	Clean_PCM(&pc);
	Close_CD();
	
	return 0;
}
