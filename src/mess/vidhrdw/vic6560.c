/***************************************************************************

  MOS video interface chip 6560

***************************************************************************/
#include <math.h>
#include "mess/machine/vc20.h"
#include "mess/vidhrdw/vic6560.h"

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

#define INTERLACE (vic6560&0x80) //is this real interlace?
// ntsc 1 - 8
// pal 5 - 19
#define XPOS (((int)vic6560[0]&0x7f)*4)
#define YPOS ((int)vic6560[1]*2-VREFRESHINLINES)

// ntsc values >= 31 behave like 31
// pal value >= 32 behave like 32
#define XSIZE ((int)vic6560[2]&0x7f)
#define YSIZE (((int)vic6560[3]&0x7e)>>1)

#define MATRIX8X16 (vic6560[3]&1) // else 8x8
#define HEIGHTPIXEL (MATRIX8X16?16:8)
#define XSIZEPIXEL (xsize*8)
#define YSIZEPIXEL (YSIZE*HEIGHTPIXEL)
// colorram and backgroundcolor are changed
#define INVERTED (!(vic6560[0x0f]&8))

#define CHARGENADDR (((int)vic6560[5]&0xf)<<10)
#define VIDEOADDR ( ( ((int)vic6560[5]&0xf0)<<(10-4))\
						| ( ((int)vic6560[2]&0x80)<<(9-7)) )

#define HELPERCOLOR (vic6560[0xe]>>4)
#define BACKGROUNDCOLOR (vic6560[0xf]>>4)
#define FRAMECOLOR (vic6560[0xf]&7)

#define FREQUENCY (vic6560[0xa]&0x7f)
#define TONEON (vic6560[0xa]&0x80)
#define VOLUME ((vic6560[0xe]&0xf)

#define VIDEORAMSIZE (YSIZE*xsize)
#define CHARGENSIZE (256*HEIGHTPIXEL)

bool vic6560_pal;

static bool framecolorchanged;
static int(*vic_dma_read)(int);

#define MAX_HSIZE (vic6560_pal?VIC6561_HSIZE:VIC6560_HSIZE)
#define MAX_VSIZE (vic6560_pal?VIC6561_VSIZE:VIC6560_VSIZE)

static UINT8 backgroundcolor, framecolor, helpercolor, xsize=0;
static UINT8 chargendirty[256]={0};

#define VIC6560_LINES 261
#define VIC6561_LINES 312

#define VIC656X_LINES (vic6560_pal?VIC6561_LINES:VIC6560_LINES)
#define VIC656X_VRETRACERATE (vic6560_pal?VIC6561_VRETRACERATE:VIC6560_VRETRACERATE)

static int rasterline(void)
{
	double a=timer_get_time();

	return (int)((a-floor(a))*VIC656X_VRETRACERATE*VIC656X_LINES)
			 %VIC656X_LINES;
}

void vic6560_init(int(*dma_read)(int))
{
	vic6560_pal=false;
	vic_dma_read=dma_read;
}

void vic6561_init(int(*dma_read)(int))
{
	vic6560_pal=true;
	vic_dma_read=dma_read;
}

int vic6560_vh_start(void)
{
	videoram_size=127*63; // max xsize, max ysize
	framecolorchanged=true;
	return generic_vh_start();
}

void vic6560_vh_stop(void)
{
	 generic_vh_stop();
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
		vic6560[offset]=data;
		switch (offset) {
			case 2:
				if (vic6560_pal)	xsize=(XSIZE>32)?32:XSIZE;
				else xsize=(XSIZE>31)?31:XSIZE;
				if (VIDEORAMSIZE>0) memset(dirtybuffer,1,VIDEORAMSIZE);
				break;
			case 0:case 1:
			case 3:case 4:
			case 5:case 6:case 7:case 8:case 9:
			case 0xf:
				if (VIDEORAMSIZE>0) memset(dirtybuffer,1,VIDEORAMSIZE);
				framecolorchanged|=true;
				framecolor=Machine->pens[FRAMECOLOR];
				backgroundcolor=Machine->pens[BACKGROUNDCOLOR];
				break;
			case 0xe:
				helpercolor=Machine->pens[HELPERCOLOR];
				if (VIDEORAMSIZE>0) memset(dirtybuffer,1,VIDEORAMSIZE);
				break;
		}
	}
}

int vic6560_port_r(int offset)
{
	int val;
	switch(offset) {
	case 3: //rasterline
		val=((rasterline()&1)<<7)|(vic6560[offset]&0x7f);
		break;
	case 4: //rasterline
		VIC_LOG(1,"vic6560_port_r",(errorlog,"rastline read\n"));
		val=(rasterline()/2)&0xff;
		break;
	case 6: //lightpen horicontal
		VIC_LOG(1,"vic6560_port_r",(errorlog,"lightpen horicontal read\n"));
		val=0;
		break;
	case 7: //lightpen vertical
		VIC_LOG(1,"vic6560_port_r",(errorlog,"lightpen vertical read\n"));
		val=0;
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

// min could be lower den max
#define BETWEEN(v,min,max) (((max)>=(min)) ? (((v)>=(min))&&((v)<(max))) \
														: (((v)<(max))||((v)>=(min))) )

void vic6560_addr_w(int offset, int data)
{
	int chargen=CHARGENADDR,chargenend=(chargen+CHARGENSIZE)&0x3fff,
		 video=VIDEOADDR, videoend=(video+VIDEORAMSIZE)&0x3fff;

	if ( BETWEEN(offset,chargen,chargenend)) {
		chargendirty[((0x4000+offset-chargen)&0x3fff)/HEIGHTPIXEL]=1;
	}
	if (BETWEEN(offset,video,videoend))
		dirtybuffer[(0x4000+offset-video)&0x3fff]=true;
}

// inform about writes to the databits 8 till 11 on the 6560 vic
void vic6560_addr8_w(int offset, int data)
{
	// vc20 behaviour (only 10 address lines used)
	int begin=VIDEOADDR&0x3ff,end=(VIDEOADDR+VIDEORAMSIZE)&0x3ff;
	if (BETWEEN(offset,begin,end))
		dirtybuffer[(0x4000+offset-begin)&0x3fff]=true;
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

static void draw_character(struct osd_bitmap *bitmap,
									struct rectangle *visible,
									int ch, int xoff, int yoff,
									int bgcolor, int color)
{
	int i,j,y,x,code;

	for (y=visible->min_y,j=yoff; y<=visible->max_y; y++,j++) {

		if (vic_dma_read) {
			code=vic_dma_read((CHARGENADDR+(ch&0xff)*HEIGHTPIXEL+j)&0x3fff)&0xff;
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
			code=vic_dma_read((CHARGENADDR+(ch&0xff)*HEIGHTPIXEL+j)&0x3fff)&0xff;
		} else {
			code=0xff;
		}
		for (x=visible->min_x,i=xoff; x<=visible->max_x; x++,i++) {
			switch((code<<(i&~1))&0xc0) {
			case 0x80: bitmap->line[y][x]=color;break;
			case 0x40: bitmap->line[y][x]=framecolor;break;
			case 0xc0: bitmap->line[y][x]=helpercolor;break;
			case 0: bitmap->line[y][x]=bgcolor;break;
			}
		}
	}
}


/***************************************************************************
  Draw the game screen in the given osd_bitmap.
  Do NOT call osd_update_display() from this function,
  it will be called by the main emulation engine.
***************************************************************************/
void vic6560_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int sx, sy, offs, x, y;

	/* for every character in the Video RAM, check if it or its
		attribute has been modified since last time and update it
		 accordingly. */
	 for (offs=0,sx = 0,sy=0; offs<VIDEORAMSIZE; offs++) {
			int ch;
		  if (vic_dma_read) {
				ch=vic_dma_read((VIDEOADDR+offs)&0x3fff);
		  } else {
				ch=0xfff;
		  }
		  if (full_refresh||dirtybuffer[offs]||chargendirty[ch&0xff]) {
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
				r.max_y = sy+YPOS+HEIGHTPIXEL-1;
				if (DOCLIP(&r,&Machine->drv->visible_area)) {
					osd_mark_dirty (r.min_x,r.min_y,r.max_x,r.max_y,0);
					/* draw the character */
					if (INVERTED) {
						if (ch&0x800) {
							draw_character_multi(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
												Machine->pens[(ch>>8)&7],backgroundcolor);
						} else {
							draw_character(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
												Machine->pens[(ch>>8)&7],backgroundcolor);
						}
					} else {
						if (ch&0x800) {
							draw_character_multi(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
												backgroundcolor,Machine->pens[(ch>>8)&7]);
						} else {
							draw_character(bitmap,&r,ch, r.min_x-sx-XPOS,r.min_y-sy-YPOS,
												backgroundcolor,Machine->pens[(ch>>8)&7]);
						}
					}

				}
		  }
		  sx+=8; if (sx>=XSIZEPIXEL) sx=0, sy+=HEIGHTPIXEL;
	}
	memset(chargendirty,0,sizeof(chargendirty));
	if (full_refresh||framecolorchanged) {
		struct rectangle r;
		r.min_x=0;r.max_x=MAX_HSIZE;//-1;
		r.min_y=0;r.max_y=MAX_VSIZE;//-1;
		if (DOCLIP(&r,&Machine->drv->visible_area)) {
			if (r.min_y<YPOS)
				osd_mark_dirty (r.min_x,r.min_y,r.max_x,YPOS-1,0);
			if (r.max_y>=YPOS+YSIZEPIXEL)
				osd_mark_dirty (r.min_x,YPOS+YSIZEPIXEL,r.max_x,r.max_y,0);
			if (r.min_x<XPOS)
				osd_mark_dirty (r.min_x,r.min_y,XPOS-1,r.max_y,0);
			if (r.max_x>=XPOS+XSIZEPIXEL)
				osd_mark_dirty(XPOS+XSIZEPIXEL,r.min_y,r.max_x,r.max_y,0);
			for (y=r.min_y; y<=r.max_y; y++) {
				if ( (y<YPOS)||(y>=YPOS+YSIZEPIXEL) ) {
					for (x=r.min_x;x<=r.max_x;x++) {
						bitmap->line[y][x]=framecolor;
					}
				} else {
					for (x=r.min_x;x<XPOS;x++) bitmap->line[y][x]=framecolor;
					for (x=XPOS+XSIZEPIXEL;x<=r.max_x;x++) bitmap->line[y][x]=framecolor;
				}
			}
		}
		framecolorchanged=false;
	}
}

