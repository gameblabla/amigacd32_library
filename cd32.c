/**
  @file cd32.c

  This is a small library for the Amiga CD32 that interfaces with AmigaOS 3.1.

  by gameblabla 2023

  Released under CC0 1.0 (https://creativecommons.org/publicdomain/zero/1.0/)
  plus a waiver of all other intellectual property. The goal of this work is to
  be and remain completely in the public domain forever, available for any use
  whatsoever.
*/

#include "cd32.h"

#define DEPTH 8
#define RBMDEPTH 8
#define ID_BNG   4
#define ID_BNG2   5
#define ID_BORDER 0

#define RBMWIDTH vwidth
#define RBMHEIGHT vheight

uint16_t vwidth;
uint16_t vheight;

uint16_t internal_width;
uint16_t internal_height;

// Global, used for holding buffer.
uint8_t *gfxbuf = NULL;

static uint8_t is_pal;

static LONG amiga_clock = 3546895; // For PAL. For NTSC : 3579545


static volatile struct Custom* custom;
struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase			*GfxBase = NULL;
struct Library			*LowLevelBase = NULL;

struct ViewPort *vport;
struct RastPort *rport;
struct Window *window = NULL;
struct Screen *screen = NULL;
#ifdef SINGLE_BUFFER
struct BitMap *myBitMaps;
#else
struct BitMap **myBitMaps;
struct ScreenBuffer *sb[2];
#endif
struct Library *IconBase = NULL;

void CDDA_Loop_check();

VOID freePlanes(struct BitMap *bitMap, LONG depth, LONG width, LONG height)
{
	SHORT plane_num;
	for (plane_num = 0; plane_num < depth; plane_num++)
	{
		if (NULL != bitMap->Planes[plane_num])
			FreeRaster(bitMap->Planes[plane_num], width, height);
	}
}

LONG setupPlanes(struct BitMap *bitMap, LONG depth, LONG width, LONG height)
{
	SHORT plane_num;
	for (plane_num = 0; plane_num < depth; plane_num++)
	{
		if (NULL != (bitMap->Planes[plane_num]=(PLANEPTR)AllocRaster(width,height)))
			BltClear(bitMap->Planes[plane_num],(width/8)*height,1);
		else
		{
			freePlanes(bitMap, depth, width, height);
			return(0);
		}
	}
	return(TRUE);
}

#ifndef SINGLE_BUFFER
struct BitMap **setupBitMaps(LONG depth, LONG width, LONG height)
{
	static struct BitMap *myBitMaps[3];
	if (NULL != (myBitMaps[0] = (struct BitMap *) AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR)))
	{
        		if (NULL != (myBitMaps[1]=(struct BitMap*)AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR)))
		{
		if (NULL != (myBitMaps[2]=(struct BitMap*)AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR)))
		{
			InitBitMap(myBitMaps[0], depth, width, height);
			InitBitMap(myBitMaps[1], depth, width, height);
           	InitBitMap(myBitMaps[2], depth, width, height);
			if (0 != setupPlanes(myBitMaps[0], depth, width, height))
			{
				if (0 != setupPlanes(myBitMaps[1], depth, width, height))
                {
                				if (0 != setupPlanes(myBitMaps[2], depth, width, height))
					return(myBitMaps);
                    				   freePlanes(myBitMaps[1], depth, width, height);
			    }
        		 				    freePlanes(myBitMaps[0], depth, width, height);
                    }
		  			  FreeMem(myBitMaps[2], (LONG)sizeof(struct BitMap));
		}
	   			   FreeMem(myBitMaps[1], (LONG)sizeof(struct BitMap));
	}
		FreeMem(myBitMaps[0], (LONG)sizeof(struct BitMap));
	}
	return(NULL);
}
#endif

#ifdef SINGLE_BUFFER
VOID	freeBitMaps(struct BitMap *myBitMaps,
#else
VOID	freeBitMaps(struct BitMap **myBitMaps,
#endif
					LONG depth, LONG width, LONG height)
{
#ifdef SINGLE_BUFFER
	freePlanes(myBitMaps, depth, width, height);
	FreeMem(myBitMaps, (LONG)sizeof(struct BitMap));
#else
	freePlanes(myBitMaps[0], depth, width, height);
	freePlanes(myBitMaps[1], depth, width, height);
	freePlanes(myBitMaps[2], depth, width, height);
	FreeMem(myBitMaps[0], (LONG)sizeof(struct BitMap));
	FreeMem(myBitMaps[1], (LONG)sizeof(struct BitMap));
	FreeMem(myBitMaps[2], (LONG)sizeof(struct BitMap));
#endif
}



void FreeTempRP( struct RastPort *rp )
{
	if( rp )
	{
		if( rp->BitMap )
		{
			//if( rev3 )
         //    FreeBitMap( rp->BitMap );
			//else myFreeBitMap( rp->BitMap );
		}
		FreeVec( rp );
	}
}

struct RastPort *MakeTempRP( struct RastPort *org )
{
	struct RastPort *rp;

	if( rp = AllocVec(sizeof(*rp), MEMF_ANY) )
	{
		memcpy( rp, org, sizeof(*rp) );
		rp->Layer = NULL;

	    rp->BitMap=AllocBitMap(org->BitMap->BytesPerRow*8,1,org->BitMap->Depth,0,org->BitMap);
		if( !rp->BitMap )
		{
			FreeVec( rp );
			rp = NULL;
		}
	}

	return rp;
}



static UWORD __chip emptypointer[] = 
{
	0x0000, 0x0000,	/* reserved, must be NULL */
	0x0000, 0x0000, 	/* 1 row of image data */
	0x0000, 0x0000	/* reserved, must be NULL */
};
static struct ScreenModeRequester *video_smr = NULL;
int mode = 0;
ULONG propertymask;
#ifndef SINGLE_BUFFER
struct RastPort temprp;
#endif
struct RastPort temprp2;
UBYTE toggle=0;

#ifndef SINGLE_BUFFER
inline void UpdateScreen_Video()
{	
	WriteChunkyPixels(&temprp2,0,0,WIDTH-1,((HEIGHT)-1),gfxbuf,vwidth);
#ifndef SINGLE_BUFFER
	while (!ChangeScreenBuffer(screen,sb[toggle]))
	WaitTOF();

	toggle^=1;
	temprp2.BitMap=myBitMaps[toggle];
#endif
}

inline void UpdateScreen_Video_partial(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{	
	WriteChunkyPixels(&temprp2,x,y,x+internal_width,y+internal_height,gfxbuf,w);
#ifndef SINGLE_BUFFER
	while (!ChangeScreenBuffer(screen,sb[toggle]))
	WaitTOF();

	toggle^=1;
	temprp2.BitMap=myBitMaps[toggle];
#endif
}
#endif

void SetPalette_Video(const uint8_t *palette, uint16_t numberofcolors)
{
#if 1
    uint_fast16_t i;
    ULONG table[256*3+1+1];
    table[0]=(numberofcolors<<16)|0;
	for (i = 0; i < numberofcolors; i++)
    {
		// << 26 for VGA palette, << 24 for normal ones
        table[i*3+1]= ((ULONG)palette[i*3+0] << 24);
        table[i*3+2]= ((ULONG)palette[i*3+1] << 24);
        table[i*3+3]= ((ULONG)palette[i*3+2] << 24);
    }    
    table[1+numberofcolors*3] = 0;  
    LoadRGB32(vport,table);
#endif
/*
#if 0
		volatile struct Custom* custom = (volatile struct Custom*) 0xDFF000;
		UWORD colorIndex;
		uint8_t palette[256 * 3];
		
		memcpy(palette, pal, 256 * 3);
		
		volatile uint16_t* const DMACONW     = (uint16_t* const) 0xDFF096;
		volatile uint16_t* const INTENAW     = (uint16_t* const) 0xDFF09A;
		volatile uint16_t* const INTREQW     = (uint16_t* const) 0xDFF09C;

        //*INTREQW = 0x7FFF; // Idem. Original Scoopex code do this twice. I have removed without problems.
        *DMACONW = 0x7FFF; // Disable all bits in DMACON
        *DMACONW = 0x87E0; // Setting DMA channels
		
		*(volatile uint16_t *)0xDFF000 = 0xfff;
		
		// Save the existing bplcon3 settings
		ULONG origBplcon3 = custom->bplcon3;

		// Loop through all palettes
		for (UWORD paletteIndex = 0; paletteIndex < 8; paletteIndex++) {
			// Select the current palette
			custom->bplcon3 = (origBplcon3 & ~(0b111 << 13)) | (paletteIndex << 13);

			// Clear LOCT bit
			custom->bplcon3 = origBplcon3 & ~(1 << 9);

			// Set the high nibbles of each color component
			for (colorIndex = 0; colorIndex < 32; colorIndex++) {
				custom->color[colorIndex] = ((palette[colorIndex * 3 + 0] & 0xF0) << 4) |
											((palette[colorIndex * 3 + 1] & 0xF0) << 0) |
											((palette[colorIndex * 3 + 2] & 0xF0) >> 4);
			}

			// Set LOCT bit
			custom->bplcon3 = origBplcon3 | (1 << 9);

			// Set the low nibbles of each color component
			for (colorIndex = 0; colorIndex < 32; colorIndex++) {
				custom->color[colorIndex] = ((palette[colorIndex * 3 + 0] & 0x0F) << 8) |
											((palette[colorIndex * 3 + 1] & 0x0F) << 4) |
											((palette[colorIndex * 3 + 2] & 0x0F) << 0);
			}
		}

		// Restore the original bplcon3 settings
		custom->bplcon3 = origBplcon3;
    
#endif
*/
}

int Init_Video(uint16_t w, uint16_t h, uint16_t iwidth, uint16_t iheight)
{
	/* I would love so much not having to depend on NDK and use direct hardware access... */
 	if (!LowLevelBase) LowLevelBase 	= OpenLibrary("lowlevel.library",39);

	if (!IntuitionBase)
	{
		if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",0)) == NULL)
		{
			LOG("No intution library\n");
			return 1;
		}
	}
	
	/* Open graphics.library */
	if (!GfxBase)
	{
		if ((GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", 0)) == NULL)
		{
			LOG("No graphics library\n");
			return 1;
		}
	}
	vwidth = w;
	vheight = h;
	internal_width = iwidth;
	internal_height = iheight;
	
	mode = BestModeID (BIDTAG_NominalWidth, vwidth,
	BIDTAG_NominalHeight, vheight,
	BIDTAG_Depth, 8,
	BIDTAG_DIPFMustNotHave, propertymask,
	TAG_DONE);
		
		
#ifdef SINGLE_BUFFER
	if (!myBitMaps)
	{
		myBitMaps = AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR);
		InitBitMap(myBitMaps, RBMDEPTH, RBMWIDTH, RBMHEIGHT);
		setupPlanes(myBitMaps, RBMDEPTH, RBMWIDTH, RBMHEIGHT);
	}
#endif
	
#ifndef SINGLE_BUFFER
	if (NULL!=(myBitMaps=setupBitMaps(RBMDEPTH,RBMWIDTH,RBMHEIGHT)))
#endif
	{
		if (screen = OpenScreenTags( NULL,
		SA_Width, WIDTH,
		SA_Height, HEIGHT,
		SA_Depth, DEPTH,
		SA_DisplayID, mode,
		SA_Quiet, TRUE,
		SA_Type, CUSTOMSCREEN|CUSTOMBITMAP,
#ifdef SINGLE_BUFFER
		SA_BitMap,myBitMaps,
#else
		SA_BitMap,myBitMaps[0],
#endif
		TAG_DONE))
		{
			vport = &screen->ViewPort;
#ifndef SINGLE_BUFFER
			sb[0] = AllocScreenBuffer(screen, myBitMaps[0],0);
			sb[1] = AllocScreenBuffer(screen, myBitMaps[1],0);
#endif
			if (window = OpenWindowTags( NULL,
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
			WA_Width, WIDTH,/*WIDTH,   */
			WA_Height, HEIGHT,/* HEIGHT,    */
			WA_CustomScreen, screen,
			TAG_DONE) )
			{
				rport = window->RPort;
				gfxbuf = AllocMem( sizeof(uint8_t)*(internal_width*internal_height), MEMF_PUBLIC|MEMF_CLEAR);
			}
		}
	}
#ifndef SINGLE_BUFFER
	InitRastPort(&temprp);
	temprp.Layer=0;
	temprp.BitMap=myBitMaps[2];
#endif
	InitRastPort(&temprp2);
	temprp2.Layer=0;
#ifdef SINGLE_BUFFER
	temprp2.BitMap=myBitMaps;
#else
	temprp2.BitMap=myBitMaps[toggle^1];
#endif
	SetPointer(window,emptypointer,1,16,0,0);
	
	// Check if we're NTSC or PAL
	is_pal = (((struct GfxBase *) GfxBase)->DisplayFlags & PAL) == PAL;
    if (is_pal) {
        amiga_clock = 3546895; // We are in PAL mode
    } else {
		amiga_clock = 3579545;
    }
	
	return 0;
}

/* Drawing stuff */

ULONG LoadFile_tobuffer(const char* fname, uint8_t* buffer)
{
    BPTR file;
    ULONG size;
 
	file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
    Read(file, buffer, size);
    Close(file);
    
    return size;
}

void LoadPalette_fromfile(const char* fname)
{
    BPTR file;
	unsigned char cur_pal[768];
    ULONG size;
 
	file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
    Read(file, cur_pal, size);
    Close(file);
    
    SetPalette_Video(cur_pal, size / 3);
}


/* AUDIO PART */
#ifndef MOD_PLAYER
static UBYTE channelPriority[] = { 1, 2, 4, 8 }; //request any available channel
static struct IOAudio *AudioIO[4]; //I/O block for I/O commands
static struct MsgPort *AudioMP[4]; //Message port for replies
static struct Message *AudioMSG[4];//Reply message ptr
static ULONG device[4];
#else
#include "ptplayer.h"
ULONG mod_size;
UBYTE *mod_data = NULL;
void *p_samples = NULL;
UBYTE start_pos = 0;
__chip UBYTE* mod_data;

void Load_MOD(char* fname)
{
	BPTR file;
	if (mod_data) return;
	
	file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    mod_size = Seek(file, 0, OFFSET_BEGINNING);
    mod_data = AllocMem(mod_size, MEMF_CHIP|MEMF_CLEAR);
    Read(file, mod_data, mod_size);
    Close(file);
    
    mt_init(&custom, mod_data, p_samples, start_pos);
    mt_Enable = 0;
}


void Play_MOD()
{
	mt_Enable = 1;
}

void Pause_MOD()
{
	mt_Enable = 0;
}

void Resume_MOD()
{
	mt_Enable = 1;
}


void Stop_MOD()
{
	mt_Enable = 0;
}


void Free_MOD()
{
    mt_Enable = 0;
    mt_end(&custom);
    mt_remove_cia(&custom);
    if (mod_data) FreeMem(mod_data, mod_size);
}

#endif

void Load_PCM(char* filename, struct AudioPCM* pcm, uint16_t frequency, uint8_t assigned_channel)
{
    BPTR audioFile = Open(filename, MODE_OLDFILE);
    
    Seek(audioFile, 0, OFFSET_END);
    pcm->size = Seek(audioFile, 0, OFFSET_BEGINNING);

    pcm->pcmData = (BYTE*)AllocMem(pcm->size, MEMF_CHIP|MEMF_CLEAR); //TODO: get actual size. one i'm testing is 9900 bytes
	
	Read(audioFile, pcm->pcmData, pcm->size); // sampleCount =
    
	pcm->frequency = frequency;
	
	Close(audioFile);
	
	pcm->assigned_channel = assigned_channel;
	
}


void Play_PCM(struct AudioPCM* pcm, int16_t repeat)
{
#ifdef MOD_PLAYER
	mt_soundfx(&custom,(BYTE *) pcm->pcmData, pcm->size >> 1, amiga_clock / (pcm->frequency), 64);
#else
	pcm->duration = repeat;
	
    //Set up the Audio I/O block to play our sound
	AudioIO[pcm->assigned_channel]->ioa_Request.io_Message.mn_ReplyPort = AudioMP[pcm->assigned_channel];
	AudioIO[pcm->assigned_channel]->ioa_Request.io_Command = CMD_WRITE;
	AudioIO[pcm->assigned_channel]->ioa_Request.io_Message.mn_Node.ln_Pri = -70;
	AudioIO[pcm->assigned_channel]->ioa_Request.io_Flags = ADIOF_PERVOL;
	AudioIO[pcm->assigned_channel]->ioa_Data = (BYTE *) pcm->pcmData;
	AudioIO[pcm->assigned_channel]->ioa_Length = pcm->size;
	AudioIO[pcm->assigned_channel]->ioa_Period = amiga_clock / (pcm->frequency);
	AudioIO[pcm->assigned_channel]->ioa_Volume = 64;
	AudioIO[pcm->assigned_channel]->ioa_Cycles = pcm->duration;

    BeginIO((struct IORequest *)AudioIO[pcm->assigned_channel]);
#endif
}

void Stop_PCM(struct AudioPCM* pcm)
{
#ifdef MOD_PLAYER
	mt_stopfx(&custom, pcm->assigned_channel);
#else
	AbortIO((struct IORequest *)AudioIO[pcm->assigned_channel]);
	pcm->duration = 0; // Needed to stop PCM sample although it may not immediatly stop right away
#endif
}

// This applies for both PTPlayer and Native
void Clean_PCM(struct AudioPCM* pcm)
{
	if (!pcm) return;
	
	if (pcm->pcmData)
	{
		FreeMem(pcm->pcmData, pcm->size);
		pcm->pcmData = NULL;
	}
}

int Init_Audio()
{
#ifdef MOD_PLAYER
	is_pal = (((struct GfxBase *) GfxBase)->DisplayFlags & PAL) == PAL;
    mt_install_cia(&custom, NULL, is_pal);
#else
	uint_fast8_t i;
    //Create the reply port
    for(i=0;i<4;i++)
    {
		AudioMP[i] = CreatePort(0, 0);
		if(AudioMP[i] == NULL){
			LOG("Error creating AudioMP\n");
			return 1;
		}
    
		//Set up AudioIO to access the audio device.
		AudioIO[i] = (struct IOAudio *)AllocMem(sizeof(struct IOAudio), MEMF_PUBLIC|MEMF_CLEAR);
		if(AudioIO[i] == NULL){
			LOG("Error initializing AudioIO\n");
			return 1;
		}
	
		//Set up the I/O block
		AudioIO[i]->ioa_Request.io_Message.mn_ReplyPort    = AudioMP[i];
		AudioIO[i]->ioa_Request.io_Message.mn_Node.ln_Pri  = 127;
		AudioIO[i]->ioa_Request.io_Command                 = ADCMD_ALLOCATE;
		AudioIO[i]->ioa_Request.io_Flags                   = ADIOF_NOWAIT;
		AudioIO[i]->ioa_AllocKey                           = 0;
		AudioIO[i]->ioa_Data                               = channelPriority;
		AudioIO[i]->ioa_Length                             = sizeof(channelPriority);
		LOG("I/O block initialized. Trying to open the audio device.\n");
		
		device[i] = OpenDevice(AUDIONAME, 0L, (struct IORequest *)AudioIO[i], 0L);
		if(device[i] != 0){
			LOG("Error opening audio device.\n");
			return 1;
		}
	}
    
	LOG("Device %s opened. Allocated a channel.\n", AUDIONAME);
#endif
	return 0;
}

void Close_Audio()
{
#ifdef MOD_PLAYER
	mt_remove_cia(&custom);
#endif
	// TODO for rest of audio
}

static struct IOStdReq     *cdda_io;
struct MsgPort      *cdda_mp;
static uint8_t cdda_loop = 0;
static uint8_t cdda_isplaying = 0;
static uint8_t cdda_isopen = 0;
static LONG cdda_currentrack = 0;

int Init_CD()
{
	#define DEFAULT_UNIT_NAME    "cd.device"
	#define DEFAULT_UNIT_NUMBER  0UL
    cdda_mp = CreateMsgPort();
    if (!cdda_mp) {
        return 1;
    }

    cdda_io = (struct IOStdReq *)CreateIORequest(cdda_mp, sizeof(struct IOStdReq));
    if (!cdda_io) {
        DeleteMsgPort(cdda_mp);
        return 1;
    }

    if (OpenDevice(DEFAULT_UNIT_NAME, DEFAULT_UNIT_NUMBER, (struct IORequest *)cdda_io, 0UL)) {
        DeleteIORequest((struct IORequest *)cdda_io);
        DeleteMsgPort(cdda_mp);
        return 1;
    }
    
    cdda_isopen = 1;
   
	return 0;
}

// 
void Close_CD()
{
	if (cdda_isopen == 1)
	{
		AbortIO((struct IORequest *)cdda_io);
		//WaitIO((struct IORequest *)cdda_io);
		CloseDevice((struct IORequest *)cdda_io);
		DeleteIORequest((struct IORequest *)cdda_io);
		DeleteMsgPort(cdda_mp);
		cdda_isplaying = 0;
		cdda_loop = 0;
		cdda_isopen = 0;
	}
}

BOOL Play_CD_Track(LONG trackNum, uint8_t loop) {
	if (cdda_isplaying == 1) return 1;

    cdda_io->io_Command = CD_PLAYTRACK;    /* Play audio tracks           */
    cdda_io->io_Length =  1;               /* Play this many audio tracks */
    cdda_io->io_Data = NULL;
    cdda_io->io_Offset = trackNum;

    cdda_loop = loop;
    cdda_isplaying = 1;
    cdda_currentrack = trackNum;
    
    /* send the io request */
    BeginIO((struct IORequest *)cdda_io);

    return 0;
}

void Stop_CDDA()
{
	// Won't do anything if nothing is being played
	if (cdda_isplaying == 0) return;

	cdda_isplaying = 0;
	cdda_loop = 0;
	AbortIO((struct IORequest *)cdda_io);
}

/* Needed to check if we need to repeat the CD */
void CDDA_Loop_check()
{
	if (cdda_isplaying == 1)
	{
		// If track is over
		if (CheckIO((struct IORequest *)cdda_io))
		{
			Stop_CDDA(); // Stop music
			if (cdda_loop == 1)
			{
				Play_CD_Track(cdda_currentrack, cdda_loop); // And replay it again
			}
			else
			{
				cdda_isplaying = 0;
			}
		}
	}
}
