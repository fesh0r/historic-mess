#include <assert.h>
#include "includes/lynx.h"
#include "cpu/m6502/m6502.h"

UINT16 lynx_granularity=1;

typedef struct {
	UINT8 data[0x100];
	UINT8 high;
	int low;
} SUZY;

static SUZY suzy={ { 0 } };

static struct {
	UINT8 *mem;
	// global
	UINT16 screen;
	UINT16 colbuf;
	UINT16 colpos; // byte where value of collision is written
	UINT16 xoff, yoff;
	// in command
	int mode;
	UINT16 cmd;
	UINT8 spritenr;
	int x,y;
	UINT16 width, height;
	int stretch, tilt;
	UINT8 color[16]; // or stored
	void (*line_function)(const int y, const int xdir);
	UINT16 bitmap;
} blitter;

#define GET_WORD(mem, index) ((mem)[(index)]|((mem)[(index)+1]<<8))

/*
mode from blitter command 
#define SHADOW         (0x07)
#define XORSHADOW      (0x06)
#define NONCOLLIDABLE  (0x05)
#define NORMAL         (0x04)
#define BOUNDARY       (0x03)
#define BOUNDARYSHADOW (0x02)
#define BKGRNDNOCOL    (0x01)
#define BKGRND         (0x00)

mode | 0x10 means without DONTCOLLIDE
*/

INLINE void lynx_plot_pixel(const int mode, const int x, const int y, const int color)
{
	int back;
	UINT8 *screen;
	UINT8 *colbuf;

	screen=blitter.mem+blitter.screen+y*80+x/2;
	colbuf=blitter.mem+blitter.colbuf+y*80+x/2;
	switch (mode) {
	case 0x00:
	case 0x01:
		if (!(x&1)) {
			*screen=(*screen&0x0f)|(blitter.color[color]<<4);
		} else {
			*screen=(*screen&0xf0)|blitter.color[color];
		}
		break;
	case 0x02:
	case 0x03:
	case 0x04:
	case 0x05:
	case 0x06:
	case 0x07:
	case 0x15: // prevents robotron from overwriting code
		if (blitter.color[color]!=0) { // for sprdemo5
//		if (color!=0) {
			if (!(x&1)) {
				*screen=(*screen&0x0f)|(blitter.color[color]<<4);
			} else {
				*screen=(*screen&0xf0)|blitter.color[color];
			}
		}
		break;
	case 0x10:
	case 0x11: // lemmings
		if (!(x&1)) {
			*screen=(*screen&0x0f)|(blitter.color[color]<<4);
			*colbuf=(*colbuf&0x0f)|(blitter.spritenr<<4);
		} else {
			*screen=(*screen&0xf0)|blitter.color[color];
			*colbuf=(*colbuf&~0xf)|(blitter.spritenr);
		}
		break;
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x16:
	case 0x17:
		if (blitter.color[color]!=0) {
//		if (color!=0) {
			if (!(x&1)) {
//				if (blitter.spritenr) {
					back=*colbuf;
					if (back&0xf0) {
						blitter.mem[blitter.colpos]=back>>4;
					}
					*colbuf=(back&~0xf0)|(blitter.spritenr<<4);
//				}
				*screen=(*screen&0x0f)|(blitter.color[color]<<4);
			} else {
//				if (blitter.spritenr) {
					back=*colbuf;
					if (back&0xf) {
						blitter.mem[blitter.colpos]=back&0xf;
					}
					*colbuf=(back&~0xf)|(blitter.spritenr);
//				}
				*screen=(*screen&0xf0)|blitter.color[color];
			}
		}
		break;
	}
}

#define INCLUDE_LYNX_LINE_FUNCTION
static void lynx_blit_2color_line(const int y, const int xdir)
{
	const int bits=1; 
	const int mask=0x1;
#include "includes/lynx.h"
}
static void lynx_blit_4color_line(const int y, const int xdir)
{
	const int bits=2; 
	const int mask=0x3;
#include "includes/lynx.h"
}
static void lynx_blit_8color_line(const int y, const int xdir)
{
	const int bits=3; 
	const int mask=0x7;
#include "includes/lynx.h"
}
static void lynx_blit_16color_line(const int y, const int xdir)
{
	const int bits=4; 
	const int mask=0xf;
#include "includes/lynx.h"
}
#undef INCLUDE_LYNX_LINE_FUNCTION

/*
2 color rle: ??
 0, 4 bit repeat count-1, 1 bit color
 1, 4 bit count of values-1, 1 bit color, ....
*/

/*
4 color rle:
 0, 4 bit repeat count-1, 2 bit color
 1, 4 bit count of values-1, 2 bit color, ....
*/

/*
8 color rle:
 0, 4 bit repeat count-1, 3 bit color
 1, 4 bit count of values-1, 3 bit color, ....
*/
	
/*
16 color rle:
 0, 4 bit repeat count-1, 4 bit color
 1, 4 bit count of values-1, 4 bit color, ....
*/
#define INCLUDE_LYNX_LINE_RLE_FUNCTION
static void lynx_blit_2color_rle_line(const int y, const int xdir)
{
	const int bits=1; 
	const int mask=0x1;
#include "includes/lynx.h"
}
static void lynx_blit_4color_rle_line(const int y, const int xdir)
{
	const int bits=2; 
	const int mask=0x3;
#include "includes/lynx.h"
}
static void lynx_blit_8color_rle_line(const int y, const int xdir)
{
	const int bits=3; 
	const int mask=0x7;
#include "includes/lynx.h"
}
static void lynx_blit_16color_rle_line(const int y, const int xdir)
{
	const int bits=4; 
	const int mask=0xf;
#include "includes/lynx.h"
}
#undef INCLUDE_LYNX_LINE_RLE_FUNCTION


static void lynx_blit_lines(void)
{
	int i, hi, y;
	int ydir=0, xdir=0;
	int flip=blitter.mem[blitter.cmd+1]&3;

	switch (blitter.mem[blitter.cmd]&0x30) {
	case 0x0000:
		xdir=ydir=1;break;
	case 0x0010:
		xdir=1;ydir=-1;
		blitter.y--;break;
	case 0x0020:
		xdir=-1;blitter.x--;ydir=1;break;
	case 0x0030:
		xdir=ydir=-1;
		blitter.y--; blitter.x--;
		break;
	}
	switch (blitter.mem[blitter.cmd+1]&3) {
	case 0: 
		flip =0;
		break;
	case 1:
		xdir*=-1;
		flip=2;
		break;
	case 2:
		ydir*=-1;
		flip=1;
		break;
	case 3:
		xdir*=-1;
		ydir*=-1;
		flip=3;
		break;
	}
	// examples sprdemo3, fat bobby for rotation

	for ( y=blitter.y, hi=0; (i=blitter.mem[blitter.bitmap]); blitter.bitmap+=i ) {
		if (i==1) {
			hi=0;
			switch (flip&3) {
			case 0:
				ydir*=-1;
				y=blitter.y;
				break;
			case 1:
				xdir*=-1;
				blitter.x+=xdir;
				y=blitter.y;
				break;
			case 2:
				ydir*=-1;
				y=blitter.y;
				break;
			case 3:
				xdir*=-1;
				blitter.x+=xdir;
				y=blitter.y;
				break;
			}
			flip++;
			if (ydir<0) y--;
			continue;
		}
		for (;(hi<blitter.height); hi+=0x100, y+=ydir) {
			if ((y>=0)&&(y<102))
				blitter.line_function(y,xdir);
			blitter.width+=blitter.stretch;
			blitter.x+=blitter.tilt;
		}
		hi-=blitter.height;
	}
}

/*
  control 0
   bit 7,6: 00 2 color
            01 4 color
            11 8 colors?
            11 16 color
   bit 5,4: 00 right down
            01 right up
            10 left down
            11 left up

#define SHADOW         (0x07)
#define XORSHADOW      (0x06)
#define NONCOLLIDABLE  (0x05)
#define NORMAL         (0x04)
#define BOUNDARY       (0x03)
#define BOUNDARYSHADOW (0x02)
#define BKGRNDNOCOL    (0x01)
#define BKGRND         (0x00)

  control 1
   bit 7: 0 bitmap rle encoded
          1 not encoded
   bit 3: 0 color info with command
          1 no color info with command

#define RELHVST        (0x30)
#define RELHVS         (0x20)
#define RELHV          (0x10)

#define SKIPSPRITE     (0x04)

#define DUP            (0x02)
#define DDOWN          (0x00)
#define DLEFT          (0x01)
#define DRIGHT         (0x00)


  coll
#define DONTCOLLIDE    (0x20)

  word next
  word data
  word x
  word y
  word width
  word height

  pixel c0 90 20 0000 datapointer x y 0100 0100 color (8 colorbytes)
  4 bit direct?
  datapointer 2 10 0
  98 (0 colorbytes)

  box c0 90 20 0000 datapointer x y width height color
  datapointer 2 10 0

  c1 98 00 4 bit direct without color bytes (raycast)

  40 10 20 4 bit rle (sprdemo2)

  line c1 b0 20 0000 datapointer x y 0100 0100 stretch tilt:x/y color (8 color bytes)
  or
  line c0 b0 20 0000 datapointer x y 0100 0100 stretch tilt:x/y color
  datapointer 2 11 0

  text ?(04) 90 20 0000 datapointer x y width height color
  datapointer 2 10 0

  stretch: hsize adder
  tilt: hpos adder

*/

static void lynx_blitter(void)
{
	static const int lynx_colors[4]={2,4,8,16};

	static void (* const blit_line[4])(const int y, const int xdir)= {
		lynx_blit_2color_line,
		lynx_blit_4color_line,
		lynx_blit_8color_line,
		lynx_blit_16color_line
	};

	static void (* const blit_rle_line[4])(const int y, const int xdir)= {
		lynx_blit_2color_rle_line,
		lynx_blit_4color_rle_line,
		lynx_blit_8color_rle_line,
		lynx_blit_16color_rle_line
	};
	int i; int o;int colors;

	blitter.mem=memory_region(REGION_CPU1);
	blitter.colbuf=GET_WORD(suzy.data, 0xa);
//	blitter.colpos=GET_WORD(suzy.data, 0x24);
	blitter.screen=GET_WORD(suzy.data, 8);
	blitter.xoff=GET_WORD(suzy.data,4);
	blitter.yoff=GET_WORD(suzy.data,6);

	for (blitter.cmd=GET_WORD(suzy.data, 0x10); blitter.cmd; 
		 blitter.cmd=GET_WORD(blitter.mem, blitter.cmd+3) ) {

		if (blitter.mem[blitter.cmd+1]&4) continue;

		blitter.colpos=GET_WORD(suzy.data, 0x24)+blitter.cmd;

		blitter.bitmap=GET_WORD(blitter.mem,blitter.cmd+5);
		blitter.x=(INT16)GET_WORD(blitter.mem, blitter.cmd+7)-blitter.xoff;
		blitter.y=(INT16)GET_WORD(blitter.mem, blitter.cmd+9)-blitter.yoff;

		blitter.mode=blitter.mem[blitter.cmd]&07;
		if (blitter.mem[blitter.cmd+1]&0x80) {
			blitter.line_function=blit_line[blitter.mem[blitter.cmd]>>6];
		} else {
			blitter.line_function=blit_rle_line[blitter.mem[blitter.cmd]>>6];
		}
		if (!(blitter.mem[blitter.cmd+2]&0x20)&&!(suzy.data[0x92]&0x20)) {
			blitter.mem[blitter.colpos]=0;
			blitter.spritenr=blitter.mem[blitter.cmd+2]&0xf;
			blitter.mode|=0x10;
		}
			
		o=0xb;
		blitter.width=0x100;
		blitter.height=0x100;
		if (blitter.mem[blitter.cmd+1]&0x30) {
			blitter.width=GET_WORD(blitter.mem, blitter.cmd+11);
			blitter.height=GET_WORD(blitter.mem, blitter.cmd+13);
			o+=4;
		}

		blitter.stretch=0;
		blitter.tilt=0;
		if (blitter.mem[blitter.cmd+1]&0x20) {
			blitter.stretch=GET_WORD(blitter.mem, blitter.cmd+o);
			o+=2;
			if (blitter.mem[blitter.cmd+1]&0x10) {
				blitter.tilt=GET_WORD(blitter.mem, blitter.cmd+o);
				o+=2;
			}
		}
		colors=lynx_colors[blitter.mem[blitter.cmd]>>6];

		if (!(blitter.mem[blitter.cmd+1]&8)) {
			for (i=0; i<colors/2; i++) {
				blitter.color[i*2]=blitter.mem[blitter.cmd+o+i]>>4;
				blitter.color[i*2+1]=blitter.mem[blitter.cmd+o+i]&0xf;
			}
		}
#if 1
		logerror("%04x %.2x %.2x %.2x x:%.4x y:%.4x",
				 blitter.cmd,
				 blitter.mem[blitter.cmd],blitter.mem[blitter.cmd+1],blitter.mem[blitter.cmd+2],
				 blitter.x,blitter.y);
		if (blitter.mem[blitter.cmd+1]&0x30) {
			logerror(" w:%.4x h:%.4x", blitter.width,blitter.height);
		}
		if (blitter.mem[blitter.cmd+1]&0x20) {
			logerror(" s:%.4x t:%.4x", blitter.stretch, blitter.tilt);
		}
		if (!(blitter.mem[blitter.cmd]&0x8)) {
			logerror(" c:");
			for (i=0; i<colors/2; i++) {
				logerror("%.2x", blitter.mem[blitter.cmd+o+i]);
			}
		}
		logerror(" %.4x\n", blitter.bitmap);
#endif
		lynx_blit_lines();
	}
}

/*
  writes (0x52 0x53) (0x54 0x55) expects multiplication result at 0x60 0x61
  writes (0x56 0x57) (0x60 0x61 0x62 0x63) division expects result at (0x52 0x53)
 */

void lynx_divide(void)
{
	UINT32 left=suzy.data[0x60]|(suzy.data[0x61]<<8)|(suzy.data[0x62]<<16)|(suzy.data[0x63]<<24);
	UINT16 right=suzy.data[0x56]|(suzy.data[0x57]<<8);
	UINT16 res;
	if (left==0) {
		res=0;
	} else if (right==0) {
		res=0xffff; //estimated
	} else {
		res=left/right;
	}
//	logerror("coprocessor %8x / %4x = %4x\n", left, right, res);
	suzy.data[0x52]=res&0xff;
	suzy.data[0x53]=res>>8;
}

void lynx_multiply(void)
{
	UINT16 left, right, res;
	left=suzy.data[0x52]|(suzy.data[0x53]<<8);
	right=suzy.data[0x54]|(suzy.data[0x55]<<8);
	res=left*right;
//	logerror("coprocessor %4x * %4x = %4x\n", left, right, res);
	suzy.data[0x60]=res&0xff;
	suzy.data[0x61]=res>>8;
}

READ_HANDLER(suzy_read)
{
	UINT8 data=0, input;
	switch (offset) {
	case 0x88:
		data=1; // must not be 0 for correct power up
		break;
	case 0x92:
		data=suzy.data[offset];
		data&=~0x80; // math finished
		data&=~1; //blitter finished
		break;
	case 0xb0:
		input=readinputport(0);
		switch (readinputport(2)&3) {
		case 1:
			data=input;
			input&=0xf;
			if (data&PAD_UP) input|=PAD_LEFT;
			if (data&PAD_LEFT) input|=PAD_DOWN;
			if (data&PAD_DOWN) input|=PAD_RIGHT;
			if (data&PAD_RIGHT) input|=PAD_UP;
			break;
		case 2:
			data=input;
			input&=0xf;
			if (data&PAD_UP) input|=PAD_RIGHT;
			if (data&PAD_RIGHT) input|=PAD_DOWN;
			if (data&PAD_DOWN) input|=PAD_LEFT;
			if (data&PAD_LEFT) input|=PAD_UP;
			break;
		}
		if (suzy.data[0x92]&8) {
			data=input&0xf;
			if (input&PAD_UP) data|=PAD_DOWN;
			if (input&PAD_DOWN) data|=PAD_UP;
			if (input&PAD_LEFT) data|=PAD_RIGHT;
			if (input&PAD_RIGHT) data|=PAD_LEFT;
		} else {
			data=input;
		}		
		break;
	case 0xb1: data=readinputport(1);break;
	case 0xb2:
		data=*(memory_region(REGION_USER1)+(suzy.high*lynx_granularity)+suzy.low);
		suzy.low=(suzy.low+1)&(lynx_granularity-1);
		break;
	default:
		data=suzy.data[offset];
	}
//	logerror("suzy read %.2x %.2x\n",offset,data);
	return data;
}

WRITE_HANDLER(suzy_write)
{
	suzy.data[offset]=data;
	logerror("suzy write %.2x %.2x\n",offset,data);
	switch(offset) {
	case 0x55: lynx_multiply();break;
	case 0x63: lynx_divide();break;
	case 0x91:
		if (data&1) {
			lynx_blitter();
		}
		break;
	}
}

/*
 0xfd0a r sync signal?
 0xfd81 r interrupt source bit 2 vertical refresh
 0xfd80 w interrupt quit
 0xfd87 w bit 1 !clr bit 0 blocknumber clk
 0xfd8b w bit 1 blocknumber hi B
 0xfd94 w 0
 0xfd95 w 4
 0xfda0-f rw farben 0..15
 0xfdb0-f rw bit0..3 farben 0..15
*/
MIKEY mikey={ { 0 } };


/*
HCOUNTER        EQU TIMER0
VCOUNTER        EQU TIMER2
SERIALRATE      EQU TIMER4

TIM_BAKUP       EQU 0   ; backup-value (count+1)
TIM_CNTRL1      EQU 1   ; timer-control register
TIM_CNT EQU 2   ; current counter
TIM_CNTRL2      EQU 3   ; dynamic control

; TIM_CNTRL1
TIM_IRQ EQU %10000000   ; enable interrupt (not TIMER4 !)
TIM_RESETDONE   EQU %01000000   ; reset timer done
TIM_MAGMODE     EQU %00100000   ; nonsense in Lynx !!
TIM_RELOAD      EQU %00010000   ; enable reload
TIM_COUNT       EQU %00001000   ; enable counter
TIM_LINK        EQU %00000111   
; link timers (0->2->4 / 1->3->5->7->Aud0->Aud1->Aud2->Aud3->1
TIM_64us        EQU %00000110
TIM_32us        EQU %00000101
TIM_16us        EQU %00000100
TIM_8us EQU %00000011
TIM_4us EQU %00000010
TIM_2us EQU %00000001
TIM_1us EQU %00000000

;TIM_CNTRL2 (read-only)
; B7..B4 unused
TIM_DONE        EQU %00001000   ; set if timer's done; reset with TIM_RESETDONE
TIM_LAST        EQU %00000100   ; last clock (??)
TIM_BORROWIN    EQU %00000010
TIM_BORROWOUT   EQU %00000001
*/
typedef struct {
	const int nr;
	int counter;
	void *timer;
	UINT8 data[4];
	double settime;
} LYNX_TIMER;
static LYNX_TIMER lynx_timer[8]= {
	{ 0 },
	{ 1 },
	{ 2 },
	{ 3 },
	{ 4 },
	{ 5 },
	{ 6 },
	{ 7 }
};

static void lynx_timer_reset(LYNX_TIMER *This)
{
	memset(&This->counter, 0, (char *)(This+1)-(char*)&(This->counter));
//	This->nr=i;
	This->settime=0.0;
}



static void lynx_timer_count_down(LYNX_TIMER *This);
static void lynx_timer_signal_irq(LYNX_TIMER *This)
{
//	if ((This->data[1]&0x80)&&!(mikey.data[0x81]&(1<<This->nr))) {
	if ((This->data[1]&0x80)) { // irq flag handling later
//	if ((This->data[1]&0x80)||(This->nr==4)) { // irq flag handling later
//	{
		cpu_set_irq_line(0, M65SC02_INT_IRQ, PULSE_LINE);
		mikey.data[0x81]|=1<<This->nr; // vertical
	}
	switch (This->nr) {
	case 0:
		if ((lynx_timer[2].data[1]&7)==7) lynx_timer_count_down(lynx_timer+2);
		break;
	case 2:
		if ((lynx_timer[4].data[1]&7)==7) lynx_timer_count_down(lynx_timer+4);
		break;
	case 1:
		if ((lynx_timer[3].data[1]&7)==7) lynx_timer_count_down(lynx_timer+3);
		break;
	case 3:
		if ((lynx_timer[5].data[1]&7)==7) lynx_timer_count_down(lynx_timer+5);
		break;
	case 5:
		if ((lynx_timer[7].data[1]&7)==7) lynx_timer_count_down(lynx_timer+7);
		break;
	case 7:
		// audio 1
		break;
	}
}

static void lynx_timer_count_down(LYNX_TIMER *This)
{
	if (This->counter>0) {
		This->counter--;
		return;
	} else if (This->counter==0) {
		lynx_timer_signal_irq(This);
		if (This->data[1]&0x10) {
			This->counter=This->data[0];
		} else {
			This->counter--;
		}
		return;
	}
}

static void lynx_timer_shot(int nr)
{
	LYNX_TIMER *This=lynx_timer+nr;
	lynx_timer_signal_irq(This);
	if (!(This->data[1]&0x10)) This->timer=NULL;
}

static double times[]= { 1e-6, 2e-6, 4e-6, 8e-6, 16e-6, 32e-6, 64e-6 };

static UINT8 lynx_timer_read(LYNX_TIMER *This, int offset)
{
	UINT8 data;
	switch (offset) {
	case 2:
		data=This->counter;
		break;
	default:
		data=This->data[offset];
	}
	logerror("timer %d read %x %.2x\n",This-lynx_timer,offset,data);
	return data;
}

static void lynx_timer_write(LYNX_TIMER *This, int offset, UINT8 data)
{
	int t;
	logerror("timer %d write %x %.2x\n",This-lynx_timer,offset,data);
	This->data[offset]=data;

	if (offset==0) This->counter=This->data[0]+1;
	if (This->timer) { timer_remove(This->timer); This->timer=NULL; }
//	if ((This->data[1]&0x80)&&(This->nr!=4)) { //timers are combined!
//	if ((This->data[1]&0x8)||(This->nr==4)) {
	if ((This->data[1]&0x8)) {
		if ((This->data[1]&7)!=7) {
			t=This->data[0]?This->data[0]:0x100;
			if (This->data[1]&0x10) {
				This->timer=timer_pulse(t*times[This->data[1]&7],
										This->nr, lynx_timer_shot);
			} else {
				This->timer=timer_set(t*times[This->data[1]&7],
									  This->nr, lynx_timer_shot);
			}
		}
	}
}

typedef struct {
	int nr;
	UINT8 data[8];
} LYNX_AUDIO;
static LYNX_AUDIO lynx_audio[4]= { 
	{ 0 },
	{ 1 },
	{ 2 },
	{ 3 } 
};

static UINT8 lynx_audio_read(LYNX_AUDIO *This, int offset)
{
	UINT8 data=This->data[offset];
	logerror("audio %d read %d %.2x\n", This->nr, offset, data);
	return data;
}

static void lynx_audio_write(LYNX_AUDIO *This, int offset, UINT8 data)
{
	This->data[offset]=data;
	logerror("audio %d write %d %.2x\n", This->nr, offset, data);
}

READ_HANDLER(mikey_read)
{
	UINT8 data=0;
	switch (offset) {
	case 0: case 1: case 2: case 3:
	case 4: case 5: case 6: case 7:
	case 8: case 9: case 0xa: case 0xb:
	case 0xc: case 0xd: case 0xe: case 0xf:
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		data=lynx_timer_read(lynx_timer+(offset/4), offset&3);
		return data;
//		break;
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
	case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
	case 0x30: case 0x31: case 0x32: case 0x33:	case 0x34: case 0x35: case 0x36: case 0x37:
	case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
		data=lynx_audio_read(lynx_audio+(offset&0x1f)/8, offset&7);
		return data;
		break;
	case 0x81:
		data=mikey.data[offset];
		mikey.data[offset]&=~0x10; // timer 4 autoquit!?
		break;
	case 0x8b:
		data=mikey.data[offset];
		data|=4; // no comlynx adapter
		break;
	case 0x8c:
		data=mikey.data[offset]&~0x40; // no serial data received
		break;
	default:
		data=mikey.data[offset];
	}
	logerror("mikey read %.2x %.2x\n",offset,data);
	return data;
}

WRITE_HANDLER(mikey_write)
{
	mikey.data[offset]=data;
	switch (offset) {
	case 0: case 1: case 2: case 3:
	case 4: case 5: case 6: case 7:
	case 8: case 9: case 0xa: case 0xb:
	case 0xc: case 0xd: case 0xe: case 0xf:
	case 0x10: case 0x11: case 0x12: case 0x13:
	case 0x14: case 0x15: case 0x16: case 0x17:
	case 0x18: case 0x19: case 0x1a: case 0x1b:
	case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		lynx_timer_write(lynx_timer+(offset/4), offset&3, data);
		return;
//		break;
	case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
	case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x2c: case 0x2d: case 0x2e: case 0x2f:
	case 0x30: case 0x31: case 0x32: case 0x33:	case 0x34: case 0x35: case 0x36: case 0x37:
	case 0x38: case 0x39: case 0x3a: case 0x3b: case 0x3c: case 0x3d: case 0x3e: case 0x3f:
		lynx_audio_write(lynx_audio+(offset&0x1f)/8, offset&7, data);
		return;
//break;		
	case 0x80:
		mikey.data[0x81]&=~data; // clear interrupt source
		break;
	case 0x87:
		if (data&2) {
			if (data&1) {
				suzy.high<<=1;
				if (mikey.data[0x8b]&2) suzy.high|=1;
				suzy.low=0;
			}
		} else {
			suzy.high=0;
			suzy.low=0;
		}
		return;
//		break;
	case 0xa0: case 0xa1: case 0xa2: case 0xa3: case 0xa4: case 0xa5: case 0xa6: case 0xa7:
	case 0xa8: case 0xa9: case 0xaa: case 0xab: case 0xac: case 0xad: case 0xae: case 0xaf:
	case 0xb0: case 0xb1: case 0xb2: case 0xb3: case 0xb4: case 0xb5: case 0xb6: case 0xb7:
	case 0xb8: case 0xb9: case 0xba: case 0xbb: case 0xbc: case 0xbd: case 0xbe: case 0xbf:
		mikey.data[offset]=data;
		palette_change_color(offset&0xf,
							 (mikey.data[0xb0+(offset&0xf)]&0xf)<<4,
							 (mikey.data[0xa0+(offset&0xf)]&0xf)<<4,
							 mikey.data[0xb0+(offset&0xf)]&0xf0 );
		return;
//		break;
	}
	logerror("mikey write %.2x %.2x\n",offset,data);
}

WRITE_HANDLER( lynx_memory_config )
{
	memory_region(REGION_CPU1)[0xfff9]=data;
	if (data&1) {
		memory_set_bankhandler_r(1, 0, MRA_RAM);
		memory_set_bankhandler_w(1, 0, MWA_RAM);
	} else {
		memory_set_bankhandler_r(1, 0, suzy_read);
		memory_set_bankhandler_w(1, 0, suzy_write);
	}
	if (data&2) {
		memory_set_bankhandler_r(2, 0, MRA_RAM);
		memory_set_bankhandler_w(2, 0, MWA_RAM);
	} else {
		memory_set_bankhandler_r(2, 0, mikey_read);
		memory_set_bankhandler_w(2, 0, mikey_write);
	}
	if (data&4) {
		memory_set_bankhandler_r(3, 0, MRA_RAM);
	} else {
		cpu_setbank(3,memory_region(REGION_CPU1)+0x10000);
		memory_set_bankhandler_r(3, 0, MRA_BANK3);
	}
	if (data&8) {
		memory_set_bankhandler_r(4, 0, MRA_RAM);
	} else {
		memory_set_bankhandler_r(4, 0, MRA_BANK4);
		cpu_setbank(4,memory_region(REGION_CPU1)+0x101fa);
	}
}

extern void lynx_machine_init(void)
{
	int i;
	lynx_memory_config(0,0);

	memset(&suzy, 0, sizeof(suzy));
	memset(&mikey, 0, sizeof(mikey));

	for (i=0; i<ARRAY_LENGTH(lynx_timer); i++) {
		lynx_timer_reset(lynx_timer+i);
	}
}