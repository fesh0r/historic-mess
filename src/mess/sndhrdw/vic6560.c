/***************************************************************************

  MOS video interface chip 6560 (and sound interface)

  main part in vidhrdw
***************************************************************************/
#include "math.h"
#include "sound/streams.h"
#include "mess/machine/vc20.h"
#include "mess/vidhrdw/vic6560.h"
#include "mame.h"

/*
 assumed model:
 each write to a ton/noise generated starts it new
 2 integrated samples: one for tone (sinus), now for noise (?)
 each generator behaves like an timer
 when it reaches 0, the next samplevalue is given out
 */

/*
 implemented model:
 each write resets generator, amount of samples to give out for one
 period.

 the appropriate value is fetched from the sample
 fast turning off of channel
 changing note, when previous note finished

 i hope streambuffer value are sign integer and the
 DAC behaves like this
 */

#define TONE1_ON (vic6560[0xa]&0x80)
#define TONE2_ON (vic6560[0xb]&0x80)
#define TONE3_ON (vic6560[0xc]&0x80)
#define NOISE_ON (vic6560[0xd]&0x80)
#define VOLUME (vic6560[0xe]&0x0f)

#define TONE1_FREQUENCY (VIC656X_CLOCK/256/(128-((vic6560[0xa]+1)&0x7f)) )
#define TONE2_FREQUENCY (VIC656X_CLOCK/128/(128-((vic6560[0xb]+1)&0x7f)) )
#define TONE3_FREQUENCY (VIC656X_CLOCK/64/(128-((vic6560[0xc]+1)&0x7f)) )
#define NOISE_FREQUENCY (VIC656X_CLOCK/32/(128-((vic6560[0xd]+1)&0x7f)) )

static int channel,
	tonesize, noisesize, // number of samples
	tone1pos=0, tone2pos=0, tone3pos=0, noisepos=0, // pos of tone
	tone1samples=1, tone2samples=1, tone3samples=1, noisesamples=1; // count of samples to give out per tone

static INT16 *tone,*noise;

void vic6560_soundport_w(int offset, int data)
{
	int old=vic6560[offset];
	switch(offset) {
	case 0xa:
		vic6560[offset]=data;
		if (!(old&0x80)&&TONE1_ON) {
			tone1pos=0;tone1samples=options.samplerate/TONE1_FREQUENCY;
			if (tone1samples==0) tone1samples=1; // plays more than complete tone in 1 outputsample
		}
		SND_LOG(1,"vic6560",(errorlog,"tone1 %.2x %d\n",data,TONE1_FREQUENCY));
		break;
	case 0xb:
		vic6560[offset]=data;
		if (!(old&0x80)&&TONE2_ON) {
			tone2pos=0;tone2samples=options.samplerate/TONE2_FREQUENCY;
			if (tone2samples==0) tone2samples=1;
		}
		SND_LOG(1,"vic6560",(errorlog,"tone2 %.2x %d\n",data,TONE2_FREQUENCY));
		break;
	case 0xc:
		vic6560[offset]=data;
		if (!(old&0x80)&&TONE3_ON) {
			tone3pos=0;tone3samples=options.samplerate/TONE3_FREQUENCY;
			if (tone2samples==0) tone2samples=1;
		}
		SND_LOG(1,"vic6560",(errorlog,"tone3 %.2x %d\n",data,TONE3_FREQUENCY));
		break;
	case 0xd:
		vic6560[offset]=data;
		if (!(old&0x80)&&NOISE_ON) {
			noisepos=0;noisesamples=options.samplerate/NOISE_FREQUENCY;
			if (noisesamples==0) noisesamples=1;
		}
		SND_LOG(1,"vic6560",(errorlog,"noise %.2x %d\n",data,NOISE_FREQUENCY));
		break;
	case 0xe:
		vic6560[offset]=(old&~0xf)|(data&0xf);
		SND_LOG(3,"vic6560",(errorlog,"volume %d\n",data&0xf));
		break;
	}
//	stream_update(channel,0);
}

/************************************/
/* Sound handler update 			*/
/************************************/
static void vic6560_update(int param, void *buffer, int length)
{
	int i, ton1=0, ton2=0, ton3=0, nois=0, v;

	for (i=0; i<length; i++ ) {
		if (TONE1_ON/*||(tone1pos!=0)*/) {
			ton1=tone[tone1pos*tonesize/tone1samples];
			tone1pos++;
#if 0
			tone1pos%=tone1samples;
#else
			if (tone1pos>=tone1samples) {
				tone1pos=0;tone1samples=options.samplerate/TONE1_FREQUENCY;
				if (tone1samples==0) tone1samples=1;
			}
#endif
		} else ton1=0;
		if (TONE2_ON/*||(tone2pos!=0)*/) {
			ton2=tone[tone2pos*tonesize/tone2samples];
			tone2pos++;
#if 0
			tone2pos%=tone2samples;
#else
			if (tone2pos>=tone2samples) {
				tone2pos=0;tone2samples=options.samplerate/TONE2_FREQUENCY;
				if (tone2samples==0) tone2samples=1;
			}
#endif
		} else ton2=0;
		if (TONE3_ON/*||(tone3pos!=0)*/) {
			ton3=tone[tone3pos*tonesize/tone3samples];
			tone3pos++;
#if 0
			tone3pos%=tone3samples;
#else
			if (tone3pos>=tone3samples) {
				tone3pos=0;tone3samples=options.samplerate/TONE3_FREQUENCY;
				if (tone3samples==0) tone3samples=1;
			}
#endif
		} else ton3=0;
		if (NOISE_ON/*||(noisepos!=0)*/) {
			nois=noise[noisepos*noisesize/noisesamples];
			noisepos++;
#if 0
			noisepos%=noisesamples;
#else
			if (noisepos>=noisesamples) {
				noisepos=0;noisesamples=options.samplerate/NOISE_FREQUENCY;
				if (noisesamples==0) noisesamples=1;
			}
#endif
		} else nois=0;
		v=((ton1+ton2+ton3+nois)*VOLUME)>>8;
		if (v>127) ((char*)buffer)[i]=127;
		else if (v<-127) ((char*)buffer)[i]=-127;
		else ((char*)buffer)[i]=v;
	}
}

/************************************/
/* Sound handler start				*/
/************************************/
int vic6560_custom_start(const struct MachineSound* driver)
{
	int i;

	// slowest played sample
	tonesize=options.samplerate/(VIC656X_CLOCK/256/(256-128) );
	if (errorlog) fprintf(errorlog,"rate:%d bits:%d\n",options.samplerate, options.samplebits);
	if (errorlog) fprintf(errorlog,"vic6560 rate:%d bits:%d size:%d\n",options.samplerate, options.samplebits, tonesize);

	channel = stream_init("PC speaker", 50, options.samplerate, 8, 0, vic6560_update);

	tone=malloc(tonesize*sizeof(tone[0]));
	if (!tone)
		return 1;

	noisesize=tonesize/4;
	noise=malloc(noisesize*sizeof(noise[0]));
	if (!noise) { free(tone);return 1; }

	for (i=0; i<tonesize; i++) {
		tone[i]=sin(2*M_PI*i/tonesize)*2048; // +- 2048, sum of 4 channels * volume make +- 30720
	}

	for (i=0; i<noisesize; i++) {
		noise[i]=(rand()*2.0/RAND_MAX-1)*2048;
	}
	return 0;
}

/************************************/
/* Sound handler stop				*/
/************************************/
void vic6560_custom_stop(void)
{
	free(tone);
	free(noise);
}

void vic6560_custom_update(void) {}

