/***************************************************************************

  MOS video interface chip 6560

***************************************************************************/
#include <math.h>
#include "mess/machine/vc20.h"
#include "mess/vidhrdw/vic6560.h"
#include "mess/machine/c1551.h"

unsigned char vic6560_palette[] = {
// ripped from vice, a very excellent emulator
// black, white, red, cyan
	0x00,0x00,0x00, 0xff,0xff,0xff, 0xf0,0x00,0x00, 0x00,0xf0,0xf0,
// purple, green, blue, yellow
	0x60,0x00,0x60, 0x00,0xa0,0x00, 0x00,0x00,0xf0, 0xd0,0xd0,0x00,
// orange, light orange, pink, light cyan,
	0xc0,0xa0,0x00, 0xff,0xa0,0x00, 0xf0,0x80,0x80, 0x00,0xff,0xff,
// light violett, light green, light blue, light yellow
	0xff,0x00,0xff, 0x00,0xff,0x00, 0x00,0xa0,0xff, 0xff,0xff,0x00
};

struct CustomSound_interface vic6560_sound_interface = {
	vic6560_custom_start,
	vic6560_custom_stop,
	vic6560_custom_update
};

UINT8 vic6560[16];

/* lightpen delivers values from internal counters
   they do not start with the visual area or frame area */
#define VIC6560_X_BEGIN 38
#define VIC6560_Y_BEGIN -6 // first 6 lines after retrace not for lightpen!
#define VIC6561_X_BEGIN 38
#define VIC6561_Y_BEGIN -6
#define VIC656X_X_BEGIN (vic6560_pal?VIC6561_X_BEGIN:VIC6560_X_BEGIN)
#define VIC656X_Y_BEGIN (vic6560_pal?VIC6561_Y_BEGIN:VIC6560_Y_BEGIN)
/* lightpen behaviour in pal or mono multicolor not tested */
#define VIC6560_X_VALUE ((LIGHTPEN_X_VALUE+VIC656X_X_BEGIN+VIC656X_MAME_XPOS)/2)
#define VIC6560_Y_VALUE ((LIGHTPEN_Y_VALUE+VIC656X_Y_BEGIN+VIC656X_MAME_YPOS)/2)

#define INTERLACE (vic6560&0x80) //only ntsc version
// ntsc 1 - 8
// pal 5 - 19
#define XPOS (((int)vic6560[0]&0x7f)*4)
#define YPOS ((int)vic6560[1]*2)

// ntsc values >= 31 behave like 31
// pal value >= 32 behave like 32
#define CHARS_X ((int)vic6560[2]&0x7f)
#define CHARS_Y (((int)vic6560[3]&0x7e)>>1)

#define MATRIX8X16 (vic6560[3]&1) // else 8x8
#define CHARHEIGHT (MATRIX8X16?16:8)
#define XSIZE (CHARS_X*8)
#define YSIZE (CHARS_Y*CHARHEIGHT)

// colorram and backgroundcolor are changed
#define INVERTED (!(vic6560[0x0f]&8))

#define CHARGENADDR (((int)vic6560[5]&0xf)<<10)
#define VIDEOADDR ( ( ((int)vic6560[5]&0xf0)<<(10-4))\
						| ( ((int)vic6560[2]&0x80)<<(9-7)) )
#define VIDEORAMSIZE (YSIZE*XSIZE)
#define CHARGENSIZE (256*HEIGHTPIXEL)

#define HELPERCOLOR (vic6560[0xe]>>4)
#define BACKGROUNDCOLOR (vic6560[0xf]>>4)
#define FRAMECOLOR (vic6560[0xf]&7)

//#define FREQUENCY (vic6560[0xa]&0x7f)
//#define TONEON (vic6560[0xa]&0x80)
//#define VOLUME ((vic6560[0xe]&0xf)

bool vic6560_pal;

static double vretracetime;
static int lastline=0;
static void vic6560_drawlines(int start, int last);

static int(*vic_dma_read)(int);
static int(*vic_dma_read_color)(int);

static int vic656x_xsize,vic656x_ysize,vic656x_lines,vic656x_vretracerate;
static int charheight, matrix8x16, inverted;
static int chars_x, chars_y;
static int xsize, ysize, xpos, ypos;
static int chargenaddr, videoaddr;
// values in videoformat
static UINT16 backgroundcolor, framecolor, helpercolor, white, black;
// arrays for bit to color conversion without condition checking
static UINT16 mono[2], monoinverted[2], multi[4], multiinverted[4];

static int rasterline(void)
{
	double a=timer_get_time()-vretracetime;
	int b=(int)(a*vic656x_vretracerate*vic656x_lines)
			 %vic656x_lines;
	return b;
}

static void vic656x_init(void)
{
	vic656x_xsize=VIC656X_XSIZE;
	vic656x_ysize=VIC656X_YSIZE;
	vic656x_lines=VIC656X_LINES;
	vic656x_vretracerate=VIC656X_VRETRACERATE;
	vretracetime=timer_get_time();
}

void vic6560_init(int(*dma_read)(int), int(*dma_read_color)(int))
{
	vic6560_pal=false;
	vic_dma_read=dma_read;
	vic_dma_read_color=dma_read_color;
	vic656x_init();
}

void vic6561_init(int(*dma_read)(int), int(*dma_read_color)(int))
{
	vic6560_pal=true;
	vic_dma_read=dma_read;
	vic_dma_read_color=dma_read_color;
	vic656x_init();
}
int	vic6560_vh_start(void)
{
	black=Machine->pens[0];
	white=Machine->pens[1];
	return generic_bitmapped_vh_start();
}

void vic6560_vh_stop(void)
{
	generic_bitmapped_vh_stop();
}

void vic6560_port_w(int offset, int data)
{
	VIC_LOG(1,"vic6560_port_w",(errorlog,"%.4x:%.2x\n",offset,data));
	switch (offset) {
	case 0xa:case 0xb:case 0xc:case 0xd:case 0xe:
		vic6560_soundport_w(offset,data);
		break;
	}
	if (vic6560[offset]!=data) {
		switch(offset) {
		case 0:case 1:case 2:case 3:case 5:
		case 0xe:case 0xf:
			vic6560_drawlines(lastline, rasterline());
			break;
		}
		vic6560[offset]=data;
		switch (offset) {
			case 0: xpos=XPOS;break;
			case 1: ypos=YPOS;break;
			case 2:
// ntsc values >= 31 behave like 31
// pal value >= 32 behave like 32
				chars_x=CHARS_X;
				videoaddr=VIDEOADDR;
				xsize=XSIZE;
				break;
			case 3:
				matrix8x16=MATRIX8X16;
				charheight=CHARHEIGHT;
				chars_y=CHARS_Y;
				ysize=YSIZE;
				break;
			case 5:
				chargenaddr=CHARGENADDR;
				videoaddr=VIDEOADDR;
				break;
			case 0xe:
				multi[3]=multiinverted[3]=helpercolor=Machine->pens[HELPERCOLOR];
				break;
			case 0xf:
				inverted=INVERTED;
				multi[1]=multiinverted[1]=framecolor=Machine->pens[FRAMECOLOR];
				mono[0]=monoinverted[1]=
				multi[0]=multiinverted[2]=backgroundcolor=Machine->pens[BACKGROUNDCOLOR];
				break;
		}
	}
}

int vic6560_port_r(int offset)
{
	static double lightpenreadtime=0.0;
	int val;

	switch(offset) {
	case 3:
		val=((rasterline()&1)<<7)|(vic6560[offset]&0x7f);
		break;
	case 4: //rasterline
//		VIC_LOG(1,"vic6560_port_r",(errorlog,"rastline read\n"));
		vic6560_drawlines(lastline,rasterline());
		val=(rasterline()/2)&0xff;
		break;
	case 6: //lightpen horicontal
	case 7: //lightpen vertical
		if (LIGHTPEN_BUTTON
			&&((timer_get_time()-lightpenreadtime)*VIC6560_VRETRACERATE>=1)){
			// only 1 update each frame
			// and diode must recognize light
			if (1) {
				vic6560[6]=VIC6560_X_VALUE;
				vic6560[7]=VIC6560_Y_VALUE;
			}
			lightpenreadtime=timer_get_time();
		}
		val=vic6560[offset];
		break;
	case 8: // poti 1
		val=PADDLE1_VALUE;break;
	case 9: // poti 2
		val=PADDLE2_VALUE;break;
	default:
		val=vic6560[offset];break;
	}
	VIC_LOG(3,"vic6560_port_r",(errorlog,"%.4x:%.2x\n",offset,val));
	return val;
}

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

static void draw_character(int ybegin, int yend,
									int ch, int yoff, int xoff,
									UINT16 *color)
{
	int y,code;

	if (Machine->color_depth==8) {
		for (y=ybegin; y<=yend; y++) {
			code=vic_dma_read((chargenaddr+ch*charheight+y)&0x3fff);
			tmpbitmap->line[y+yoff][xoff]=color[code>>7];
			tmpbitmap->line[y+yoff][xoff+1]=color[(code>>6)&1];
			tmpbitmap->line[y+yoff][xoff+2]=color[(code>>5)&1];
			tmpbitmap->line[y+yoff][xoff+3]=color[(code>>4)&1];
			tmpbitmap->line[y+yoff][xoff+4]=color[(code>>3)&1];
			tmpbitmap->line[y+yoff][xoff+5]=color[(code>>2)&1];
			tmpbitmap->line[y+yoff][xoff+6]=color[(code>>1)&1];
			tmpbitmap->line[y+yoff][xoff+7]=color[code&1];
		}
	} else {
		for (y=ybegin; y<=yend; y++) {
			code=vic_dma_read((chargenaddr+ch*charheight+y)&0x3fff);
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff)=color[code>>7];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+1)=color[(code>>6)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+2)=color[(code>>5)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+3)=color[(code>>4)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+4)=color[(code>>3)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+5)=color[(code>>2)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+6)=color[(code>>1)&1];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+7)=color[code&1];
		}
	}
}

static void draw_character_multi(int ybegin, int yend,
									int ch, int yoff, int xoff, UINT16 *color)
{
	int y,code;

	if (Machine->color_depth==8) {
		for (y=ybegin; y<=yend; y++) {
			code=vic_dma_read((chargenaddr+ch*charheight+y)&0x3fff);
			tmpbitmap->line[y+yoff][xoff+1]=
			tmpbitmap->line[y+yoff][xoff]=color[code>>6];
			tmpbitmap->line[y+yoff][xoff+3]=
			tmpbitmap->line[y+yoff][xoff+2]=color[(code>>4)&3];
			tmpbitmap->line[y+yoff][xoff+5]=
			tmpbitmap->line[y+yoff][xoff+4]=color[(code>>2)&3];
			tmpbitmap->line[y+yoff][xoff+7]=
			tmpbitmap->line[y+yoff][xoff+6]=color[code&3];
		}
	} else {
		for (y=ybegin; y<=yend; y++) {
			code=vic_dma_read((chargenaddr+ch*charheight+y)&0x3fff);
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+1)=
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff)=color[code>>6];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+3)=
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+2)=color[(code>>4)&3];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+5)=
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+4)=color[(code>>2)&3];
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+7)=
			*((UINT16*)tmpbitmap->line[y+yoff]+xoff+6)=color[code&3];
		}
	}
}


INLINE void draw_pointer(struct osd_bitmap *bitmap,
						struct rectangle *visible, int xoff, int yoff)
{
	// this is a a static graphical object
	// should be easy to convert to gfx_element!?
	static UINT8 blackmask[]={0x00,0x70,0x60,0x50,0x08,0x04,0x00,0x00 };
	static UINT8 whitemask[]={0xf0,0x80,0x80,0x80,0x00,0x00,0x00,0x00 };
	int i,j,y,x;

	if (Machine->color_depth==8) {
		for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {
			for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
				if ((blackmask[j]<<i)&0x80)
					bitmap->line[y][x]=black;
				else if ((whitemask[j]<<(i&~1))&0x80)
					bitmap->line[y][x]=white;
			}
		}
	} else {
		for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {
			for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
				if ((blackmask[j]<<i)&0x80)
					*((UINT16*)bitmap->line[y]+x)=black;
				else if ((whitemask[j]<<(i&~1))&0x80)
					*((UINT16*)bitmap->line[y]+x)=white;
			}
		}
	}
}

static void memset16(void *dest,UINT16 value, int size)
{
	register int i;
	for (i=0;i<size;i++) ((UINT16*)dest)[i]=value;
}

static void vic6560_drawlines(int first, int last)
{
	int line,vline;
	int offs, yoff, xoff, ybegin, yend, i;
	int attr, ch;

	lastline=last;
	if (first>=last) return;

	if (Machine->color_depth==8) {
		for (line=first;(line<ypos)&&(line<last);line++) {
			memset(tmpbitmap->line[line],framecolor,vic656x_xsize);
		}
	} else {
		for (line=first;(line<ypos)&&(line<last);line++) {
			memset16(tmpbitmap->line[line],framecolor,vic656x_xsize);
		}
	}
	for (vline=line-ypos; (line<last)&&(line<ypos+ysize);) {
		if (matrix8x16) {
			offs=(vline>>4)*chars_x;
			yoff=(vline&~0xf)+ypos;
			ybegin=vline&0xf;
			yend=(vline+0xf<last-ypos)?0xf:((last-line)&0xf)+ybegin;
		} else {
			offs=(vline>>3)*chars_x;
			yoff=(vline&~7)+ypos;
			ybegin=vline&7;
			yend=(vline+7<last-ypos)?7:((last-line)&7)+ybegin;
		}

		if (xpos>0) {
			if (Machine->color_depth==8) {
				for (i=ybegin;i<=yend;i++)
					memset(tmpbitmap->line[yoff+i],framecolor,xpos);
			} else {
				for (i=ybegin;i<=yend;i++)
					memset16(tmpbitmap->line[yoff+i],framecolor,xpos);
			}
		}
		for (xoff=xpos; (xoff<xpos+xsize)&&(xoff<vic656x_xsize); xoff+=8,offs++) {
			ch=vic_dma_read((videoaddr+offs)&0x3fff);
			attr=(vic_dma_read_color((videoaddr+offs)&0x3fff))&0xf;
			if (inverted) {
				if (attr&8) {
					multiinverted[0]=Machine->pens[attr&7];
					draw_character_multi(ybegin, yend, ch, yoff,xoff,multiinverted);
				} else {
					monoinverted[0]=Machine->pens[attr];
					draw_character(ybegin, yend, ch, yoff,xoff,monoinverted);
				}
			} else {
				if (attr&8) {
					multi[2]=Machine->pens[attr&7];
					draw_character_multi(ybegin, yend, ch, yoff,xoff,multi);
				} else {
					mono[1]=Machine->pens[attr];
					draw_character(ybegin, yend, ch, yoff,xoff,mono);
				}
			}
		}
		if (xoff<vic656x_xsize) {
			if (Machine->color_depth==8) {
				for (i=ybegin;i<=yend;i++)
					memset(tmpbitmap->line[yoff+i]+xoff, framecolor, vic656x_xsize-xoff);
			} else {
				for (i=ybegin;i<=yend;i++)
					memset16((UINT16*)tmpbitmap->line[yoff+i]+xoff, framecolor, vic656x_xsize-xoff);
			}
		}
		if (matrix8x16) {
			vline=(vline+16)&~0xf;
			line=vline+ypos;
		} else {
			vline=(vline+8)&~7;
			line=vline+ypos;
		}
	}
	if (Machine->color_depth==8) {
		for (;line<last;line++) {
			memset(tmpbitmap->line[line],framecolor,vic656x_xsize);
		}
	} else {
		for (;line<last;line++) {
			memset16(tmpbitmap->line[line],framecolor,vic656x_xsize);
		}
	}
}

void vic6560_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	struct rectangle r;
	int y,x;

	vic6560_drawlines(lastline, vic656x_lines);
	lastline=0;
	vretracetime=timer_get_time();
	if (LIGHTPEN_POINTER) {

		r.min_x = LIGHTPEN_X_VALUE-1+VIC656X_MAME_XPOS;
		r.max_x = r.min_x+8-1;
		r.min_y = LIGHTPEN_Y_VALUE-1+VIC656X_MAME_YPOS;
		r.max_y = r.min_y+8-1;

		if (DOCLIP(&r,&Machine->drv->visible_area)) {
			osd_mark_dirty (r.min_x,r.min_y,r.max_x,r.max_y,0);
			draw_pointer(tmpbitmap,&r,
				r.min_x-(LIGHTPEN_X_VALUE+VIC656X_MAME_XPOS-1),
				r.min_y-(LIGHTPEN_Y_VALUE+VIC656X_MAME_YPOS-1));
		}
	}

	generic_bitmapped_vh_screenrefresh(bitmap,1);

	{
		int x0;
		char text[50];

		vc20_tape_status(text, sizeof(text));
		if (text[0]!=0) {
			x0 = (Machine->uiwidth - (strlen(text)) * Machine->uifont->width) / 2;
			y = Machine->uiymin + Machine->uiheight - Machine->uifont->height*2 - 4;

			for( x = 0; text[x]; x++ )	{
				drawgfx(bitmap,Machine->uifont,text[x],0,0,0,
							x0+x*Machine->uifont->width,y,0,TRANSPARENCY_NONE,0);
			}
		}
		cbm_drive_status(&vc20_drive8,text,sizeof(text));
		if (text[0]!=0) {
			x0 = (Machine->uiwidth - (strlen(text)) * Machine->uifont->width) / 2;
			y = Machine->uiymin + Machine->uiheight - Machine->uifont->height*3 - 6;

			for( x = 0; text[x]; x++ )	{
				drawgfx(bitmap,Machine->uifont,text[x],0,0,0,
							x0+x*Machine->uifont->width,y,0,TRANSPARENCY_NONE,0);
			}
			y-=Machine->uifont->height+2;
		}
		cbm_drive_status(&vc20_drive9,text,sizeof(text));
		if (text[0]!=0) {
			x0 = (Machine->uiwidth - (strlen(text)) * Machine->uifont->width) / 2;
			y = Machine->uiymin + Machine->uiheight - Machine->uifont->height*4 - 8;

			for( x = 0; text[x]; x++ )	{
				drawgfx(bitmap,Machine->uifont,text[x],0,0,0,
							x0+x*Machine->uifont->width,y,0,TRANSPARENCY_NONE,0);
			}
			y-=Machine->uifont->height+2;
		}
	}
}
