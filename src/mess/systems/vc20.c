/***************************************************************************

	simple and quick commodore vic20 home computer

***************************************************************************/
#include "mess/machine/vc20.h"
#include "mess/machine/6522via2.h"
#include "driver.h"

static struct MemoryReadAddress vc20_readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
//	{ 0x0400, 0x0fff, MRA_RAM }, // ram, rom or nothing; I think read 0xff!
	{ 0x1000, 0x1fff, MRA_RAM },
//	{ 0x2000, 0x3fff, MRA_RAM }, // ram, rom or nothing
//	{ 0x4000, 0x5fff, MRA_RAM }, // ram, rom or nothing
//	{ 0x6000, 0x7fff, MRA_RAM }, // ram, rom or nothing
	{ 0x8000, 0x8fff, MRA_ROM },
	{ 0x9000, 0x900f, vic6560_port_r },
	{ 0x9010, 0x910f, MRA_NOP },
	{ 0x9110, 0x911f, via2_0_r },
	{ 0x9120, 0x912f, via2_1_r },
	{ 0x9130, 0x93ff, MRA_NOP },
	{ 0x9400, 0x97ff, MRA_RAM }, //color ram 4 bit
	{ 0x9800, 0x9fff, MRA_NOP },
//	{ 0xa000, 0xbfff, MRA_RAM }, // or nothing
	{ 0xc000, 0xffff, MRA_ROM },
	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress vc20_writemem[] =
{
	{ 0x0000, 0x03ff, vc20_write,&vc20_memory },
	{ 0x1000, 0x1fff, vc20_write_1000,&vc20_memory_1000 },
	{ 0x8000, 0x8fff, MWA_ROM },
	{ 0x9000, 0x900f, vic6560_port_w },
	{ 0x9010, 0x910f, MWA_NOP },
	{ 0x9110, 0x911f, via2_0_w },
	{ 0x9120, 0x912f, via2_1_w },
	{ 0x9130, 0x93ff, MWA_NOP },
	{ 0x9400, 0x97ff, vc20_write_9400,&vc20_memory_9400 },
	{ 0x9800, 0x9fff, MWA_NOP },
	{ 0xc000, 0xffff, MWA_ROM },
	{ -1 }  /* end of table */
};

INPUT_PORTS_START( vc20 )
	PORT_START /* in 0	joystick */
	PORT_BIT( 0x80, IP_ACTIVE_LOW,	IPT_JOYSTICK_RIGHT | IPF_8WAY )
	PORT_BIT( 0x40, IP_ACTIVE_LOW,	IPT_UNUSED )
	PORT_BIT( 0x20, IP_ACTIVE_LOW,	IPT_BUTTON1)
	PORT_BIT( 0x10, IP_ACTIVE_LOW,	IPT_JOYSTICK_LEFT | IPF_8WAY )
	PORT_BIT( 0x08, IP_ACTIVE_LOW,	IPT_JOYSTICK_DOWN | IPF_8WAY )
	PORT_BIT( 0x04, IP_ACTIVE_LOW,	IPT_JOYSTICK_UP | IPF_8WAY )
	PORT_BIT( 0x03, IP_ACTIVE_LOW,	IPT_UNUSED )

	PORT_START /* in 1	paddle buttons */
	PORT_BIT ( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x10, IP_ACTIVE_LOW, IPT_BUTTON1, "Paddle 1 Button", KEYCODE_INSERT, 0)
	PORT_BIT ( 0x0c, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2, "Paddle 2 Button", KEYCODE_DEL, 0)
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START /* in 2	paddle 1 */
	PORT_ANALOGX(0xff,128,IPT_PADDLE|IPF_REVERSE,30,20,0,0,255,KEYCODE_HOME,KEYCODE_PGUP,0,0)

	PORT_START /* in 3	paddle 2 */
	PORT_ANALOGX(0xff,128,IPT_PADDLE|IPF_PLAYER2|IPF_REVERSE,30,20,0,0,255,KEYCODE_END,KEYCODE_PGDN,0,0)

	PORT_START /* in 4 keyrow 0 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "DEL INST",          KEYCODE_BACKSPACE,  IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Pound",             KEYCODE_MINUS,      IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, "+",                 KEYCODE_PLUS_PAD,   IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "9 )   RVS-ON",      KEYCODE_9,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "7 '   BLU",         KEYCODE_7,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "5 %   PUR",         KEYCODE_5,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "3 #   RED",         KEYCODE_3,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "1 !   BLK",         KEYCODE_1,          IP_JOY_NONE)

	PORT_START /* in 5 keyrow 1 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "RETURN",            KEYCODE_ENTER,      IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "*",                 KEYCODE_ASTERISK,   IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, "P",                 KEYCODE_P,          IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "I",                 KEYCODE_I,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Y",                 KEYCODE_Y,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "R",                 KEYCODE_R,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "W",                 KEYCODE_W,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Arrow-Left",        KEYCODE_TILDE,      IP_JOY_NONE)

	PORT_START /* in 6 keyrow 2 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "CRSR-RIGHT LEFT",   KEYCODE_6_PAD,      IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "; ]",               KEYCODE_QUOTE,      IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, "L",                 KEYCODE_L,          IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "J",                 KEYCODE_J,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "G",                 KEYCODE_G,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "D",                 KEYCODE_D,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "A",                 KEYCODE_A,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "CTRL",              KEYCODE_RCONTROL,   IP_JOY_NONE)

	PORT_START /* in 7 keyrow 3 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "CRSR-DOWN UP",      KEYCODE_2_PAD,      IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "/ ?",               KEYCODE_SLASH,      IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, ", <",               KEYCODE_COMMA,      IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "N",                 KEYCODE_N,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "V",                 KEYCODE_V,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "X",                 KEYCODE_X,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Left-Shift",        KEYCODE_LSHIFT,     IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "STOP RUN",          KEYCODE_TAB,        IP_JOY_NONE)

	PORT_START /* in 8 keyrow 4 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "f1 f2",             KEYCODE_F1,         IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Right-Shift",       KEYCODE_RSHIFT,     IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, ". >",               KEYCODE_STOP,       IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "M",                 KEYCODE_M,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "B",                 KEYCODE_B,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "C",                 KEYCODE_C,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Z",                 KEYCODE_Z,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Space",             KEYCODE_SPACE,      IP_JOY_NONE)

	PORT_START /* in 9 keyrow 5 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "f3 f4",             KEYCODE_F2,         IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "=",                 KEYCODE_BACKSLASH,  IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, ": [",               KEYCODE_COLON,      IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "K",                 KEYCODE_K,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "H",                 KEYCODE_H,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "F",                 KEYCODE_F,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "S",                 KEYCODE_S,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "CBM",               KEYCODE_RALT,       IP_JOY_NONE)

	PORT_START /* in 10 keyrow 6 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "f5 f6",             KEYCODE_F3,         IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Arrow-Up Pi",       KEYCODE_CLOSEBRACE, IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, "At",                KEYCODE_OPENBRACE,  IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "O",                 KEYCODE_O,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "U",                 KEYCODE_U,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "T",                 KEYCODE_T,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "E",                 KEYCODE_E,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "Q",                 KEYCODE_Q,          IP_JOY_NONE)

	PORT_START /* in 11 keyrow 7 */
	PORT_BITX( 0x80, IP_ACTIVE_LOW,  IPT_KEYBOARD, "f7 f8",             KEYCODE_F4,         IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_LOW,  IPT_KEYBOARD, "HOME CLR",          KEYCODE_EQUALS,     IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_LOW,  IPT_KEYBOARD, "-",                 KEYCODE_MINUS_PAD,  IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_LOW,  IPT_KEYBOARD, "0     RVS-OFF",     KEYCODE_0,          IP_JOY_NONE)
	PORT_BITX( 0x08, IP_ACTIVE_LOW,  IPT_KEYBOARD, "8 (   YEL",         KEYCODE_8,          IP_JOY_NONE)
	PORT_BITX( 0x04, IP_ACTIVE_LOW,  IPT_KEYBOARD, "6 &   GRN",         KEYCODE_6,          IP_JOY_NONE)
	PORT_BITX( 0x02, IP_ACTIVE_LOW,  IPT_KEYBOARD, "4 $   CYN",         KEYCODE_4,          IP_JOY_NONE)
	PORT_BITX( 0x01, IP_ACTIVE_LOW,  IPT_KEYBOARD, "2 \"   WHT",        KEYCODE_2,          IP_JOY_NONE)

	PORT_START /* in 12 special keys */
	PORT_BITX( 0x80, IP_ACTIVE_HIGH, IPF_TOGGLE,   "SHIFT-LOCK (switch)",   KEYCODE_CAPSLOCK,   IP_JOY_NONE)
	PORT_BITX( 0x40, IP_ACTIVE_HIGH, IPT_KEYBOARD, "RESTORE",               KEYCODE_PRTSCR,     IP_JOY_NONE)
	PORT_BITX( 0x20, IP_ACTIVE_HIGH, IPT_KEYBOARD, "Special CRSR Up",       KEYCODE_8_PAD,      IP_JOY_NONE)
	PORT_BITX( 0x10, IP_ACTIVE_HIGH, IPT_KEYBOARD, "Special CRSR Left",     KEYCODE_4_PAD,      IP_JOY_NONE)
	PORT_BIT ( 0x0f, IP_ACTIVE_HIGH, IPT_UNUSED )

	PORT_START /* dsw 13 */
	PORT_DIPNAME ( 0x07, 0, "RAM Cartridge",IP_KEY_NONE )
	PORT_DIPSETTING(	0, "None" )
	PORT_DIPSETTING(	1, "3k" )
	PORT_DIPSETTING(	2, "8k" )
	PORT_DIPSETTING(	3, "16k" )
	PORT_DIPSETTING(	4, "32k" )
	PORT_DIPSETTING(	5, "Custom" )
	PORT_DIPNAME   ( 0x08, 0, " Ram at 0x0400 till 0x0fff", IP_KEY_NONE)
	PORT_DIPSETTING( 0x08, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x10, 0, " Ram at 0x2000 till 0x3fff", IP_KEY_NONE)
	PORT_DIPSETTING( 0x10, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x20, 0, " Ram at 0x4000 till 0x5fff", IP_KEY_NONE)
	PORT_DIPSETTING( 0x20, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x40, 0, " Ram at 0x6000 till 0x7fff", IP_KEY_NONE)
	PORT_DIPSETTING( 0x40, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x80, 0, " Ram at 0xa000 till 0xbfff", IP_KEY_NONE)
	PORT_DIPSETTING( 0x80, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )

	PORT_START /* dsw 14 */
	PORT_DIPNAME   ( 0x80, 0x80, "Joystick", IP_KEY_NONE)
	PORT_DIPSETTING( 0x80, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x40, 0x40, "Paddles", IP_KEY_NONE)
	PORT_DIPSETTING( 0x40, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
	PORT_DIPNAME   ( 0x20, 0x00, "Lightpen", IP_KEY_NONE)
	PORT_DIPSETTING( 0x20, "Yes" )
	PORT_DIPSETTING( 0x00, "No" )
INPUT_PORTS_END

/* Initialise the vc20 palette */
static void vc20_init_palette(unsigned char *sys_palette, unsigned short *sys_colortable,const unsigned char *color_prom)
{
	memcpy(sys_palette,vic6560_palette,sizeof(vic6560_palette));
//	memcpy(sys_colortable,cga_colortable,sizeof(cga_colortable));
}

// ntsc version
static struct MachineDriver vic20_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			VIC6560_CLOCK,
			0,
			vc20_readmem,vc20_writemem,
			0,0,
			vc20_frame_interrupt,1,
		  },
	},
	VIC6560_VRETRACERATE, DEFAULT_REAL_60HZ_VBLANK_DURATION,		 /* frames per second, vblank duration */
	0,
	vic20_init_machine,
	vc20_shutdown_machine,

	/* video hardware */
	VIC6560_HSIZE,										/* screen width */
	VIC6560_VSIZE,										/* screen height */
	{ 0,VIC6560_HSIZE-1, 0,VIC6560_VSIZE-1 },					/* visible_area */
	0,						/* graphics decode info */
	sizeof(vic6560_palette) / sizeof(vic6560_palette[0]) / 3,
	0,
	vc20_init_palette,											/* convert color prom */

	VIDEO_TYPE_RASTER|VIDEO_SUPPORTS_DIRTY,
	0,
	vic6560_vh_start,
	vic6560_vh_stop,
	vic6560_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &vic6560_sound_interface },
	}
};

/***************************************************************************

  Game driver(s)

***************************************************************************/
ROM_START(vic20)
	ROM_REGION(0x10000)
	ROM_LOAD("basic",  0xc000, 0x2000, 0xdb4c43c1)
	ROM_LOAD("kernal",    0xe000, 0x2000, 0xe5e7c174)
	ROM_LOAD("chargen",     0x8000, 0x1000, 0x83e032a6)
ROM_END

struct GameDriver vic20_driver =
{
	__FILE__,
	0,
	"vic20",
	"Commodore VIC20 (ntsc)",
	"198?",
	"CBM",
	"Peter Trauner\n"
	"Marko Makela (MOS6560 Docu)",
	0,
	&vic20_machine_driver,
	0,

	rom_vic20, 			/* rom module */
	0, //vc20_rom_load, // called from program, needs allocated memory
	vc20_rom_id,				/* identify rom images */
	0,	// file extensions
	2,						/* number of ROM slots */
	2,						/* number of floppy drives supported */
	0,						/* number of hard drives supported */
	1,						/* number of cassette drives supported */
	0, 	/* rom decoder */
	0,						/* opcode decoder */
	0,						/* pointer to sample names */
	0,						/* sound_prom */

	input_ports_vc20, 	/* input ports */

	0,						/* color_prom */
	0, // vic6560_palette,				/* color palette */
	0, 		/* color lookup table */

	GAME_COMPUTER|GAME_IMPERFECT_SOUND|ORIENTATION_DEFAULT,	/* flags */

	0,						/* hiscore load */
	0,						/* hiscore save */
};

/***************/
/* pal version */
/***************/
ROM_START(vc20)
	ROM_REGION(0x10000)
	ROM_LOAD("basic",  0xc000, 0x2000, 0xdb4c43c1)
	ROM_LOAD("kernal.pal",    0xe000, 0x2000, 0x4be07cb4)
	ROM_LOAD("chargen",     0x8000, 0x1000, 0x83e032a6)
ROM_END

static struct MachineDriver vc20_machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			VIC6561_CLOCK,
			0,
			vc20_readmem,vc20_writemem,
			0,0,
			vc20_frame_interrupt,1,
		  },
	},
	VIC6561_VRETRACERATE, DEFAULT_REAL_60HZ_VBLANK_DURATION,		 /* frames per second, vblank duration */
	0,
	vc20_init_machine,
	vc20_shutdown_machine,

	/* video hardware */
	VIC6561_HSIZE,					/* screen width */
	VIC6561_VSIZE,										/* screen height */
	{ 0,VIC6561_HSIZE-1, 0,VIC6561_VSIZE-1 },					/* visible_area */
	0,						/* graphics decode info */
	sizeof(vic6560_palette) / sizeof(vic6560_palette[0]) / 3,
	0,
	vc20_init_palette,		/* convert color prom */

	VIDEO_TYPE_RASTER| VIDEO_SUPPORTS_DIRTY,
	0,
	vic6560_vh_start,
	vic6560_vh_stop,
	vic6560_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{ SOUND_CUSTOM, &vic6560_sound_interface },
	}
};

struct GameDriver vc20_driver =
{
	__FILE__,
	&vic20_driver,
	"vc20",
	"Commodore VC20 (pal)",
	"198?",
	"CBM",
	"Peter Trauner\n"
	"Marko Makela (MOS6560 Docu)",
	0,
	&vc20_machine_driver,
	0,

	rom_vc20, 			/* rom module */
	0,//vc20_rom_load, // called from program, needs allocated memory
	vc20_rom_id,				/* identify rom images */
	0,	// file extensions
	2,						/* number of ROM slots */
	2,						/* number of floppy drives supported */
	0,						/* number of hard drives supported */
	1,						/* number of cassette drives supported */
	0, 	/* rom decoder */
	0,						/* opcode decoder */
	0,						/* pointer to sample names */
	0,						/* sound_prom */

	input_ports_vc20, 	/* input ports */

	0,						/* color_prom */
	0, //vic6560_palette,				/* color palette */
	0, 		/* color lookup table */

	GAME_COMPUTER|GAME_IMPERFECT_SOUND|ORIENTATION_DEFAULT,	/* flags */

	0,						/* hiscore load */
	0,						/* hiscore save */
};
