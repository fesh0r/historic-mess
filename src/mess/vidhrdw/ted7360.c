/***************************************************************************

	TExt Display 7360

***************************************************************************/
#include <math.h>
#include "mess/machine/c16.h"
#include "mess/vidhrdw/ted7360.h"


#define VREFRESHINLINES 28

#define TIMER1HELPER (ted7360[0]|(ted7360[1]<<8))
#define TIMER2HELPER (ted7360[2]|(ted7360[3]<<8))
#define TIMER3HELPER (ted7360[4]|(ted7360[5]<<8))
#define TIMER1 (TIMER1HELPER?TIMER1HELPER:0x10000)
#define TIMER2 (TIMER2HELPER?TIMER2HELPER:0x10000)
#define TIMER3 (TIMER3HELPER?TIMER3HELPER:0x10000)

#define VERTICALPOS (ted7360[6]&7)
#define HORICONTALPOS (ted7360[7]&7)
#define SCREENOFF (ted7360[6]&0x10)
#define TEST (ted7360[6]&0x80)
#define HIRESON (ted7360[6]&0x20)
#define MULTICOLORON (ted7360[7]&0x10)
#define REVERSEON (!(ted7360[7]&0x80))
// hardware inverts character when bit 7 set (character taken &0x7f)
// instead of fetching character with higher number!
#define LINES25 (ted7360[6]&8) // else 24 Lines
#define LINES (LINES25?25:24)
#define YSIZE (LINES*8)
#define COLUMNS40 (ted7360[7]&8) // else 38 Columns
#define COLUMNS (COLUMNS40?40:38)
#define XSIZE (COLUMNS*8)
#define INROM (ted7360[0x12]&4)

#define RASTERLINE ( ((ted7360[0xa]&1)<<8)|ted7360[0xb])
#define CURSOR1POS ( ted7360[0xd]|((ted7360[0xc]&3)<<8) )
#define CURSOR2POS ( ted7360[0x1b]|((ted7360[0x1a]&3)<<8) )
#define CURSORRATE ( (ted7360[0x1f]&0x7c)>>2 )

#define BACKGROUNDCOLOR (Machine->pens[ted7360[0x15]&0x7f])
#define FOREGROUNDCOLOR (Machine->pens[ted7360[0x16]&0x7f])
#define MULTICOLOR1 (Machine->pens[ted7360[0x17]&0x7f])
#define MULTICOLOR2 (Machine->pens[ted7360[0x18]&0x7f])
#define FRAMECOLOR (Machine->pens[ted7360[0x19]&0x7f])

#define CHARGENADDR ( ((ted7360[0x13]&0xfc)<<8)|(INROM?0x10000:0) )
#define BITMAPADDR ((ted7360[0x12]&0x38)<<10)
#define VIDEOADDR ((ted7360[0x14]&0xf8)<<8)

#define XPOS 0
#define YPOS 0

unsigned char ted7360_palette[] = {
// black, white, red, cyan
// purple, green, blue, yellow
// orange, light orange, pink, light cyan,
// light violett, light green, light blue, light yellow
// these 16 colors are 8 times here in different luminance (dark..light)
// taken from digitized tv screenshot
0x06,0x01,0x03, 0x2b,0x2b,0x2b, 0x67,0x0e,0x0f, 0x00,0x3f,0x42,
0x57,0x00,0x6d, 0x00,0x4e,0x00, 0x19,0x1c,0x94, 0x38,0x38,0x00,
0x56,0x20,0x00, 0x4b,0x28,0x00, 0x16,0x48,0x00, 0x69,0x07,0x2f,
0x00,0x46,0x26, 0x06,0x2a,0x80, 0x2a,0x14,0x9b, 0x0b,0x49,0x00,

0x00,0x03,0x02, 0x3d,0x3d,0x3d, 0x75,0x1e,0x20, 0x00,0x50,0x4f,
0x6a,0x10,0x78, 0x04,0x5c,0x00, 0x2a,0x2a,0xa3, 0x4c,0x47,0x00,
0x69,0x2f,0x00, 0x59,0x38,0x00, 0x26,0x56,0x00, 0x75,0x15,0x41,
0x00,0x58,0x3d, 0x15,0x3d,0x8f, 0x39,0x22,0xae, 0x19,0x59,0x00,

0x00,0x03,0x04, 0x42,0x42,0x42, 0x7b,0x28,0x20, 0x02,0x56,0x59,
0x6f,0x1a,0x82, 0x0a,0x65,0x09, 0x30,0x34,0xa7, 0x50,0x51,0x00, 
0x6e,0x36,0x00, 0x65,0x40,0x00, 0x2c,0x5c,0x00, 0x7d,0x1e,0x45, 
0x01,0x61,0x45, 0x1c,0x45,0x99, 0x42,0x2d,0xad, 0x1d,0x62,0x00, 

0x05,0x00,0x02, 0x56,0x55,0x5a, 0x90,0x3c,0x3b, 0x17,0x6d,0x72, 
0x87,0x2d,0x99, 0x1f,0x7b,0x15, 0x46,0x49,0xc1, 0x66,0x63,0x00,
0x84,0x4c,0x0d, 0x73,0x55,0x00, 0x40,0x72,0x00, 0x91,0x33,0x5e, 
0x19,0x74,0x5c, 0x32,0x59,0xae, 0x59,0x3f,0xc3, 0x32,0x76,0x00,

0x02,0x01,0x06, 0x84,0x7e,0x85, 0xbb,0x67,0x68, 0x45,0x96,0x96, 
0xaf,0x58,0xc3, 0x4a,0xa7,0x3e, 0x73,0x73,0xec, 0x92,0x8d,0x11, 
0xaf,0x78,0x32, 0xa1,0x80,0x20, 0x6c,0x9e,0x12, 0xba,0x5f,0x89, 
0x46,0x9f,0x83, 0x61,0x85,0xdd, 0x84,0x6c,0xef, 0x5d,0xa3,0x29, 

0x02,0x00,0x0a, 0xb2,0xac,0xb3, 0xe9,0x92,0x92, 0x6c,0xc3,0xc1, 
0xd9,0x86,0xf0, 0x79,0xd1,0x76, 0x9d,0xa1,0xff, 0xbd,0xbe,0x40, 
0xdc,0xa2,0x61, 0xd1,0xa9,0x4c, 0x93,0xc8,0x3d, 0xe9,0x8a,0xb1, 
0x6f,0xcd,0xab, 0x8a,0xb4,0xff, 0xb2,0x9a,0xff, 0x88,0xcb,0x59, 

0x02,0x00,0x0a, 0xc7,0xca,0xc9, 0xff,0xac,0xac, 0x85,0xd8,0xe0,
0xf3,0x9c,0xff, 0x92,0xea,0x8a, 0xb7,0xba,0xff, 0xd6,0xd3,0x5b,
0xf3,0xbe,0x79, 0xe6,0xc5,0x65, 0xb0,0xe0,0x57, 0xff,0xa4,0xcf,
0x89,0xe5,0xc8, 0xa4,0xca,0xff, 0xca,0xb3,0xff, 0xa2,0xe5,0x7a,

0x01,0x01,0x01, 0xff,0xff,0xff, 0xff,0xf6,0xf2, 0xd1,0xff,0xff,
0xff,0xe9,0xff, 0xdb,0xff,0xd3, 0xfd,0xff,0xff, 0xff,0xff,0xa3,
0xff,0xff,0xc1, 0xff,0xff,0xb2, 0xfc,0xff,0xa2, 0xff,0xee,0xff,
0xd1,0xff,0xff, 0xeb,0xff,0xff, 0xff,0xf8,0xff, 0xed,0xff,0xbc
};

struct CustomSound_interface ted7360_sound_interface = {
	ted7360_custom_start,
	ted7360_custom_stop,
	ted7360_custom_update
};

UINT8 ted7360[0x20]={0};

bool ted7360_pal;
bool ted7360_rom;

static void *timer1=0, *timer2=0, *timer3=0, *rastertimer;
static bool cursor1=false;
static bool framecolorchanged;
static int(*vic_dma_read)(int);

#ifndef RASTERLINEBASED
static UINT8 chargendirty[256]={0};
//static UINT8 bitmapdirty[8*40*25]={0};
#else
static UINT8 screen[200][320];
static int lastline=0;
static void drawlines(int first, int last);

#endif


static void ted7360_set_interrupt(int mask)
{
	ted7360[9]|=mask;
	if ( (ted7360[0xa]&ted7360[9]&0x5e) ) {
		if (!(ted7360[9]&0x80) ) {
			ted7360[9]|=0x80;
			c16_interrupt(1);
		}
	}
}

static void ted7360_clear_interrupt(int mask)
{
	ted7360[9]&=~mask;
	if ((ted7360[9]&0x80)&&!(ted7360[9]&ted7360[0xa]&0x5e)) {
		ted7360[9]&=~0x80;
		c16_interrupt(0);
	}
}

static int rasterline(void)
{
	double a=timer_get_time();

	return (int)((a-floor(a))*TED7360_VRETRACERATE*TED7360_LINES)
			 %TED7360_LINES;
}

static int rastercolumn(void)
{
	double a=timer_get_time();

	return (int)((a-floor(a))*TED7360_VRETRACERATE*TED7360_LINES*57*8)
			 %(57*8);
}

#define TEDTIME_IN_CYCLES(a) ((double)(a)/TED7360_CLOCK)
#define TEDTIME_TO_CYCLES(a) ((int)((a)*TED7360_CLOCK))

static void ted7360_timer_timeout(int which)
{
	VIC_LOG(3,"ted7360 ",(errorlog,"timer %d timeout\n",which));
	switch (which) {
	case 1:
		timer1=timer_set(TEDTIME_IN_CYCLES(0x10000),1, ted7360_timer_timeout );
		ted7360_set_interrupt(8);
		break;
	case 2:
		timer2=timer_set(TEDTIME_IN_CYCLES(0x10000),2, ted7360_timer_timeout );
		ted7360_set_interrupt(0x10);
		break;
	case 3:
		timer3=timer_set(TEDTIME_IN_CYCLES(0x10000),3, ted7360_timer_timeout );
		ted7360_set_interrupt(0x40);
		break;
	case 4:
		rastertimer=timer_set(1.0/TED7360_VRETRACERATE, 4, ted7360_timer_timeout);
		ted7360_set_interrupt(2);
		break;
	}
}

static void set_rasterline(void)
{
	if (RASTERLINE>=TED7360_LINES) {
		if (rastertimer) {
			timer_remove(rastertimer);rastertimer=0;
		}
	} else {
		int diff;
		double time;
		if (RASTERLINE-rasterline()>0) diff=(RASTERLINE-rasterline())%TED7360_LINES;
		else diff=RASTERLINE+TED7360_LINES-rasterline();
		time=(double)diff/(TED7360_VRETRACERATE*TED7360_LINES);
		if (rastertimer) {
			timer_reset(rastertimer,time);
		} else {
			rastertimer=timer_set(time, 4, ted7360_timer_timeout);
		}
	}
}

void ted7360_init(int(*dma_read)(int), int pal)
{
	vic_dma_read=dma_read;
	ted7360_pal=pal;
}

void ted7360_frame_interrupt(void)
{
	static int count;
	if ((ted7360[0x1f]&0xf)>=0x0f) {
//	if (count>=CURSORRATE) {
		if (CURSOR1POS<videoram_size) dirtybuffer[CURSOR1POS]=1;
		cursor1^=true;
		ted7360[0x1f]&=~0xf;
		count=0;
	} else ted7360[0x1f]++;
}

int ted7360_vh_start(void)
{
	videoram_size=1000;
	framecolorchanged=true;
	return generic_vh_start();
}

void ted7360_vh_stop(void)
{
	 generic_vh_stop();
}

void ted7360_port_w(int offset, int data)
{
	int old;
	VIC_LOG(3,"ted7360_port_w",(errorlog,"%.2x:%.2x\n",offset,data));
	switch (offset) {
	case 0xe:case 0xf:case 0x10:case 0x11:case 0x12:
		ted7360_soundport_w(offset,data);
		break;
	}
	switch (offset) {
	// timer zaehlen runter, unterschreiten sie null gibt es einen
	// interrrupt, und beginnen von 65535 wieder nach unten zu zaehlen
	case 0: // stop timer 1
		ted7360[offset]=data;
		if (timer1) { timer_remove(timer1);timer1=0; }
		VIC_LOG(1,"ted7360 ",(errorlog,"stop timer 1 %.4x\n",TIMER1));
		ted7360_clear_interrupt(8);
		break;
	case 1: // start timer 1
		ted7360[offset]=data;
		VIC_LOG(1,"ted7360 ",(errorlog,"start timer 1 %.4x\n",TIMER1));
		if (!timer1) timer1=timer_set(TEDTIME_IN_CYCLES(TIMER1),1, ted7360_timer_timeout );
		else timer_reset(timer1,TEDTIME_IN_CYCLES(TIMER1) );
		ted7360_clear_interrupt(8);
		break;
	case 2: // stop timer 2
		ted7360[offset]=data;
		if (timer2) { timer_remove(timer2);timer2=0; }
		VIC_LOG(1,"ted7360 ",(errorlog,"stop timer 2 %.4x\n",TIMER2));
		ted7360_clear_interrupt(0x10);
		break;
	case 3: // start timer 2
		ted7360[offset]=data;
		VIC_LOG(1,"ted7360 ",(errorlog,"start timer 2 %.4x\n",TIMER2));
		if (!timer2) timer2=timer_set(TEDTIME_IN_CYCLES(TIMER2),2, ted7360_timer_timeout );
		else timer_reset(timer2,TEDTIME_IN_CYCLES(TIMER2) );
		ted7360_clear_interrupt(0x10);
		break;
	case 4: // stop timer 3
		ted7360[offset]=data;
		if (timer3) { timer_remove(timer3);timer3=0; }
		VIC_LOG(1,"ted7360 ",(errorlog,"stop timer 3 %.4x\n",TIMER3));
		ted7360_clear_interrupt(0x40);
		break;
	case 5: // start timer 3
		ted7360[offset]=data;
		VIC_LOG(1,"ted7360 ",(errorlog,"start timer 3 %.4x\n",TIMER3));
		if (!timer3) timer3=timer_set(TEDTIME_IN_CYCLES(TIMER3),3, ted7360_timer_timeout );
		else timer_reset(timer3,TEDTIME_IN_CYCLES(TIMER3) );
		ted7360_clear_interrupt(0x40);
		break;
	case 6:
		if (ted7360[offset]!=data) {
#ifdef RASTERLINEBASED
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
			VIC_LOG(1,"ted7360_port_w",(errorlog,"%s %s %s %s %s vertical %d\n",
						TEST?"test":"",data&0x40?"ECM":"",HIRESON?"hires":"",
						SCREENOFF?"":"screen", LINES25?"25lines":"24lines", VERTICALPOS));
		}
		break;
	case 7:
		if (ted7360[offset]!=data) {
#ifdef RASTERLINEBASED
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
			VIC_LOG(1,"ted7360_port_w",(errorlog,"%s %s %s %s %s horicontal %d\n",
						REVERSEON?"reverseon":"",data&0x40?"pal":"ntsc",data&0x20?"hori freeze":"",
						MULTICOLORON?"multicolor":"", COLUMNS40?"40columns":"38columns", HORICONTALPOS));
		}
		break;
	case 8:
//			VIC_LOG(1,"ted7360_port_r",(errorlog,"keyboard write %.2x\n",data));
		ted7360[offset]=data;
		break;
	case 9:
		old=ted7360[offset];
		ted7360[offset]&=0x80;
		ted7360[offset]|=data&0x7f;
		if (RASTERLINE!=rasterline()) ted7360[offset]&=~2;
		if (old&0x80) ted7360_clear_interrupt(0);
		else ted7360_set_interrupt(0);
		break;
	case 0xa:
		old=data;
		ted7360[offset]=data|0xa0;
		ted7360[9]=(ted7360[9]&0xa1)|(ted7360[9]&data&0x5e);
		if (ted7360[9]&0x80) ted7360_clear_interrupt(0);
		if ((data^old)&1) {
			set_rasterline();
//			DBG_LOG(1,"set rasterline hi",(errorlog,"soll:%d\n",RASTERLINE));
		}
		break;
	case 0xb:
		if (data!=ted7360[offset]) {
			ted7360[offset]=data;
			set_rasterline();
//			DBG_LOG(1,"set rasterline lo",(errorlog,"soll:%d\n",RASTERLINE));
		}
		ted7360_clear_interrupt(2);
		break;
	case 0xc:case 0xd:
#ifdef RASTERLINEBASED
		if (ted7360[offset]!=data) {
			drawlines(lastline, rasterline());
			ted7360[offset]=data;
		}
#else
		if (CURSOR1POS<videoram_size) dirtybuffer[CURSOR1POS]=1;
		ted7360[offset]=data;
		if (CURSOR1POS<videoram_size) dirtybuffer[CURSOR1POS]=1;
#endif
		break;
	case 0x12:
		if (ted7360[offset]!=data) {
#ifdef RASTERLINEBASED
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
			VIC_LOG(1,"ted7360_port_w",(errorlog,"bitmap %.4x %s\n",
						BITMAPADDR,INROM?"rom":"ram"));
		}
		break;
	case 0x13:
		if (ted7360[offset]!=data) {
#ifdef RASTERLINEBASED
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
			VIC_LOG(1,"ted7360_port_w",(errorlog,"chargen %.4x %s %d\n",
						CHARGENADDR,data&2?"":"doubleclock", data &1));
		}
		break;
	case 0x14:
		if (ted7360[offset]!=data) {
#ifdef RASTERLINEBASED
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
			VIC_LOG(1,"ted7360_port_w",(errorlog,"videoram %.4x\n",
						VIDEOADDR));
		}
		break;
	case 0x15: case 0x16: case 0x17: case 0x18: case 0x19:
#ifdef RASTERLINEBASED
		if (ted7360[offset]!=data) {
			drawlines(lastline, rasterline());
#else
			memset(dirtybuffer,1, videoram_size);
#endif
			ted7360[offset]=data;
		}
		break;
	case 0x1c:
		ted7360[offset]=data; //?
		VIC_LOG(1,"ted7360_port_w",(errorlog,"write to rasterline high %.2x\n",data));
		break;
	case 0x1f:
		ted7360[offset]=data;
		VIC_LOG(1,"ted7360_port_w",(errorlog,"write to cursorblink %.2x\n",data));
		break;
	default:
		ted7360[offset]=data;
		break;
	}
}

int ted7360_port_r(int offset)
{
	int val=0;

	switch(offset) {
	case 0: // timer 1 low byte
		if (timer1) val=TEDTIME_TO_CYCLES(timer_timeleft(timer1))&0xff;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 1 lo read %.2x\n",val));
		break;
	case 1: // timer 1 high byte
		if (timer1) val=TEDTIME_TO_CYCLES(timer_timeleft(timer1))>>8;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 1 hi read %.2x\n",val));
		break;
	case 2: // timer 2 low byte
		if (timer2) val=TEDTIME_TO_CYCLES(timer_timeleft(timer2))&0xff;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 2 lo read %.2x\n",val));
		break;
	case 3: // timer 2 high byte
		if (timer2) val=TEDTIME_TO_CYCLES(timer_timeleft(timer2))>>8;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 2 hi read %.2x\n",val));
		break;
	case 4: // timer 3 low byte
		if (timer3) val=TEDTIME_TO_CYCLES(timer_timeleft(timer3))&0xff;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 3 lo read %.2x\n",val));
		break;
	case 5: // timer 3 high byte
		if (timer3) val=TEDTIME_TO_CYCLES(timer_timeleft(timer3))>>8;
		else val=ted7360[offset];
//		VIC_LOG(1,"ted7360_port_r",(errorlog,"timer 3 hi read %.2x\n",val));
		break;
	case 7:
		val=(ted7360[offset]&~0x40);
		if (!ted7360_pal) val|=0x40;
		break;
	case 8:
		val=c16_read_keyboard();
		break;
	case 9:
		val=ted7360[offset];
		break;
	case 0xa:
		val=ted7360[offset];
		//ted7360_clear_interrupt(2);
		break;
	case 0xb:
		val=ted7360[offset];
		ted7360_clear_interrupt(2);
		break;
	case 0x13:
		val=ted7360[offset]&~1;
		if (ted7360_rom) val|=1;
		break;
	case 0x1c: //rasterline
		val=((rasterline()&0x100)>>8)|0xfe; // expected by matrix
		break;
	case 0x1d: //rasterline
		val=rasterline()&0xff;
		break;
	case 0x1e: //rastercolumn
		val=rastercolumn()/2;
		break;
	case 0x1f:
		val=((rasterline()&7)<<4)|(ted7360[offset]&0x0f);
		VIC_LOG(1,"ted7360_port_w",(errorlog,"read from cursorblink %.2x\n",val));
		break;
	default:
		val=ted7360[offset];
		break;
	}
	VIC_LOG(3,"ted7360_port_r",(errorlog,"%.2x:%.2x\n",offset,val));
	return val;
}

#ifndef RASTERLINEBASED
void ted7360_addr_w(int offset, int data)
{
	int chargen=CHARGENADDR, video=VIDEOADDR, bitmap=BITMAPADDR;

	if (DIPMEMORY==MEMORY16K) {
		if (chargen<=0x10000) chargen&=0x3fff;
		video&=0x3fff;
		bitmap&=0x3fff;
	} else if (DIPMEMORY==MEMORY32K) {
		if (chargen<=0x10000) chargen&=0x7fff;
		video&=0x7fff;
		bitmap&=0x7fff;
	}
	if (!HIRESON&&(offset>=chargen)&&(offset<chargen+0x800))
		chargendirty[(offset-chargen)>>8]=1;
	if ( ( (offset>=video)&&(offset<video+videoram_size) )
		||( (offset>=video+0x400)&&(offset<video+0x400+videoram_size) ) )
		dirtybuffer[(offset-video)&0x3ff]=true;
	if (HIRESON&&(offset>=bitmap)&&(offset<bitmap+40*25*8))
		dirtybuffer[(offset-bitmap)>>3]=true;
}
#endif

static int DOCLIP(struct rectangle *r1, const struct rectangle *r2)
{
	 if (r1->min_x > r2->max_x) return 0;
	 if (r1->max_x < r2->min_x) return 0;
	 if (r1->min_y > r2->max_y) return 0;
	 if (r1->max_y < r2->min_y) return 0;
	 if (r1->min_x < r2->min_x) r1->min_x = r2->min_x;
	 if (r1->max_x > r2->max_x) r1->max_x = r2->max_x;
	 if (r1->min_y < r2->min_y) r1->min_y = r2->min_y;
	 if (r1->max_y > r2->max_y) r1->max_y = r2->max_y;
	return 1;
}

#ifndef RASTERLINEBASED
static void draw_character(struct osd_bitmap *bitmap,
									struct rectangle *visible,
									int ch, int xoff, int yoff,
									int bgcolor, int color)
{
	int i,j,y,x,code;

	for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {

		if (vic_dma_read) {
			code=vic_dma_read(CHARGENADDR+ch*8+j);
		} else {
			code=0xff;
		}
		for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
			if ((code<<i)&0x80) {
				bitmap->line[y][x]=color;
			} else {
				bitmap->line[y][x]=bgcolor;
			}
		}
	}
}

static void draw_character_multi(struct osd_bitmap *bitmap,
									struct rectangle *visible,
									int ch, int xoff, int yoff,
									int bgcolor, int color)
{
	int i,j,y,x,code;

	for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {
		if (vic_dma_read) {
			code=vic_dma_read(CHARGENADDR+ch*8+j);
		} else {
			code=0xff;
		}
		for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
			switch((code<<(i&~1))&0xc0) {
			case 0xc0: bitmap->line[y][x]=color;break;
			case 0x80: bitmap->line[y][x]=MULTICOLOR1;break;
			case 0x40: bitmap->line[y][x]=FOREGROUNDCOLOR;break;
			case 0: bitmap->line[y][x]=bgcolor;break;
			}
		}
	}
}

static void ted7360_drawcharacter(struct osd_bitmap *bitmap, int full_refresh)
{
	int sx, sy, offs, col, row;
	/* for every character in the Video RAM, check if it or its
		attribute has been modified since last time and update it
		 accordingly. */
	 for (offs=COLUMNS40?0:1, col=0, row=0, sx=offs*8,sy=0; offs<videoram_size; offs++) {
		int ch;
		  if (vic_dma_read) {
				ch=vic_dma_read((VIDEOADDR|0x400)+offs);
		  } else {
				ch=0xff;
		  }
		  if (full_refresh||dirtybuffer[offs]||chargendirty[ch]) {
				struct rectangle r;
				dirtybuffer[offs] = 0;

				r.min_x = sx+XPOS;
				r.max_x = sx+XPOS+8-1;
				r.min_y = sy+YPOS;
				r.max_y = sy+YPOS+8-1;
				if (DOCLIP(&r,&Machine->drv->visible_area)) {
					int attr;
					if (vic_dma_read) {
						attr=vic_dma_read(((VIDEOADDR)+offs));
					} else {
						attr=0xff;
					}
					osd_mark_dirty (r.min_x,r.min_y,r.max_x,r.max_y,0);
					/* draw the character */
					if (cursor1 && (offs==CURSOR1POS)) {
						int x,y;

						for (y=r.min_y; y<=r.max_y; y++) {
							for (x=r.min_x; x<=r.max_x; x++) {
								bitmap->line[y][x]=Machine->pens[attr&0x7f];
							}
						}
					} else if (REVERSEON&&(ch&0x80)) {
						if (MULTICOLORON&&(attr&8)) {
							draw_character_multi(bitmap,&r,ch&~0x80, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
									BACKGROUNDCOLOR,Machine->pens[attr&0x77]);
						} else { // multicolor and hardware reverse?
							draw_character(bitmap,&r,ch&~0x80, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
									Machine->pens[attr&0x7f],BACKGROUNDCOLOR);
						}
					} else {
						if (MULTICOLORON&&(attr&8)) {
							draw_character_multi(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
									BACKGROUNDCOLOR,Machine->pens[attr&0x77]);
						} else {
							draw_character(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
									BACKGROUNDCOLOR,Machine->pens[attr&0x7f]);
						}
					}

				}
		  }
		  sx+=8;col++;
		  if (col>=COLUMNS) {
				col=0; row++;
				sx=0; sy+=8;
				if (!COLUMNS40) {
					offs+=2;
					sx=8;
				}
		  }
	}
	memset(chargendirty,0,sizeof(chargendirty));
}

static void draw_bitmap(struct osd_bitmap *bitmap,
									struct rectangle *visible,
									int ch, int xoff, int yoff,
									int bgcolor, int color)
{
	int i,j,y,x,code;

	for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {

		if (vic_dma_read) {
			code=vic_dma_read(BITMAPADDR+ch*8+j);
		} else {
			code=0xff;
		}
		for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
			if ((code<<i)&0x80) {
				bitmap->line[y][x]=color;
			} else {
				bitmap->line[y][x]=bgcolor;
			}
		}
	}
}

static void draw_bitmap_multi(struct osd_bitmap *bitmap,
									struct rectangle *visible,
									int ch, int xoff, int yoff,
									int bgcolor, int color)
{
	int i,j,y,x,code;

	for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {
		if (vic_dma_read) {
			code=vic_dma_read(BITMAPADDR+ch*8+j);
		} else {
			code=0xff;
		}
		for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
			switch((code<<(i&~1))&0xc0) {
			case 0xc0: bitmap->line[y][x]=FOREGROUNDCOLOR;break;
			case 0x80: bitmap->line[y][x]=color;break;
			case 0x40: bitmap->line[y][x]=bgcolor;break;
			case 0: bitmap->line[y][x]=BACKGROUNDCOLOR;break;
			}
		}
	}
}

static void ted7360_drawbitmap(struct osd_bitmap *bitmap, int full_refresh)
{
	int sx, sy, offs, col, row;
	/* for every character in the Video RAM, check if it or its
		attribute has been modified since last time and update it
		 accordingly. */
	 for (offs=COLUMNS40?0:1,sx =offs*8,sy=0, col=0, row=0; offs<videoram_size; offs++) {
		  if (full_refresh||dirtybuffer[offs]) {

				struct rectangle r;
#if 0
				if ( (offs==0)&&errorlog) {
					fprintf(errorlog,"chargen:%.4x videoram:%.4x xsize:%d ysize:%d\n",
								CHARGENADDR,VIDEOADDR, xsize, YSIZE);
				}
#endif
				dirtybuffer[offs] = 0;

				r.min_x = sx+XPOS;
				r.max_x = sx+XPOS+8-1;
				r.min_y = sy+YPOS;
				r.max_y = sy+YPOS+8-1;
				if (DOCLIP(&r,&Machine->drv->visible_area)) {
					int attr, ch, c1, c2;
					  if (vic_dma_read) {
						ch=vic_dma_read((VIDEOADDR|0x400)+offs);
					  } else {
						ch=0xff;
					  }
					if (vic_dma_read) {
						attr=vic_dma_read(((VIDEOADDR)+offs));
					} else {
						attr=0xff;
					}
					c1=((ch>>4)&0xf)|(attr<<4);
					c2=(ch&0xf)|(attr&0x70);
					osd_mark_dirty (r.min_x,r.min_y,r.max_x,r.max_y,0);
					/* draw the character */
					if (MULTICOLORON) {
						draw_bitmap_multi(bitmap,&r,offs, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
								Machine->pens[c1&0x7f],Machine->pens[c2&0x7f]);
					} else {
						draw_bitmap(bitmap,&r,offs, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
								Machine->pens[c2&0x7f],Machine->pens[c1&0x7f]);
					}

				}
		  }
		  sx+=8;col++;
		  if (col>=COLUMNS) {
				col=0; row++;
				sx=0; sy+=8;
				if (!COLUMNS40) {
					offs+=2;
					sx=8;
				}
		  }
	}
}

/***************************************************************************
  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function,
  it will be called by the main emulation engine.
***************************************************************************/
void ted7360_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	if (HIRESON) {
		ted7360_drawbitmap(bitmap, full_refresh);
	} else {
		ted7360_drawcharacter(bitmap, full_refresh);
	}
	if (full_refresh||framecolorchanged) {
		int x,y;
		struct rectangle r;
		r.min_x=0;r.max_x=320;//-1;
		r.min_y=0;r.max_y=200;//-1;
		if (DOCLIP(&r,&Machine->drv->visible_area)) {
			if (r.min_y<YPOS)
				osd_mark_dirty (r.min_x,r.min_y,r.max_x,YPOS-1,0);
			if (r.max_y>=YPOS+YSIZE)
				osd_mark_dirty (r.min_x,YPOS+YSIZE,r.max_x,r.max_y,0);
			if (r.min_x<XPOS)
				osd_mark_dirty (r.min_x,r.min_y,XPOS-1,r.max_y,0);
			if (r.max_x>=XPOS+XSIZE)
				osd_mark_dirty(XPOS+XSIZE,r.min_y,r.max_x,r.max_y,0);
			for (y=r.min_y; y<=r.max_y; y++) {
				if ( (y<YPOS)||(y>=YPOS+YSIZE) ) {
					for (x=r.min_x;x<=r.max_x;x++) {
						bitmap->line[y][x]=FRAMECOLOR;
					}
				} else {
					for (x=r.min_x;x<XPOS;x++) bitmap->line[y][x]=FRAMECOLOR;
					for (x=XPOS+XSIZE;x<=r.max_x;x++) bitmap->line[y][x]=FRAMECOLOR;
				}
			}
		}
		framecolorchanged=false;
	}
}
#else


static void draw_character(int ybegin, int yend,
									int ch, int yoff, int xoff,
									int bgcolor, int color)
{
	int y,x,code;

	for (y=ybegin; y<=yend; y++) {

		if (vic_dma_read) {
			code=vic_dma_read(CHARGENADDR+ch*8+y);
		} else {
			code=0xff;
		}
		for (x=0; x<=7; x++) {
			if ((code<<x)&0x80) {
				screen[y+yoff][x+xoff]=color;
			} else {
				screen[y+yoff][x+xoff]=bgcolor;
			}
		}
	}
}

static void draw_character_multi(int ybegin, int yend,
									int ch, int yoff, int xoff,
									int bgcolor, int color)
{
	int y,x,code;

	for (y=ybegin; y<=yend; y++) {

		if (vic_dma_read) {
			code=vic_dma_read(CHARGENADDR+ch*8+y);
		} else {
			code=0xff;
		}
		for (x=0; x<=7; x++) {
			switch((code<<(x&~1))&0xc0) {
			case 0xc0: screen[y+yoff][x+xoff]=color;break;
			case 0x80: screen[y+yoff][x+xoff]=MULTICOLOR1;break;
			case 0x40: screen[y+yoff][x+xoff]=FOREGROUNDCOLOR;break;
			case 0: screen[y+yoff][x+xoff]=bgcolor;break;
			}
		}
	}
}

static void draw_bitmap(int ybegin, int yend,
									int ch, int yoff, int xoff,
									int bgcolor, int color)
{
	int y,x,code;

	for (y=ybegin; y<=yend; y++) {

		if (vic_dma_read) {
			code=vic_dma_read(BITMAPADDR+ch*8+y);
		} else {
			code=0xff;
		}
		for (x=0; x<=7; x++) {
			if ((code<<x)&0x80) {
				screen[y+yoff][x+xoff]=color;
			} else {
				screen[y+yoff][x+xoff]=bgcolor;
			}
		}
	}
}

static void draw_bitmap_multi(int ybegin, int yend,
									int ch, int yoff, int xoff,
									int bgcolor, int color)
{
	int y,x,code;

	for (y=ybegin; y<=yend; y++) {
		if (vic_dma_read) {
			code=vic_dma_read(BITMAPADDR+ch*8+y);
		} else {
			code=0xff;
		}
		for (x=0; x<=7; x++) {
			switch((code<<(x&~1))&0xc0) {
			case 0xc0: screen[y+yoff][x+xoff]=FOREGROUNDCOLOR;break;
			case 0x80: screen[y+yoff][x+xoff]=color;break;
			case 0x40: screen[y+yoff][x+xoff]=bgcolor;break;
			case 0: screen[y+yoff][x+xoff]=BACKGROUNDCOLOR;break;
			}
		}
	}
}

static void drawlines(int first, int last)
{
	int line;

#if 0
	if (errorlog&&( (first!=0)||(last!=200)) )
		DBG_LOG(1,"drawlines",(errorlog,"first: %d last: %d\n",first,last));
#endif

//	if (first>last) { last=first, first=0; }
//	if (last>200) last=200;
//	if (first>200) return;

	lastline=last;
	if (HIRESON) {
		for (line=first; (line<last)&&(line<200); line=(line+8)&~7) {
			int offs=(line>>3)*40, yoff=line&~7, xoff,
				ybegin=line&3,	yend=(line+7<last)?7:((last-line)&7);

			for (xoff=0; xoff<320; xoff+=8,offs++) {
				int attr, ch, c1, c2;
				if (vic_dma_read) {
					ch=vic_dma_read((VIDEOADDR|0x400)+offs);
				} else {
					ch=0xff;
				}
				if (vic_dma_read) {
					attr=vic_dma_read(((VIDEOADDR)+offs));
				} else {
					attr=0xff;
				}
				c1=((ch>>4)&0xf)|(attr<<4);
				c2=(ch&0xf)|(attr&0x70);
				/* draw the character */
				if (MULTICOLORON) {
					draw_bitmap_multi(ybegin,yend,offs, yoff,xoff,
							Machine->pens[c1&0x7f],Machine->pens[c2&0x7f]);
				} else {
					draw_bitmap(ybegin, yend,offs, yoff, xoff,
							Machine->pens[c2&0x7f],Machine->pens[c1&0x7f]);
				}
		  }
		}
	} else {
		for (line=first; (line<last)&&(line<200); line=(line+8)&~7) {
			int offs=(line>>3)*40, yoff=line&~7, xoff,
				ybegin=line&3, yend=(line+7<last)?7:((last-line)&7);

			for (xoff=0; xoff<320; xoff+=8,offs++) {
				int attr, ch;
				if (vic_dma_read) {
					ch=vic_dma_read((VIDEOADDR|0x400)+offs);
				} else {
					ch=0xff;
				}
				if (vic_dma_read) {
					attr=vic_dma_read(((VIDEOADDR)+offs));
				} else {
					attr=0xff;
				}

				/* draw the character */
				if (cursor1 && (offs==CURSOR1POS)) {
					int x,y;
					for (y=ybegin; y<=yend; y++) {
						for (x=0; x<=7; x++) {
							screen[yoff+y][xoff+x]=Machine->pens[attr&0x7f];
						}
					}
				} else if (REVERSEON&&(ch&0x80)) {
					if (MULTICOLORON&&(attr&8)) {
						draw_character_multi(ybegin, yend,ch&~0x80, yoff,xoff,
								BACKGROUNDCOLOR,Machine->pens[attr&0x77]);
					} else { // multicolor and hardware reverse?
						draw_character(ybegin, yend,ch&~0x80, yoff,xoff,
								Machine->pens[attr&0x7f],BACKGROUNDCOLOR);
					}
				} else {
					if (MULTICOLORON&&(attr&8)) {
						draw_character_multi(ybegin, yend,ch, yoff,xoff,
								BACKGROUNDCOLOR,Machine->pens[attr&0x77]);
					} else {
						draw_character(ybegin, yend,ch, yoff,xoff,
								BACKGROUNDCOLOR,Machine->pens[attr&0x7f]);
					}
				}
			}
		}
	}
}


void ted7360_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	struct rectangle r;
	int y;

	drawlines(lastline, 200);
	lastline=0;

	r.min_x=0;r.max_x=320;//-1;
	r.min_y=0;r.max_y=200;//-1;
	if (DOCLIP(&r,&Machine->drv->visible_area)) {
		osd_mark_dirty(r.min_x,r.min_y,r.max_x,r.max_y,0);
		for (y=r.min_y; y<=r.max_y; y++) {
			memcpy(bitmap->line[y]+r.min_x,screen[y]+r.min_x, r.max_x-r.min_x);
		}
	}
}
#endif
