/**
  @file cd32.c

  This is a small library for the Amiga CD32 that interfaces with AmigaOS 1.3.

  by gameblabla 2023

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/

#include "cdtv.h"

static uint16_t global_width, global_height;
static uint8_t global_depth;

uint8_t is_pal;
LONG amiga_clock = 3546895; // For PAL. For NTSC : 3579545

static volatile struct Custom* custom;
struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase			*GfxBase = NULL;
struct Library			*LowLevelBase = NULL;
struct ExecBase* SysBase = NULL;


struct ViewPort *vport;
struct RastPort *rport;
struct Window *window = NULL;
struct Screen *screen = NULL;
struct Screen *myscreen;
struct Window *mywindow;
struct ScreenBuffer *sb[BUFFERS];
struct BitMap *offScreenBitmap[BUFFERS];
struct Library *IconBase = NULL;

void CDDA_Loop_check();

static UWORD __chip emptypointer[] = 
{
	0x0000, 0x0000,	/* reserved, must be NULL */
	0x0000, 0x0000, 	/* 1 row of image data */
	0x0000, 0x0000	/* reserved, must be NULL */
};
static struct ScreenModeRequester *video_smr = NULL;
int mode = 0;
ULONG propertymask;
struct RastPort temprp2;
UBYTE toggle=0;
uint8_t bufferSwap;

LONG setupPlanes(struct BitMap *bitMap, LONG depth, LONG width, LONG height)
{
	SHORT plane_num;
	for (plane_num = 0; plane_num < depth; plane_num++)
	{
		bitMap->Planes[plane_num]=(PLANEPTR)AllocRaster(width,height);
		BltClear(bitMap->Planes[plane_num],(width>>3)*height,1);
	}
	return(TRUE);
}

#if BUFFERS > 1
inline void UpdateScreen_Video()
{	
		WaitBlit();
		WaitTOF(); 
		// WaitTOF needs to be here and not after otherwise :
		// - Blitter won't work properly
		// - CD Audio won't work
		// This took me 2 hours before i figured out it was relying on this...
		while (!ChangeScreenBuffer(myscreen,sb[bufferSwap]))
#if BUFFERS == 3
		bufferSwap = (bufferSwap + 1) % 3;
#else
        bufferSwap ^=1;
#endif
		temprp2.BitMap = offScreenBitmap[bufferSwap];
}
#endif

void SetPalette_Video(const uint8_t *palette, uint16_t numberofcolors)
{
    LoadRGB4(&myscreen->ViewPort, (UWORD*)palette, numberofcolors);
}

int Init_Video(uint16_t w, uint16_t h, uint16_t depth)
{
	uint8_t i;
	
	if (!SysBase) SysBase = (struct ExecBase*) OpenLibrary("exec.library", 0);
	
	/* I would love so much not having to depend on NDK and use direct hardware access... */
 	if (!LowLevelBase) LowLevelBase = OpenLibrary("lowlevel.library",39);

	if (!IntuitionBase)
	{
		if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",0)) == NULL)
		{
			INTERNAL_LIB_LOG("No intution library\n");
			return 1;
		}
	}
	
	/* Open graphics.library */
	if (!GfxBase)
	{
		if ((GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", 0)) == NULL)
		{
			INTERNAL_LIB_LOG("No graphics library\n");
			return 1;
		}
	}
	
	global_width = w;
	global_height = h;
	bufferSwap = 0;
	global_depth = depth;
	
	SetTaskPri(FindTask(NULL), 20);
	
	if (!myscreen)
	{
		for(i=0;i<BUFFERS;i++)
		{
			// Don't use AllocBitmap for older NDK
#if OLDER_NDK == 1
			offScreenBitmap[i] = (struct BitMap *) AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR);
			InitBitMap(offScreenBitmap[i], global_depth, global_width, global_height);
			setupPlanes(offScreenBitmap[i], global_depth, global_width, global_height);
			//offScreenBitmap[i] = AllocBitMap(global_width, global_height, global_depth, BMF_INTERLEAVED, NULL);
#else
			offScreenBitmap[i] = AllocBitMap(global_width, global_height, global_depth, BMF_INTERLEAVED, NULL);
#endif
		}
		
#if OLDER_NDK == 1
		struct NewScreen newScreen = {
		.Width       = global_width,
		.Height      = global_height,
		.Depth       = global_depth,
		.DetailPen   = 0,
		.BlockPen    = 0,
		.Type        = CUSTOMSCREEN|CUSTOMBITMAP,
		.Font        = NULL,
		.DefaultTitle= (UBYTE*) "",
		.CustomBitMap= offScreenBitmap[0]
		};
		myscreen = OpenScreen(&newScreen);
#else
		myscreen = OpenScreenTags( NULL,
			SA_Width, global_width,
			SA_Height, global_height,
			SA_Depth, global_depth,
			SA_Quiet, TRUE,
			SA_Type, CUSTOMSCREEN|CUSTOMBITMAP,
			SA_BitMap,offScreenBitmap[0],
			TAG_DONE);
#endif

#if BUFFERS > 1
		for(i=0;i<BUFFERS;i++)
		{
			sb[i] = AllocScreenBuffer(myscreen, offScreenBitmap[i],0);	
		}
#endif

#if OLDER_NDK == 1
	  struct NewWindow newWindow = {
		.LeftEdge   = 0,
		.TopEdge    = 0,
		.Width      = global_width,
		.Height     = global_height,
		.DetailPen  = 0,
		.BlockPen   = 0,
		.IDCMPFlags = 0,
		.Flags      = WFLG_SIMPLE_REFRESH | WFLG_BORDERLESS,
		.Title      = (UBYTE*) "My Window",
		.Screen     = myscreen,
		.BitMap     = NULL,
		.Type       = CUSTOMSCREEN,
		.FirstGadget= NULL,
		.CheckMark  = NULL,
		};
		mywindow = OpenWindow(&newWindow);
#else
		mywindow = OpenWindowTags( NULL,
				WA_CloseGadget, FALSE,
				WA_DepthGadget, FALSE,
				WA_DragBar, FALSE,
				WA_SizeGadget, FALSE,
				WA_Gadgets, FALSE,
				WA_Borderless, TRUE,
				WA_NoCareRefresh, TRUE,
				WA_SimpleRefresh, TRUE,
				WA_RMBTrap, FALSE,
				WA_Activate, TRUE,
				WA_IDCMP, NULL,
				WA_Width, global_width,
				WA_Height, global_height,
				WA_CustomScreen, myscreen,
		TAG_DONE);
#endif
	}

	
#if BUFFERS != 1
    InitRastPort(&temprp2);
    temprp2.Layer=0;
	bufferSwap = 0;
    temprp2.BitMap=offScreenBitmap[bufferSwap^1];
#endif
	//SetPointer(window,emptypointer,1,16,0,0);
	
	// Check if we're NTSC or PAL
	/*is_pal = (((struct GfxBase *) GfxBase)->DisplayFlags & PAL) == PAL;
    if (is_pal) {
        amiga_clock = 3546895; // We are in PAL mode
    } else {
		amiga_clock = 3579545;
    }*/
    // Assume PAL, for now.
    amiga_clock = 3546895;
    is_pal = 1;
	
	return 0;
}

/* Drawing stuff */

ULONG LoadImage_native(const char* fname, struct ImageVSprite* buffer, uint16_t width, uint16_t height)
{
    ULONG size;

    buffer->x = 0;
    buffer->y = 0;
    buffer->width = width;
    buffer->height = height;
    
	BPTR file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
#if OLDER_NDK == 1
	// Only non interleaved frames unlike KICK3.1
	buffer->imgdata = AllocMem(sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR);
	InitBitMap(buffer->imgdata, global_depth, width, height);
	setupPlanes(buffer->imgdata, global_depth, width, height);
	for (uint8_t i = 0; i < global_depth; i++) {
		Read(file, buffer->imgdata->Planes[i], (width * (height >> 3)));
	}
	Close(file);
#else
    buffer->imgdata = AllocBitMap(width, height, global_depth, BMF_INTERLEAVED, NULL);
    Read(file, buffer->imgdata->Planes[0], size);
	Close(file);
#endif

    
    return size;
}

void LoadPalette_fromfile(const char* fname)
{
    BPTR file;
	unsigned char cur_pal[128];
    ULONG size;
 
	file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
    Read(file, cur_pal, size);
    Close(file);
    
    SetPalette_Video(cur_pal, size >> 1);
}

#if 0
// We are using a macro for this as we want to lessen the load
// See cdtv.h
void DrawImage_native(struct ImageVSprite buffer)
{
	BltBitMap(buffer.imgdata, 0, 0, FINAL_BITMAP, buffer.x, buffer.y, buffer.width, buffer.height, 0xC0, 0xFF, NULL);
}
#endif
