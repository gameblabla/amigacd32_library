#include "audio_paula.h"

volatile struct Custom* custom;
extern uint8_t is_pal;
extern LONG amiga_clock;

#define	CDTV_PLAYTRACK		43

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


static uint8_t cdda_loop = 0;
static uint8_t cdda_isplaying = 0;
static uint8_t cdda_isopen = 0;
static LONG cdda_currentrack = 0;
static struct IOStdReq     *cdda_io;
struct MsgPort      *cdda_mp;
BYTE unit = 0; // The CDTV device unit number


int Init_CD()
{
#ifdef CDTV
	if (!(cdda_mp = CreatePort(0, 0))) {
	  return 1;
	}

	if (!(cdda_io = CreateStdIO(cdda_mp))) {
	  DeletePort(cdda_mp);
	  return 1;
	}

	if (OpenDevice("cdtv.device", 0, (struct IORequest *) cdda_io, 0)) {
	  DeletePort(cdda_mp);
	  DeleteExtIO((struct IORequest*)cdda_io);
	  return 1;
	}
#else
    cdda_mp = CreateMsgPort();
    if (!cdda_mp) {
        return 1;
    }

    cdda_io = (struct IOStdReq *)CreateIORequest(cdda_mp, sizeof(struct IOStdReq));
    if (!cdda_io) {
        DeleteMsgPort(cdda_mp);
        return 1;
    }

    if (OpenDevice("cd.device", 0UL, (struct IORequest *)cdda_io, 0UL)) {
        DeleteIORequest((struct IORequest *)cdda_io);
        DeleteMsgPort(cdda_mp);
        return 1;
    }
#endif

    cdda_isopen = 1;
   
	return 0;
}

// 
void Close_CD()
{
#ifdef CDTV
	if (cdda_isopen == 1)
	{
		AbortIO((struct IORequest *)cdda_io);
		CloseDevice((struct IORequest *)cdda_io);
		DeleteIORequest((struct IORequest *)cdda_io);
		DeletePort(cdda_mp);
		cdda_isplaying = 0;
		cdda_loop = 0;
		cdda_isopen = 0;
	}
#else
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
#endif
}

BOOL Play_CD_Track(LONG trackNum, uint8_t loop) {
	if (cdda_isplaying == 1) return 1;

#ifdef CDTV
	cdda_io->io_Command = CDTV_PLAYTRACK;
	cdda_io->io_Offset  = trackNum;		// Track to start
	cdda_io->io_Length  = trackNum+1;	// Ending track
	cdda_io->io_Data    = NULL;
	BeginIO( (struct IORequest *) cdda_io);
#else
	// For CD32, it's a little different but it's also better
    cdda_io->io_Command = CD_PLAYTRACK;
    cdda_io->io_Length =  1;
    cdda_io->io_Data = NULL;
    cdda_io->io_Offset = trackNum;
    
    /* send the io request */
    BeginIO((struct IORequest *)cdda_io);
#endif

    cdda_loop = loop;
    cdda_isplaying = 1;
    cdda_currentrack = trackNum;

    return 0;
}

void Stop_CDDA()
{
	// Won't do anything if nothing is being played
	if (cdda_isplaying == 0) return;

	cdda_isplaying = 0;
	cdda_loop = 0;
#ifdef CDTV
	AbortIO((struct IORequest *)cdda_io);

	cdda_io->io_Command = CMD_STOP;
	cdda_io->io_Offset  = 0;
	cdda_io->io_Length  = 0;
	cdda_io->io_Data    = NULL;
	BeginIO( (struct IORequest *) cdda_io);
#else
	AbortIO((struct IORequest *)cdda_io);
#endif
}

/* Needed to check if we need to repeat the CD */
void CDDA_Loop_check()
{
	if (cdda_isplaying == 1)
	{
		// If track is over
		if (CheckIO((struct IORequest *)cdda_io))
		{
#ifndef CDTV
			Stop_CDDA(); // Stop music
#endif
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
