#include "cd32.h"

struct AudioPCM pc;

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
    BPTR file;
    ULONG size;
    int ret;
	FILE* fp;

	Init_Video(320,256,320,256);
	Init_Audio();
	Init_CD();
#ifdef VIDEO_PLAYBACK
	CDPLAYER_PlayVideo(1, "vid.cdxl", 220, 1);
	Init_Video(320,256,320,256); /* For whatever reason, this is needed */
#endif

    LoadFile_tobuffer("image.raw", gfxbuf);
	LoadPalette_fromfile("image.pal");

	Load_PCM("sound.raw", &pc, 11025, 0);
	
	/* There's nothing preventing you from playing both CDDA and a MOD track file at the same time !
	 * But PTPlayer does have a small performance impact so preferably, you should stick with
	 * CDDA music... unless of course, you are concerned about space being allocated to music
	 * and would rather use that space for other things. (namely CDXL videos !)
	 * */
#ifdef MOD_PLAYER
	Load_MOD("mymod.mod");
	Play_MOD();
#else
	BOOL success = Play_CD_Track(2, LOOP_CDDA);
#endif

	while(1)
	{
		Check_input();
		UpdateScreen_Video();
		CDDA_Loop_check();
	}
	
	Clean_PCM(&pc);
	Close_CD();
	
	return 0;
}
