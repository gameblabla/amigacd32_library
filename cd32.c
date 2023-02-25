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

static LONG amiga_clock = 3546895; // For PAL. For NTSC : 3579545

struct IntuitionBase	*IntuitionBase = NULL;
struct GfxBase			*GfxBase = NULL;
struct Library			*LowLevelBase = NULL;

struct ViewPort *vport;
struct RastPort *rport;
struct Window *window = NULL;
struct Screen *screen = NULL;
struct BitMap *myBitMaps;
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

VOID	freeBitMaps(struct BitMap *myBitMaps, LONG depth, LONG width, LONG height)
{
	freePlanes(myBitMaps, depth, width, height);
	FreeMem(myBitMaps, (LONG)sizeof(struct BitMap));
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

struct RastPort temprp2;
ULONG table[256*3+1+1];
UBYTE toggle=0;

inline void UpdateScreen_Video()
{	
	WriteChunkyPixels(&temprp2,0,0,WIDTH-1,((HEIGHT)-1),gfxbuf,vwidth);
}

inline void UpdateScreen_Video_partial(uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{	
	WriteChunkyPixels(&temprp2,x,y,x+internal_width,y+internal_height,gfxbuf,w);
}


void SetPalette_Video(const uint8_t *palette, uint16_t numberofcolors)
{
    UWORD i;
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
}

/* Init Video library, errors out if it encounters issues */

int Init_Video(uint16_t w, uint16_t h, uint16_t iwidth, uint16_t iheight)
{
	SysBase = *(struct ExecBase **)4;
	if (!(SysBase->AttnFlags & AFF_68020))
	{
		LOG("This needs a 68020+ CPU!\n");
		return 0;
	}
	
 	LowLevelBase 	= OpenLibrary("lowlevel.library",39);

	if ((IntuitionBase = (struct IntuitionBase *)OpenLibrary("intuition.library",0)) == NULL)
	{
		LOG("No intution library\n");
		return 1;
	}

	/* Open graphics.library */
	if ((GfxBase = (struct GfxBase *) OpenLibrary("graphics.library", 0)) == NULL)
	{
		LOG("No graphics library\n");
		return 1;
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
	
	myBitMaps = AllocMem((LONG)sizeof(struct BitMap), MEMF_CHIP|MEMF_CLEAR);
	InitBitMap(myBitMaps, RBMDEPTH, RBMWIDTH, RBMHEIGHT);
	setupPlanes(myBitMaps, RBMDEPTH, RBMWIDTH, RBMHEIGHT);
	
	if (screen = OpenScreenTags( NULL,
		SA_Width, WIDTH,
		SA_Height, HEIGHT,
		SA_Depth, DEPTH,
		SA_DisplayID, mode,
		SA_Quiet, TRUE,
		SA_Type, CUSTOMSCREEN|CUSTOMBITMAP,
		SA_BitMap,myBitMaps,
		TAG_DONE))
		{
			vport = &screen->ViewPort;
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

	InitRastPort(&temprp2);
	temprp2.Layer=0;
	temprp2.BitMap=myBitMaps;
	SetPointer(window,emptypointer,1,16,0,0);
	
	// Check if we're NTSC or PAL
    struct ViewPort* vp = &(((struct Screen*)GfxBase->ActiView)->ViewPort);
    ULONG modeid = GetVPModeID(vp);
    if (modeid == LORESLACE_KEY || modeid == HIRESLACE_KEY || modeid == SUPERLACE_KEY) {
        amiga_clock = 3546895; // We are in PAL mode
    } else {
		amiga_clock = 3579545;
    }
	
	return 0;
}

/* Drawing stuff */

void LoadFile_tobuffer(const char* fname, uint8_t* buffer)
{
    BPTR file;
    ULONG size;
 
	file = Open(fname, MODE_OLDFILE);
    Seek(file, 0, OFFSET_END);
    size = Seek(file, 0, OFFSET_BEGINNING);
    Read(file, buffer, size);
    Close(file);
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
static UBYTE channelPriority[] = { 1, 2, 4, 8 }; //request any available channel
static struct IOAudio *AudioIO[4]; //I/O block for I/O commands
static struct MsgPort *AudioMP[4]; //Message port for replies
static struct Message *AudioMSG[4];//Reply message ptr
static ULONG device[4];

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
}

void Stop_PCM(struct AudioPCM* pcm)
{
	AbortIO((struct IORequest *)AudioIO[pcm->assigned_channel]);
	pcm->duration = 0; // Needed to stop PCM sample although it may not immediatly stop right away
}

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
	int i;
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
	return 0;
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
