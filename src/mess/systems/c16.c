/***************************************************************************

	simple and quick commodore vic20 home computer

***************************************************************************/
#include "mess/machine/c16.h"
#include "driver.h"

/*
	commodore c16/c116/plus 4
	16 KByte (C16/C116) or 32 KByte or 64 KByte (plus4) RAM
	32 KByte Rom (C16/C116) 64 KByte Rom (plus4)
	availability to append additional 64 KByte Rom

	ports 0xfd00 till 0xff3f are always read/writeable for the cpu
		for the video interface chip it seams to read from
		ram or from rom in this	area

	writes go always to ram
	only 16 KByte Ram mapped to 0x4000,0x8000,0xc000
	only 32 KByte Ram mapped to 0x8000

	rom bank at 0x8000: 16K Byte(low bank)
		first: basic
		second(plus 4 only): plus4 rom low
		third: expansion slot
		fourth: expansion slot
	rom bank at 0xc000: 16K Byte(high bank)
		first: kernal
		second(plus 4 only): plus4 rom high
		third: expansion slot
		fourth: expansion slot
	writes to 0xfddx select rom banks:
		address line 0 and 1: rom bank low
		address line 2 and 3: rom bank high

	writes to 0xff3e switches to roms (0x8000 till 0xfd00, 0xff40 till 0xffff)
	writes to 0xff3f switches to rams

	at 0xfc00 till 0xfcff is ram or rom kernal readable
*/

static struct MemoryReadAddress c16_readmem[] =
{
	{ 0x0000, 0x0001, c16_m7501_port_r },
	{ 0x0002, 0x3fff, MRA_RAM },
	{ 0x4000, 0x7fff, MRA_BANK1 }, // only ram memory configuration
	{ 0x8000, 0xbfff, MRA_BANK2 },
	{ 0xc000, 0xfbff, MRA_BANK3 },
	{ 0xfc00, 0xfcff, MRA_BANK4 },
//	{ 0xfd00, 0xfd0f, c16_6551_port_r }, // acia nur plus4
	{ 0xfd10, 0xfd1f, c16_6529b_port_r }, // pio
	{ 0xfd30, 0xfd3f, c16_6529_port_r }, // 6529 keyboard matrix
//	{ 0xfec0, 0xfedf, c16_iec9_port_r }, // configured in c16_common_init
//	{ 0xfee0, 0xfeff, c16_iec8_port_r }, // configured in c16_common_init
	{ 0xff00, 0xff1f, ted7360_port_r },
	{ 0xff20, 0xffff, MRA_BANK8 },
//	{ 0x10000, 0x3ffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress c16_writemem[] =
{
	{ 0x0000, 0x0001, c16_m7501_port_w, &c16_memory },
#ifdef RASTERLINEBASED
	{ 0x0002, 0x3fff, MWA_RAM },
#else
	{ 0x0002, 0x3fff, c16_write_0002 },
#endif
//	{ 0x4000, 0x7fff, c16_write_4000 }, //configured in c16_common_init
//	{ 0x8000, 0xbfff, c16_write_8000 }, //configured in c16_common_init
//	{ 0xc000, 0xfcff, c16_write_c000 }, //configured in c16_common_init
//	{ 0xfd00, 0xfd0f, c16_6551_port_w }, // acia 6551 nur plus4
	{ 0xfd10, 0xfd1f, c16_6529b_port_w }, // pio 6529b
	{ 0xfd30, 0xfd3f, c16_6529_port_w },// 6529 keyboard matrix
	{ 0xfdd0, 0xfddf, c16_select_roms }, // rom chips selection
//	{ 0xfec0, 0xfedf, c16_iec9_port_w }, //configured in c16_common_init
//	{ 0xfee0, 0xfeff, c16_iec8_port_w }, //configured in c16_common_init
	{ 0xff00, 0xff1f, ted7360_port_w },
//	{ 0xff20, 0xff3d, MWA_write_ff20 }, //configure in c16_common_init
	{ 0xff3e, 0xff3e, c16_switch_to_rom },
	{ 0xff3f, 0xff3f, c16_switch_to_ram },
//	{ 0xff40, 0xffff, c16_write_ff40 }, //configure in c16_common_init
//	{ 0x10000, 0x3ffff, MWA_ROM },
	{ -1 }  /* end of table */
};

#define DIPS_BOTH \
	PORT_START \
	PORT_BIT( 8, IP_ACTIVE_HIGH, IPT_BUTTON1) \
	PORT_BIT( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT | IPF_8WAY )\
	PORT_BIT( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT | IPF_8WAY )\
	PORT_BIT( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP | IPF_8WAY ) \
	PORT_BIT( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN | IPF_8WAY )\
	PORT_BIT ( 0x7, 0x0,	 IPT_UNUSED )\
	PORT_START \
	PORT_BITX( 8, IP_ACTIVE_HIGH, IPT_BUTTON1|IPF_PLAYER2, "P2 Button", KEYCODE_LALT,JOYCODE_2_BUTTON1 )\
	PORT_BITX( 0x80, IP_ACTIVE_HIGH, IPT_JOYSTICK_LEFT|IPF_PLAYER2 | IPF_8WAY,"P2 Left",KEYCODE_DEL,JOYCODE_2_LEFT )\
	PORT_BITX( 0x40, IP_ACTIVE_HIGH, IPT_JOYSTICK_RIGHT|IPF_PLAYER2 | IPF_8WAY,"P2 Right",KEYCODE_PGDN,JOYCODE_2_RIGHT )\
	PORT_BITX( 0x20, IP_ACTIVE_HIGH, IPT_JOYSTICK_UP|IPF_PLAYER2 | IPF_8WAY,"P2 Up", KEYCODE_HOME, JOYCODE_2_UP)\
	PORT_BITX( 0x10, IP_ACTIVE_HIGH, IPT_JOYSTICK_DOWN|IPF_PLAYER2 | IPF_8WAY,"P2 Down", KEYCODE_END, JOYCODE_2_DOWN)\
	PORT_BIT ( 0x7, 0x0,	 IPT_UNUSED )\
	PORT_START\
	PORT_BITX( 0x8000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "ESC", KEYCODE_TILDE, IP_JOY_NONE)\
	PORT_BITX( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "1 !   BLK   ORNG", KEYCODE_1, IP_JOY_NONE)\
	PORT_BITX( 0x2000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "2 \"   WHT   BRN", KEYCODE_2, IP_JOY_NONE)\
	PORT_BITX( 0x1000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "3 #   RED   YL GRN", KEYCODE_3, IP_JOY_NONE)\
	PORT_BITX( 0x0800, IP_ACTIVE_HIGH, IPT_UNKNOWN, "4 $   CYN   PINK", KEYCODE_4, IP_JOY_NONE)\
	PORT_BITX( 0x0400, IP_ACTIVE_HIGH, IPT_UNKNOWN, "5 %   PUR   BL GRN", KEYCODE_5, IP_JOY_NONE)\
	PORT_BITX( 0x0200, IP_ACTIVE_HIGH, IPT_UNKNOWN, "6 &   GRN   L BLU", KEYCODE_6, IP_JOY_NONE)\
	PORT_BITX( 0x0100, IP_ACTIVE_HIGH, IPT_UNKNOWN, "7 '   BLU   D BLU", KEYCODE_7, IP_JOY_NONE)\
	PORT_BITX( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN, "8 (   YEL   L GRN", KEYCODE_8, IP_JOY_NONE)\
	PORT_BITX( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN, "9 )   RVS-ON", KEYCODE_9, IP_JOY_NONE)\
	PORT_BITX( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN, "0 ^   RVS-OFF", KEYCODE_0, IP_JOY_NONE)\
	PORT_BITX( 0x0010, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Cursor Left", KEYCODE_4_PAD, IP_JOY_NONE)\
	PORT_BITX( 0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Cursor Right", KEYCODE_6_PAD, IP_JOY_NONE)\
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Cursor Up", KEYCODE_8_PAD, IP_JOY_NONE)\
	PORT_BITX( 0x0002, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Cursor Down", KEYCODE_2_PAD, IP_JOY_NONE)\
	PORT_BITX( 0x0001, IP_ACTIVE_HIGH, IPT_UNKNOWN, "DEL INST", KEYCODE_BACKSPACE, IP_JOY_NONE)\
	PORT_START\
	PORT_BITX( 0x8000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "CTRL", KEYCODE_RCONTROL, IP_JOY_NONE)\
	PORT_BITX( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Q", KEYCODE_Q, IP_JOY_NONE)\
	PORT_BITX( 0x2000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "W", KEYCODE_W, IP_JOY_NONE)\
	PORT_BITX( 0x1000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "E", KEYCODE_E, IP_JOY_NONE)\
	PORT_BITX( 0x0800, IP_ACTIVE_HIGH, IPT_UNKNOWN, "R", KEYCODE_R, IP_JOY_NONE)\
	PORT_BITX( 0x0400, IP_ACTIVE_HIGH, IPT_UNKNOWN, "T", KEYCODE_T, IP_JOY_NONE)\
	PORT_BITX( 0x0200, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Y", KEYCODE_Y, IP_JOY_NONE)\
	PORT_BITX( 0x0100, IP_ACTIVE_HIGH, IPT_UNKNOWN, "U", KEYCODE_U, IP_JOY_NONE)\
	PORT_BITX( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN, "I", KEYCODE_I, IP_JOY_NONE)\
	PORT_BITX( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN, "O", KEYCODE_O, IP_JOY_NONE)\
	PORT_BITX( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN, "P", KEYCODE_P, IP_JOY_NONE)\
	PORT_BITX( 0x0010, IP_ACTIVE_HIGH, IPT_UNKNOWN, "At", KEYCODE_OPENBRACE, IP_JOY_NONE)\
	PORT_BITX( 0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN, "+", KEYCODE_CLOSEBRACE, IP_JOY_NONE)\
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_UNKNOWN, "-",KEYCODE_MINUS, IP_JOY_NONE)\
	PORT_BITX( 0x0002, IP_ACTIVE_HIGH, IPT_UNKNOWN, "HOME CLEAR", KEYCODE_EQUALS, IP_JOY_NONE)\
	PORT_BITX( 0x0001, IP_ACTIVE_HIGH, IPT_UNKNOWN, "STOP RUN", KEYCODE_TAB, IP_JOY_NONE)\
	PORT_START\
	PORT_BITX( 0x8000, 1, IPF_TOGGLE, "SHIFT-LOCK (switch)", KEYCODE_CAPSLOCK, IP_JOY_NONE)\
	PORT_BITX( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "A", KEYCODE_A, IP_JOY_NONE)\
	PORT_BITX( 0x2000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "S", KEYCODE_S, IP_JOY_NONE)\
	PORT_BITX( 0x1000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "D", KEYCODE_D, IP_JOY_NONE)\
	PORT_BITX( 0x0800, IP_ACTIVE_HIGH, IPT_UNKNOWN, "F", KEYCODE_F, IP_JOY_NONE)\
	PORT_BITX( 0x0400, IP_ACTIVE_HIGH, IPT_UNKNOWN, "G", KEYCODE_G, IP_JOY_NONE)\
	PORT_BITX( 0x0200, IP_ACTIVE_HIGH, IPT_UNKNOWN, "H", KEYCODE_H, IP_JOY_NONE)\
	PORT_BITX( 0x0100, IP_ACTIVE_HIGH, IPT_UNKNOWN, "J", KEYCODE_J, IP_JOY_NONE)\
	PORT_BITX( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN, "K", KEYCODE_K, IP_JOY_NONE)\
	PORT_BITX( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN, "L", KEYCODE_L, IP_JOY_NONE)\
	PORT_BITX( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN, ": [", KEYCODE_COLON, IP_JOY_NONE)\
	PORT_BITX( 0x0010, IP_ACTIVE_HIGH, IPT_UNKNOWN, "; ]", KEYCODE_QUOTE, IP_JOY_NONE)\
	PORT_BITX( 0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN, "*", KEYCODE_BACKSLASH, IP_JOY_NONE)\
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_UNKNOWN, "RETURN",KEYCODE_ENTER, IP_JOY_NONE)\
	PORT_BITX( 0x0002, IP_ACTIVE_HIGH, IPT_UNKNOWN, "CBM", KEYCODE_RALT, IP_JOY_NONE)\
	PORT_BITX( 0x0001, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Left-Shift", KEYCODE_LSHIFT, IP_JOY_NONE)\
	PORT_START \
	PORT_BITX( 0x8000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Z", KEYCODE_Z, IP_JOY_NONE)\
	PORT_BITX( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "X", KEYCODE_X, IP_JOY_NONE)\
	PORT_BITX( 0x2000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "C", KEYCODE_C, IP_JOY_NONE)\
	PORT_BITX( 0x1000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "V", KEYCODE_V, IP_JOY_NONE)\
	PORT_BITX( 0x0800, IP_ACTIVE_HIGH, IPT_UNKNOWN, "B", KEYCODE_B, IP_JOY_NONE)\
	PORT_BITX( 0x0400, IP_ACTIVE_HIGH, IPT_UNKNOWN, "N", KEYCODE_N, IP_JOY_NONE)\
	PORT_BITX( 0x0200, IP_ACTIVE_HIGH, IPT_UNKNOWN, "M", KEYCODE_M, IP_JOY_NONE)\
	PORT_BITX( 0x0100, IP_ACTIVE_HIGH, IPT_UNKNOWN, ", <", KEYCODE_COMMA, IP_JOY_NONE)\
	PORT_BITX( 0x0080, IP_ACTIVE_HIGH, IPT_UNKNOWN, ". >", KEYCODE_STOP, IP_JOY_NONE)\
	PORT_BITX( 0x0040, IP_ACTIVE_HIGH, IPT_UNKNOWN, "/ ?", KEYCODE_SLASH, IP_JOY_NONE)\
	PORT_BITX( 0x0020, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Right-Shift", KEYCODE_RSHIFT, IP_JOY_NONE)\
	PORT_BITX( 0x0010, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Pound", KEYCODE_INSERT, IP_JOY_NONE)\
	PORT_BITX( 0x0008, IP_ACTIVE_HIGH, IPT_UNKNOWN, "= Pi Arrow-Left", KEYCODE_PGUP, IP_JOY_NONE)\
	PORT_BITX( 0x0004, IP_ACTIVE_HIGH, IPT_UNKNOWN, "Space", KEYCODE_SPACE, IP_JOY_NONE)\
	PORT_BITX( 0x0002, IP_ACTIVE_HIGH, IPT_UNKNOWN, "f1 f4", KEYCODE_F1, IP_JOY_NONE)\
	PORT_BITX( 0x0001, IP_ACTIVE_HIGH, IPT_UNKNOWN, "f2 f5", KEYCODE_F2, IP_JOY_NONE)\
	PORT_START \
	PORT_BITX( 0x8000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "f3 f6", KEYCODE_F3, IP_JOY_NONE)\
	PORT_BITX( 0x4000, IP_ACTIVE_HIGH, IPT_UNKNOWN, "HELP f7", KEYCODE_F4, IP_JOY_NONE)\
	PORT_START \
	PORT_DIPNAME ( 0x80, 0x80, "Joystick 1", IP_KEY_NONE)\
	PORT_DIPSETTING(  0, "off" )\
	PORT_DIPSETTING(	0x80, "on" )\
	PORT_DIPNAME ( 0x40, 0x40, "Joystick 2", IP_KEY_NONE)\
	PORT_DIPSETTING(  0, "off" )\
	PORT_DIPSETTING(0x40, "on" )\
	PORT_DIPNAME ( 0x20, 0x20, "IEC Bus 8/C1551 Floppy Drive", IP_KEY_NONE)\
	PORT_DIPSETTING(  0, "off" )\
	PORT_DIPSETTING(0x20, "on" )\
	PORT_DIPNAME ( 0x10, 0x10, "IEC Bus 9/C1551 Floppy Drive", IP_KEY_NONE)\
	PORT_DIPSETTING(  0, "off" )\
	PORT_DIPSETTING(0x10, "on" )

INPUT_PORTS_START( c16 )
	DIPS_BOTH
	PORT_DIPNAME ( 3, 0, "Memory", IP_KEY_NONE)
	PORT_DIPSETTING(  0, "16 KByte" )
	PORT_DIPSETTING(	2, "32 KByte" )
	PORT_DIPSETTING(	3, "64 KByte" )
INPUT_PORTS_END

INPUT_PORTS_START( plus4 )
	DIPS_BOTH
	PORT_BIT ( 0x3, 0x3,	 IPT_UNUSED ) // 64K Memory
INPUT_PORTS_END

/* Initialise the c16 palette */
static void c16_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable,const unsigned char *color_prom)
{
	memcpy(sys_palette,ted7360_palette,sizeof(ted7360_palette));
}

static struct MachineDriver c16_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6510,// MOS7501 has no nmi line
			1400000, //TED7360PAL_CLOCK/2
			0,
			c16_readmem,c16_writemem,
			0,0,
			c16_frame_interrupt,1,
		  },
	},
	TED7360PAL_VRETRACERATE, DEFAULT_REAL_60HZ_VBLANK_DURATION,		 /* frames per second, vblank duration */
	0,
	c16_init_machine,
	c16_shutdown_machine,

	/* video hardware */
	320,										/* screen width */
	200,										/* screen height */
	{ 0,320-1, 0,200-1 },					/* visible_area */
	0,						/* graphics decode info */
	sizeof(ted7360_palette) / sizeof(ted7360_palette[0]) / 3,
	0,
	c16_init_palette,											/* convert color prom */

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	ted7360_vh_start,
	ted7360_vh_stop,
	ted7360_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &ted7360_sound_interface },
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/
ROM_START(c16)
	ROM_REGION(0x40000)
	ROM_LOAD("basic.80",  0x10000, 0x4000, 0x74eaae87)
//	ROM_LOAD("kernel.c0",    0x14000, 0x4000, 0x71c07bd4)
	ROM_LOAD("kernel2.c0",    0x14000, 0x4000, 0x77bab934)
//	ROM_LOAD("kernelv4.nts", 0x14000,0x4000,0x799a633d)
ROM_END

struct GameDriver c16_driver =
{
	__FILE__,
	0,
	"c16",
	"Commodore C16/116 (PAL)",
	"198?",
	"CBM",
	"Peter Trauner",
	0,
	&c16_machine_driver,
	c16_driver_init,

	rom_c16, 			/* rom module */
	0,//c16_rom_load, // called from program, needs allocated memory
	c16_rom_id,				/* identify rom images */
	0,	// file extensions
	2,						/* number of ROM slots */
	2,						/* number of floppy drives supported */
	0,						/* number of hard drives supported */
	1,						/* number of cassette drives supported */
	0, 	/* rom decoder */
	0,						/* opcode decoder */
	0,						/* pointer to sample names */
	0,						/* sound_prom */

	input_ports_c16, 	/* input ports */

	0,						/* color_prom */
	0, // vic6560_palette,				/* color palette */
	0, 		/* color lookup table */

	GAME_COMPUTER|GAME_IMPERFECT_SOUND|GAME_IMPERFECT_COLORS|ORIENTATION_DEFAULT,	/* flags */

	0,						/* hiscore load */
	0,						/* hiscore save */
};

/***************/
/* pal version */
/***************/
ROM_START(plus4)
	ROM_REGION(0x40000)
#if 1
	ROM_LOAD("basic.80",  0x10000, 0x4000, 0x74eaae87)
	ROM_LOAD("kernelv4.nts", 0x14000,0x4000,0x799a633d)
//	ROM_LOAD("kernel.c0",    0x14000, 0x4000, 0x71c07bd4)
	ROM_LOAD("3plus1lo.rom",    0x18000, 0x4000, 0x4fd1d8cb)
	ROM_LOAD("3plus1hi.rom",    0x1c000, 0x4000, 0xaab61387)
#else //cbm364 prototype !?
	ROM_LOAD("kern364p.bin", 0x14000,0x4000,0x84fd4f7a)
//	ROM_LOAD("spk3cc4.bin", 0x18000,0x4000,0x5227c2ee)
#endif
ROM_END
static struct MachineDriver plus4_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6510,// MOS 7501
			TED7360PAL_CLOCK/2,
			0,
			c16_readmem,c16_writemem,
			0,0,
			c16_frame_interrupt,1,
		  },
	},
	TED7360NTSC_VRETRACERATE, DEFAULT_REAL_60HZ_VBLANK_DURATION,		 /* frames per second, vblank duration */
	0,
	c16_init_machine,
	c16_shutdown_machine,

	/* video hardware */
	320,					/* screen width */
	200,										/* screen height */
	{ 0,320-1, 0,200-1 },					/* visible_area */
	0,						/* graphics decode info */
	sizeof(ted7360_palette) / sizeof(ted7360_palette[0]) / 3,
	0,
	c16_init_palette,		/* convert color prom */

	VIDEO_TYPE_RASTER| VIDEO_SUPPORTS_DIRTY,
	0,
	ted7360_vh_start,
	ted7360_vh_stop,
	ted7360_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &ted7360_sound_interface },
	}
};

struct GameDriver plus4_driver =
{
	__FILE__,
	&c16_driver,
	"plus4",
	"Commodore +4 (NTSC)",
	"198?",
	"CBM",
	"Peter Trauner",
	0,
	&plus4_machine_driver,
	plus4_driver_init,

	rom_plus4, 			/* rom module */
	0,//c16_rom_load, // called from program, needs allocated memory
	c16_rom_id,				/* identify rom images */
	0,	// file extensions
	2,						/* number of ROM slots */
	2,						/* number of floppy drives supported */
	0,						/* number of hard drives supported */
	1,						/* number of cassette drives supported */
	0, 	/* rom decoder */
	0,						/* opcode decoder */
	0,						/* pointer to sample names */
	0,						/* sound_prom */

	input_ports_plus4, 	/* input ports */

	0,						/* color_prom */
	0, //vic6560_palette,				/* color palette */
	0, 		/* color lookup table */

	GAME_COMPUTER|GAME_IMPERFECT_SOUND|GAME_IMPERFECT_COLORS|ORIENTATION_DEFAULT,	/* orientation */

	0,						/* hiscore load */
	0,						/* hiscore save */
};
