/*
  Experimental ti99/2 driver

  Will not work without a TMS9995 emulator (TMS9900 variant...).

TODO :
  * write TMS9995 emulator
  * find a TI99/2 ROM dump (some TI99/2 ARE in private hands)
  * implement ROM paging if necessary
  * test the driver !
  * clean up video
  * understand the "viden" pin
  * implement cassette
  * implement Hex-Bus

  Raphael Nabet (who really has too much time to waste), december 1999
*/

/*
  TI99/2 facts :

References :
* TI99/2 main logic board schematics, 83/03/28-30 (on ftp.whtech.com, or just as me)

general :
* prototypes in 1983
* uses a 10.7MHz TMS9995 CPU, with the following characteristics
  - 8-bit data bus
  - 256 bytes 16-bit RAM
  - only available int lines are INT4 (used by vdp), INT1*, and NMI* (both free for expansion)
  - on-chip decrementer (0xfffa-0xfffb)
  - Unlike tms9900, CRU address range is full 0x0000-0xFFFE (A0 is not used as address).
    But unlike tms9900, tms9995 does not have to support external opcodes.
* 24 or 32kb ROM (16kb plain (1kb of which used by vdp), 16kb split into 2 8kb pages)
* 4kb 8-bit RAM, 248 bytes 16-bit RAM
* custom vdp shares CPU RAM/ROM.  The display is quite alike to tms9928 graphics mode, except
  that colors are a static B&W, and no sprite is displayed. The config (particularily the
  table base addresses) cannot be changed.  Since TI located the pattern generator table in
  ROM, we cannot even redefine the char patterns ! VBL int triggers int4 on tms9995.
* CRU is handled by one single custom chip, so the schematics don't show many details :-( .
* I/O :
  - 48-key keyboard.  Same as TI99/4a, without alpha lock, and with an additional break key.
    Note that the hardware now makes the difference between the two shift keys.
  - cassette I/O (one unit)
  - ALC bus (must be another name for Hex-Bus)
* 60-pin expansion/cartidge port on the back

memory map :
* 0x0000-0x4000 : system ROM (0x1C00-0x2000 (?) : char ROM used by vdp)
* 0x4000-0x6000 : system ROM, mapped to either of two distinct 8kb pages according to the S0
  bit from the keyboard interface (!), which is used to select the first key row.
  [only on second-generation TI99/2s, first generation models had only 24kb of ROM]
* 0x6000-0xE000 : free for expansion
* 0xE000-0xF000 : 8-bit "system" RAM (0xEC00-0xEF00 used by vdp)
* 0xF000-0xF0FB : 16-bit processor RAM (on tms9995)
* 0xF0FC-0xFFF9 : free for expansion
* 0xFFFA-0xFFFB : tms9995 internal clock
* 0xFFFC-0xFFFF : 16-bit processor RAM (provides the NMI vector)

CRU map :
* 0x0000-0x1EFC : reserved
* 0x1EE0-0x1EFE : tms9995 flags
* 0x1F00-0x1FD8 : reserved
* 0x1FDA : tms9995 MID flag
* 0x1FDC-0x1FFF : reserved
* 0x2000-0xE000 : unaffected
* 0xE400-0xE40E : keyboard I/O (8 bits input, either 3 or 6 bit output)
* 0xE80C : cassette I/O
* 0xE80A : ALC BAV
* 0xE808 : ALC HSK
* 0xE800-0xE808 : ALC data (I/O)
* 0xE80E : video enable (probably input - seems to come from VDP, and is present on the
  expansion connector)
* 0xF000-0xFFFE : reserved
*/

/*
  TI99/8 preliminary infos (all I know :-( ) :

  name : Texas Instruments Computer TI99/8 (no "Home")

References :
* machine room <http://...>
* TI99/8 user manual

general :
* a few dozen units built in 1983, never commercialized
* same CPU as TI99/2
* 220kb of ROM, including monitor, GPL interpreter (personnal guess), TI-extended basic II,
  and a P-code interpreter with a few utilities.  The user could change the CPU speed to
  improve compatibility with TI99/4x modules.
* 64kb 8-bit RAM, 248 bytes 16-bit RAM, 16kb vdp RAM
* tms9928anc vdp : quite like tms9928a, but two additionnal "split-screen" modes
* I/O
  - 54-key keyboard, plus 2 optional joysticks
  - sound and speech (both ti99/4-like)
  - Hex-Bus
  - Cassette
* cartidge port on the front
* 50-pin(?) expansion port on the back (so, it was not even the same as TI99/2 ????)

memory map :
* 0x8000-unknown address (0x801f?) : custom RAM mapper chip.  4kb page size,
  total address space 16Mb(?).

*/

#include "driver.h"
#include "mess/vidhrdw/tms9928a.h"



static void ti99_2_init_machine(void)
{
}

static void ti99_2_stop_machine(void)
{
}

static int ti99_2_vblank_interrupt(void)
{
  TMS9928A_interrupt();

  /* We trigger a level-4 interrupt.  The PULSE_LINE is a mere guess.
  Also, I don't know which IRQ line numbering scheme will be used. */
  cpu_set_irq_line(0, 1/*4*/, PULSE_LINE);

  return ignore_interrupt();
}

/*
  TI99/2 vdp emulation.

  For now, we just trick the tms9928a core into emulating the ti99/2 vdp.
  A ugly hack, but it should work, and this is all I want for now.
*/

/*static unsigned char ti99_2_palette[] =
{
  0, 0, 0,
  255, 255, 255
};

static unsigned short ti99_2_colortable[] =
{
  0, 1,
  0, 2
};

static void ti99_2_init_palette(unsigned char *palette, unsigned short *colortable, const unsigned char *)
{
	memcpy(palette, & ti99_2_palette, sizeof(ti99_2_palette));
	memcpy(colortable, & ti99_2_colortable, sizeof(ti99_2_colortable));
}*/

static int ti99_2_vh_start(void)
{
  int i;

  if (TMS9928A_start(0x1000)) /* 4 kb of video RAM */
    return 1; /* error condition */

  /* graphics mode */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x80);

  TMS9928A_register_w(0x40);
  TMS9928A_register_w(0x81);

  /* Pattern names at offset 0 */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x82);

  /* Color at offset 768 */
  TMS9928A_register_w(0x0C);
  TMS9928A_register_w(0x83);

  /* Pattern generator at offset 2048 */
  TMS9928A_register_w(0x01);
  TMS9928A_register_w(0x84);

  /* Sprite attribute at offset 1024 */
  TMS9928A_register_w(0x10);
  TMS9928A_register_w(0x85);

  /* Sprite generator at offset 0 */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x86);

  /* Don't care */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x87);

  /* Write color table */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x43);
  for (i=0; i<32; i++)
    TMS9928A_data_w(0x1F);

  /* Write empty sprite attribute list */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x44);
  TMS9928A_data_w(0xD0);

  /* Write pattern generator table from ROM */
  TMS9928A_register_w(0x00);
  TMS9928A_register_w(0x48);
  for (i=0; i<2048; i++)
    TMS9928A_data_w((Machine->memory_region[0])[0x1C00 + (i & 0x3FF)]);

  return 0; /* no error */
}

static void ti99_2_vh_stop()
{
  TMS9928A_stop();
}

static void ti99_2_vh_refresh (struct osd_bitmap *bmp, int full_refresh)
{
  TMS9928A_refresh(bmp, full_refresh);
}

unsigned char *video_ram;

static void ti99_2_video_w(int offset, int data)
{
  if (data != video_ram[0xec00 + offset])
  {
    video_ram[0xec00 + offset] = data;

    TMS9928A_register_w(offset & 0xff);
    TMS9928A_register_w(0x40 | ((offset >> 8) & 0x03));
    TMS9928A_data_w(data);
  }
}

static struct MemoryReadAddress ti99_2_readmem[] =
{
  { 0x0000, 0x3fff, MRA_ROM },          /*system ROM*/
  { 0x4000, 0x5fff, MRA_ROM/*MRA_BANKED1*/ },   /*system ROM, sometimes banked*/
  { 0x6000, 0xdfff, MRA_NOP },          /*free for expansion*/
  { 0xe000, 0xefff, MRA_RAM },          /*system RAM*/
  { 0xf000, 0xf0f7, MRA_RAM },          /*processor RAM (huh... should be in a tms9995.c...)*/
  { 0xf0f8, 0xfff9, MRA_NOP },          /*free for expansion (???)*/
  { 0xfffa, 0xfffb, MRA_NOP },          /*on-chip decrementer*/
  { -1 }    /* end of table */
};

static struct MemoryWriteAddress ti99_2_writemem[] =
{
  { 0x0000, 0x3fff, MWA_ROM },          /*system ROM*/
  { 0x4000, 0x5fff, MWA_ROM },          /*system ROM, sometimes banked*/
  { 0x6000, 0xdfff, MWA_NOP },          /*free for expansion*/
  { 0xe000, 0xebff, MWA_RAM },          /*system RAM*/
  { 0xec00, 0xeeff, ti99_2_video_w, & video_ram },  /*system RAM : used for video*/
  { 0xef00, 0xefff, MWA_RAM },          /*system RAM*/
  { 0xf000, 0xf0f7, MWA_RAM },          /*processor RAM (huh... should be in some "tms9995.c" file...)*/
  { 0xf0f8, 0xfff9, MWA_NOP },          /*free for expansion (???)*/
  { 0xfffa, 0xfffb, MRA_NOP },          /*on-chip decrementer*/
  { -1 }    /* end of table */
};

/*
CRU map :
* 0x0000-0x2000 : reserved
* 0x2000-0xE000 : unaffected
* 0xE400-0xE40E : keyboard I/O (8bits input, either 3 or 6 bit output)
* 0xE80C : cassette I/O
* 0xE80A : ALC BAV
* 0xE808 : ALC HSK
* 0xE800-0xE808 : ALC data (I/O)
* 0xE80E : video enable (probably input)
* 0xF000-0xFFFE : reserved
*/

static int KeyRow = 0;

static void ti99_2_write_kbd(int offset, int data)
{
  offset &= 0x7;  /* other address lines are not decoded */

  if (offset <= 2)
    /* this implementation is just a guess */
    if (data)
      KeyRow |= 1 << offset;
    else
      KeyRow &= ~ (1 << offset);
}

static void ti99_2_write_misc_cru(int offset, int data)
{
  offset &= 0x7;  /* other address lines are not decoded */

  switch (offset)
  {
  case 0:
  case 1:
  case 2:
  case 3:
    /* ALC I/O */
    break;
  case 4:
    /* ALC HSK */
    break;
  case 5:
    /* ALC BAV */
    break;
  case 6:
    /* cassette output */
    break;
  case 7:
    /* video enable */
    break;
  }
}

static struct IOWritePort ti99_2_writeport[] =
{
  {0x7200, 0x73ff, ti99_2_write_kbd},
  {0x7400, 0x75ff, ti99_2_write_misc_cru},
  /*{0x7600, 0x77ff, ti99_2_write_both_cru},*/  /* just a guess which must be wrong... */
  { -1 }    /* end of table */
};

int ti99_2_read_kbd(int offset)
{
  return readinputport(KeyRow);
}

int ti99_2_read_misc_cru(int offset)
{
  return 0;
}

static struct IOReadPort ti99_2_readport[] =
{
  {0x0E40, 0x0E7f, ti99_2_read_kbd},
  {0x0E80, 0x0Eff, ti99_2_read_misc_cru},
  { -1 }    /* end of table */
};


/* ti99/2 : 54-key keyboard */
INPUT_PORTS_START(input_ports_ti99_2)

  PORT_START    /* col 0 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "1 ! DEL", KEYCODE_1, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "2 @ INS", KEYCODE_2, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "3 #", KEYCODE_3, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "4 $ CLEAR", KEYCODE_4, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, "5 % BEGIN", KEYCODE_5, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "6 ^ PROC'D", KEYCODE_6, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "7 & AID", KEYCODE_7, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "8 * REDO", KEYCODE_8, IP_JOY_NONE)

  PORT_START    /* col 1 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "q Q", KEYCODE_Q, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "w W ~", KEYCODE_W, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "e E (UP)", KEYCODE_E, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "r R [", KEYCODE_R, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, "t T ]", KEYCODE_T, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "y Y", KEYCODE_Y, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "i I ?", KEYCODE_I, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "9 ( BACK", KEYCODE_9, IP_JOY_NONE)

  PORT_START    /* col 2 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "a A", KEYCODE_A, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "s S (LEFT)", KEYCODE_S, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "d D (RIGHT)", KEYCODE_D, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "f F {", KEYCODE_F, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, "h H", KEYCODE_H, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "u U _", KEYCODE_U, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "o O '", KEYCODE_O, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "0 )", KEYCODE_0, IP_JOY_NONE)

  PORT_START    /* col 3 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "z Z \\", KEYCODE_Z, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "x X (DOWN)", KEYCODE_X, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "c C `", KEYCODE_C, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "g G }", KEYCODE_G, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, "j J", KEYCODE_J, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "k K", KEYCODE_K, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "p P \"", KEYCODE_P, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "= + QUIT", KEYCODE_EQUALS, IP_JOY_NONE)

  PORT_START    /* col 4 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "SHIFT", KEYCODE_LSHIFT/*KEYCODE_CAPSLOCK*/, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "CTRL", KEYCODE_LCONTROL, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "v V", KEYCODE_V, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "n N", KEYCODE_N, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, ", <", KEYCODE_COMMA, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "l L", KEYCODE_L, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "; :", KEYCODE_COLON, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "/ -", KEYCODE_SLASH, IP_JOY_NONE)

  PORT_START    /* col 5 */
    PORT_BITX(0x80, IP_ACTIVE_LOW, IPT_KEYBOARD, "BREAK", KEYCODE_ESC, IP_JOY_NONE)
    PORT_BITX(0x40, IP_ACTIVE_LOW, IPT_KEYBOARD, "(SPACE)", KEYCODE_SPACE, IP_JOY_NONE)
    PORT_BITX(0x20, IP_ACTIVE_LOW, IPT_KEYBOARD, "b B", KEYCODE_B, IP_JOY_NONE)
    PORT_BITX(0x10, IP_ACTIVE_LOW, IPT_KEYBOARD, "m M", KEYCODE_M, IP_JOY_NONE)
    PORT_BITX(0x08, IP_ACTIVE_LOW, IPT_KEYBOARD, ". >", KEYCODE_STOP, IP_JOY_NONE)
    PORT_BITX(0x04, IP_ACTIVE_LOW, IPT_KEYBOARD, "FCTN", KEYCODE_LALT, IP_JOY_NONE)
    PORT_BITX(0x02, IP_ACTIVE_LOW, IPT_KEYBOARD, "SHIFT", KEYCODE_RSHIFT, IP_JOY_NONE)
    PORT_BITX(0x01, IP_ACTIVE_LOW, IPT_KEYBOARD, "ENTER", KEYCODE_ENTER, IP_JOY_NONE)

  PORT_START    /* col 6 : not connected */
    PORT_BITX(0xFF, IP_ACTIVE_LOW, IPT_UNUSED, "unused", IP_KEY_NONE, IP_JOY_NONE)

  PORT_START    /* col 7 : not connected */
    PORT_BITX(0xFF, IP_ACTIVE_LOW, IPT_UNUSED, "unused", IP_KEY_NONE, IP_JOY_NONE)

INPUT_PORTS_END

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ -1 }    /* end of array */
};

static void tms9928A_init_palette(unsigned char *palette, unsigned short *colortable, const unsigned char *)
{
	memcpy(palette, & TMS9928A_palette, sizeof(TMS9928A_palette));
	memcpy(colortable, & TMS9928A_colortable, sizeof(TMS9928A_colortable));
}

static struct MachineDriver machine_driver_ti99_2 =
{
	/* basic machine hardware */
	{
		{
#if 0
			/*we NEED a tms9995*/
			CPU_TMS9995,
			10700000,     /* 10.7 Mhz*/
#else
			CPU_TMS9900,
			5000000,     /* 10.7 Mhz*/
#endif
			0,            /* Memory region #0 */
			ti99_2_readmem, ti99_2_writemem, ti99_2_readport, ti99_2_writeport,
			ti99_2_vblank_interrupt, 1,
			0, 0,
			0
		},
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION, /* frames per second, vblank duration */
	1,
	ti99_2_init_machine,
	ti99_2_stop_machine,

	/* video hardware */
	256,                      /* screen width */
	192,                      /* screen height */
	{ 0, 256-1, 0, 192-1},    /* visible_area */
	gfxdecodeinfo,            /* graphics decode info (???)*/
	/*2*/TMS9928A_PALETTE_SIZE,   /* palette is 3*total_colors bytes long */
	/*2*/TMS9928A_COLORTABLE_SIZE,  /* length in shorts of the color lookup table */
	/*ti99_2_init_palette*/tms9928A_init_palette,   /* palette init */

	VIDEO_TYPE_RASTER,
	0,
	ti99_2_vh_start,
	ti99_2_vh_stop,
	ti99_2_vh_refresh,

	/* sound hardware */
	0,
	0,0,0,
	{ /* no sound ! */
	}
};


/*
  ROM loading
*/
ROM_START(ti99_2)
	/*CPU memory space*/
	ROM_REGION(0x10000)
	ROM_LOAD("992rom.bin", 0x0000, 0x6000, 0x00000000)      /* system ROMs */

ROM_END

struct GameDriver ti99_2_driver =
{
	__FILE__,
	0,
	"ti99",
	"TI99/2 Home Computer",
	"1983 (prototypes)",
	"Texas Instrument",
	"R Nabet.",
	GAME_COMPUTER,    /*needed on macintosh*/
	&machine_driver_ti99_2,
	0,    /* optional function to be called during initialization */

	rom_ti99_2,
	0,    /* load rom_file images */
	0,    /* identify rom images */
	0,
	0,            /* number of ROM slots */
	/* a TI99/2 console has one slot for cartidges and expansions, but I don't know anything
	  about the two or three cartidges developped for this console. */
	0/*4*/,       /* number of floppy drives supported - supported through hex-bus */
	0,            /* number of hard drives supported */
	0/*1*/,       /* number of cassette drives supported */
	0,            /* rom decoder */
	0,            /* opcode decoder */
	0,            /* pointer to sample names */
	0,            /* sound_prom */

	input_ports_ti99_2,

	0,            /* color_prom */
	0,            /* color palette - obsolete */
	0,            /* color lookup table - obsolete */

	ORIENTATION_DEFAULT,  /* orientation */

	0,            /* hiscore load */
	0,            /* hiscore save */
};

