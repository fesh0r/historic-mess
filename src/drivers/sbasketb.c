/***************************************************************************

Super Basketball memory map (preliminary)
(Hold down Start 1 & Start 2 keys to enter test mode on start up;
 use Start 1 to change modes)

MAIN BOARD:
0000-2fff RAM
2500-2900 Sprite RAM
3000-33ff Color RAM
3400-37ff Video RAM
6000-ffff ROM

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "M6809/M6809.h"


extern unsigned char *sbasketb_scroll;
extern unsigned char *sbasketb_palettebank;
void sbasketb_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);
void sbasketb_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *konami_dac;

extern struct VLM5030interface konami_vlm5030_interface;
extern struct SN76496interface konami_sn76496_interface;
extern struct DACinterface konami_dac_interface;
void konami_dac_w(int offset,int data);


void hyperspt_sound_w(int offset , int data);

int sbasketb_sh_timer_r(int offset)
{
	int clock;

#define TIMER_RATE 1024

	clock = cpu_gettotalcycles() / TIMER_RATE;

	return (clock & 0x3) | (VLM5030_BSY()? 0x04 : 0);
}

static void sbasketb_sh_irqtrigger_w(int offset,int data)
{
	cpu_cause_interrupt(1,0xff);
}



void sbasketb_init_machine(void)
{
        /* Set optimization flags for M6809 */
        m6809_Flags = M6809_FAST_S | M6809_FAST_U;
}



static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x3bff, MRA_RAM },    /* RAM and Video RAM */
	{ 0x3c10, 0x3c10, MRA_NOP },    /* ???? */
	{ 0x3e00, 0x3e00, input_port_0_r },
	{ 0x3e01, 0x3e01, input_port_1_r },
	{ 0x3e02, 0x3e02, input_port_2_r },
	{ 0x3e03, 0x3e03, MRA_NOP }, 	/* maybe there's a flip screen bit here */
	{ 0x3e80, 0x3e80, input_port_3_r },
	{ 0x3f00, 0x3f00, input_port_4_r },
	{ 0x6000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x2fff, MWA_RAM },
	{ 0x2500, 0x28ff, MWA_RAM, &spriteram, &spriteram_size },
	{ 0x3000, 0x33ff, colorram_w, &colorram },
	{ 0x3400, 0x37ff, videoram_w, &videoram, &videoram_size },
	{ 0x3800, 0x3bff, MWA_RAM },
	{ 0x3c00, 0x3c00, MWA_NOP },  /* Gets written to it all the time. Dunno what it does */
	{ 0x3c20, 0x3c20, MWA_RAM, &sbasketb_palettebank },  /* sprite palette bank */
	{ 0x3c80, 0x3c80, MWA_NOP },  /* ??? */
	{ 0x3c81, 0x3c81, interrupt_enable_w },  /* Interrupt enable */
	{ 0x3c83, 0x3c83, MWA_NOP },  /* Coin counter 1 */
	{ 0x3c84, 0x3c84, MWA_NOP },  /* Coin counter 2 */
	{ 0x3c85, 0x3c85, MWA_NOP },  /* Counter gets incremented on every interrupt */
	{ 0x3d00, 0x3d00, soundlatch_w },  /* Sound Command */
	{ 0x3d80, 0x3d80, sbasketb_sh_irqtrigger_w },  /* trigger IRQ on sound board? */
	{ 0x3f80, 0x3f80, MWA_RAM, &sbasketb_scroll },  /* Scroll amount */
	{ 0x6000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress sound_readmem[] =
{
	{ 0x0000, 0x3fff, MRA_ROM },
	{ 0x4000, 0x43ff, MRA_RAM },
	{ 0x6000, 0x6000, soundlatch_r },
	{ 0x8000, 0x8000, sbasketb_sh_timer_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress sound_writemem[] =
{
	{ 0x0000, 0x3fff, MWA_ROM },
	{ 0x4000, 0x43ff, MWA_RAM },
	{ 0xa000, 0xa000, VLM5030_data_w }, /* speech data */
	{ 0xc000, 0xdfff, hyperspt_sound_w },     /* speech and output controll */
	{ 0xe000, 0xe000, konami_dac_w, &konami_dac },
	{ 0xe001, 0xe001, SN76496_0_w },	/* SN76496 command */
	{ 0xe002, 0xe002, MWA_NOP },	/* SN76496 command */

	{ -1 }  /* end of table */
};



INPUT_PORTS_START( input_ports )
	PORT_START	/* IN0 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN1 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* IN2 */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_8WAY | IPF_COCKTAIL )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON3 | IPF_COCKTAIL  )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* DSW0 */
	PORT_DIPNAME( 0x03, 0x00, "Game Time", IP_KEY_NONE )
	PORT_DIPSETTING(    0x03, "30" )
	PORT_DIPSETTING(    0x01, "40" )
	PORT_DIPSETTING(    0x02, "50" )
	PORT_DIPSETTING(    0x00, "60" )
	PORT_DIPNAME( 0x04, 0x00, "Cabinet", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Upright" )
	PORT_DIPSETTING(    0x04, "Cocktail" )
	PORT_DIPNAME( 0x08, 0x08, "Starting Score", IP_KEY_NONE )
	PORT_DIPSETTING(    0x08, "70-78" )
	PORT_DIPSETTING(    0x00, "100-115" )
	PORT_DIPNAME( 0x10, 0x00, "Ranking", IP_KEY_NONE )
	PORT_DIPSETTING(    0x00, "Data Remaining" )
	PORT_DIPSETTING(    0x10, "Data Initialized" )
	PORT_DIPNAME( 0x60, 0x60, "Difficulty", IP_KEY_NONE )
	PORT_DIPSETTING(    0x60, "Easy" )
	PORT_DIPSETTING(    0x40, "Medium" )
	PORT_DIPSETTING(    0x20, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x80, 0x00, "Demo Sounds", IP_KEY_NONE )
	PORT_DIPSETTING(    0x80, "Off" )
	PORT_DIPSETTING(    0x00, "On" )

	PORT_START	/* DSW1 */
	PORT_DIPNAME( 0x0f, 0x0f, "Coin A", IP_KEY_NONE )
	PORT_DIPSETTING(    0x02, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x05, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x08, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x04, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x01, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0f, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x03, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x07, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0x0e, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x06, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0x0d, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0x0c, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0x0b, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0x0a, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x09, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
	PORT_DIPNAME( 0xf0, 0xf0, "Coin B", IP_KEY_NONE )
	PORT_DIPSETTING(    0x20, "4 Coins/1 Credit" )
	PORT_DIPSETTING(    0x50, "3 Coins/1 Credit" )
	PORT_DIPSETTING(    0x80, "2 Coins/1 Credit" )
	PORT_DIPSETTING(    0x40, "3 Coins/2 Credits" )
	PORT_DIPSETTING(    0x10, "4 Coins/3 Credits" )
	PORT_DIPSETTING(    0xf0, "1 Coin/1 Credit" )
	PORT_DIPSETTING(    0x30, "3 Coins/4 Credits" )
	PORT_DIPSETTING(    0x70, "2 Coins/3 Credits" )
	PORT_DIPSETTING(    0xe0, "1 Coin/2 Credits" )
	PORT_DIPSETTING(    0x60, "2 Coins/5 Credits" )
	PORT_DIPSETTING(    0xd0, "1 Coin/3 Credits" )
	PORT_DIPSETTING(    0xc0, "1 Coin/4 Credits" )
	PORT_DIPSETTING(    0xb0, "1 Coin/5 Credits" )
	PORT_DIPSETTING(    0xa0, "1 Coin/6 Credits" )
	PORT_DIPSETTING(    0x90, "1 Coin/7 Credits" )
	PORT_DIPSETTING(    0x00, "Free Play" )
INPUT_PORTS_END




static struct GfxLayout charlayout =
{
	8,8,    /* 8*8 characters */
	512,    /* 512 characters */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 }, /* the bitplanes are packed */
	{ 7*4*8, 6*4*8, 5*4*8, 4*4*8, 3*4*8, 2*4*8, 1*4*8, 0*4*8 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4 },
	8*4*8     /* every char takes 32 consecutive bytes */
};

static struct GfxLayout spritelayout =
{
	16,16,  /* 16*16 sprites */
	128 * 3,/* 384 sprites */
	4,      /* 4 bits per pixel */
	{ 0, 1, 2, 3 },        /* the bitplanes are packed */
	{ 15*4*16, 14*4*16, 13*4*16, 12*4*16, 11*4*16, 10*4*16, 9*4*16, 8*4*16,
	   7*4*16, 6*4*16, 5*4*16, 4*4*16, 3*4*16, 2*4*16, 1*4*16, 0*4*16 },
	{ 0*4, 1*4, 2*4, 3*4, 4*4, 5*4, 6*4, 7*4,
					8*4, 9*4, 10*4, 11*4, 12*4, 13*4, 14*4, 15*4 },
	32*4*8    /* every sprite takes 128 consecutive bytes */
};



static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x0000, &charlayout,       0, 16 },
	{ 1, 0x4000, &spritelayout, 16*16, 16*16 },
	{ -1 } /* end of array */
};



static unsigned char color_prom[] =
{
	/* 405e17 - palette red component */
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x00,0x00,0x0D,0x09,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x0D,0x00,0x09,0x09,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x00,0x00,0x00,0x0B,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x00,0x00,0x0F,0x0B,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x04,0x00,0x0F,0x0B,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x09,0x00,0x00,0x0F,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x0F,0x00,0x00,0x00,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x0A,0x00,0x04,0x00,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x06,0x00,0x00,0x0F,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x0F,0x00,0x0F,0x0F,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x09,0x00,0x00,0x0A,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x0F,0x00,0x09,0x09,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x09,0x00,0x00,0x0A,0x0F,0x09,0x0D,
	0x00,0x0E,0x0D,0x06,0x0F,0x0F,0x04,0x0A,0x04,0x09,0x00,0x00,0x0A,0x0F,0x09,0x0D,
	0x00,0x0D,0x0A,0x06,0x0F,0x00,0x04,0x09,0x04,0x06,0x02,0x0F,0x0F,0x0F,0x09,0x0D,
	0x00,0x0D,0x00,0x0D,0x08,0x00,0x0C,0x06,0x0A,0x09,0x00,0x00,0x0F,0x0C,0x0F,0x00,
	/* 405e16 - palette green component */
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x00,0x00,0x0D,0x09,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x0D,0x00,0x09,0x09,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x00,0x00,0x00,0x0B,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x0F,0x00,0x04,0x0B,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x0A,0x00,0x04,0x0B,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x00,0x00,0x06,0x00,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x0F,0x00,0x0D,0x07,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x07,0x00,0x0A,0x07,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x05,0x00,0x09,0x0F,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x00,0x00,0x0F,0x00,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x03,0x00,0x0B,0x0A,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x03,0x00,0x0B,0x0A,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x03,0x00,0x0B,0x0A,0x00,0x09,0x0D,
	0x00,0x0A,0x08,0x00,0x03,0x0F,0x0A,0x07,0x00,0x03,0x00,0x0B,0x0A,0x00,0x09,0x0D,
	0x00,0x0A,0x07,0x00,0x03,0x00,0x0A,0x07,0x00,0x05,0x02,0x0F,0x0F,0x00,0x09,0x0D,
	0x00,0x0D,0x0D,0x0D,0x09,0x0E,0x01,0x07,0x08,0x06,0x00,0x07,0x03,0x06,0x00,0x08,
	/* 405e18 - palette blue component */
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x00,0x00,0x0D,0x09,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0D,0x00,0x09,0x09,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0F,0x00,0x00,0x0B,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x00,0x00,0x00,0x0B,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0F,0x00,0x00,0x0B,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0E,0x00,0x00,0x0A,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x00,0x00,0x00,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0A,0x00,0x0F,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x04,0x00,0x00,0x04,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x0A,0x00,0x0F,0x0A,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x02,0x00,0x00,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x02,0x00,0x00,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x02,0x00,0x00,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x00,0x0F,0x05,0x00,0x02,0x00,0x00,0x00,0x0A,0x09,0x0D,
	0x00,0x06,0x05,0x00,0x00,0x0F,0x0F,0x05,0x00,0x04,0x02,0x07,0x00,0x0A,0x09,0x0D,
	0x00,0x0D,0x00,0x00,0x09,0x0D,0x01,0x08,0x06,0x0F,0x0F,0x0F,0x00,0x01,0x0A,0x00,
	/* 405e20 - character lookup table */
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x01,0x02,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x01,0x02,0x02,0x06,0x06,0x06,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x02,0x0D,0x01,0x03,0x01,0x04,0x05,0x0D,0x07,0x0A,0x09,0x0C,0x08,0x06,0x04,
	0x00,0x03,0x07,0x04,0x0D,0x03,0x05,0x08,0x05,0x08,0x0A,0x09,0x0C,0x07,0x0E,0x0F,
	0x00,0x04,0x0E,0x05,0x04,0x04,0x06,0x07,0x08,0x03,0x0A,0x04,0x0D,0x04,0x0E,0x0F,
	0x00,0x05,0x08,0x06,0x04,0x07,0x06,0x07,0x08,0x09,0x0A,0x03,0x04,0x03,0x0E,0x0F,
	0x00,0x06,0x02,0x07,0x0E,0x08,0x04,0x07,0x08,0x0A,0x0A,0x00,0x0A,0x07,0x0E,0x0F,
	0x00,0x07,0x02,0x08,0x00,0x09,0x06,0x00,0x00,0x09,0x0A,0x0B,0x0D,0x00,0x00,0x00,
	0x00,0x08,0x05,0x09,0x04,0x0A,0x00,0x07,0x08,0x0A,0x0A,0x05,0x0D,0x07,0x0E,0x03,
	0x00,0x06,0x06,0x06,0x04,0x03,0x06,0x0D,0x07,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x06,0x06,0x01,0x04,0x0C,0x06,0x05,0x0D,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x06,0x01,0x06,0x07,0x0D,0x06,0x08,0x0D,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x08,
	0x00,0x01,0x01,0x01,0x07,0x03,0x06,0x08,0x0D,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x08,
	0x00,0x0D,0x00,0x0E,0x04,0x05,0x06,0x08,0x07,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x0F,0x0E,0x02,0x0F,0x04,0x01,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	/* 405e19 - sprite lookup table */
	0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x05,0x04,0x04,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x04,0x0E,0x0F,
	0x00,0x01,0x02,0x0F,0x04,0x03,0x06,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0F,0x0E,0x0F,
	0x00,0x02,0x07,0x03,0x09,0x02,0x0B,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0B,0x0C,0x0C,
	0x00,0x01,0x01,0x00,0x09,0x01,0x0B,0x00,0x08,0x0F,0x00,0x01,0x0F,0x0F,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x09,0x06,0x0B,0x07,0x08,0x00,0x02,0x00,0x03,0x00,0x0E,0x0F,
	0x00,0x02,0x07,0x0F,0x09,0x02,0x0B,0x07,0x08,0x09,0x0A,0x0B,0x0C,0x0B,0x0C,0x0C,
	0x00,0x01,0x02,0x03,0x04,0x0F,0x06,0x07,0x08,0x09,0x0F,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0C,0x06,0x07,0x08,0x09,0x05,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0E,0x06,0x07,0x08,0x09,0x0E,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0A,0x06,0x07,0x08,0x09,0x05,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x04,0x06,0x07,0x08,0x09,0x0F,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0F,0x06,0x07,0x08,0x09,0x05,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0D,0x06,0x07,0x08,0x09,0x0E,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0E,0x06,0x07,0x08,0x09,0x05,0x0B,0x0C,0x0D,0x0E,0x0F,
	0x00,0x01,0x02,0x03,0x04,0x0A,0x06,0x07,0x08,0x09,0x0F,0x0B,0x0C,0x0D,0x0E,0x0F
};

/* filename for trackn field sample files */
static const char *sbasketball_sample_names[] =
{
	"00.sam","01.sam","02.sam","03.sam","04.sam","05.sam","06.sam","07.sam",
	"08.sam","09.sam","0a.sam","0b.sam","0c.sam","0d.sam","0e.sam","0f.sam",
	"10.sam","11.sam","12.sam","13.sam","14.sam","15.sam","16.sam","17.sam",
	"18.sam","19.sam","1a.sam","1b.sam","1c.sam","1d.sam","1e.sam","1f.sam",
	"20.sam","21.sam","22.sam","23.sam","24.sam","25.sam","26.sam","27.sam",
	"28.sam","29.sam","2a.sam","2b.sam","2c.sam","2d.sam","2e.sam","2f.sam",
	"30.sam","31.sam","32.sam","33.sam",
	0
};

#if 0
static struct SN76496interface sn76496_interface =
{
	1,	/* 1 chip */
	1789750,	/* 1.78975 Mhz ? */
	{ 255 }
};
#endif

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6809,
			1400000,        /* 1.400 Mhz ??? */
			0,
			readmem,writemem,0,0,
			interrupt,1
		},
		{
			CPU_Z80 | CPU_AUDIO_CPU,
			3579500,	/* 3.5795 Mhz ? */
			2,
			sound_readmem,sound_writemem,0,0,
			ignore_interrupt,1	/* interrupts are triggered by the main CPU */
		}
	},
	60, DEFAULT_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	sbasketb_init_machine,

	/* video hardware */
	32*8, 32*8, { 2*8, 30*8-1, 0*8, 32*8-1 },
	gfxdecodeinfo,
	256,16*16+16*16*16,
	sbasketb_vh_convert_color_prom,

	VIDEO_TYPE_RASTER,
	0,
	generic_vh_start,
	generic_vh_stop,
	sbasketb_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_DAC,
			&konami_dac_interface
		},
		{
			SOUND_SN76496,
			&konami_sn76496_interface
		},
		{
			SOUND_VLM5030,
			&konami_vlm5030_interface
		}
	}
};



/***************************************************************************

  Game driver(s)

***************************************************************************/

ROM_START( sbasketb_rom )
        ROM_REGION(0x10000)     /* 64k for code */
        ROM_LOAD( "sbb_j13.bin", 0x6000, 0x2000, 0x31c615be )
        ROM_LOAD( "sbb_j11.bin", 0x8000, 0x4000, 0x0f580fca )
        ROM_LOAD( "sbb_j09.bin", 0xc000, 0x4000, 0xb5e9f53d )

        ROM_REGION(0x10000)    /* temporary space for graphics (disposed after conversion) */
        ROM_LOAD( "sbb_e12.bin", 0x0000, 0x4000, 0xc34f6429 )
        ROM_LOAD( "sbb_h06.bin", 0x4000, 0x4000, 0x613e1e6c )
        ROM_LOAD( "sbb_h08.bin", 0x8000, 0x4000, 0x5eb646ec )
        ROM_LOAD( "sbb_h10.bin", 0xc000, 0x4000, 0xd02d8a3d )

        ROM_REGION(0x10000)     /* 64k for audio cpu */
        ROM_LOAD( "sbb_e13.bin", 0x0000, 0x2000, 0x3d1ba1cb )
        ROM_LOAD( "sbb_e15.bin", 0x2000, 0x2000, 0x1c954ee7 )
        ROM_REGION(0x10000)     /* 64k for speeck rom */
        ROM_LOAD( "sbb_e15.bin", 0x0000, 0x2000, 0x1c954ee7 )
ROM_END


static int hiload(void)
{
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	/* check if the hi score table has already been initialized */
	if (memcmp(&RAM[0x2bb7],"\x15\x1e\x14",3) == 0)
	{
			void *f;


			if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
			{
				osd_fread(f,&RAM[0x2b24],0x96);
				osd_fclose(f);
			}

			return 1;
	}
	else return 0;  /* we can't load the hi scores yet */
}

static void hisave(void)
{
	void *f;
	unsigned char *RAM = Machine->memory_region[Machine->drv->cpu[0].memory_region];


	if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
	{
			osd_fwrite(f,&RAM[0x2b24],0x96);
			osd_fclose(f);
	}
}



struct GameDriver sbasketb_driver =
{
	__FILE__,
	0,
	"sbasketb",
	"Super Basketball",
	"1984",
	"Konami",
	"Zsolt Vasvari\nTim Linquist (color info)\nMarco Cassili\nTatsuyuki Satoh(speech sound)",
	0,
	&machine_driver,

	sbasketb_rom,
	0, 0,
	sbasketball_sample_names,
	0,	/* sound_prom */

	input_ports,

	color_prom, 0, 0,
	ORIENTATION_DEFAULT,

	hiload, hisave
};