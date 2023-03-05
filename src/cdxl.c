/************************************************************************
**********                                                     **********
**********                     CDXL Tools                      **********
**********                     ----------	               **********
**********                                                     **********
**********           Copyright (C) Pantaray Inc. 1991          **********
**********               Ukiah, CA  707-462-4878               **********
**********                All Rights Reserved.                 **********
**********                                                     **********
*************************************************************************
***
***  MODULE:
***
***	Example.c
***
***  PURPOSE:
***
***	Show example of simple CDXL player
***
***  HISTORY:
***
***	1.00 1a22 Ken Yeast	First Release.
***	1.01 1b12 Ken Yeast	Added back in audio support
***	1.02 1c17 Ken Yeast	Use Size for frame size
***				CDTV_NAME
***				Used Intuition to center screen in a more compatible
***				way.
***				Forgot to CloseDevice and DeletePort on CDTVIOReq!
***	1.03 1c23 Ken Yeast	Changed to ask Audio.device for channels
***				Also to double buffer the audio
***				Init/CDPLAYER_QuitCDTV
***				Init/CDPLAYER_QuitViewInfo
***				StartAudio
***	1.04 1c27 Ken Yeast	OK, LoadRGB4 is too slow.  Changed to create view/etc.
***				and then copy and use the resulting copper lists.
***				CopyCopper
***				CopyColors
***	1.05 2102 Ken Yeast	Cleanup and preload two frames
***	1.06 2115 Ken Yeast	Removed DOSBase
***	1.07 2120 Ken Yeast	Example does little stdio and is mostly using Wait,
***				so we can just check for the SIGBREAKF_CTRL_C signal.
***				Will triple buffer if audio is present
***				Some cleanup
***	1.08 2120 Ken Yeast	Second Release
***	1.09 2128 Ken Yeast	Can be played multiple times or forever
***				StopAudio
***	     2218 Ken Yeast	Checked out with some XL streams on CDTV
***				Stop one frame early
***	1.10 2303 Ken Yeast	Official Second Release
***
************************************************************************/


#define	VER 1
#define	REV 10


#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include <exec/types.h>
#include <exec/memory.h>
#include <exec/lists.h>
#include <exec/interrupts.h>
#include <exec/io.h>
#include <devices/audio.h>
#include <libraries/dos.h>
#include <graphics/gfx.h>
#include <graphics/gfxbase.h>
#include <graphics/view.h>
#include <graphics/copper.h>
#include <hardware/custom.h>
#include <hardware/dmabits.h>
#include <hardware/intbits.h>

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/graphics.h>
#include <proto/intuition.h>

#include <clib/alib_protos.h>

#include "internal_cdxl.h"
#include "pan.h"

#include <intuition/intuition.h>
#include <clib/exec_protos.h>
#include <clib/graphics_protos.h>
#include <clib/intuition_protos.h>
#include <clib/lowlevel_protos.h>
#include <clib/exec_protos.h>
#include <clib/dos_protos.h>
#include <clib/alib_protos.h>
#include <graphics/gfxbase.h>



/***********************************************************************
***
***  Definitions
***
************************************************************************/

typedef		short			SHORT;
typedef		char *			CSTR;

#define		INTDIV( a, b )		( ( (a) + ( (b) / 2 ) ) / (b) )

// 130 * CDTV_SECTOR_SIZE
// ./agaconv myvideo.webm a.cdxl --std-cdxl --color-mode=ocs5 --width=256 --height=160 --frequency=11025 --audio-mode=mono --fps=10

// 150 * CDTV_SECTOR_SIZE
// ./agaconv myvideo.webm a.cdxl --std-cdxl --color-mode=ocs5 --width=256 --height=192 --frequency=11025 --audio-mode=mono --fps=10

// 200 * CDTV_SECTOR_SIZE
// ./agaconv myvideo.mp4 a.cdxl --std-cdxl --color-mode=ham6 --width=256 --height=192 --frequency=11025 --audio-mode=mono --fps=10
// Also applies for EHB video mode

// 210 * CDTV_SECTOR_SIZE
// ./agaconv myvideo.mp4 a.cdxl --std-cdxl --color-mode=ham8 --width=256 --height=160 --frequency=11025 --audio-mode=mono --fps=10

// 230 * CDTV_SECTOR_SIZE
// ./agaconv myvideo.webm a.cdxl --std-cdxl --color-mode=ocs5 --width=256 --height=192 --frequency=11025 --audio-mode=mono --fps=15

#define		CDTV_SECTOR_SIZE	2048

/*
#ifdef CD32
#define		CDXL_SPEED_BPS		( 134 * CDTV_SECTOR_SIZE )
#else
#define		CDXL_SPEED_BPS		( 75 * CDTV_SECTOR_SIZE )
#endif
*/

static uint32_t CDXL_SPEED_BPS;

// Buffering
#define		MAX_NODES		3
#define		NEXT_FRAME( n, m )	( ( (n) == ( m - 1 ) ) ? 0 : ( (n) + 1 ) )

// Copper lists
#define		COPPER_BUFFER_SIZE	2048
#define		COPPER_END		0xfffffffe
#define		COPPER_MOVE_COLOR0	( 0x0180 )

// Audio has different periods on NTSC vs PAL
#define		NTSC_FREQ		3579545
#define		PAL_FREQ		3546895

// Hardware Interrupt control
#define		ENABLE_INTR( f )	CustomPtr->intena = (INTF_SETCLR | (f) )
#define		DISABLE_INTR( f )	CustomPtr->intena = (f)
#define		CLEAR_INTR( f )		CustomPtr->intreq = (f)
#define		ENABLE_DMA( f )		CustomPtr->dmacon = (DMAF_SETCLR | DMAF_MASTER | (f) )
#define		DISABLE_DMA( f )	CustomPtr->dmacon = (f)


/***********************************************************************
***
***  Function Prototypes
***
***********************************************************************/

void	main( LONG, CSTR * );
#ifdef __GNUC__
void AudioIntrCode( void );
void CDXLIntrCode(struct CDXL * );
#else
void	__interrupt __saveds AudioIntrCode( void );
void	__interrupt __saveds __asm CDXLIntrCode( register __a2 struct CDXL * );
#endif
void	CDXLPlay( ULONG, ULONG, ULONG, LONG );
void	CopyColors( UWORD *, UWORD *, SHORT );
UWORD	*CopyCopper( UWORD * );
void	Error( CSTR );
void	CDPLAYER_Init( void );
void	InitAudio( void );
void	InitCDTV( void );
void	InitCDXL( void );
static void	InitViewInfo( void );
void	CDPLAYER_Quit( LONG );
void	CDPLAYER_QuitAudio( void );
void	CDPLAYER_QuitCDTV( void );
void	CDPLAYER_QuitCDXL( void );
void	CDPLAYER_QuitViewInfo( void );
void	SetViewInfo( LONG, PAN *, ULONG );
void	StartAudio( void );
void	StopAudio( void );


/***********************************************************************
***
***  Global Variables
***
***********************************************************************/

extern struct	GfxBase *GfxBase;

// Program
static struct	Task *OurTask			= NULL;

// cdtv.device
static struct	IOStdReq *CDTVIOReq		= NULL;
static struct	MsgPort *CDTVIOPort		= NULL;

// CDXL
static SHORT	MaxNodes			= MAX_NODES;
static struct	MinList	CDXLList;
static struct	CDXL	CDXLNodes[ MAX_NODES ];

// Files
static CSTR	CDXLFile			= NULL;
static ULONG	CDXLSector			= 0;
static ULONG	CDXLNumSectors			= 0;
static ULONG	CDXLNumFrames			= 0;

// CDXL Frame
static PAN	FirstPan;
static UBYTE	*FrameBuffer[ MAX_NODES ];
static UWORD	*FrameCMap[ MAX_NODES ];
static UWORD	*FrameAudio[ MAX_NODES ];
static ULONG	Count				= 0;

// Display
static struct	View	 CDXLView[ MAX_NODES ];
static struct	ViewPort CDXLVP[ MAX_NODES ];
static struct	BitMap	 CDXLBM[ MAX_NODES ];
static struct	RasInfo	 CDXLRI[ MAX_NODES ];
static UWORD	*Copper[ MAX_NODES ];
static UWORD	*Color[ MAX_NODES ];

// Signals
static LONG	SwitchSignal			= -1;
static LONG	SwitchSignalBit			= -1;

// Misc
static struct	Custom *CustomPtr		= (struct Custom *) 0x00dff000;

// Audio
static SHORT	AudioHandlerInUse 		= FALSE;
static struct	Interrupt AudioInterrupt	= { NULL, NULL, NT_INTERRUPT, 5, "AI_Example", NULL, (void (*)( void )) &AudioIntrCode };
static struct	Interrupt *OldAudioVector	= NULL;
static struct	IOAudio AudioMain;
static struct	MsgPort *AudioMainPort		= NULL;
static ULONG	AudioFrame			= 0;

static uint_fast8_t CanStopProgram = 0;


/***********************************************************************
***
***  main
***
***	main entry point
***
***********************************************************************/

void CDPLAYER_PlayVideo(LONG NumPlays, char* fname, uint32_t speed, uint_fast8_t InputStopPlayback)
{
	CDXLFile = fname;
	CDXL_SPEED_BPS = speed * CDTV_SECTOR_SIZE;
	CanStopProgram = InputStopPlayback;
	
	CDPLAYER_Init();

	CDXLPlay( CDXLSector, CDXLNumSectors, CDXLNumFrames, NumPlays );

	CDPLAYER_Quit( RETURN_OK );
}


/***********************************************************************
***
***  AudioIntrCode
***
************************************************************************/

#ifdef __GNUC__
void AudioIntrCode(
	void
	)
	{
	CLEAR_INTR( INTF_AUD0 );

	AudioFrame = NEXT_FRAME( AudioFrame, MaxNodes );
	CustomPtr->aud[ 0 ].ac_ptr = CustomPtr->aud[ 1 ].ac_ptr = FrameAudio[ AudioFrame ];
	}
#else
void __interrupt __saveds AudioIntrCode(
	void
	)
	{
	CLEAR_INTR( INTF_AUD0 );

	AudioFrame = NEXT_FRAME( AudioFrame, MaxNodes );
	CustomPtr->aud[ 0 ].ac_ptr = CustomPtr->aud[ 1 ].ac_ptr = FrameAudio[ AudioFrame ];
	}
#endif

/***********************************************************************
***
***  CDXLIntrCode
***
************************************************************************/
#ifdef __GNUC__
void CDXLIntrCode(struct CDXL *NodeCompleted) {
    Count++;
    Signal(OurTask, SwitchSignal);
}

#else
void __interrupt __saveds __asm CDXLIntrCode(register __a2 struct CDXL *NodeCompleted)
{
	Count++;
	Signal( OurTask, SwitchSignal );
}
#endif


/***********************************************************************
***
***  CDXLPlay
***
***	Note:
***	This example assumes a CDXL file with all frames equal.
***	Video with audio uses triple buffering, vs. double buffering.
***	Play until (NumFrames-1)
***
************************************************************************/

void CDXLPlay(
	ULONG Sector,
	ULONG NumSectors,
	ULONG NumFrames,
	LONG NumPlays
	)
	{
	ULONG BufferNum;
	LONG PortSignal;
	LONG SignalMask;
	LONG SignalIn;
	SHORT FramesToBuffer;
	SHORT NumColors		= ( CMAP_SIZE( &FirstPan ) >> 1 );
	BOOL ForEver		= ( NumPlays == -1 );
	BOOL AudioStarted;
	BOOL StopProgram	= FALSE;
	uint32_t state0;

	do
		{
		// CDXL counters and flags
		Count		= 0;
		BufferNum	= 0;
		AudioFrame	= 0;
		AudioStarted	= FALSE;
		FramesToBuffer	= ( ( MaxNodes > 2 ) ? ( MaxNodes - 2 ) : 0 );

		// Signal from device, make sure it is clear.
		PortSignal	= ( 1 << CDTVIOPort->mp_SigBit );
		SetSignal( 0, PortSignal );
		SignalMask	= ( SwitchSignal | PortSignal | SIGBREAKF_CTRL_C );

		// Start CDXL with cdtv.device
		CDTVIOReq->io_Command = CD_READXL;
		CDTVIOReq->io_Offset  = Sector;
		CDTVIOReq->io_Length  = NumSectors;
		CDTVIOReq->io_Data    = (APTR) CDXLList.mlh_Head;
		SendIO( (struct IORequest *) CDTVIOReq );

		while ( Count < ( NumFrames - 1 ) )
			{
			SignalIn = Wait( SignalMask );
			if ( SignalIn & SIGBREAKF_CTRL_C )
				{
				StopProgram = TRUE;
				break;
				}

			if ( SignalIn & PortSignal )
				break;

			// May need to buffer some frames
			if ( FramesToBuffer > 0 )
				{
				FramesToBuffer--;
				continue;
				}

			// Start audio only once
			if ( ( ! AudioStarted ) && FirstPan.AudioSize )
				{
				AudioStarted = TRUE;
				StartAudio();
				}

			// Insert colors and display frame
			CopyColors( FrameCMap[ BufferNum ], Color[ BufferNum ], NumColors );
			GfxBase->LOFlist = Copper[ BufferNum ];

			// Switch to other buffer
			BufferNum = NEXT_FRAME( BufferNum, MaxNodes );
			}

		// May need to turn off audio
		if ( AudioStarted )
			{
			StopAudio();
			AudioStarted = FALSE;
			}

		// This correctly terminates a CDXL
		AbortIO( (struct IORequest *) CDTVIOReq );
		WaitIO(  (struct IORequest *) CDTVIOReq );

		// Then a SEEK is required to force the drive to stop (sometimes necessary)
		CDTVIOReq->io_Command = CDTV_SEEK;
		CDTVIOReq->io_Offset  = 0;
		CDTVIOReq->io_Length  = 0;
		CDTVIOReq->io_Data    = NULL;
		(void) DoIO( (struct IORequest *) CDTVIOReq );

		state0 = ReadJoyPort(0);

		if (CanStopProgram)
		{
			if ((state0 & JPF_BUTTON_RED) == JPF_BUTTON_RED)
			{
				break;
			}
		}
		
		if ( StopProgram )
			break;

		} while ( ForEver || ( --NumPlays > 0 ) );
	}


/***********************************************************************
***
***  chkabort
***
***	Remove Lattice's CTRL-C
***
************************************************************************/

void chkabort(
	void
	)
	{
	}


/***********************************************************************
***
***  CopyColors
***
************************************************************************/

void CopyColors(
	UWORD *CMap,
	UWORD *CopperCMap,
	SHORT NumColors
	)
	{
	do
		{
		// Skip over Load instruction
		CopperCMap++;

		// Copy color
		*CopperCMap++ = *CMap++;
		} while ( --NumColors );
	}


/***********************************************************************
***
***  CopyCopper
***
************************************************************************/

UWORD *CopyCopper(
	UWORD *Copper
	)
	{
	UWORD *Colors	 = ( GfxBase->LOFlist + 3 );
	ULONG *Ptr	 = (ULONG *) GfxBase->LOFlist;
	ULONG *CopperPtr = (ULONG *) Copper;
	SHORT i;

	for ( i = 0; i < ( COPPER_BUFFER_SIZE >> 2 ); i++ )
		{
		*CopperPtr = *Ptr;

		// Look for start of color table
		if ( ( *( (UWORD *) Ptr ) & 0x1f ) == COPPER_MOVE_COLOR0 )
			Colors = (UWORD *) CopperPtr;

		// Check for end of copper
		if ( *Ptr == COPPER_END )
			break;

		Ptr++;
		CopperPtr++;
		}

	return( Colors );
	}


/***********************************************************************
***
***  CXBRK
***
***	Remove Lattice's CTRL-C
***
************************************************************************/

void CXBRK(
	void
	)
	{
	}


/***********************************************************************
***
***  Error
***
***********************************************************************/

void Error(
	CSTR str
	)
	{
	puts( str );
	CDPLAYER_Quit( RETURN_FAIL );
	}


/***********************************************************************
***
***  Init
***
***********************************************************************/

void CDPLAYER_Init(
	void
	)
	{
	/*
	if ( ! ( GfxBase = (struct GfxBase *) OpenLibrary( "graphics.library", 0 ) ) )
		Error( "NOT an Amiga System" );

	if ( ! ( IntuitionBase = (struct IntuitionBase *) OpenLibrary( "intuition.library", 0 ) ) )
		Error( "Intuition not available" );
*/
	// Setup for communications with CDTV
	InitCDTV();

	// Setup CDXL variables and read first pan for sizes
	InitCDXL();

	// Audio
	InitAudio();

	// setup for view
	InitViewInfo();
	}


/***********************************************************************
***
***  InitAudio
***
***	Four audio requests because we are double buffering left & right.
***	Ask for left and right
***
***	left,	right,	right,	left
***	1,	2,	4,	8
***
************************************************************************/

void InitAudio(
	void
	)
	{
	static UBYTE AskChannel[] =
		{
		( 1 + 2 ),
		( 1 + 4 ),
		( 4 + 8 ),
		( 2 + 8 )
		};
	ULONG Frequency;
	LONG AudioRate;
	UWORD AudioPeriod;

	// May not be necessary
	if ( FirstPan.AudioSize == 0 )
		return;

	// Setup request for initial channel allocation
	if ( ! ( AudioMainPort = CreatePort( 0, 0 ) ) )
		Error( "Could not create port for audio" );

	AudioMain.ioa_Request.io_Message.mn_ReplyPort	= AudioMainPort;
	AudioMain.ioa_Request.io_Message.mn_Node.ln_Pri	= 127;
	AudioMain.ioa_Request.io_Command		= ADCMD_ALLOCATE;
	AudioMain.ioa_Request.io_Flags			= ADIOF_NOWAIT;
	AudioMain.ioa_AllocKey				= 0;
	AudioMain.ioa_Data				= AskChannel;
	AudioMain.ioa_Length				= sizeof( AskChannel );

	if ( OpenDevice( "audio.device", 0, (struct IORequest *) &AudioMain, 0 ) )
		Error( "Cannot obtain audio channels" );

	// Setup audio interrupt
	CLEAR_INTR( INTF_AUD0 );
	OldAudioVector	  = SetIntVector( INTB_AUD0, &AudioInterrupt );
	AudioHandlerInUse = TRUE;

	// Calculate period
	Frequency	  = ( ( GfxBase->DisplayFlags & PAL ) ? PAL_FREQ : NTSC_FREQ );
	AudioRate	  = INTDIV( ( CDXL_SPEED_BPS * FirstPan.AudioSize ), FirstPan.Size );
	AudioPeriod	  = (UWORD) INTDIV( Frequency, AudioRate );

	// Set hardware
	CustomPtr->aud[ 0 ].ac_vol = CustomPtr->aud[ 1 ].ac_vol = 60;
	CustomPtr->aud[ 0 ].ac_per = CustomPtr->aud[ 1 ].ac_per = AudioPeriod;
	CustomPtr->aud[ 0 ].ac_ptr = CustomPtr->aud[ 1 ].ac_ptr = FrameAudio[ 0 ];
	CustomPtr->aud[ 0 ].ac_len = CustomPtr->aud[ 1 ].ac_len = ( FirstPan.AudioSize / sizeof( UWORD) );
	}


/***********************************************************************
***
***  InitCDTV
***
************************************************************************/

void InitCDTV(
	void
	)
	{
	if ( ! ( CDTVIOPort = CreatePort( 0, 0 ) ) )
		Error( "Cannot create a CDTV port" );

	if ( ! ( CDTVIOReq = CreateStdIO( CDTVIOPort ) ) )
		Error( "Cannot attach CDTV StdIO" );

	if ( OpenDevice( CDTV_NAME, 0, (struct IORequest *) CDTVIOReq, 0 ) )
		Error( "CDTV Device will not open" );
	}


/***********************************************************************
***
***  InitCDXL
***
***	- Get the sector for CDXL (where it starts)
***	- Get size (# of sectors, # of frames)
***	- Read first pan for sizes
***	- Double buffer for video, triple buffer if using audio as well
***	- Allocate frame buffers and prepare CDXL list
***	- Signal for frame changing
***
************************************************************************/

void InitCDXL(
	void
	)
	{
	struct	FileLock *FileLock;
	struct	FileInfoBlock *fib;
	ULONG	Size;
	LONG	fsLock;
	LONG	File;
	UBYTE	fibbuf[ ( sizeof( struct FileInfoBlock ) + 3 ) ];
	SHORT	i;

	if ( ! ( fsLock = Lock( CDXLFile, ACCESS_READ ) ) )
		Error( "CDXL file not available" );

	FileLock = (struct FileLock *) BADDR( fsLock );

	// Pluck the sector right out of public part of the structure
	CDXLSector = FileLock->fl_Key;

	// Get size of file.
	// (Force longword alignment of fib pointer.)
	fib = (struct FileInfoBlock *) ( (LONG) ( fibbuf + 3 ) & ~3L );
	Examine( fsLock, fib );
	Size = fib->fib_Size;

	UnLock( fsLock );

	// Get the first PAN
	if ( ! ( File = Open( CDXLFile, MODE_OLDFILE ) ) )
		Error( "CDXL file not available" );

	if ( Read( File, &FirstPan, PAN_SIZE ) != PAN_SIZE )
		{
		Close( File );
		Error( "Not a CDXL file" );
		}

	Close( File );

	CDXLNumSectors = ( Size / CDTV_SECTOR_SIZE );
	CDXLNumFrames  = ( Size / FirstPan.Size );

	// Video with audio need triple buffering, video just double
	MaxNodes = ( ( FirstPan.AudioSize ) ? 3 : 2 );

	// Now that we know size of frame, allocate our two buffers, set ptrs,
	// and prepare CDXL List (static)
	NewList( (struct List *) &CDXLList );

	for ( i = 0; i < MaxNodes; i++ )
		{
			
		if ( ! ( FrameBuffer[ i ] = AllocMem( FirstPan.Size, ( MEMF_CHIP | MEMF_CLEAR ) ) ) )
			Error( "Not enough memory for CDXL frame buffers" );

		FrameCMap[ i ]	= (UWORD *) ( FrameBuffer[ i ] + PAN_SIZE );
		FrameAudio[ i ]	= (UWORD *) ( FrameBuffer[ i ] + FRAME_SIZE( &FirstPan ) - FirstPan.AudioSize );

		AddTail( (struct List *) &CDXLList, (struct Node *) &CDXLNodes[ i ] );
		CDXLNodes[ i ].Buffer	= FrameBuffer[ i ];
		CDXLNodes[ i ].Length	= FirstPan.Size;
		CDXLNodes[ i ].DoneFunc	= CDXLIntrCode;
		}

	// Tie list together to give us endless loop
	CDXLList.mlh_Head->mln_Pred     = CDXLList.mlh_TailPred;
	CDXLList.mlh_TailPred->mln_Succ = CDXLList.mlh_Head;

	// Signal used when frame is ready
	if ( ( SwitchSignalBit = AllocSignal( -1 ) ) == -1 )
		Error( "Cannot get signals" );

	SwitchSignal = ( 1 << SwitchSignalBit );

	// setup for CDXLIntrCode
	OurTask = FindTask( NULL );
	}


/***********************************************************************
***
***  InitViewInfo
***
************************************************************************/

static void InitViewInfo(
	void
	)
	{
	LONG File;
	SHORT i;

	// Setup views
	if ( ! ( File = Open( CDXLFile, MODE_OLDFILE ) ) )
		Error( "CDXL file not available" );

	for ( i = 0; i < MaxNodes; i++ )
		SetViewInfo( File, &FirstPan, i );

	Close( File );
	}


/***********************************************************************
***
***  CDPLAYER_Quit
***
***********************************************************************/

void CDPLAYER_Quit(LONG Code
)
	{

	static SHORT CDPLAYER_Quitting = FALSE;

	if ( CDPLAYER_Quitting )
		return;

	CDPLAYER_Quitting = TRUE;
/*
	if ( IntuitionBase )
		RemakeDisplay();
*/
	CDPLAYER_QuitViewInfo();

	CDPLAYER_QuitAudio();

	CDPLAYER_QuitCDXL();

	CDPLAYER_QuitCDTV();

/*
	if ( IntuitionBase )
		CloseLibrary( (struct Library *) IntuitionBase );

	if ( GfxBase )
		CloseLibrary( (struct Library *) GfxBase );

*/
	//exit( Code );
	}


/***********************************************************************
***
***  CDPLAYER_QuitAudio
***
************************************************************************/

void CDPLAYER_QuitAudio(
	void
	)
	{
	if ( AudioHandlerInUse )
		{
		StopAudio();
		SetIntVector( INTB_AUD0, OldAudioVector );
		}

	if ( AudioMain.ioa_Request.io_Device )
		{
		AudioMain.ioa_Request.io_Command = ADCMD_FREE;
		DoIO( (struct IORequest *) &AudioMain );

		CloseDevice( (struct IORequest *) &AudioMain );

		if ( AudioMainPort )
			DeletePort( AudioMainPort );
		}
	}


/***********************************************************************
***
***  CDPLAYER_QuitCDTV
***
************************************************************************/

void CDPLAYER_QuitCDTV(
	void
	)
	{
	if ( CDTVIOReq )
		{
		if ( CDTVIOReq->io_Device )
			CloseDevice( (struct IORequest *) CDTVIOReq );

		DeleteStdIO( CDTVIOReq );
		}

	if ( CDTVIOPort )
		DeletePort( CDTVIOPort );
	}


/***********************************************************************
***
***  CDPLAYER_QuitCDXL
***
************************************************************************/

void CDPLAYER_QuitCDXL(
	void
	)
	{
	SHORT i;

	if ( SwitchSignalBit != -1 )
		FreeSignal( SwitchSignalBit );

	for ( i = 0; i < MaxNodes; i++ )
		if ( FrameBuffer[ i ] )
			FreeMem( FrameBuffer[ i ], FirstPan.Size );
	}


/***********************************************************************
***
***  CDPLAYER_QuitViewInfo
***
************************************************************************/

void CDPLAYER_QuitViewInfo(
	void
	)
	{
	SHORT i;

	for ( i = 0; i < MaxNodes; i++ )
		{
		if ( Copper[ i ] )
			FreeMem( Copper[ i ], COPPER_BUFFER_SIZE );

		FreeVPortCopLists( &CDXLVP[ i ] );

		if ( CDXLVP[ i ].ColorMap )
			FreeColorMap( CDXLVP[ i ].ColorMap );

		if ( CDXLView[ i ].LOFCprList )
			FreeCprList( CDXLView[ i ].LOFCprList );

		if ( CDXLView[ i ].SHFCprList )
			FreeCprList( CDXLView[ i ].SHFCprList );
		}
	}


/***********************************************************************
***
***  SetViewInfo
***
***	Somebody tell me a better way of centering a view, overscan or
***	not...
***
************************************************************************/

void SetViewInfo(
	LONG File,
	PAN *Pan,
	ULONG Num
	)
	{
	struct View *CurrentView;
	UBYTE *Buffer;
	LONG PlaneSize;
	LONG i;
	SHORT OriginCenterX;
	SHORT OriginCenterY;
	SHORT VP_X;
	SHORT VP_Y;
	SHORT NewX;
	SHORT NewY;
	SHORT GoHiRes = FALSE;
	SHORT GoLace  = FALSE;

	// Init structures
	InitView( &CDXLView[ Num ] );
	InitVPort( &CDXLVP[ Num ] );

	if ( ! ( CDXLVP[ Num ].ColorMap = GetColorMap( 64 ) ) )
		Error( "Not enough memory for ColorMap" );

	if ( ! ( Copper[ Num ] = AllocMem( COPPER_BUFFER_SIZE, MEMF_CHIP ) ) )
		Error( "Not enough memory for Copper Lists" );

	// Pre-Fill Buffers
	if ( Read( File, FrameBuffer[ Num ], Pan->Size ) != Pan->Size )
		Error( "Not a CDXL file" );

	// InterLace and High Resolution guesses
	if ( Pan->XSize >= 640 )
		GoHiRes = TRUE;

	if ( Pan->YSize >= 400 )
		GoLace = TRUE;

	CDXLView[ Num ].ViewPort = &CDXLVP[ Num ];

	// Choose mode
	CDXLView[ Num ].Modes = 0;
	switch ( PI_VIDEO( Pan ) )
		{
		case ( PIV_HAM ):
			CDXLView[ Num ].Modes = HAM;
			break;

		case ( PIV_YUV ):
			Error( "Cannot handle YUV video" );
			break;

		case ( PIV_AVM ):
			GoHiRes = TRUE;
			break;

		case ( PIV_STANDARD ):
			if ( Pan->PixelSize == 6 )
				CDXLView[ Num ].Modes = EXTRA_HALFBRITE;
			break;

		default:
			Error( "Unknown PAN Info" );
			break;
		}

	if ( GoLace )
		CDXLView[ Num ].Modes |= LACE;

	CDXLVP[ Num ].Modes = CDXLView[ Num ].Modes;

	// NOTE: The view does not care about HIRES
	if ( GoHiRes )
		CDXLVP[ Num ].Modes |= HIRES;

	CDXLVP[ Num ].DWidth   = Pan->XSize;
	CDXLVP[ Num ].DHeight  = Pan->YSize;
	CDXLVP[ Num ].RasInfo  = &CDXLRI[ Num ];
	CDXLRI[ Num ].RxOffset = 0;
	CDXLRI[ Num ].RyOffset = 0;
	CDXLRI[ Num ].Next     = 0;

	// Center images (use view and viewport to center small images)
	// Remember:
	//	CurrentView->DxOffset & OriginCenterX are lo-res
	//	CurrentView->DyOffset & OriginCenterY are non-interlace
	//	Views are lo-res specifications ONLY!
	//	ViewPorts may be lo-res or hi-res
	CurrentView	= ViewAddress();
	OriginCenterX	= ( CurrentView->ViewPort->DWidth  >> ( ( CurrentView->ViewPort->Modes & HIRES ) ? 2 : 1 ) );
	OriginCenterY	= ( CurrentView->ViewPort->DHeight >> ( ( CurrentView->ViewPort->Modes & LACE  ) ? 2 : 1 ) );

	NewX = ( CurrentView->DxOffset + OriginCenterX - ( Pan->XSize >> ( ( GoHiRes ) ? 2 : 1 ) ) );
	NewY = ( CurrentView->DyOffset + OriginCenterY - ( Pan->YSize >> ( ( GoLace  ) ? 2 : 1 ) ) );

	// Check Min for View
	if ( NewX < 98 )
		NewX = 98;

	if ( NewY < 25 )
		NewY = 25;

	// Check Max for View
	if ( NewX > 129 )
		{
		VP_X = ( NewX - 129 );
		NewX = 129;
		}
	else
		VP_X = 0;

	if ( NewY > 56 )
		{
		VP_Y = ( NewY - 56 );
		NewY = 56;
		}
	else
		VP_Y = 0;

	CDXLView[ Num ].DxOffset = NewX;
	CDXLView[ Num ].DyOffset = NewY;
	CDXLVP[ Num ].DxOffset   = ( VP_X << ( ( GoHiRes ) ? 1 : 0 ) );
	CDXLVP[ Num ].DyOffset   = VP_Y;

	// Set Bit Plane Ptr's, handle interleaved (for display)
	Buffer = ( FrameBuffer[ Num ] + PAN_SIZE + CMAP_SIZE( Pan ) );
	InitBitMap( &CDXLBM[ Num ], Pan->PixelSize, Pan->XSize, Pan->YSize );

	if ( PI_PIXEL( Pan ) == PIF_LINES )
		{
		PlaneSize = CDXLBM[ Num ].BytesPerRow;

		CDXLBM[ Num ].BytesPerRow *= Pan->PixelSize;
		}
	else
		PlaneSize = ( CDXLBM[ Num ].BytesPerRow * Pan->YSize );

	for ( i = 0; i < Pan->PixelSize; i++ )
		CDXLBM[ Num ].Planes[ i ] = &Buffer[ ( i * PlaneSize ) ];

	// Make View
	LoadRGB4( &CDXLVP[ Num ], FrameCMap[ Num ], CMAP_SIZE( Pan ) );
	CDXLRI[ Num ].BitMap = &CDXLBM[ Num ];
	MakeVPort( &CDXLView[ Num ], &CDXLVP[ Num ] );
	MrgCop( &CDXLView[ Num ] );
	LoadView( &CDXLView[ Num ] );

	Color[ Num ] = CopyCopper( Copper[ Num ] );
	}


/***********************************************************************
***
***  StartAudio
***
***	Start interrupts for switching audio buffers
***	Set Ptr and Length
***
************************************************************************/

void StartAudio(
	void
	)
	{
	CLEAR_INTR( INTF_AUD0 );
	ENABLE_DMA( DMAF_AUD0 );
	ENABLE_DMA( DMAF_AUD1 );
	ENABLE_INTR( INTF_AUD0 );
	}


/***********************************************************************
***
***  StopAudio
***
************************************************************************/

void StopAudio(
	void
	)
	{
	DISABLE_DMA( DMAF_AUD0 );
	DISABLE_DMA( DMAF_AUD1 );
	DISABLE_INTR( INTF_AUD0 );
	CLEAR_INTR( INTF_AUD0 );
	}
