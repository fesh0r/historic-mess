/***************************************************************************

  Data East 16 bit games - Bryan McPhail, mish@tendril.force9.net

  This file contains drivers for:

    * Heavy Barrel                            (USA set)
    * Heavy Barrel                            (World set)
	* Bad Dudes vs. Dragonninja               (USA set)
    * Dragonninja                             (Japanese version of above)
	* Birdy Try                               (Japanese set)
    * Robocop                                 (World bootleg rom set)
    * Robocop                                 (World rev 3)
    * Robocop                                 (USA rev 1)
    * Robocop                                 (USA rev 0)
    * Hippodrome                              (USA set)
    * Fighting Fantasy                        (Japanese version of above)
    * Sly Spy                                 (USA rev 3)
    * Sly Spy                                 (USA rev 2)
    * Secret Agent                            (World set)
    * Midnight Resistance                     (World set)
	* Midnight Resistance                     (USA set)
    * Midnight Resistance                     (Japanese set)
	* Boulderdash                             (World set)

	Heavy Barrel, Bad Dudes, Robocop, Birdy Try & Hippodrome use the 'MEC-M1'
motherboard and varying game boards.  Sly Spy, Midnight Resistance and
Boulderdash use the same graphics chips but are different pcbs.

	There are Secret Agent (bootleg) and Robocop (bootleg) sets to add.

  Thanks to Gouky & Richard Bush for information along the way, especially
  Gouky's patch for Bad Dudes & YM3812 information!
	Thanks to JC Alexander for fix to Robocop ending!

Cheats:

Hippodrome: Level number held in 0xFF8011

Bad Dudes: level number held in FF821D
Put a bpx at 0x1b94 to halt at level check and choose level or end-seq
Sly Spy: bpx at 5d8 for level check, can go to any level.
(see comments in patch section below).

Mid res: bpx at c02 for level check, can go straight to credits.
         Level number held in 100006 (word)
         bpx 10a6 - so you can choose level at start of game.
         Put PC to 0xa1e in game to trigger continue screen

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"
#include "cpu/h6280/h6280.h"

/* Video emulation definitions */
int  dec0_vh_start(void);
void dec0_vh_stop(void);
int  dec0_nodma_vh_start(void);
void dec0_nodma_vh_stop(void);
void hbarrel_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void baddudes_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void birdtry_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void robocop_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void hippodrm_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void slyspy_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);
void midres_vh_screenrefresh(struct osd_bitmap *bitmap,int full_refresh);

extern unsigned char *dec0_pf1_rowscroll,*dec0_pf2_rowscroll,*dec0_pf3_rowscroll;
extern unsigned char *dec0_pf1_colscroll,*dec0_pf2_colscroll,*dec0_pf3_colscroll;
extern unsigned char *dec0_pf1_data,*dec0_pf2_data,*dec0_pf3_data;

void dec0_pf1_control_0_w(int offset,int data);
void dec0_pf1_control_1_w(int offset,int data);
void dec0_pf1_rowscroll_w(int offset,int data);
void dec0_pf1_colscroll_w(int offset,int data);
void dec0_pf1_data_w(int offset,int data);
int dec0_pf1_data_r(int offset);
void dec0_pf2_control_0_w(int offset,int data);
void dec0_pf2_control_1_w(int offset,int data);
void dec0_pf2_rowscroll_w(int offset,int data);
void dec0_pf2_colscroll_w(int offset,int data);
void dec0_pf2_data_w(int offset,int data);
int dec0_pf2_data_r(int offset);
void dec0_pf3_control_0_w(int offset,int data);
void dec0_pf3_control_1_w(int offset,int data);
void dec0_pf3_rowscroll_w(int offset,int data);
void dec0_pf3_colscroll_w(int offset,int data);
int dec0_pf3_colscroll_r(int offset);
void dec0_pf3_data_w(int offset,int data);
int dec0_pf3_data_r(int offset);
void dec0_priority_w(int offset,int data);
void dec0_update_sprites(int offset, int data);

void dec0_paletteram_w_rg(int offset,int data);
void dec0_paletteram_w_b(int offset,int data);

int dec0_pf3_data_8bit_r(int offset);
void dec0_pf3_data_8bit_w(int offset, int data);
void dec0_pf3_control_8bit_w(int offset, int data);

/* System prototypes - from machine/dec0.c */
extern void dec0_custom_memory(void);
extern int dec0_controls_read(int offset);
extern int dec0_rotary_read(int offset);
extern int midres_controls_read(int offset);
extern int slyspy_controls_read(int offset);
extern int slyspy_protection_r(int offset);
extern void slyspy_state_w(int offset,int data);
extern int slyspy_state_r(int offset);
extern void slyspy_240000_w(int offset,int data);
extern void slyspy_242000_w(int offset,int data);
extern void slyspy_246000_w(int offset,int data);
extern void slyspy_248000_w(int offset,int data);
extern void slyspy_24c000_w(int offset,int data);
extern void slyspy_24e000_w(int offset,int data);

extern void dec0_i8751_write(int data);
extern void dec0_i8751_reset(void);
extern int hippodrm_prot_r(int offset);
extern void hippodrm_prot_w(int offset,int data);
extern int hippodrm_shared_r(int offset);
extern void hippodrm_shared_w(int offset,int data);

unsigned char *dec0_ram;

/******************************************************************************/

static void dec0_control_w(int offset,int data)
{
	switch (offset)
	{
		case 0: /* Playfield & Sprite priority */
			dec0_priority_w(0,data);
			break;

		case 2: /* DMA flag */
			dec0_update_sprites(0,0);
			break;

		case 4: /* 6502 sound cpu */
			soundlatch_w(0,data & 0xff);
			cpu_cause_interrupt(1,M6502_INT_NMI);
			break;

		case 6: /* Intel 8751 microcontroller - Bad Dudes, Heavy Barrel, Birdy Try only */
			dec0_i8751_write(data);
			break;

		case 8: /* Interrupt ack (VBL - IRQ 6) */
			break;

		case 0xa: /* ? */
 			if (errorlog) fprintf(errorlog,"CPU #0 PC %06x: warning - write %02x to unmapped memory address %06x\n",cpu_get_pc(),data,0x30c010+offset);
			break;

		case 0xe: /* Reset Intel 8751? - not sure, all the games write here at startup */
			dec0_i8751_reset();
 			if (errorlog) fprintf(errorlog,"CPU #0 PC %06x: warning - write %02x to unmapped memory address %06x\n",cpu_get_pc(),data,0x30c010+offset);
			break;

		default:
			if (errorlog) fprintf(errorlog,"CPU #0 PC %06x: warning - write %02x to unmapped memory address %06x\n",cpu_get_pc(),data,0x30c010+offset);
			break;
	}
}

static void slyspy_control_w(int offset, int data)
{
    switch (offset) {
    	case 0:
			soundlatch_w(0,data & 0xff);
			cpu_cause_interrupt(1,H6280_INT_NMI);
			break;
		case 2:
			dec0_priority_w(0,data);
			break;
    }
}

static void midres_sound_w(int offset, int data)
{
	soundlatch_w(0,data & 0xff);
	cpu_cause_interrupt(1,H6280_INT_NMI);
}

/******************************************************************************/

static struct MemoryReadAddress dec0_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x242800, 0x243fff, MRA_BANK3 }, /* Robocop only */
	{ 0x244000, 0x245fff, dec0_pf1_data_r },
	{ 0x24a000, 0x24a7ff, dec0_pf2_data_r },
	{ 0x24c800, 0x24c87f, dec0_pf3_colscroll_r },
	{ 0x24d000, 0x24d7ff, dec0_pf3_data_r },
	{ 0x300000, 0x30001f, dec0_rotary_read },
	{ 0x30c000, 0x30c00b, dec0_controls_read },
	{ 0x310000, 0x3107ff, paletteram_word_r },
	{ 0x314000, 0x3147ff, paletteram_2_word_r },
	{ 0xff8000, 0xffbfff, MRA_BANK1 }, /* Main ram */
	{ 0xffc000, 0xffc7ff, MRA_BANK2 }, /* Sprites */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress dec0_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },

	{ 0x240000, 0x240007, dec0_pf1_control_0_w },	/* text layer */
	{ 0x240010, 0x240017, dec0_pf1_control_1_w },
 	{ 0x242000, 0x24207f, dec0_pf1_colscroll_w, &dec0_pf1_colscroll },
	{ 0x242400, 0x2427ff, dec0_pf1_rowscroll_w, &dec0_pf1_rowscroll },
	{ 0x242800, 0x243fff, MWA_BANK3 }, /* Robocop only */
	{ 0x244000, 0x245fff, dec0_pf1_data_w, &dec0_pf1_data },

	{ 0x246000, 0x246007, dec0_pf2_control_0_w },	/* first tile layer */
	{ 0x246010, 0x246017, dec0_pf2_control_1_w },
	{ 0x248000, 0x24807f, dec0_pf2_colscroll_w, &dec0_pf2_colscroll },
	{ 0x248400, 0x2487ff, dec0_pf2_rowscroll_w, &dec0_pf2_rowscroll },
	{ 0x24a000, 0x24a7ff, dec0_pf2_data_w, &dec0_pf2_data },

	{ 0x24c000, 0x24c007, dec0_pf3_control_0_w },	/* second tile layer */
	{ 0x24c010, 0x24c017, dec0_pf3_control_1_w },
	{ 0x24c800, 0x24c87f, dec0_pf3_colscroll_w, &dec0_pf3_colscroll },
	{ 0x24cc00, 0x24cfff, dec0_pf3_rowscroll_w, &dec0_pf3_rowscroll },
	{ 0x24d000, 0x24d7ff, dec0_pf3_data_w, &dec0_pf3_data },

	{ 0x30c010, 0x30c01f, dec0_control_w },	/* Priority, sound, etc. */
	{ 0x310000, 0x3107ff, dec0_paletteram_w_rg, &paletteram },	/* Red & Green bits */
	{ 0x314000, 0x3147ff, dec0_paletteram_w_b, &paletteram_2 },	/* Blue bits */
	{ 0xff8000, 0xffbfff, MWA_BANK1, &dec0_ram },
	{ 0xffc000, 0xffc7ff, MWA_BANK2, &spriteram },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress robocop_sub_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 }, /* Main ram */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress robocop_sub_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 }, /* Main ram */
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress hippodrm_sub_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x180000, 0x1800ff, hippodrm_shared_r },
	{ 0x1a1000, 0x1a17ff, dec0_pf3_data_8bit_r },
	{ 0x1d0000, 0x1d00ff, hippodrm_prot_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 }, /* Main ram */
	{ 0x1ff402, 0x1ff403, input_port_5_r }, /* VBL */
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress hippodrm_sub_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x180000, 0x1800ff, hippodrm_shared_w },
	{ 0x1a0000, 0x1a001f, dec0_pf3_control_8bit_w },
	{ 0x1a1000, 0x1a17ff, dec0_pf3_data_8bit_w },
	{ 0x1d0000, 0x1d00ff, hippodrm_prot_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 }, /* Main ram */
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress slyspy_readmem[] =
{
	{ 0x000000, 0x05ffff, MRA_ROM },
	{ 0x244000, 0x244001, slyspy_state_r }, /* protection */
	{ 0x304000, 0x307fff, MRA_BANK1 }, /* Sly spy main ram */
	{ 0x308000, 0x3087ff, MRA_BANK2 }, /* Sprites */
	{ 0x310000, 0x3107ff, paletteram_word_r },
	{ 0x314008, 0x31400f, slyspy_controls_read },
	{ 0x31c000, 0x31c00f, slyspy_protection_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress slyspy_writemem[] =
{
	{ 0x000000, 0x05ffff, MWA_ROM },

	/* These locations aren't real!  They are just there so memory is allocated */
	{ 0x232000, 0x23207f, MWA_NOP, &dec0_pf2_colscroll },
	{ 0x232400, 0x2327ff, MWA_NOP, &dec0_pf2_rowscroll },
	{ 0x23c000, 0x23c07f, MWA_NOP, &dec0_pf1_colscroll },
	{ 0x23c400, 0x23c7ff, MWA_NOP, &dec0_pf1_rowscroll },
	{ 0x200000, 0x2007ff, MWA_NOP, &dec0_pf2_data },
	{ 0x202000, 0x203fff, MWA_NOP, &dec0_pf1_data },

	{ 0x244000, 0x244001, MWA_NOP }, /* Extra protection? */

	/* The location of p1 & pf2 can change between these according to protection */
	{ 0x240000, 0x241fff, slyspy_240000_w },
	{ 0x242000, 0x243fff, slyspy_242000_w },
	{ 0x246000, 0x247fff, slyspy_246000_w },
	{ 0x248000, 0x249fff, slyspy_248000_w },
	{ 0x24c000, 0x24dfff, slyspy_24c000_w },
	{ 0x24e000, 0x24ffff, slyspy_24e000_w },

	{ 0x24a000, 0x24a001, slyspy_state_w }, /* Protection */

	/* Pf3 is unaffected by protection */
	{ 0x300000, 0x300007, dec0_pf3_control_0_w },
	{ 0x300010, 0x300017, dec0_pf3_control_1_w },
	{ 0x300800, 0x30087f, dec0_pf3_colscroll_w, &dec0_pf3_colscroll },
	{ 0x300c00, 0x300fff, dec0_pf3_rowscroll_w, &dec0_pf3_rowscroll },
	{ 0x301000, 0x3017ff, dec0_pf3_data_w, &dec0_pf3_data },

	{ 0x304000, 0x307fff, MWA_BANK1, &dec0_ram }, /* Sly spy main ram */
	{ 0x308000, 0x3087ff, MWA_BANK2, &spriteram },
	{ 0x310000, 0x3107ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x314000, 0x314003, slyspy_control_w },
	{ 0x31c000, 0x31c00f, MWA_NOP },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress midres_readmem[] =
{
	{ 0x000000, 0x07ffff, MRA_ROM },
	{ 0x100000, 0x103fff, MRA_BANK1 },
	{ 0x120000, 0x1207ff, MRA_BANK2 },
	{ 0x180000, 0x18000f, midres_controls_read },
	{ 0x320000, 0x321fff, dec0_pf1_data_r },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress midres_writemem[] =
{
	{ 0x000000, 0x07ffff, MWA_ROM },
	{ 0x100000, 0x103fff, MWA_BANK1, &dec0_ram },
	{ 0x120000, 0x1207ff, MWA_BANK2, &spriteram },
	{ 0x140000, 0x1407ff, paletteram_xxxxBBBBGGGGRRRR_word_w, &paletteram },
	{ 0x160000, 0x160001, dec0_priority_w },
	{ 0x180008, 0x18000f, MWA_NOP }, /* ?? watchdog ?? */
	{ 0x1a0000, 0x1a0001, midres_sound_w },

	{ 0x200000, 0x200007, dec0_pf2_control_0_w },
	{ 0x200010, 0x200017, dec0_pf2_control_1_w },
	{ 0x220000, 0x2207ff, dec0_pf2_data_w, &dec0_pf2_data },
	{ 0x220800, 0x220fff, dec0_pf2_data_w },	/* mirror address used in end sequence */
	{ 0x240000, 0x24007f, dec0_pf2_colscroll_w, &dec0_pf2_colscroll },
	{ 0x240400, 0x2407ff, dec0_pf2_rowscroll_w, &dec0_pf2_rowscroll },

	{ 0x280000, 0x280007, dec0_pf3_control_0_w },
	{ 0x280010, 0x280017, dec0_pf3_control_1_w },
	{ 0x2a0000, 0x2a07ff, dec0_pf3_data_w, &dec0_pf3_data },
	{ 0x2c0000, 0x2c007f, dec0_pf3_colscroll_w, &dec0_pf3_colscroll },
	{ 0x2c0400, 0x2c07ff, dec0_pf3_rowscroll_w, &dec0_pf3_rowscroll },

	{ 0x300000, 0x300007, dec0_pf1_control_0_w },
	{ 0x300010, 0x300017, dec0_pf1_control_1_w },
	{ 0x320000, 0x321fff, dec0_pf1_data_w, &dec0_pf1_data },
	{ 0x340000, 0x34007f, dec0_pf1_colscroll_w, &dec0_pf1_colscroll },
	{ 0x340400, 0x3407ff, dec0_pf1_rowscroll_w, &dec0_pf1_rowscroll },
	{ -1 }  /* end of table */
};

/******************************************************************************/

static void YM3812_w(int offset, int data)
{
	switch (offset) {
	case 0:
		YM3812_control_port_0_w(0,data);
		break;
	case 1:
		YM3812_write_port_0_w(0,data);
		break;
	}
}

static void YM2203_w(int offset, int data)
{
	switch (offset) {
	case 0:
		YM2203_control_port_0_w(0,data);
		break;
	case 1:
		YM2203_write_port_0_w(0,data);
		break;
	}
}

static struct MemoryReadAddress dec0_s_readmem[] =
{
	{ 0x0000, 0x05ff, MRA_RAM },
	{ 0x3000, 0x3000, soundlatch_r },
	{ 0x3800, 0x3800, OKIM6295_status_0_r },
	{ 0x8000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress dec0_s_writemem[] =
{
	{ 0x0000, 0x05ff, MWA_RAM },
	{ 0x0800, 0x0801, YM2203_w },
	{ 0x1000, 0x1001, YM3812_w },
	{ 0x3800, 0x3800, OKIM6295_data_0_w },
	{ 0x8000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

/* Physical memory map (21 bits) */
static struct MemoryReadAddress slyspy_s_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x0a0000, 0x0a0001, MRA_NOP }, /* Protection counter */
	{ 0x0e0000, 0x0e0001, OKIM6295_status_0_r },
	{ 0x0f0000, 0x0f0001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress slyspy_s_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x090000, 0x090001, YM3812_w },
	{ 0x0b0000, 0x0b0001, YM2203_w},
	{ 0x0e0000, 0x0e0001, OKIM6295_data_0_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 }, /* Main ram */
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

static struct MemoryReadAddress midres_s_readmem[] =
{
	{ 0x000000, 0x00ffff, MRA_ROM },
	{ 0x130000, 0x130001, OKIM6295_status_0_r },
	{ 0x138000, 0x138001, soundlatch_r },
	{ 0x1f0000, 0x1f1fff, MRA_BANK8 },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress midres_s_writemem[] =
{
	{ 0x000000, 0x00ffff, MWA_ROM },
	{ 0x108000, 0x108001, YM3812_w },
	{ 0x118000, 0x118001, YM2203_w },
	{ 0x130000, 0x130001, OKIM6295_data_0_w },
	{ 0x1f0000, 0x1f1fff, MWA_BANK8 }, /* Main ram */
	{ 0x1ff402, 0x1ff403, H6280_irq_status_w },
	{ -1 }  /* end of table */
};

/******************************************************************************/

#define DEC0_PLAYER1_CONTROL \
	PORT_START /* Player 1 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 3 - unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 4 - unused */

#define DEC0_PLAYER2_CONTROL \
	PORT_START /* Player 2 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 3 - unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED ) /* Button 4 - unused */

#define DEC0_MACHINE_CONTROL \
	PORT_START /* Credits, start buttons */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_UNUSED ) /* PL1 Button 5 - unused */ \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED ) /* PL2 Button 5 - unused */ \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_START1 ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_START2 ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_COIN1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_COIN2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_COIN3 ) /* Service */ \
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_VBLANK )

#define DEC0_COIN_SETTING \
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Coin_A ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x01, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x03, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x02, DEF_STR( 1C_2C ) ) \
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Coin_B ) ) \
	PORT_DIPSETTING(    0x00, DEF_STR( 3C_1C ) ) \
	PORT_DIPSETTING(    0x04, DEF_STR( 2C_1C ) ) \
	PORT_DIPSETTING(    0x0c, DEF_STR( 1C_1C ) ) \
	PORT_DIPSETTING(    0x08, DEF_STR( 1C_2C ) ) \

INPUT_PORTS_START( hbarrel )
	DEC0_PLAYER1_CONTROL
	DEC0_PLAYER2_CONTROL
	DEC0_MACHINE_CONTROL

	PORT_START	/* Dip switch bank 1 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x30, 0x30, DEF_STR( Bonus_Life ) )
	PORT_DIPSETTING(    0x30, "30k 80k 160k" )
	PORT_DIPSETTING(    0x10, "50k 120k 190k" )
	PORT_DIPSETTING(    0x20, "100k 200k 300k" )
	PORT_DIPSETTING(    0x00, "150k 300k 450k" )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, 0, KEYCODE_Z, KEYCODE_X, 0, 0 )

	PORT_START	/* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, 0, KEYCODE_N, KEYCODE_M, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( baddudes )
	DEC0_PLAYER1_CONTROL
	DEC0_PLAYER2_CONTROL
	DEC0_MACHINE_CONTROL

	PORT_START	/* DSW0 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x02, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* player 1 12-way rotary control - converted in controls_r() */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused */

	PORT_START	/* player 2 12-way rotary control - converted in controls_r() */
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNKNOWN )	/* unused */
INPUT_PORTS_END

INPUT_PORTS_START( robocop )
	DEC0_PLAYER1_CONTROL
	DEC0_PLAYER2_CONTROL
	DEC0_MACHINE_CONTROL

	PORT_START	/* DSW0 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, "Player Energy" )
	PORT_DIPSETTING(    0x01, "Low" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "High" )
	PORT_DIPSETTING(    0x00, "Very High" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x0c, "Easy" )
	PORT_DIPSETTING(    0x04, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, "Bonus Stage Energy" )
	PORT_DIPSETTING(    0x00, "Low" )
	PORT_DIPSETTING(    0x20, "High" )
	PORT_DIPNAME( 0x40, 0x40, "Brink Time" )
	PORT_DIPSETTING(    0x40, "Normal" )
	PORT_DIPSETTING(    0x00, "Less" )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( hippodrm )
	DEC0_PLAYER1_CONTROL
	DEC0_PLAYER2_CONTROL
	DEC0_MACHINE_CONTROL

	PORT_START	/* Dip switch bank 1 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "2" )
	PORT_DIPSETTING(    0x02, "3" )
	PORT_DIPSETTING(    0x00, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x04, "Difficult" )
	PORT_DIPSETTING(    0x00, "Very Difficult" )
	PORT_DIPNAME( 0x30, 0x30, "Player & Enemy Energy" )
	PORT_DIPSETTING(    0x10, "Low" )
	PORT_DIPSETTING(    0x20, "Medium" )
	PORT_DIPSETTING(    0x30, "High" )
	PORT_DIPSETTING(    0x00, "Very High" )
	PORT_DIPNAME( 0x40, 0x40, "Enemy Power on Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START
	PORT_BIT( 0x01, IP_ACTIVE_HIGH, IPT_VBLANK )
INPUT_PORTS_END

#define DEC1_PLAYER1_CONTROL \
	PORT_START /* Player 1 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) /* button 3 - unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START1 )

#define DEC1_PLAYER2_CONTROL \
	PORT_START /* Player 2 controls */ \
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP    | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT  | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_8WAY | IPF_COCKTAIL ) \
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1 | IPF_COCKTAIL ) \
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON2 | IPF_COCKTAIL ) \
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED ) /* button 3 - unused */ \
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_START2 )


INPUT_PORTS_START( slyspy )
	DEC1_PLAYER1_CONTROL
	DEC1_PLAYER2_CONTROL

	PORT_START	/* Credits, start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )	/* screwed up colors with ACTIVE_LOW */
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, "Energy" )
	PORT_DIPSETTING(    0x01, "Low" )
	PORT_DIPSETTING(    0x03, "Medium" )
	PORT_DIPSETTING(    0x02, "High" )
	PORT_DIPSETTING(    0x00, "Very High" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

INPUT_PORTS_START( midres )
	DEC1_PLAYER1_CONTROL
	DEC1_PLAYER2_CONTROL

	PORT_START	/* Credits */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x01, "1" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "5" )
	PORT_BITX( 0,       0x00, IPT_DIPSWITCH_SETTING | IPF_CHEAT, "Infinite", IP_KEY_NONE, IP_JOY_NONE )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x04, "Easy" )
	PORT_DIPSETTING(    0x0c, "Normal" )
	PORT_DIPSETTING(    0x08, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x20, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x00, "Allow Continue" )
	PORT_DIPSETTING(    0x40, DEF_STR( No ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )

	PORT_START	/* player 1 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE, 25, 10, 0, 0, 0, KEYCODE_Z, KEYCODE_X, 0, 0 )

	PORT_START	/* player 2 12-way rotary control - converted in controls_r() */
	PORT_ANALOGX( 0xff, 0x00, IPT_DIAL | IPF_REVERSE | IPF_PLAYER2, 25, 10, 0, 0, 0, KEYCODE_N, KEYCODE_M, 0, 0 )
INPUT_PORTS_END

INPUT_PORTS_START( bouldash )
	DEC1_PLAYER1_CONTROL
	DEC1_PLAYER2_CONTROL

	PORT_START	/* Credits, start buttons */
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_COIN3 )
	PORT_BIT( 0x08, IP_ACTIVE_HIGH, IPT_VBLANK )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START	/* Dip switch bank 1 */
	DEC0_COIN_SETTING
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unused ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, DEF_STR( Demo_Sounds ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x20, DEF_STR( On ) )
	PORT_DIPNAME( 0x40, 0x40, DEF_STR( Flip_Screen ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x80, 0x00, DEF_STR( Cabinet ) )
	PORT_DIPSETTING(    0x00, DEF_STR( Upright ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Cocktail ) )

	PORT_START	/* Dip switch bank 2 */
	PORT_DIPNAME( 0x03, 0x03, DEF_STR( Lives ) )
	PORT_DIPSETTING(    0x00, "2" )
	PORT_DIPSETTING(    0x03, "3" )
	PORT_DIPSETTING(    0x02, "4" )
	PORT_DIPSETTING(    0x01, "5" )
	PORT_DIPNAME( 0x0c, 0x0c, DEF_STR( Difficulty ) )
	PORT_DIPSETTING(    0x08, "Easy" )
	PORT_DIPSETTING(    0x04, "Medium" )
	PORT_DIPSETTING(    0x0c, "Hard" )
	PORT_DIPSETTING(    0x00, "Hardest" )
	PORT_DIPNAME( 0x10, 0x10, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x10, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
	PORT_DIPNAME( 0x20, 0x20, "Game" )
	PORT_DIPSETTING(    0x20, "Part 1" )
	PORT_DIPSETTING(    0x00, "Part 2" )
	PORT_DIPNAME( 0x40, 0x40, "Allow Continue" )
	PORT_DIPSETTING(    0x00, DEF_STR( No ) )
	PORT_DIPSETTING(    0x40, DEF_STR( Yes ) )
	PORT_DIPNAME( 0x80, 0x80, DEF_STR( Unknown ) )
	PORT_DIPSETTING(    0x80, DEF_STR( Off ) )
	PORT_DIPSETTING(    0x00, DEF_STR( On ) )
INPUT_PORTS_END

/******************************************************************************/

static struct GfxLayout charlayout =
{
	8,8,	/* 8*8 chars */
	4096,
	4,		/* 4 bits per pixel  */
	{ 0x00000*8, 0x10000*8, 0x8000*8, 0x18000*8 },
	{ 0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8 },
	8*8	/* every char takes 8 consecutive bytes */
};

static struct GfxLayout tilelayout =
{
	16,16,
	4096,
	4,
	{ 0x20000*8, 0x60000*8, 0x00000*8, 0x40000*8 },
	{ 16*8+0, 16*8+1, 16*8+2, 16*8+3, 16*8+4, 16*8+5, 16*8+6, 16*8+7,
			0, 1, 2, 3, 4, 5, 6, 7 },
	{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8,
			8*8, 9*8, 10*8, 11*8, 12*8, 13*8, 14*8, 15*8 },
	16*16
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0x000000, &charlayout,   0, 16 },	/* Characters 8x8 */
	{ 1, 0x020000, &tilelayout, 512, 16 },	/* Tiles 16x16 */
	{ 1, 0x0a0000, &tilelayout, 768, 16 },	/* Tiles 16x16 */
	{ 1, 0x120000, &tilelayout, 256, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

static struct GfxDecodeInfo midres_gfxdecodeinfo[] =
{
	{ 1, 0x000000, &charlayout, 256, 16 },	/* Characters 8x8 */
	{ 1, 0x020000, &tilelayout, 512, 16 },	/* Tiles 16x16 */
	{ 1, 0x0a0000, &tilelayout, 768, 16 },	/* Tiles 16x16 */
	{ 1, 0x120000, &tilelayout,   0, 16 },	/* Sprites 16x16 */
	{ -1 } /* end of array */
};

/******************************************************************************/

static void sound_irq(int linestate)
{
	cpu_set_irq_line(1,0,linestate); /* IRQ */
}

static void sound_irq2(int linestate)
{
	cpu_set_irq_line(1,1,linestate); /* IRQ2 */
}

static struct YM2203interface ym2203_interface =
{
	1,
	1500000,	/* 12MHz clock divided by 8 = 1.50 MHz */
	{ YM2203_VOL(35,90) },
	AY8910_DEFAULT_GAIN,
	{ 0 },
	{ 0 },
	{ 0 },
	{ 0 }
};

static struct YM3812interface ym3812_interface =
{
	1,			/* 1 chip */
	3000000,	/* 12MHz clock divided by 4 = 3.00 MHz */
	{ 40 },
	{ sound_irq },
};

static struct YM3812interface ym3812b_interface =
{
	1,			/* 1 chip */
	3000000,
	{ 40 },
	{ sound_irq2 },
};

static struct OKIM6295interface okim6295_interface =
{
	1,                  /* 1 chip */
	{ 8000 },           /* 8000Hz frequency */
	{ 3 },              /* memory region 3 */
	{ 80 }
};

/******************************************************************************/

static struct MachineDriver hbarrel_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL, level 5 interrupts from i8751 */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
 	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	hbarrel_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver baddudes_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL, level 5 interrupts from i8751 */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
 	32*8, 64*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	baddudes_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver birdtry_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL, level 5 interrupts from i8751 */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
 	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	birdtry_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver robocop_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_H6280,
			21477200/16, /* 21.4772MHz clock */
			robocop_sub_readmem,robocop_sub_writemem,0,0,
			ignore_interrupt,0
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	robocop_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver robocopb_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	robocop_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver hippodrm_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			10000000,
			dec0_readmem,dec0_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_M6502 | CPU_AUDIO_CPU,
			1500000,
			dec0_s_readmem,dec0_s_writemem,0,0,
			ignore_interrupt,0
		},
		{
			CPU_H6280,
			21477200/16, /* 21.4772MHz clock */
			hippodrm_sub_readmem,hippodrm_sub_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	5,	/* Interleave between H6280 & 68000 */
	0,

	/* video hardware */
 	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_vh_start,
	dec0_vh_stop,
	hippodrm_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver slyspy_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,
			slyspy_readmem,slyspy_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU,
			3000000,
			slyspy_s_readmem,slyspy_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
 	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_nodma_vh_start,
	dec0_nodma_vh_stop,
	slyspy_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812b_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

static struct MachineDriver midres_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			12000000,
			midres_readmem,midres_writemem,0,0,
			m68_level6_irq,1 /* VBL */
		},
		{
			CPU_H6280 | CPU_AUDIO_CPU,
			3000000,
			midres_s_readmem,midres_s_writemem,0,0,
			ignore_interrupt,0
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,	/* 1 CPU slice per frame - interleaving is forced when a sound command is written */
	0,

	/* video hardware */
	32*8, 32*8, { 0*8, 32*8-1, 1*8, 31*8-1 },

	midres_gfxdecodeinfo,
	1024, 1024,
	0,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	dec0_nodma_vh_start,
	dec0_nodma_vh_stop,
	midres_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_YM2203,
			&ym2203_interface
		},
		{
			SOUND_YM3812,
			&ym3812b_interface
		},
		{
			SOUND_OKIM6295,
			&okim6295_interface
		}
	}
};

/******************************************************************************/

ROM_START( hbarrel )
	ROM_REGIONX( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "hb_ec04.rom",  0x00000, 0x10000, 0xd01bc3db )
	ROM_LOAD_ODD ( "hb_ec01.rom",  0x00000, 0x10000, 0x6756f8ae )
	ROM_LOAD_EVEN( "hb05.bin",     0x20000, 0x10000, 0x2087d570 )
	ROM_LOAD_ODD ( "hb02.bin",     0x20000, 0x10000, 0x815536ae )
	ROM_LOAD_EVEN( "hb_ec06.rom",  0x40000, 0x10000, 0x61ec20d8 )
	ROM_LOAD_ODD ( "hb_ec03.rom",  0x40000, 0x10000, 0x720c6b13 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "hb_ec25.rom",  0x000000, 0x10000, 0x2e5732a2 )	/* chars */
	ROM_LOAD( "hb_ec26.rom",  0x010000, 0x10000, 0x161a2c4d )
	ROM_LOAD( "hb18.bin",     0x020000, 0x10000, 0xef664373 )	/* tiles */
	ROM_LOAD( "hb17.bin",     0x030000, 0x10000, 0xa4f186ac )
	ROM_LOAD( "hb20.bin",     0x040000, 0x10000, 0x2fc13be0 )
	ROM_LOAD( "hb19.bin",     0x050000, 0x10000, 0xd6b47869 )
	ROM_LOAD( "hb22.bin",     0x060000, 0x10000, 0x50d6a1ad )
	ROM_LOAD( "hb21.bin",     0x070000, 0x10000, 0xf01d75c5 )
	ROM_LOAD( "hb24.bin",     0x080000, 0x10000, 0xae377361 )
	ROM_LOAD( "hb23.bin",     0x090000, 0x10000, 0xbbdaf771 )
	ROM_LOAD( "hb29.bin",     0x0a0000, 0x10000, 0x5514b296 )	/* tiles */
	/* b0000-bfff empty */
	ROM_LOAD( "hb30.bin",     0x0c0000, 0x10000, 0x5855e8ef )
	/* d0000-dfff empty */
	ROM_LOAD( "hb27.bin",     0x0e0000, 0x10000, 0x99db7b9c )
	/* f0000-ffff empty */
	ROM_LOAD( "hb28.bin",     0x100000, 0x10000, 0x33ce2b1a )
	/* 110000-11fff empty */
	ROM_LOAD( "hb15.bin",     0x120000, 0x10000, 0x21816707 )	/* sprites */
	ROM_LOAD( "hb16.bin",     0x130000, 0x10000, 0xa5684574 )
	ROM_LOAD( "hb11.bin",     0x140000, 0x10000, 0x5c768315 )
	ROM_LOAD( "hb12.bin",     0x150000, 0x10000, 0x8b64d7a4 )
	ROM_LOAD( "hb13.bin",     0x160000, 0x10000, 0x56e3ed65 )
	ROM_LOAD( "hb14.bin",     0x170000, 0x10000, 0xbedfe7f3 )
	ROM_LOAD( "hb09.bin",     0x180000, 0x10000, 0x26240ea0 )
	ROM_LOAD( "hb10.bin",     0x190000, 0x10000, 0x47d95447 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "hb_ec07.rom",  0x8000, 0x8000, 0x16a5a1aa )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "hb_ec08.rom",  0x0000, 0x10000, 0x2159a609 )
ROM_END

ROM_START( hbarrelu )
	ROM_REGIONX( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "hb04.bin",     0x00000, 0x10000, 0x4877b09e )
	ROM_LOAD_ODD ( "hb01.bin",     0x00000, 0x10000, 0x8b41c219 )
	ROM_LOAD_EVEN( "hb05.bin",     0x20000, 0x10000, 0x2087d570 )
	ROM_LOAD_ODD ( "hb02.bin",     0x20000, 0x10000, 0x815536ae )
	ROM_LOAD_EVEN( "hb06.bin",     0x40000, 0x10000, 0xda4e3fbc )
	ROM_LOAD_ODD ( "hb03.bin",     0x40000, 0x10000, 0x7fed7c46 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "hb25.bin",     0x000000, 0x10000, 0x8649762c )	/* chars */
	ROM_LOAD( "hb26.bin",     0x010000, 0x10000, 0xf8189bbd )
	ROM_LOAD( "hb18.bin",     0x020000, 0x10000, 0xef664373 )	/* tiles */
	ROM_LOAD( "hb17.bin",     0x030000, 0x10000, 0xa4f186ac )
	ROM_LOAD( "hb20.bin",     0x040000, 0x10000, 0x2fc13be0 )
	ROM_LOAD( "hb19.bin",     0x050000, 0x10000, 0xd6b47869 )
	ROM_LOAD( "hb22.bin",     0x060000, 0x10000, 0x50d6a1ad )
	ROM_LOAD( "hb21.bin",     0x070000, 0x10000, 0xf01d75c5 )
	ROM_LOAD( "hb24.bin",     0x080000, 0x10000, 0xae377361 )
	ROM_LOAD( "hb23.bin",     0x090000, 0x10000, 0xbbdaf771 )
	ROM_LOAD( "hb29.bin",     0x0a0000, 0x10000, 0x5514b296 )	/* tiles */
	/* b0000-bfff empty */
	ROM_LOAD( "hb30.bin",     0x0c0000, 0x10000, 0x5855e8ef )
	/* d0000-dfff empty */
	ROM_LOAD( "hb27.bin",     0x0e0000, 0x10000, 0x99db7b9c )
	/* f0000-ffff empty */
	ROM_LOAD( "hb28.bin",     0x100000, 0x10000, 0x33ce2b1a )
	/* 110000-11fff empty */
	ROM_LOAD( "hb15.bin",     0x120000, 0x10000, 0x21816707 )	/* sprites */
	ROM_LOAD( "hb16.bin",     0x130000, 0x10000, 0xa5684574 )
	ROM_LOAD( "hb11.bin",     0x140000, 0x10000, 0x5c768315 )
	ROM_LOAD( "hb12.bin",     0x150000, 0x10000, 0x8b64d7a4 )
	ROM_LOAD( "hb13.bin",     0x160000, 0x10000, 0x56e3ed65 )
	ROM_LOAD( "hb14.bin",     0x170000, 0x10000, 0xbedfe7f3 )
	ROM_LOAD( "hb09.bin",     0x180000, 0x10000, 0x26240ea0 )
	ROM_LOAD( "hb10.bin",     0x190000, 0x10000, 0x47d95447 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "hb07.bin",     0x8000, 0x8000, 0xa127f0f7 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "hb08.bin",     0x0000, 0x10000, 0x645c5b68 )
ROM_END

ROM_START( baddudes )
	ROM_REGIONX( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code, middle 0x20000 unused */
	ROM_LOAD_EVEN( "baddudes.4",   0x00000, 0x10000, 0x4bf158a7 )
	ROM_LOAD_ODD ( "baddudes.1",   0x00000, 0x10000, 0x74f5110c )
	ROM_LOAD_EVEN( "baddudes.6",   0x40000, 0x10000, 0x3ff8da57 )
	ROM_LOAD_ODD ( "baddudes.3",   0x40000, 0x10000, 0xf8f2bd94 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "baddudes.25",  0x000000, 0x04000, 0xbcf59a69 )	/* chars */
	/* 04000-07fff empty */
	ROM_CONTINUE(            0x008000, 0x04000 )
	/* 0c000-0ffff empty */
	ROM_LOAD( "baddudes.26",  0x010000, 0x04000, 0x9aff67b8 )
	/* 14000-17fff empty */
	ROM_CONTINUE(            0x018000, 0x04000 )
	/* 1c000-1ffff empty */
	ROM_LOAD( "baddudes.18",  0x020000, 0x10000, 0x05cfc3e5 )	/* tiles */
	/* 30000-3ffff empty */
	ROM_LOAD( "baddudes.20",  0x040000, 0x10000, 0xe11e988f )
	/* 50000-5ffff empty */
	ROM_LOAD( "baddudes.22",  0x060000, 0x10000, 0xb893d880 )
	/* 70000-7ffff empty */
	ROM_LOAD( "baddudes.24",  0x080000, 0x10000, 0x6f226dda )
	/* 90000-9ffff empty */
	ROM_LOAD( "baddudes.30",  0x0c0000, 0x08000, 0x982da0d1 )	/* tiles */
	/* c8000-dffff empty */
	ROM_CONTINUE(            0x0a0000, 0x08000 )	/* the two halves are swapped */
	/* a8000-bffff empty */
	ROM_LOAD( "baddudes.28",  0x100000, 0x08000, 0xf01ebb3b )
	/* 108000-11ffff empty */
	ROM_CONTINUE(            0x0e0000, 0x08000 )
	/* e8000-fffff empty */
	ROM_LOAD( "baddudes.15",  0x120000, 0x10000, 0xa38a7d30 )	/* sprites */
	ROM_LOAD( "baddudes.16",  0x130000, 0x08000, 0x17e42633 )
	/* 138000-13ffff empty */
	ROM_LOAD( "baddudes.11",  0x140000, 0x10000, 0x3a77326c )
	ROM_LOAD( "baddudes.12",  0x150000, 0x08000, 0xfea2a134 )
	/* 158000-15ffff empty */
	ROM_LOAD( "baddudes.13",  0x160000, 0x10000, 0xe5ae2751 )
	ROM_LOAD( "baddudes.14",  0x170000, 0x08000, 0xe83c760a )
	/* 178000-17ffff empty */
	ROM_LOAD( "baddudes.9",   0x180000, 0x10000, 0x6901e628 )
	ROM_LOAD( "baddudes.10",  0x190000, 0x08000, 0xeeee8a1a )
	/* 198000-19ffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "baddudes.7",   0x8000, 0x8000, 0x9fb1ef4b )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "baddudes.8",   0x0000, 0x10000, 0x3c87463e )
ROM_END

ROM_START( drgninja )
	ROM_REGIONX( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code, middle 0x20000 unused */
	ROM_LOAD_EVEN( "drgninja.04",  0x00000, 0x10000, 0x41b8b3f8 )
	ROM_LOAD_ODD ( "drgninja.01",  0x00000, 0x10000, 0xe08e6885 )
	ROM_LOAD_EVEN( "drgninja.06",  0x40000, 0x10000, 0x2b81faf7 )
	ROM_LOAD_ODD ( "drgninja.03",  0x40000, 0x10000, 0xc52c2e9d )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "drgninja.25",  0x000000, 0x04000, 0x6791bc20 )	/* chars */
	/* 04000-07fff empty */
	ROM_CONTINUE(            0x008000, 0x04000 )
	/* 0c000-0ffff empty */
	ROM_LOAD( "drgninja.26",  0x010000, 0x04000, 0x5d75fc8f )
	/* 14000-17fff empty */
	ROM_CONTINUE(            0x018000, 0x04000 )
	/* 1c000-1ffff empty */
	ROM_LOAD( "baddudes.18",  0x020000, 0x10000, 0x05cfc3e5 )	/* tiles */
	/* 30000-3ffff empty */
	ROM_LOAD( "baddudes.20",  0x040000, 0x10000, 0xe11e988f )
	/* 50000-5ffff empty */
	ROM_LOAD( "baddudes.22",  0x060000, 0x10000, 0xb893d880 )
	/* 70000-7ffff empty */
	ROM_LOAD( "baddudes.24",  0x080000, 0x10000, 0x6f226dda )
	/* 90000-9ffff empty */
	ROM_LOAD( "drgninja.30",  0x0c0000, 0x08000, 0x2438e67e )	/* tiles */
	/* c8000-dffff empty */
	ROM_CONTINUE(            0x0a0000, 0x08000 )	/* the two halves are swapped */
	/* a8000-bffff empty */
	ROM_LOAD( "drgninja.28",  0x100000, 0x08000, 0x5c692ab3 )
	/* 108000-11ffff empty */
	ROM_CONTINUE(            0x0e0000, 0x08000 )
	/* e8000-fffff empty */
	ROM_LOAD( "drgninja.15",  0x120000, 0x10000, 0x5617d67f )	/* sprites */
	ROM_LOAD( "baddudes.16",  0x130000, 0x08000, 0x17e42633 )
	/* 138000-13ffff empty */
	ROM_LOAD( "drgninja.11",  0x140000, 0x10000, 0xba83e8d8 )
	ROM_LOAD( "baddudes.12",  0x150000, 0x08000, 0xfea2a134 )
	/* 158000-15ffff empty */
	ROM_LOAD( "drgninja.13",  0x160000, 0x10000, 0xfd91e08e )
	ROM_LOAD( "baddudes.14",  0x170000, 0x08000, 0xe83c760a )
	/* 178000-17ffff empty */
	ROM_LOAD( "baddudes.9",   0x180000, 0x10000, 0x6901e628 )
	ROM_LOAD( "baddudes.10",  0x190000, 0x08000, 0xeeee8a1a )
	/* 198000-19ffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "drgninja.07",  0x8000, 0x8000, 0x001d2f51 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "baddudes.8",   0x0000, 0x10000, 0x3c87463e )
ROM_END

ROM_START( birdtry )
	ROM_REGIONX( 0x60000, REGION_CPU1 )	/* 6*64k for 68000 code */
	ROM_LOAD_EVEN( "ek-04.bin",     0x00000, 0x10000, 0x5f0f4686 )
	ROM_LOAD_ODD ( "ek-01.bin",     0x00000, 0x10000, 0x47f470db )
	ROM_LOAD_EVEN( "ek-05.bin",     0x20000, 0x10000, 0xb508cffd )
	ROM_LOAD_ODD ( "ek-02.bin",     0x20000, 0x10000, 0x0195d989 )
	ROM_LOAD_EVEN( "ek-06.bin",     0x40000, 0x10000, 0x301d57d8 )
	ROM_LOAD_ODD ( "ek-03.bin",     0x40000, 0x10000, 0x73b0acc5 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ek-25.bin",     0x000000, 0x08000, 0x4df134ad )	/* chars */
	ROM_LOAD( "ek-26.bin",     0x010000, 0x08000, 0xa00d3e8e )

	ROM_LOAD( "ek-18.bin",     0x020000, 0x10000, 0x9886fb70 )	/* tiles */
	ROM_LOAD( "ek-17.bin",     0x030000, 0x10000, 0xbed91bf7 )
	ROM_LOAD( "ek-20.bin",     0x040000, 0x10000, 0x45d53965 )
	ROM_LOAD( "ek-19.bin",     0x050000, 0x10000, 0xc2949dd2 )
	ROM_LOAD( "ek-22.bin",     0x060000, 0x10000, 0x7f2cc80a )
	ROM_LOAD( "ek-21.bin",     0x070000, 0x10000, 0x281bc793 )
	ROM_LOAD( "ek-24.bin",     0x080000, 0x10000, 0x2244cc75 )
	ROM_LOAD( "ek-23.bin",     0x090000, 0x10000, 0xd0ed0116 )

	/* This game doesn't seem to have the extra playfield chip, so no roms */

	ROM_LOAD( "ek-15.bin",     0x120000, 0x10000, 0xa6a041a3 )	/* sprites */
	ROM_LOAD( "ek-16.bin",     0x130000, 0x08000, 0x784f62b0 )
	ROM_LOAD( "ek-11.bin",     0x140000, 0x10000, 0x9224a6b9 )
	ROM_LOAD( "ek-12.bin",     0x150000, 0x08000, 0x12deecfa )
	ROM_LOAD( "ek-13.bin",     0x160000, 0x10000, 0x1f023459 )
	ROM_LOAD( "ek-14.bin",     0x170000, 0x08000, 0x57d54943 )
	ROM_LOAD( "ek-09.bin",     0x180000, 0x10000, 0x6d2d488a )
	ROM_LOAD( "ek-10.bin",     0x190000, 0x08000, 0x580ba206 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "ek-07.bin",     0x8000, 0x8000, 0x236549bc )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "ek-08.bin",     0x0000, 0x10000, 0xbe3db6cb )
ROM_END

ROM_START( robocop )
	ROM_REGIONX( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ep05-3", 0x00000, 0x10000, 0xba69bf84 )
	ROM_LOAD_ODD ( "ep01-3", 0x00000, 0x10000, 0x2a9f6e2c )
	ROM_LOAD_EVEN( "ep04-3", 0x20000, 0x10000, 0x39181778 )
	ROM_LOAD_ODD ( "ep00-3", 0x20000, 0x10000, 0xe128541f )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ep23", 0x000000, 0x10000, 0xa77e4ab1 )	/* chars */
	ROM_LOAD( "ep22", 0x010000, 0x10000, 0x9fbd6903 )
	ROM_LOAD( "ep20", 0x020000, 0x10000, 0x1d8d38b8 )	/* tiles */
	ROM_LOAD( "ep21", 0x040000, 0x10000, 0x187929b2 )
	ROM_LOAD( "ep18", 0x060000, 0x10000, 0xb6580b5e )
	ROM_LOAD( "ep19", 0x080000, 0x10000, 0x9bad01c7 )
	ROM_LOAD( "ep14", 0x0a0000, 0x08000, 0xca56ceda )	/* tiles */
	ROM_LOAD( "ep15", 0x0c0000, 0x08000, 0xa945269c )
	ROM_LOAD( "ep16", 0x0e0000, 0x08000, 0xe7fa4d58 )
	ROM_LOAD( "ep17", 0x100000, 0x08000, 0x84aae89d )
	ROM_LOAD( "ep07", 0x120000, 0x10000, 0x495d75cf )	/* sprites */
	ROM_LOAD( "ep06", 0x130000, 0x08000, 0xa2ae32e2 )
	/* 98000-9ffff empty */
	ROM_LOAD( "ep11", 0x140000, 0x10000, 0x62fa425a )
	ROM_LOAD( "ep10", 0x150000, 0x08000, 0xcce3bd95 )
	/* b8000-bffff empty */
	ROM_LOAD( "ep09", 0x160000, 0x10000, 0x11bed656 )
	ROM_LOAD( "ep08", 0x170000, 0x08000, 0xc45c7b4c )
	/* d8000-dffff empty */
	ROM_LOAD( "ep13", 0x180000, 0x10000, 0x8fca9f28 )
	ROM_LOAD( "ep12", 0x190000, 0x08000, 0x3cd1d0c3 )
	/* f8000-fffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "ep03-3", 0x08000, 0x08000, 0x5b164b24 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "ep02", 0x00000, 0x10000, 0x711ce46f )

	ROM_REGIONX( 0x10000, REGION_CPU3 )	/* HuC6280 CPU */
	/* Filled in later */
ROM_END

ROM_START( robocopu )
	ROM_REGIONX( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ep05-1", 0x00000, 0x10000, 0x8de5cb3d )
	ROM_LOAD_ODD ( "ep01-1", 0x00000, 0x10000, 0xb3c6bc02 )
	ROM_LOAD_EVEN( "ep04", 0x20000, 0x10000, 0xc38b9d18 )
	ROM_LOAD_ODD ( "ep00", 0x20000, 0x10000, 0x374c91aa )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ep23", 0x000000, 0x10000, 0xa77e4ab1 )	/* chars */
	ROM_LOAD( "ep22", 0x010000, 0x10000, 0x9fbd6903 )
	ROM_LOAD( "ep20", 0x020000, 0x10000, 0x1d8d38b8 )	/* tiles */
	ROM_LOAD( "ep21", 0x040000, 0x10000, 0x187929b2 )
	ROM_LOAD( "ep18", 0x060000, 0x10000, 0xb6580b5e )
	ROM_LOAD( "ep19", 0x080000, 0x10000, 0x9bad01c7 )
	ROM_LOAD( "ep14", 0x0a0000, 0x08000, 0xca56ceda )	/* tiles */
	ROM_LOAD( "ep15", 0x0c0000, 0x08000, 0xa945269c )
	ROM_LOAD( "ep16", 0x0e0000, 0x08000, 0xe7fa4d58 )
	ROM_LOAD( "ep17", 0x100000, 0x08000, 0x84aae89d )
	ROM_LOAD( "ep07", 0x120000, 0x10000, 0x495d75cf )	/* sprites */
	ROM_LOAD( "ep06", 0x130000, 0x08000, 0xa2ae32e2 )
	/* 98000-9ffff empty */
	ROM_LOAD( "ep11", 0x140000, 0x10000, 0x62fa425a )
	ROM_LOAD( "ep10", 0x150000, 0x08000, 0xcce3bd95 )
	/* b8000-bffff empty */
	ROM_LOAD( "ep09", 0x160000, 0x10000, 0x11bed656 )
	ROM_LOAD( "ep08", 0x170000, 0x08000, 0xc45c7b4c )
	/* d8000-dffff empty */
	ROM_LOAD( "ep13", 0x180000, 0x10000, 0x8fca9f28 )
	ROM_LOAD( "ep12", 0x190000, 0x08000, 0x3cd1d0c3 )
	/* f8000-fffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "ep03", 0x08000, 0x08000, 0x1089eab8 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "ep02", 0x00000, 0x10000, 0x711ce46f )

	ROM_REGIONX( 0x10000, REGION_CPU3 )	/* HuC6280 CPU */
	/* Filled in later */
ROM_END

ROM_START( robocpu0 )
	ROM_REGIONX( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "ep05", 0x00000, 0x10000, 0xc465bdd8 )
	ROM_LOAD_ODD ( "ep01", 0x00000, 0x10000, 0x1352d36e )
	ROM_LOAD_EVEN( "ep04", 0x20000, 0x10000, 0xc38b9d18 )
	ROM_LOAD_ODD ( "ep00", 0x20000, 0x10000, 0x374c91aa )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ep23", 0x000000, 0x10000, 0xa77e4ab1 )	/* chars */
	ROM_LOAD( "ep22", 0x010000, 0x10000, 0x9fbd6903 )
	ROM_LOAD( "ep20", 0x020000, 0x10000, 0x1d8d38b8 )	/* tiles */
	ROM_LOAD( "ep21", 0x040000, 0x10000, 0x187929b2 )
	ROM_LOAD( "ep18", 0x060000, 0x10000, 0xb6580b5e )
	ROM_LOAD( "ep19", 0x080000, 0x10000, 0x9bad01c7 )
	ROM_LOAD( "ep14", 0x0a0000, 0x08000, 0xca56ceda )	/* tiles */
	ROM_LOAD( "ep15", 0x0c0000, 0x08000, 0xa945269c )
	ROM_LOAD( "ep16", 0x0e0000, 0x08000, 0xe7fa4d58 )
	ROM_LOAD( "ep17", 0x100000, 0x08000, 0x84aae89d )
	ROM_LOAD( "ep07", 0x120000, 0x10000, 0x495d75cf )	/* sprites */
	ROM_LOAD( "ep06", 0x130000, 0x08000, 0xa2ae32e2 )
	/* 98000-9ffff empty */
	ROM_LOAD( "ep11", 0x140000, 0x10000, 0x62fa425a )
	ROM_LOAD( "ep10", 0x150000, 0x08000, 0xcce3bd95 )
	/* b8000-bffff empty */
	ROM_LOAD( "ep09", 0x160000, 0x10000, 0x11bed656 )
	ROM_LOAD( "ep08", 0x170000, 0x08000, 0xc45c7b4c )
	/* d8000-dffff empty */
	ROM_LOAD( "ep13", 0x180000, 0x10000, 0x8fca9f28 )
	ROM_LOAD( "ep12", 0x190000, 0x08000, 0x3cd1d0c3 )
	/* f8000-fffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "ep03", 0x08000, 0x08000, 0x1089eab8 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "ep02", 0x00000, 0x10000, 0x711ce46f )

	ROM_REGIONX( 0x10000, REGION_CPU3 )	/* HuC6280 CPU */
	/* Filled in later */
ROM_END

ROM_START( robocopb )
	ROM_REGIONX( 0x40000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "robop_05.rom", 0x00000, 0x10000, 0xbcef3e9b )
	ROM_LOAD_ODD ( "robop_01.rom", 0x00000, 0x10000, 0xc9803685 )
	ROM_LOAD_EVEN( "robop_04.rom", 0x20000, 0x10000, 0x9d7b79e0 )
	ROM_LOAD_ODD ( "robop_00.rom", 0x20000, 0x10000, 0x80ba64ab )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ep23", 0x000000, 0x10000, 0xa77e4ab1 )	/* chars */
	ROM_LOAD( "ep22", 0x010000, 0x10000, 0x9fbd6903 )
	ROM_LOAD( "ep20", 0x020000, 0x10000, 0x1d8d38b8 )	/* tiles */
	ROM_LOAD( "ep21", 0x040000, 0x10000, 0x187929b2 )
	ROM_LOAD( "ep18", 0x060000, 0x10000, 0xb6580b5e )
	ROM_LOAD( "ep19", 0x080000, 0x10000, 0x9bad01c7 )
	ROM_LOAD( "ep14", 0x0a0000, 0x08000, 0xca56ceda )	/* tiles */
	ROM_LOAD( "ep15", 0x0c0000, 0x08000, 0xa945269c )
	ROM_LOAD( "ep16", 0x0e0000, 0x08000, 0xe7fa4d58 )
	ROM_LOAD( "ep17", 0x100000, 0x08000, 0x84aae89d )
	ROM_LOAD( "ep07", 0x120000, 0x10000, 0x495d75cf )	/* sprites */
	ROM_LOAD( "ep06", 0x130000, 0x08000, 0xa2ae32e2 )
	/* 98000-9ffff empty */
	ROM_LOAD( "ep11", 0x140000, 0x10000, 0x62fa425a )
	ROM_LOAD( "ep10", 0x150000, 0x08000, 0xcce3bd95 )
	/* b8000-bffff empty */
	ROM_LOAD( "ep09", 0x160000, 0x10000, 0x11bed656 )
	ROM_LOAD( "ep08", 0x170000, 0x08000, 0xc45c7b4c )
	/* d8000-dffff empty */
	ROM_LOAD( "ep13", 0x180000, 0x10000, 0x8fca9f28 )
	ROM_LOAD( "ep12", 0x190000, 0x08000, 0x3cd1d0c3 )
	/* f8000-fffff empty */

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 Sound */
	ROM_LOAD( "ep03-3", 0x08000, 0x08000, 0x5b164b24 )

	ROM_REGION(0x10000)	/* ADPCM samples */
	ROM_LOAD( "ep02", 0x00000, 0x10000, 0x711ce46f )
ROM_END

ROM_START( hippodrm )
	ROM_REGIONX( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "ew02",         0x00000, 0x10000, 0xdf0d7dc6 )
	ROM_LOAD_ODD ( "ew01",         0x00000, 0x10000, 0xd5670aa7 )
	ROM_LOAD_EVEN( "ew05",         0x20000, 0x10000, 0xc76d65ec )
	ROM_LOAD_ODD ( "ew00",         0x20000, 0x10000, 0xe9b427a6 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ew14",         0x000000, 0x10000, 0x71ca593d )	/* chars */
	ROM_LOAD( "ew13",         0x010000, 0x10000, 0x86be5fa7 )
	ROM_LOAD( "ew19",         0x020000, 0x08000, 0x6b80d7a3 )	/* tiles */
	ROM_LOAD( "ew18",         0x040000, 0x08000, 0x78d3d764 )
	ROM_LOAD( "ew20",         0x060000, 0x08000, 0xce9f5de3 )
	ROM_LOAD( "ew21",         0x080000, 0x08000, 0x487a7ba2 )
	ROM_LOAD( "ew24",         0x0a0000, 0x08000, 0x4e1bc2a4 )	/* tiles */
	ROM_LOAD( "ew25",         0x0c0000, 0x08000, 0x9eb47dfb )
	ROM_LOAD( "ew23",         0x0e0000, 0x08000, 0x9ecf479e )
	ROM_LOAD( "ew22",         0x100000, 0x08000, 0xe55669aa )
	ROM_LOAD( "ew15",         0x120000, 0x10000, 0x95423914 )	/* sprites */
	ROM_LOAD( "ew16",         0x130000, 0x10000, 0x96233177 )
	ROM_LOAD( "ew10",         0x140000, 0x10000, 0x4c25dfe8 )
	ROM_LOAD( "ew11",         0x150000, 0x10000, 0xf2e007fc )
	ROM_LOAD( "ew06",         0x160000, 0x10000, 0xe4bb8199 )
	ROM_LOAD( "ew07",         0x170000, 0x10000, 0x470b6989 )
	ROM_LOAD( "ew17",         0x180000, 0x10000, 0x8c97c757 )
	ROM_LOAD( "ew12",         0x190000, 0x10000, 0xa2d244bc )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 sound */
	ROM_LOAD( "ew04",         0x8000, 0x8000, 0x9871b98d )

	ROM_REGION(0x10000)	/* ADPCM sounds */
	ROM_LOAD( "ew03",         0x0000, 0x10000, 0xb606924d )

	ROM_REGIONX( 0x10000, REGION_CPU3 ) /* Encrypted code bank */
	ROM_LOAD( "ew08",         0x00000, 0x10000, 0x53010534 )
ROM_END

ROM_START( ffantasy )
	ROM_REGIONX( 0x40000, REGION_CPU1 )	/* 4*64k for 68000 code */
	ROM_LOAD_EVEN( "ev02",         0x00000, 0x10000, 0x797a7860 )
	ROM_LOAD_ODD ( "ev01",         0x00000, 0x10000, 0x0f17184d )
	ROM_LOAD_EVEN( "ew05",         0x20000, 0x10000, 0xc76d65ec )
	ROM_LOAD_ODD ( "ew00",         0x20000, 0x10000, 0xe9b427a6 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "ev14",         0x000000, 0x10000, 0x686f72c1 )	/* chars */
	ROM_LOAD( "ev13",         0x010000, 0x10000, 0xb787dcc9 )
	ROM_LOAD( "ew19",         0x020000, 0x08000, 0x6b80d7a3 )	/* tiles */
	ROM_LOAD( "ew18",         0x040000, 0x08000, 0x78d3d764 )
	ROM_LOAD( "ew20",         0x060000, 0x08000, 0xce9f5de3 )
	ROM_LOAD( "ew21",         0x080000, 0x08000, 0x487a7ba2 )
	ROM_LOAD( "ew24",         0x0a0000, 0x08000, 0x4e1bc2a4 )	/* tiles */
	ROM_LOAD( "ew25",         0x0c0000, 0x08000, 0x9eb47dfb )
	ROM_LOAD( "ew23",         0x0e0000, 0x08000, 0x9ecf479e )
	ROM_LOAD( "ew22",         0x100000, 0x08000, 0xe55669aa )
	ROM_LOAD( "ev15",         0x120000, 0x10000, 0x1d80f797 )	/* sprites */
	ROM_LOAD( "ew16",         0x130000, 0x10000, 0x96233177 )
	ROM_LOAD( "ev10",         0x140000, 0x10000, 0xc4e7116b )
	ROM_LOAD( "ew11",         0x150000, 0x10000, 0xf2e007fc )
	ROM_LOAD( "ev06",         0x160000, 0x10000, 0x6c794f1a )
	ROM_LOAD( "ew07",         0x170000, 0x10000, 0x470b6989 )
	ROM_LOAD( "ev17",         0x180000, 0x10000, 0x045509d4 )
	ROM_LOAD( "ew12",         0x190000, 0x10000, 0xa2d244bc )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* 6502 sound */
	ROM_LOAD( "ew04",         0x8000, 0x8000, 0x9871b98d )

	ROM_REGION(0x10000)	/* ADPCM sounds */
	ROM_LOAD( "ew03",         0x0000, 0x10000, 0xb606924d )

	ROM_REGIONX( 0x10000, REGION_CPU3 ) /* Encrypted code bank */
	ROM_LOAD( "ew08",         0x00000, 0x10000, 0x53010534 )
ROM_END

ROM_START( slyspy )
	ROM_REGIONX( 0x60000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fa14-3.17l",   0x00000, 0x10000, 0x54353a84 )
	ROM_LOAD_ODD ( "fa12-2.9l",    0x00000, 0x10000, 0x1b534294 )
	ROM_LOAD_EVEN( "fa15.19l",     0x20000, 0x10000, 0x04a79266 )
	ROM_LOAD_ODD ( "fa13.11l",     0x20000, 0x10000, 0x641cc4b3 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fa05.11a",     0x008000, 0x04000, 0x09802924 )	/* chars */
	ROM_CONTINUE(             0x000000, 0x04000 )	/* the two halves are swapped */
	ROM_LOAD( "fa04.9a",      0x018000, 0x04000, 0xec25b895 )
	ROM_CONTINUE(             0x010000, 0x04000 )
	ROM_LOAD( "fa07.17a",     0x020000, 0x08000, 0xe932268b )	/* tiles */
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "fa06.15a",     0x060000, 0x08000, 0xc4dd38c0 )
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "fa09.22a",     0x0a0000, 0x10000, 0x1395e9be )	/* tiles */
	ROM_CONTINUE(             0x0c0000, 0x10000 )
	ROM_LOAD( "fa08.21a",     0x0e0000, 0x10000, 0x4d7464db )
	ROM_CONTINUE(             0x100000, 0x10000 )

	ROM_LOAD( "fa01.4a",      0x120000, 0x20000, 0x99b0cd92 )	/* sprites */
	ROM_LOAD( "fa03.7a",      0x140000, 0x20000, 0x0e7ea74d )
	ROM_LOAD( "fa00.2a",      0x160000, 0x20000, 0xf7df3fd7 )
	ROM_LOAD( "fa02.5a",      0x180000, 0x20000, 0x84e8da9d )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fa10.5h",      0x00000, 0x10000, 0xdfd2ff25 )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fa11.11k",     0x00000, 0x20000, 0x4e547bad )
ROM_END

ROM_START( slyspy2 )
	ROM_REGIONX( 0x60000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fa14-2.bin",   0x00000, 0x10000, 0x0e431e39 )
	ROM_LOAD_ODD ( "fa12-2.9l",    0x00000, 0x10000, 0x1b534294 )
	ROM_LOAD_EVEN( "fa15.19l",     0x20000, 0x10000, 0x04a79266 )
	ROM_LOAD_ODD ( "fa13.11l",     0x20000, 0x10000, 0x641cc4b3 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fa05.11a",     0x008000, 0x04000, 0x09802924 )	/* chars */
	/* 0c000-0ffff empty */
	ROM_CONTINUE(             0x000000, 0x04000 )	/* the two halves are swapped */
	/* 04000-07fff empty */
	ROM_LOAD( "fa04.9a",      0x018000, 0x04000, 0xec25b895 )
	/* 1c000-1ffff empty */
	ROM_CONTINUE(             0x010000, 0x04000 )
	/* 14000-17fff empty */
	ROM_LOAD( "fa07.17a",     0x020000, 0x08000, 0xe932268b )	/* tiles */
	/* 28000-3ffff empty */
	ROM_CONTINUE(             0x040000, 0x08000 )
	/* 48000-5ffff empty */
	ROM_LOAD( "fa06.15a",     0x060000, 0x08000, 0xc4dd38c0 )
	/* 68000-7ffff empty */
	ROM_CONTINUE(             0x080000, 0x08000 )
	/* 88000-9ffff empty */
	ROM_LOAD( "fa09.22a",     0x0a0000, 0x10000, 0x1395e9be )	/* tiles */
	/* b0000-bffff empty */
	ROM_CONTINUE(             0x0c0000, 0x10000 )
	/* d0000-dffff empty */
	ROM_LOAD( "fa08.21a",     0x0e0000, 0x10000, 0x4d7464db )
	/* f0000-fffff empty */
	ROM_CONTINUE(             0x100000, 0x10000 )
	/* 110000-11ffff empty */
	ROM_LOAD( "fa01.4a",      0x120000, 0x20000, 0x99b0cd92 )	/* sprites */
	ROM_LOAD( "fa03.7a",      0x140000, 0x20000, 0x0e7ea74d )
	ROM_LOAD( "fa00.2a",      0x160000, 0x20000, 0xf7df3fd7 )
	ROM_LOAD( "fa02.5a",      0x180000, 0x20000, 0x84e8da9d )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fa10.5h",      0x00000, 0x10000, 0xdfd2ff25 )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fa11.11k",     0x00000, 0x20000, 0x4e547bad )
ROM_END

ROM_START( secretag )
	ROM_REGIONX( 0x60000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fb14.bin",   0x00000, 0x10000, 0x9be6ac90 )
	ROM_LOAD_ODD ( "fb12.bin",   0x00000, 0x10000, 0x28904b6b )
	ROM_LOAD_EVEN( "fb15.bin",   0x20000, 0x10000, 0x106bb26c )
	ROM_LOAD_ODD ( "fb13.bin",   0x20000, 0x10000, 0x90523413 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fa05.11a",     0x008000, 0x04000, 0x09802924 )	/* chars */
	/* 0c000-0ffff empty */
	ROM_CONTINUE(             0x000000, 0x04000 )	/* the two halves are swapped */
	/* 04000-07fff empty */
	ROM_LOAD( "fa04.9a",      0x018000, 0x04000, 0xec25b895 )
	/* 1c000-1ffff empty */
	ROM_CONTINUE(             0x010000, 0x04000 )
	/* 14000-17fff empty */
	ROM_LOAD( "fa07.17a",     0x020000, 0x08000, 0xe932268b )	/* tiles */
	/* 28000-3ffff empty */
	ROM_CONTINUE(             0x040000, 0x08000 )
	/* 48000-5ffff empty */
	ROM_LOAD( "fa06.15a",     0x060000, 0x08000, 0xc4dd38c0 )
	/* 68000-7ffff empty */
	ROM_CONTINUE(             0x080000, 0x08000 )
	/* 88000-9ffff empty */
	ROM_LOAD( "fa09.22a",     0x0a0000, 0x10000, 0x1395e9be )	/* tiles */
	/* b0000-bffff empty */
	ROM_CONTINUE(             0x0c0000, 0x10000 )
	/* d0000-dffff empty */
	ROM_LOAD( "fa08.21a",     0x0e0000, 0x10000, 0x4d7464db )
	/* f0000-fffff empty */
	ROM_CONTINUE(             0x100000, 0x10000 )
	/* 110000-11ffff empty */
	ROM_LOAD( "fa01.4a",      0x120000, 0x20000, 0x99b0cd92 )	/* sprites */
	ROM_LOAD( "fa03.7a",      0x140000, 0x20000, 0x0e7ea74d )
	ROM_LOAD( "fa00.2a",      0x160000, 0x20000, 0xf7df3fd7 )
	ROM_LOAD( "fa02.5a",      0x180000, 0x20000, 0x84e8da9d )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fa10.5h",      0x00000, 0x10000, 0xdfd2ff25 )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fa11.11k",     0x00000, 0x20000, 0x4e547bad )
ROM_END

#if 0
ROM_START( secretab )
	ROM_REGIONX( 0x60000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "sa_05.bin",    0x00000, 0x10000, 0x54353a84 )
	ROM_LOAD_ODD ( "sa_03.bin",    0x00000, 0x10000, 0x1b534294 )
	ROM_LOAD_EVEN( "sa_06.bin",    0x20000, 0x10000, 0x04a79266 )
	ROM_LOAD_ODD ( "sa_04.bin",    0x20000, 0x10000, 0x641cc4b3 )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fa05.11a",     0x008000, 0x04000, 0x09802924 )	/* chars */
	ROM_CONTINUE(             0x000000, 0x04000 )	/* the two halves are swapped */
	ROM_LOAD( "fa04.9a",      0x018000, 0x04000, 0xec25b895 )
	ROM_CONTINUE(             0x010000, 0x04000 )
	ROM_LOAD( "fa07.17a",     0x020000, 0x08000, 0xe932268b )	/* tiles */
	ROM_CONTINUE(             0x040000, 0x08000 )
	ROM_LOAD( "fa06.15a",     0x060000, 0x08000, 0xc4dd38c0 )
	ROM_CONTINUE(             0x080000, 0x08000 )
	ROM_LOAD( "fa09.22a",     0x0a0000, 0x10000, 0x1395e9be )	/* tiles */
	ROM_CONTINUE(             0x0c0000, 0x10000 )
	ROM_LOAD( "fa08.21a",     0x0e0000, 0x10000, 0x4d7464db )
	ROM_CONTINUE(             0x100000, 0x10000 )

	ROM_LOAD( "fa01.4a",      0x120000, 0x20000, 0x99b0cd92 )	/* sprites */
	ROM_LOAD( "fa03.7a",      0x140000, 0x20000, 0x0e7ea74d )
	ROM_LOAD( "fa00.2a",      0x160000, 0x20000, 0xf7df3fd7 )
	ROM_LOAD( "fa02.5a",      0x180000, 0x20000, 0x84e8da9d )

	ROM_REGION (0x10000)	/* Sound CPU */
	ROM_LOAD( "fa10.5h",      0x00000, 0x10000, 0xdfd2ff25 )

	ROM_REGION (0x20000)	/* ADPCM samples */
	ROM_LOAD( "fa11.11k",     0x00000, 0x20000, 0x4e547bad )
ROM_END
#endif

ROM_START( midres )
	ROM_REGIONX( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fk_14.rom",    0x00000, 0x20000, 0xde7522df )
	ROM_LOAD_ODD ( "fk_12.rom",    0x00000, 0x20000, 0x3494b8c9 )
	ROM_LOAD_EVEN( "fl15",         0x40000, 0x20000, 0x1328354e )
	ROM_LOAD_ODD ( "fl13",         0x40000, 0x20000, 0xe3b3955e )

	ROM_REGION_DISPOSE(0x1a0000) /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fk_05.rom",         0x008000, 0x08000, 0x3cdb7453 )	/* chars */
	ROM_CONTINUE(                  0x000000, 0x08000 )	/* the two halves are swapped */
	ROM_LOAD( "fk_04.rom",         0x018000, 0x08000, 0x325ba20c )
	ROM_CONTINUE(                  0x010000, 0x08000 )
	ROM_LOAD( "fl09",              0x020000, 0x20000, 0x907d5910 )	/* tiles */
	ROM_LOAD( "fl08",              0x040000, 0x20000, 0xa936c03c )
	ROM_LOAD( "fl07",              0x060000, 0x20000, 0x2068c45c )
	ROM_LOAD( "fl06",              0x080000, 0x20000, 0xb7241ab9 )
	ROM_LOAD( "fl11",              0x0a0000, 0x10000, 0xb86b73b4 )	/* tiles */
	/* 0d0000-0dffff empty */
	ROM_CONTINUE(                  0x0c0000, 0x10000 )
	/* 110000-11ffff empty */
	ROM_LOAD( "fl10",              0x0e0000, 0x10000, 0x92245b29 )
	/* 0b0000-0bffff empty */
	ROM_CONTINUE(                  0x100000, 0x10000 )
	/* 0f0000-0fffff empty */
	ROM_LOAD( "fl01",              0x120000, 0x20000, 0x2c8b35a7 )	/* sprites */
	ROM_LOAD( "fl03",              0x140000, 0x20000, 0x1eefed3c )
	ROM_LOAD( "fl00",              0x160000, 0x20000, 0x756fb801 )
	ROM_LOAD( "fl02",              0x180000, 0x20000, 0x54d2c120 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fl16",              0x00000, 0x10000, 0x66360bdf )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fl17",              0x00000, 0x20000, 0x9029965d )
ROM_END

ROM_START( midresu )
	ROM_REGIONX( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fl14",         0x00000, 0x20000, 0x2f9507a2 )
	ROM_LOAD_ODD ( "fl12",         0x00000, 0x20000, 0x3815ad9f )
	ROM_LOAD_EVEN( "fl15",         0x40000, 0x20000, 0x1328354e )
	ROM_LOAD_ODD ( "fl13",         0x40000, 0x20000, 0xe3b3955e )

	ROM_REGION_DISPOSE(0x1a0000) /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fl05",              0x008000, 0x08000, 0xd75aba06 )	/* chars */
	ROM_CONTINUE(                  0x000000, 0x08000 )	/* the two halves are swapped */
	ROM_LOAD( "fl04",              0x018000, 0x08000, 0x8f5bbb79 )
	ROM_CONTINUE(                  0x010000, 0x08000 )
	ROM_LOAD( "fl09",              0x020000, 0x20000, 0x907d5910 )	/* tiles */
	ROM_LOAD( "fl08",              0x040000, 0x20000, 0xa936c03c )
	ROM_LOAD( "fl07",              0x060000, 0x20000, 0x2068c45c )
	ROM_LOAD( "fl06",              0x080000, 0x20000, 0xb7241ab9 )
	ROM_LOAD( "fl11",              0x0a0000, 0x10000, 0xb86b73b4 )	/* tiles */
	/* 0d0000-0dffff empty */
	ROM_CONTINUE(                  0x0c0000, 0x10000 )
	/* 110000-11ffff empty */
	ROM_LOAD( "fl10",              0x0e0000, 0x10000, 0x92245b29 )
	/* 0b0000-0bffff empty */
	ROM_CONTINUE(                  0x100000, 0x10000 )
	/* 0f0000-0fffff empty */
	ROM_LOAD( "fl01",              0x120000, 0x20000, 0x2c8b35a7 )	/* sprites */
	ROM_LOAD( "fl03",              0x140000, 0x20000, 0x1eefed3c )
	ROM_LOAD( "fl00",              0x160000, 0x20000, 0x756fb801 )
	ROM_LOAD( "fl02",              0x180000, 0x20000, 0x54d2c120 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fl16",              0x00000, 0x10000, 0x66360bdf )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fl17",              0x00000, 0x20000, 0x9029965d )
ROM_END

ROM_START( midresj )
	ROM_REGIONX( 0x80000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fh14",         0x00000, 0x20000, 0x6d632a51 )
	ROM_LOAD_ODD ( "fh12",         0x00000, 0x20000, 0x45143384 )
	ROM_LOAD_EVEN( "fl15",         0x40000, 0x20000, 0x1328354e )
	ROM_LOAD_ODD ( "fl13",         0x40000, 0x20000, 0xe3b3955e )

	ROM_REGION_DISPOSE(0x1a0000) /* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fk_05.rom",         0x008000, 0x08000, 0x3cdb7453 )	/* chars */
	ROM_CONTINUE(                  0x000000, 0x08000 )	/* the two halves are swapped */
	ROM_LOAD( "fk_04.rom",         0x018000, 0x08000, 0x325ba20c )
	ROM_CONTINUE(                  0x010000, 0x08000 )
	ROM_LOAD( "fl09",              0x020000, 0x20000, 0x907d5910 )	/* tiles */
	ROM_LOAD( "fl08",              0x040000, 0x20000, 0xa936c03c )
	ROM_LOAD( "fl07",              0x060000, 0x20000, 0x2068c45c )
	ROM_LOAD( "fl06",              0x080000, 0x20000, 0xb7241ab9 )
	ROM_LOAD( "fl11",              0x0a0000, 0x10000, 0xb86b73b4 )	/* tiles */
	/* 0d0000-0dffff empty */
	ROM_CONTINUE(                  0x0c0000, 0x10000 )
	/* 110000-11ffff empty */
	ROM_LOAD( "fl10",              0x0e0000, 0x10000, 0x92245b29 )
	/* 0b0000-0bffff empty */
	ROM_CONTINUE(                  0x100000, 0x10000 )
	/* 0f0000-0fffff empty */
	ROM_LOAD( "fl01",              0x120000, 0x20000, 0x2c8b35a7 )	/* sprites */
	ROM_LOAD( "fl03",              0x140000, 0x20000, 0x1eefed3c )
	ROM_LOAD( "fl00",              0x160000, 0x20000, 0x756fb801 )
	ROM_LOAD( "fl02",              0x180000, 0x20000, 0x54d2c120 )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fh16",              0x00000, 0x10000, 0x00736f32 )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fh17",              0x00000, 0x20000, 0xc7b0a24e )
ROM_END

ROM_START( bouldash )
	ROM_REGIONX( 0x60000, REGION_CPU1 ) /* 68000 code */
	ROM_LOAD_EVEN( "fn-15",   0x00000, 0x10000, 0xca19a967 )
	ROM_LOAD_ODD ( "fn-12",   0x00000, 0x10000, 0x242bdc2a )
	ROM_LOAD_EVEN( "fn-16",   0x20000, 0x10000, 0xb7217265 )
	ROM_LOAD_ODD ( "fn-13",   0x20000, 0x10000, 0x19209ef4 )
	ROM_LOAD_EVEN( "fn-17",   0x40000, 0x10000, 0x78a632a1 )
	ROM_LOAD_ODD ( "fn-14",   0x40000, 0x10000, 0x69b6112d )

	ROM_REGION_DISPOSE(0x1a0000)	/* temporary space for graphics (disposed after conversion) */
	ROM_LOAD( "fn-04",      0x008000, 0x08000, 0x40f5a760 )	/* chars */
	ROM_CONTINUE(           0x000000, 0x08000 )	/* the two halves are swapped */
	ROM_LOAD( "fn-05",      0x018000, 0x08000, 0x824f2168 )
	ROM_CONTINUE(           0x010000, 0x08000 )
	ROM_LOAD( "fn-07",      0x020000, 0x08000, 0xeac6a3b3 )	/* tiles */
	ROM_CONTINUE(           0x040000, 0x08000 )
	ROM_LOAD( "fn-06",      0x060000, 0x08000, 0x3feee292 )
	ROM_CONTINUE(           0x080000, 0x08000 )
	ROM_LOAD( "fn-09",      0x0a0000, 0x10000, 0xc2b27bd2 )	/* tiles */
	ROM_CONTINUE(           0x0c0000, 0x10000 )
	ROM_LOAD( "fn-08",      0x0e0000, 0x10000, 0x5ac97178 )
	ROM_CONTINUE(           0x100000, 0x10000 )
	ROM_LOAD( "fn-01",      0x120000, 0x10000, 0x9333121b )	/* sprites */
	ROM_LOAD( "fn-03",      0x140000, 0x10000, 0x254ba60f )
	ROM_LOAD( "fn-00",      0x160000, 0x10000, 0xec18d098 )
	ROM_LOAD( "fn-02",      0x180000, 0x10000, 0x4f060cba )

	ROM_REGIONX( 0x10000, REGION_CPU2 )	/* Sound CPU */
	ROM_LOAD( "fn-10",      0x00000, 0x10000, 0xc74106e7 )

	ROM_REGION(0x20000)	/* ADPCM samples */
	ROM_LOAD( "fn-11",      0x00000, 0x10000, 0x990fd8d9 )
ROM_END

/******************************************************************************/

static void h6280_decrypt(int memory_area)
{
	int i;
	unsigned char *RAM = memory_region(memory_area);

	/* Read each byte, decrypt it */
	for (i=0x00000; i<0x10000; i++)
		RAM[i]=(RAM[i] & 0x7e) | ((RAM[i] & 0x1) << 7) | ((RAM[i] & 0x80) >> 7);
}

static void hippodrm_patch(void)
{
	unsigned char *RAM = memory_region(4);

	h6280_decrypt(4);

	/* The protection cpu has additional memory mapped protection! */
	RAM[0x189]=0x60; /* RTS prot area */
	RAM[0x1af]=0x60; /* RTS prot area */
	RAM[0x1db]=0x60; /* RTS prot area */
	RAM[0x21a]=0x60; /* RTS prot area */
}

static void slyspy_patch(void)
{
	unsigned char *RAM = memory_region(2);

	h6280_decrypt(2);

	/* Slyspy sound cpu has some protection */
	RAM[0xf2d]=0xea;
	RAM[0xf2e]=0xea;
}

/******************************************************************************/

static int robocopp_hiload(void)
{
	void *f;

	/* check if the hi score table has already been initialized */

	if (READ_WORD(&dec0_ram[0x0ed8]) == 0x4d55 && READ_WORD(&dec0_ram[0x0eda]) == 0x5250 &&
		READ_WORD(&dec0_ram[0x0f68]) == 0x504f && READ_WORD(&dec0_ram[0x0f6a]) == 0x4c49 )
	{
		if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
		{
			osd_fread_msbfirst(f,&dec0_ram[0x0ed8],16*10);

/* MISH :  The following lines DO NOT look endian friendly... */

			dec0_ram[0x3522]=dec0_ram[0x0ee0];
			dec0_ram[0x3523]=dec0_ram[0x0ee1];
			dec0_ram[0x3524]=dec0_ram[0x0ee2];
			dec0_ram[0x3525]=dec0_ram[0x0ee3];
			osd_fclose(f);
		}
		return 1;
	}
	else return 0;  /* we can't load the hi scores yet */
}

static void robocopp_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x0ed8],16*10);
                osd_fclose(f);
				dec0_ram[0x0ed8]=0;
        }
}

static int hbarrel_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x3e9c]) == 0x10 && READ_WORD(&dec0_ram[0x3ea0]) == 0x09 &&
			READ_WORD(&dec0_ram[0x3ef0]) == 0x55 && READ_WORD(&dec0_ram[0x3ef2]) == 0x4d26)
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x3e9c],4*10);
                        osd_fread_msbfirst(f,&dec0_ram[0x3ecc],4*10);
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void hbarrel_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x3e9c],4*10);
                osd_fwrite_msbfirst(f,&dec0_ram[0x3ecc],4*10);
                osd_fclose(f);
        }
}

static int hbarrelj_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x3e78]) == 0x10 && READ_WORD(&dec0_ram[0x3e7c]) == 0x09 &&
			READ_WORD(&dec0_ram[0x3ecc]) == 0x55 && READ_WORD(&dec0_ram[0x3ece]) == 0x4d26 )
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x3e78],4*10);
                        osd_fread_msbfirst(f,&dec0_ram[0x3ea8],4*10);
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void hbarrelj_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x3e78],4*10);
                osd_fwrite_msbfirst(f,&dec0_ram[0x3ea8],4*10);
                osd_fclose(f);
        }
}

static int baddudes_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x28fe]) == 0x4d49 && READ_WORD(&dec0_ram[0x2900]) == 0x4e00 &&
			READ_WORD(&dec0_ram[0x299E]) == 0x444d && READ_WORD(&dec0_ram[0x29a0]) == 0x5900 )
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x28fe],8*20);
                        dec0_ram[0x01d4]=dec0_ram[0x2903];
                        dec0_ram[0x01d7]=dec0_ram[0x2902];
                        dec0_ram[0x01d6]=dec0_ram[0x2905];
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}


static void baddudes_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x28fe],8*20);
                osd_fclose(f);
        }
}

static int drgninja_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x28f8]) == 0x4d49 && READ_WORD(&dec0_ram[0x28fa]) == 0x4e00 &&
			READ_WORD(&dec0_ram[0x2998]) == 0x444d && READ_WORD(&dec0_ram[0x299a]) == 0x5900)
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x28f8],8*20);
                        dec0_ram[0x01d4]=dec0_ram[0x28fd];
                        dec0_ram[0x01d7]=dec0_ram[0x28fc];
                        dec0_ram[0x01d6]=dec0_ram[0x28ff];
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void drgninja_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x28f8],8*20);
                osd_fclose(f);
        }
}

static int hippodrm_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x3e00]) == 0x0800 && READ_WORD(&dec0_ram[0x3e04]) == 0x0700  &&
			READ_WORD(&dec0_ram[0x3e4c]) == 0x4352 && READ_WORD(&dec0_ram[0x3e4e]) == 0x5801  )
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x3e00],80);
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void hippodrm_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x3e00],80);
                osd_fclose(f);
        }
}

static int slyspy_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x0000]) == 0x30 && READ_WORD(&dec0_ram[0x0004]) == 0x28 &&
			READ_WORD(&dec0_ram[0x0098]) == 0x3030 && READ_WORD(&dec0_ram[0x009c]) == 0x3030)
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0],160);
                        dec0_ram[0x2adc]=dec0_ram[0];
                        dec0_ram[0x2add]=dec0_ram[1];
                        dec0_ram[0x2ade]=dec0_ram[2];
                        dec0_ram[0x2adf]=dec0_ram[3];
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void slyspy_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0],160);
                osd_fclose(f);
				dec0_ram[0x0098] = 0;
        }
}

static int midres_hiload(void)
{
        void *f;

        /* check if the hi score table has already been initialized */

        if (READ_WORD(&dec0_ram[0x26ea]) == 0x10 && READ_WORD(&dec0_ram[0x26ee]) == 0x09 &&
			READ_WORD(&dec0_ram[0x2736]) == 0x55 && READ_WORD(&dec0_ram[0x2738]) == 0x4d26)
        {
                if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,0)) != 0)
                {
                        osd_fread_msbfirst(f,&dec0_ram[0x26ea],80);
                        osd_fclose(f);
                }
                return 1;
        }
        else return 0;  /* we can't load the hi scores yet */
}

static void midres_hisave(void)
{
        void *f;

        if ((f = osd_fopen(Machine->gamedrv->name,0,OSD_FILETYPE_HIGHSCORE,1)) != 0)
        {
                osd_fwrite_msbfirst(f,&dec0_ram[0x26ea],80);
                osd_fclose(f);
        }
}

/******************************************************************************/

struct GameDriver driver_hbarrel =
{
	__FILE__,
	0,
	"hbarrel",
	"Heavy Barrel (US)",
	"1987",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&hbarrel_machine_driver,
	dec0_custom_memory,

	rom_hbarrelu,
	0, 0,
	0,
	0,

	input_ports_hbarrel,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_ROTATE_270,
	hbarrel_hiload, hbarrel_hisave
};

struct GameDriver driver_hbarrelw =
{
	__FILE__,
	&driver_hbarrel,
	"hbarrelw",
	"Heavy Barrel (World)",
	"1987",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&hbarrel_machine_driver,
	dec0_custom_memory,

	rom_hbarrel,
	0, 0,
	0,
	0,

	input_ports_hbarrel,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_ROTATE_270,
	hbarrelj_hiload, hbarrelj_hisave
};

struct GameDriver driver_baddudes =
{
	__FILE__,
	0,
	"baddudes",
	"Bad Dudes vs. Dragonninja (US)",
	"1988",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&baddudes_machine_driver,
	dec0_custom_memory,

	rom_baddudes,
	0, 0,
	0,
	0,

	input_ports_baddudes,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	baddudes_hiload, baddudes_hisave
};

struct GameDriver driver_drgninja =
{
	__FILE__,
	&driver_baddudes,
	"drgninja",
	"Dragonninja (Japan)",
	"1988",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&baddudes_machine_driver,
	dec0_custom_memory,

	rom_drgninja,
	0, 0,
	0,
	0,

	input_ports_baddudes,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	drgninja_hiload, drgninja_hisave
};

struct GameDriver driver_birdtry =
{
	__FILE__,
	0,
	"birdtry",
	"Birdie Try (Japan)",
	"1988",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&birdtry_machine_driver,
	dec0_custom_memory,

	rom_birdtry,
	0, 0,
	0,
	0,

	input_ports_hbarrel, /* For now */

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_ROTATE_270 | GAME_NOT_WORKING,
	0, 0
};

struct GameDriver driver_robocop =
{
	__FILE__,
	0,
	"robocop",
	"Robocop (World revision 3)",
	"1988",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&robocop_machine_driver,
	dec0_custom_memory,

	rom_robocop,
	0, 0,
	0,
	0,

	input_ports_robocop,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT | GAME_NOT_WORKING,
	robocopp_hiload, robocopp_hisave
};

struct GameDriver driver_robocopu =
{
	__FILE__,
	&driver_robocop,
	"robocopu",
	"Robocop (US revision 1)",
	"1988",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&robocop_machine_driver,
	dec0_custom_memory,

	rom_robocopu,
	0, 0,
	0,
	0,

	input_ports_robocop,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT | GAME_NOT_WORKING,
	robocopp_hiload, robocopp_hisave
};

struct GameDriver driver_robocpu0 =
{
	__FILE__,
	&driver_robocop,
	"robocpu0",
	"Robocop (US revision 0)",
	"1988",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&robocop_machine_driver,
	dec0_custom_memory,

	rom_robocpu0,
	0, 0,
	0,
	0,

	input_ports_robocop,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT | GAME_NOT_WORKING,
	robocopp_hiload,robocopp_hisave
};

struct GameDriver driver_robocopb =
{
	__FILE__,
	&driver_robocop,
	"robocopb",
	"Robocop (World bootleg)",
	"1988",
	"bootleg",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&robocopb_machine_driver,
	dec0_custom_memory,

	rom_robocopb,
	0, 0,
	0,
	0,

	input_ports_robocop,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	robocopp_hiload, robocopp_hisave
};

struct GameDriver driver_hippodrm =
{
	__FILE__,
	0,
	"hippodrm",
	"Hippodrome (US)",
	"1989",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&hippodrm_machine_driver,
	dec0_custom_memory,

	rom_hippodrm,
	hippodrm_patch, 0,
	0,
	0,

	input_ports_hippodrm,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	hippodrm_hiload,hippodrm_hisave
};

struct GameDriver driver_ffantasy =
{
	__FILE__,
	&driver_hippodrm,
	"ffantasy",
	"Fighting Fantasy (Japan)",
	"1989",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&hippodrm_machine_driver,
	dec0_custom_memory,

	rom_ffantasy,
	hippodrm_patch, 0,
	0,
	0,

	input_ports_hippodrm,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	hippodrm_hiload,hippodrm_hisave
};

struct GameDriver driver_slyspy =
{
	__FILE__,
	0,
	"slyspy",
	"Sly Spy (US revision 3)",
	"1989",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&slyspy_machine_driver,
	dec0_custom_memory,

	rom_slyspy,
	slyspy_patch,0,
	0,
	0,

	input_ports_slyspy,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	slyspy_hiload,slyspy_hisave
};

struct GameDriver driver_slyspy2 =
{
	__FILE__,
	&driver_slyspy,
	"slyspy2",
	"Sly Spy (US revision 2)",
	"1989",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&slyspy_machine_driver,
	dec0_custom_memory,

	rom_slyspy2,
	slyspy_patch,0,
	0,
	0,

	input_ports_slyspy,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	slyspy_hiload,slyspy_hisave
};

struct GameDriver driver_secretag =
{
	__FILE__,
	&driver_slyspy,
	"secretag",
	"Secret Agent (World)",
	"1989",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&slyspy_machine_driver,
	dec0_custom_memory,

	rom_secretag,
	slyspy_patch,0,
	0,
	0,

	input_ports_slyspy,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	slyspy_hiload,slyspy_hisave
};

#if 0
struct GameDriver driver_secretab =
{
	__FILE__,
	&driver_slyspy,
	"secretab",
	"Sly Spy (revision 2)",
	"1989",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&slyspy_machine_driver,
	dec0_custom_memory,

	rom_secretab,
	0,0,
	0,
	0,

	input_ports_slyspy,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	slyspy_hiload,slyspy_hisave
};
#endif

struct GameDriver driver_midres =
{
	__FILE__,
	0,
	"midres",
	"Midnight Resistance (World)",
	"1989",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&midres_machine_driver,
	dec0_custom_memory,

	rom_midres,
	0, 0,
	0,
	0,

	input_ports_midres,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	midres_hiload, midres_hisave
};

struct GameDriver driver_midresu =
{
	__FILE__,
	&driver_midres,
	"midresu",
	"Midnight Resistance (US)",
	"1989",
	"Data East USA",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&midres_machine_driver,
	dec0_custom_memory,

	rom_midresu,
	0, 0,
	0,
	0,

	input_ports_midres,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	midres_hiload, midres_hisave
};

struct GameDriver driver_midresj =
{
	__FILE__,
	&driver_midres,
	"midresj",
	"Midnight Resistance (Japan)",
	"1989",
	"Data East Corporation",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&midres_machine_driver,
	dec0_custom_memory,

	rom_midresj,
	0, 0,
	0,
	0,

	input_ports_midres,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	midres_hiload, midres_hisave
};

struct GameDriver driver_bouldash =
{
	__FILE__,
	0,
	"bouldash",
	"Boulder Dash / Boulder Dash Part 2 (World)",
	"1990",
	"Data East Corporation (licensed from First Star)",
	"Bryan McPhail (MAME driver)\nNicola Salmoria (additional code)",
	0,
	&slyspy_machine_driver,
	dec0_custom_memory,

	rom_bouldash,
	slyspy_patch, 0,
	0,
	0,

	input_ports_bouldash,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0,0
};
