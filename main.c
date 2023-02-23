#include "cd32.h"

unsigned char cur_pal[768];

struct AudioPCM pc;
void Check_input()
{
	uint32_t state0;
	state0 = ReadJoyPort(0);
	
	if ((state0 & JPF_JOY_UP) == JPF_JOY_UP)
	{
		memset(gfxbuf, 0, 320*100);
	}
	else if ((state0 & JPF_JOY_DOWN) == JPF_JOY_DOWN)
	{
		memset(gfxbuf, 5, 320*100);
	}
	
	if ((state0 & JPF_BUTTON_RED) == JPF_BUTTON_RED)
	{
		Stop_CDDA();
		Stop_PCM(&pc);
	}
	
	if ((state0 & JPF_BUTTON_BLUE) == JPF_BUTTON_BLUE)
	{
		Play_CD_Track(2, LOOP_CDDA);
		Play_PCM(&pc);
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
	FILE* fp;

	int ret = Init_Video();
	if (ret == 1) return 1;
	
	ret = Init_Audio();
	if (ret == 1) return 1;

    file = Open("image.raw", MODE_OLDFILE);
    size = vwidth*vheight;
	Read(file, gfxbuf, size);
    Close(file);
    
	file = Open("image.pal", MODE_OLDFILE);
    size = 768;
    Read(file, cur_pal, size);
    Close(file);

	SetPalette_Video(cur_pal);
	
	Load_PCM("sound.raw", &pc, -1, 11025, 0);
	Play_PCM(&pc);
	
	Init_CD();
	
	BOOL success = Play_CD_Track(2, LOOP_CDDA);
	
	while(1)
	{
		Check_input();
		UpdateScreen_Video();
	}
	return 0;
}
