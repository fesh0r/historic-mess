/***************************************************************************

    sndhrdw/pc.c

	Functions to emulate a PC PIC timer 2 used for simple tone generation

	TODO:
	Add support for the SN76496 used in the Tandy1000/PCjr
	It most probably is on port 0xc0, but I'm not sure...

****************************************************************************/
#include "sound/streams.h"
#include "mess/machine/pc.h"

#define BASECLOCK	1193180

extern int PIT_clock[3];

static int channel = 0;
static int baseclock = 0;
static int speaker_gate = 0;

/************************************/
/* Sound handler start				*/
/************************************/
int pc_sh_start(void)
{
	if (errorlog) fprintf(errorlog, "pc_sh_start\n");
	channel = stream_init("PC speaker", 50, Machine->sample_rate, 8, 0, pc_sh_update);
    return 0;
}

int pc_sh_custom_start(const struct MachineSound* driver) { 
	return pc_sh_start();
}

/************************************/
/* Sound handler stop				*/
/************************************/
void pc_sh_stop(void)
{
	if (errorlog) fprintf(errorlog, "pc_sh_stop\n");
}

void pc_sh_speaker(int mode)
{
	if( mode == speaker_gate )
		return;

    stream_update(channel,0);

    switch( mode )
	{
		case 0: /* completely off */
			SND_LOG(1,"PC_speaker",(errorlog,"off\n"));
			speaker_gate = 0;
            break;
		case 1: /* completely on */
			SND_LOG(1,"PC_speaker",(errorlog,"on\n"));
			speaker_gate = 1;
            break;
		case 2: /* play the tone */
			SND_LOG(1,"PC_speaker",(errorlog,"tone\n"));
			speaker_gate = 2;
            break;
    }
	speaker_gate = mode;
}

void pc_sh_custom_update(void) {}

/************************************/
/* Sound handler update 			*/
/************************************/
void pc_sh_update(int param, void *buffer, int length)
{
	static int incr = 0, signal = 127;
	signed char *sample = buffer;
	register int i, rate = Machine->sample_rate / 2;

	if( PIT_clock[2] )
		baseclock = BASECLOCK / PIT_clock[2];
	else
		baseclock = BASECLOCK / 65536;

	switch( speaker_gate )
	{
	case 0: /* speaker off */
		for( i = 0; i < length; i++ )
			sample[i] = 0;
		break;
	case 1: /* speaker on */
		for( i = 0; i < length; i++ )
			sample[i] = 127;
		break;
	case 2: /* speaker gate tone from PIT channel #2 */
		for( i = 0; i < length; i++ )
		{
			sample[i] = signal;
			incr -= baseclock;
			while( incr < 0 )
			{
				incr += rate;
				signal = -signal;
			}
		}
	}
}

