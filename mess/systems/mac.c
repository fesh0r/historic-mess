/*
 *	Mac Plus & 512ke emulation
 *
 *	Nate Woods, Raphael Nabet
 *
 *
 *		0x000000 - 0x3fffff 	RAM/ROM (switches based on overlay)
 *		0x400000 - 0x4fffff 	ROM
 *		0x580000 - 0x5fffff 	5380 NCR/Symbios SCSI peripherals chip (Mac Plus only)
 *		0x600000 - 0x6fffff 	RAM
 *		0x800000 - 0x9fffff 	Zilog 8530 SCC (Serial Control Chip) Read
 *		0xa00000 - 0xbfffff 	Zilog 8530 SCC (Serial Control Chip) Write
 *		0xc00000 - 0xdfffff 	IWM (Integrated Woz Machine; floppy)
 *		0xe80000 - 0xefffff 	Rockwell 6522 VIA
 *		0xf00000 - 0xffffef 	??? (the ROM appears to be accessing here)
 *		0xfffff0 - 0xffffff 	Auto Vector
 *
 *
 *	Interrupts:
 *		M68K:
 *			Level 1 from VIA
 *			Level 2 from SCC
 *			Level 4 : Interrupt switch (not implemented)
 *
 *		VIA:
 *			CA1 from VBLANK
 *			CA2 from 1 Hz clock (RTC)
 *			CB1 from Keyboard Clock
 *			CB2 from Keyboard Data
 *			SR	from Keyboard Data Ready
 *
 *		SCC:
 *			PB_EXT	from mouse Y circuitry
 *			PA_EXT	from mouse X circuitry
 *
 */

#include "driver.h"
#include "inputx.h"
#include "vidhrdw/generic.h"
#include "machine/6522via.h"
#include "includes/mac.h"
#include "videomap.h"


static ADDRESS_MAP_START(mac512ke_map, ADDRESS_SPACE_PROGRAM, 16)
	AM_RANGE(0x800000, 0x9fffff) AM_READ(mac_scc_r)
	AM_RANGE(0xa00000, 0xbfffff) AM_WRITE(mac_scc_w)
	AM_RANGE(0xc00000, 0xdfffff) AM_READWRITE(mac_iwm_r, mac_iwm_w)
	AM_RANGE(0xe80000, 0xefffff) AM_READWRITE(mac_via_r, mac_via_w)
	AM_RANGE(0xfffff0, 0xffffff) AM_READWRITE(mac_autovector_r, mac_autovector_w)
ADDRESS_MAP_END

static ADDRESS_MAP_START(macplus_map, ADDRESS_SPACE_PROGRAM, 16)
	AM_RANGE(0x580000, 0x5fffff) AM_READWRITE(macplus_scsi_r, macplus_scsi_w)
	AM_RANGE(0x800000, 0x9fffff) AM_READ(mac_scc_r)
	AM_RANGE(0xa00000, 0xbfffff) AM_WRITE(mac_scc_w)
	AM_RANGE(0xc00000, 0xdfffff) AM_READWRITE(mac_iwm_r, mac_iwm_w)
	AM_RANGE(0xe80000, 0xefffff) AM_READWRITE(mac_via_r, mac_via_w)
	AM_RANGE(0xfffff0, 0xffffff) AM_READWRITE(mac_autovector_r, mac_autovector_w)
ADDRESS_MAP_END



static struct CustomSound_interface custom_interface =
{
	mac_sh_start,
	mac_sh_stop,
	mac_sh_update
};

static MACHINE_DRIVER_START( mac512ke )
	/* basic machine hardware */
	MDRV_CPU_ADD_TAG("main", M68000, 7833600)        /* 7.8336 Mhz */
	MDRV_CPU_PROGRAM_MAP(mac512ke_map, 0)
	MDRV_CPU_VBLANK_INT(mac_interrupt, 370)
	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_REAL_60HZ_VBLANK_DURATION)
	MDRV_INTERLEAVE(1)

	MDRV_MACHINE_INIT( mac )

    /* video hardware */
	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_BEFORE_VBLANK)
	MDRV_SCREEN_SIZE(512, 342)
	MDRV_VISIBLE_AREA(0, 512-1, 0, 342-1)
	MDRV_PALETTE_LENGTH(2)
	MDRV_COLORTABLE_LENGTH(2)
	MDRV_PALETTE_INIT(mac)

	MDRV_VIDEO_START(mac)
	MDRV_VIDEO_UPDATE(videomap)

	/* sound hardware */
	MDRV_SOUND_ADD(CUSTOM, custom_interface)

	MDRV_NVRAM_HANDLER(mac)
MACHINE_DRIVER_END



static MACHINE_DRIVER_START( macplus )
	MDRV_IMPORT_FROM( mac512ke )
	MDRV_CPU_MODIFY( "main" )
	MDRV_CPU_PROGRAM_MAP(macplus_map, 0)
MACHINE_DRIVER_END



INPUT_PORTS_START( macplus )
	PORT_START /* 0: Mouse - button */
	PORT_BITX( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1, "Mouse Button", KEYCODE_NONE, JOYCODE_MOUSE_1_BUTTON1)

	PORT_START /* 1: Mouse - X AXIS */
	PORT_ANALOGX( 0xff, 0x00, IPT_MOUSE_X | IPF_PLAYER1, 100, 0, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE )

	PORT_START /* 2: Mouse - Y AXIS */
	PORT_ANALOGX( 0xff, 0x00, IPT_MOUSE_Y | IPF_PLAYER1, 100, 0, 0, 0, IP_KEY_NONE, IP_KEY_NONE, IP_JOY_NONE, IP_JOY_NONE )

	/* R Nabet 000531 : pseudo-input ports with keyboard layout */
	/* we only define US layout for keyboard - international layout is different! */
	/* note : 16 bits at most per port! */

	/* main keyboard pad */

	PORT_START	/* 3 */
	PORT_KEY2(0x0001, IP_ACTIVE_HIGH, "A", KEYCODE_A, IP_JOY_NONE, 'a', 'A')
	PORT_KEY2(0x0002, IP_ACTIVE_HIGH, "S", KEYCODE_S, IP_JOY_NONE, 's', 'S')
	PORT_KEY2(0x0004, IP_ACTIVE_HIGH, "D", KEYCODE_D, IP_JOY_NONE, 'd', 'D')
	PORT_KEY2(0x0008, IP_ACTIVE_HIGH, "F", KEYCODE_F, IP_JOY_NONE, 'f', 'F')
	PORT_KEY2(0x0010, IP_ACTIVE_HIGH, "H", KEYCODE_H, IP_JOY_NONE, 'h', 'H')
	PORT_KEY2(0x0020, IP_ACTIVE_HIGH, "G", KEYCODE_G, IP_JOY_NONE, 'g', 'G')
	PORT_KEY2(0x0040, IP_ACTIVE_HIGH, "Z", KEYCODE_Z, IP_JOY_NONE, 'z', 'Z')
	PORT_KEY2(0x0080, IP_ACTIVE_HIGH, "X", KEYCODE_X, IP_JOY_NONE, 'x', 'X')
	PORT_KEY2(0x0100, IP_ACTIVE_HIGH, "C", KEYCODE_C, IP_JOY_NONE, 'c', 'C')
	PORT_KEY2(0x0200, IP_ACTIVE_HIGH, "V", KEYCODE_V, IP_JOY_NONE, 'v', 'V')
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)	/* extra key on ISO : */
	PORT_KEY2(0x0800, IP_ACTIVE_HIGH, "B", KEYCODE_B, IP_JOY_NONE, 'b', 'B')
	PORT_KEY2(0x1000, IP_ACTIVE_HIGH, "Q", KEYCODE_Q, IP_JOY_NONE, 'q', 'Q')
	PORT_KEY2(0x2000, IP_ACTIVE_HIGH, "W", KEYCODE_W, IP_JOY_NONE, 'w', 'W')
	PORT_KEY2(0x4000, IP_ACTIVE_HIGH, "E", KEYCODE_E, IP_JOY_NONE, 'e', 'E')
	PORT_KEY2(0x8000, IP_ACTIVE_HIGH, "R", KEYCODE_R, IP_JOY_NONE, 'r', 'R')

	PORT_START	/* 4 */
	PORT_KEY2(0x0001, IP_ACTIVE_HIGH, "Y", KEYCODE_Y, IP_JOY_NONE, 'y', 'Y')
	PORT_KEY2(0x0002, IP_ACTIVE_HIGH, "T", KEYCODE_T, IP_JOY_NONE, 't', 'T')
	PORT_KEY2(0x0004, IP_ACTIVE_HIGH, "1", KEYCODE_1, IP_JOY_NONE, '1', '!')
	PORT_KEY2(0x0008, IP_ACTIVE_HIGH, "2", KEYCODE_2, IP_JOY_NONE, '2', '@')
	PORT_KEY2(0x0010, IP_ACTIVE_HIGH, "3", KEYCODE_3, IP_JOY_NONE, '3', '#')
	PORT_KEY2(0x0020, IP_ACTIVE_HIGH, "4", KEYCODE_4, IP_JOY_NONE, '4', '$')
	PORT_KEY2(0x0040, IP_ACTIVE_HIGH, "6", KEYCODE_6, IP_JOY_NONE, '6', '^')
	PORT_KEY2(0x0080, IP_ACTIVE_HIGH, "5", KEYCODE_5, IP_JOY_NONE, '5', '%')
	PORT_KEY2(0x0100, IP_ACTIVE_HIGH, "=", KEYCODE_EQUALS, IP_JOY_NONE, '=', '+')
	PORT_KEY2(0x0200, IP_ACTIVE_HIGH, "9", KEYCODE_9, IP_JOY_NONE, '9', '(')
	PORT_KEY2(0x0400, IP_ACTIVE_HIGH, "7", KEYCODE_7, IP_JOY_NONE, '7', '&')
	PORT_KEY2(0x0800, IP_ACTIVE_HIGH, "-", KEYCODE_MINUS, IP_JOY_NONE, '-', '_')
	PORT_KEY2(0x1000, IP_ACTIVE_HIGH, "8", KEYCODE_8, IP_JOY_NONE, '8', '*')
	PORT_KEY2(0x2000, IP_ACTIVE_HIGH, "0", KEYCODE_0, IP_JOY_NONE, '0', ')')
	PORT_KEY2(0x4000, IP_ACTIVE_HIGH, "]", KEYCODE_CLOSEBRACE, IP_JOY_NONE, ']', '}')
	PORT_KEY2(0x8000, IP_ACTIVE_HIGH, "O", KEYCODE_O, IP_JOY_NONE, 'o', 'O')

	PORT_START	/* 5 */
	PORT_KEY2(0x0001, IP_ACTIVE_HIGH, "U", KEYCODE_U, IP_JOY_NONE, 'u', 'U')
	PORT_KEY2(0x0002, IP_ACTIVE_HIGH, "[", KEYCODE_OPENBRACE, IP_JOY_NONE, '[', '{')
	PORT_KEY2(0x0004, IP_ACTIVE_HIGH, "I", KEYCODE_I, IP_JOY_NONE, 'i', 'I')
	PORT_KEY2(0x0008, IP_ACTIVE_HIGH, "P", KEYCODE_P, IP_JOY_NONE, 'p', 'P')
	PORT_KEY1(0x0010, IP_ACTIVE_HIGH, "Return", KEYCODE_ENTER, IP_JOY_NONE, '\r')
	PORT_KEY2(0x0020, IP_ACTIVE_HIGH, "L", KEYCODE_L, IP_JOY_NONE, 'l', 'L')
	PORT_KEY2(0x0040, IP_ACTIVE_HIGH, "J", KEYCODE_J, IP_JOY_NONE, 'j', 'J')
	PORT_KEY2(0x0080, IP_ACTIVE_HIGH, "'", KEYCODE_QUOTE, IP_JOY_NONE, '\'', '\"')
	PORT_KEY2(0x0100, IP_ACTIVE_HIGH, "K", KEYCODE_K, IP_JOY_NONE, 'k', 'K')
	PORT_KEY2(0x0200, IP_ACTIVE_HIGH, ";", KEYCODE_COLON, IP_JOY_NONE, ';', ':')
	PORT_KEY2(0x0400, IP_ACTIVE_HIGH, "\\", KEYCODE_BACKSLASH, IP_JOY_NONE, '\\', '|')
	PORT_KEY2(0x0800, IP_ACTIVE_HIGH, ",", KEYCODE_COMMA, IP_JOY_NONE, ',', '<')
	PORT_KEY2(0x1000, IP_ACTIVE_HIGH, "/", KEYCODE_SLASH, IP_JOY_NONE, '/', '?')
	PORT_KEY2(0x2000, IP_ACTIVE_HIGH, "N", KEYCODE_N, IP_JOY_NONE, 'n', 'N')
	PORT_KEY2(0x4000, IP_ACTIVE_HIGH, "M", KEYCODE_M, IP_JOY_NONE, 'm', 'M')
	PORT_KEY2(0x8000, IP_ACTIVE_HIGH, ".", KEYCODE_STOP, IP_JOY_NONE, '.', '>')

	PORT_START	/* 6 */
	PORT_KEY1(0x0001, IP_ACTIVE_HIGH, "Tab", KEYCODE_TAB, IP_JOY_NONE, '\t')
	PORT_KEY1(0x0002, IP_ACTIVE_HIGH, "Space", KEYCODE_SPACE, IP_JOY_NONE, ' ')
	PORT_KEY2(0x0004, IP_ACTIVE_HIGH, "`", KEYCODE_TILDE, IP_JOY_NONE, '`', '~')
	PORT_KEY1(0x0008, IP_ACTIVE_HIGH, "Backspace", KEYCODE_BACKSPACE, IP_JOY_NONE, '\010')
	PORT_BIT( 0x0010, IP_ACTIVE_HIGH, IPT_UNUSED)	/* keyboard Enter : */
	PORT_BIT( 0x0020, IP_ACTIVE_HIGH, IPT_UNUSED)	/* escape: */	
	PORT_BIT( 0x0040, IP_ACTIVE_HIGH, IPT_UNUSED)	/* ??? */
	PORT_BITX(0x0080, IP_ACTIVE_HIGH, IPT_KEYBOARD, "Command", KEYCODE_LCONTROL, IP_JOY_NONE)
	PORT_KEY1(0x0100, IP_ACTIVE_HIGH, "Shift", KEYCODE_LSHIFT, IP_JOY_NONE, UCHAR_SHIFT_1)
	PORT_KEY1(0x0100, IP_ACTIVE_HIGH, "Shift", KEYCODE_RSHIFT, IP_JOY_NONE, UCHAR_SHIFT_1)
	PORT_BITX(0x0200, IP_ACTIVE_HIGH, IPT_KEYBOARD | IPF_TOGGLE, "Caps Lock", KEYCODE_CAPSLOCK, IP_JOY_NONE)
	PORT_KEY1(0x0400, IP_ACTIVE_HIGH, "Option", KEYCODE_LALT, IP_JOY_NONE, UCHAR_SHIFT_2)
	PORT_BIT( 0x0800, IP_ACTIVE_HIGH, IPT_UNUSED)	/* Control: */
	PORT_BIT( 0x1000, IP_ACTIVE_HIGH, IPT_UNUSED)	/* keypad pseudo-keycode */
	PORT_BIT( 0xE000, IP_ACTIVE_HIGH, IPT_UNUSED)	/* ??? */

	/* keypad */
	PORT_START /* 7 */
	PORT_BIT (0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0002, IP_ACTIVE_HIGH, ". (KP)", KEYCODE_DEL_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(DEL_PAD))
	PORT_KEY1(0x0004, IP_ACTIVE_HIGH, "* (KP)", KEYCODE_ASTERISK, IP_JOY_NONE, UCHAR_MAMEKEY(ASTERISK))
	PORT_BIT (0x0038, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0040, IP_ACTIVE_HIGH, "+ (KP)", KEYCODE_PLUS_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(PLUS_PAD))
	PORT_KEY1(0x0080, IP_ACTIVE_HIGH, "Clear (KP)", /*KEYCODE_NUMLOCK*/KEYCODE_DEL, IP_JOY_NONE, UCHAR_MAMEKEY(DEL))
	PORT_KEY1(0x0100, IP_ACTIVE_HIGH, "= (KP)", /*KEYCODE_OTHER*/KEYCODE_NUMLOCK, IP_JOY_NONE, UCHAR_MAMEKEY(NUMLOCK))
	PORT_BIT (0x0E00, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x1000, IP_ACTIVE_HIGH, "Enter (KP)", KEYCODE_ENTER_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(ENTER_PAD))
	PORT_KEY1(0x2000, IP_ACTIVE_HIGH, "/ (KP)", KEYCODE_SLASH_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(SLASH_PAD))
	PORT_KEY1(0x4000, IP_ACTIVE_HIGH, "- (KP)", KEYCODE_MINUS_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(MINUS_PAD))
	PORT_BIT (0x8000, IP_ACTIVE_HIGH, IPT_UNUSED)

	PORT_START /* 8 */
	PORT_BIT( 0x0001, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_BIT( 0x0002, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0004, IP_ACTIVE_HIGH, "0 (KP)", KEYCODE_0_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(0_PAD))
	PORT_KEY1(0x0008, IP_ACTIVE_HIGH, "1 (KP)", KEYCODE_1_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(1_PAD))
	PORT_KEY1(0x0010, IP_ACTIVE_HIGH, "2 (KP)", KEYCODE_2_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(2_PAD))
	PORT_KEY1(0x0020, IP_ACTIVE_HIGH, "3 (KP)", KEYCODE_3_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(3_PAD))
	PORT_KEY1(0x0040, IP_ACTIVE_HIGH, "4 (KP)", KEYCODE_4_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(4_PAD))
	PORT_KEY1(0x0080, IP_ACTIVE_HIGH, "5 (KP)", KEYCODE_5_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(5_PAD))
	PORT_KEY1(0x0100, IP_ACTIVE_HIGH, "6 (KP)", KEYCODE_6_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(6_PAD))
	PORT_KEY1(0x0200, IP_ACTIVE_HIGH, "7 (KP)", KEYCODE_7_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(7_PAD))
	PORT_BIT( 0x0400, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0800, IP_ACTIVE_HIGH, "8 (KP)", KEYCODE_8_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(8_PAD))
	PORT_KEY1(0x1000, IP_ACTIVE_HIGH, "9 (KP)", KEYCODE_9_PAD, IP_JOY_NONE, UCHAR_MAMEKEY(9_PAD))
	PORT_BIT( 0xE000, IP_ACTIVE_HIGH, IPT_UNUSED)

	/* Arrow keys */
	PORT_START /* 9 */
	PORT_BIT( 0x0003, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0004, IP_ACTIVE_HIGH, "Right Arrow", KEYCODE_RIGHT, IP_JOY_NONE, UCHAR_MAMEKEY(RIGHT))
	PORT_BIT( 0x0038, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0040, IP_ACTIVE_HIGH, "Left Arrow", KEYCODE_LEFT, IP_JOY_NONE, UCHAR_MAMEKEY(LEFT))
	PORT_BIT( 0x0080, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x0100, IP_ACTIVE_HIGH, "Down Arrow", KEYCODE_DOWN, IP_JOY_NONE, UCHAR_MAMEKEY(DOWN))
	PORT_BIT( 0x1E00, IP_ACTIVE_HIGH, IPT_UNUSED)
	PORT_KEY1(0x2000, IP_ACTIVE_HIGH, "Up Arrow", KEYCODE_UP, IP_JOY_NONE, UCHAR_MAMEKEY(UP))
	PORT_BIT( 0xC000, IP_ACTIVE_HIGH, IPT_UNUSED)

INPUT_PORTS_END



/***************************************************************************

  Game driver(s)

  The Mac driver uses a convention of placing the BIOS in REGION_USER1

***************************************************************************/

ROM_START( mac512ke )
	ROM_REGION(0x20000, REGION_USER1, 0)
	ROM_LOAD16_WORD_SWAP( "macplus.rom",  0x00000, 0x20000, CRC(b2102e8e))
ROM_END


ROM_START( macplus )
	ROM_REGION(0x20000, REGION_USER1, 0)
	ROM_LOAD16_WORD_SWAP( "macplus.rom",  0x00000, 0x20000, CRC(b2102e8e))
ROM_END


SYSTEM_CONFIG_START(mac_base)
	CONFIG_DEVICE_LEGACY(IO_FLOPPY, 2, "dsk\0img\0", DEVICE_LOAD_RESETS_NONE, OSD_FOPEN_RW_OR_READ, NULL, NULL, device_load_mac_floppy, device_unload_mac_floppy, NULL)
SYSTEM_CONFIG_END


SYSTEM_CONFIG_START(mac128k)
	CONFIG_IMPORT_FROM(mac_base)
	CONFIG_RAM_DEFAULT(0x020000)
SYSTEM_CONFIG_END

SYSTEM_CONFIG_START(mac512k)
	CONFIG_IMPORT_FROM(mac_base)
	CONFIG_RAM_DEFAULT(0x080000)
SYSTEM_CONFIG_END

SYSTEM_CONFIG_START(macplus)
	CONFIG_IMPORT_FROM(mac_base)

	CONFIG_RAM			(0x080000)
	CONFIG_RAM_DEFAULT	(0x100000)
	CONFIG_RAM			(0x200000)
	CONFIG_RAM			(0x280000)
	CONFIG_RAM			(0x400000)
SYSTEM_CONFIG_END




/*	   YEAR		NAME	  PARENT	COMPAT	MACHINE   INPUT		INIT			CONFIG		COMPANY				FULLNAME */
/*COMPX( 1984,	mac128k,  0, 		0,		mac128k,  macplus,	mac128k512k,	macplus,	"Apple Computer",	"Macintosh 128k",  0 )
COMPX( 1984,	mac512k,  mac128k,	0,		mac128k,  macplus,  mac128k512k,	macplus,	"Apple Computer",	"Macintosh 512k",  0 )*/
COMPX( 1986,	mac512ke, macplus,  0,		mac512ke, macplus,  mac512ke,		mac512k,	"Apple Computer",	"Macintosh 512ke", 0 )
COMPX( 1986,	macplus,  0,		0,		macplus,  macplus,  macplus,		macplus,	"Apple Computer",	"Macintosh Plus",  0 )



/* ----------------------------------------------------------------------- */

#if 0

/* Early Mac2 driver - does not work at all, but enabled me to disassemble the ROMs */

static ADDRESS_MAP_START (mac2_readmem, ADDRESS_SPACE_PROGRAM, 16)

	AM_RANGE( 0x00000000, 0x007fffff) AM_READ( MRA8_RAM )	/* ram */
	AM_RANGE( 0x00800000, 0x008fffff) AM_READ( MRA8_ROM )	/* rom */
	AM_RANGE( 0x00900000, 0x00ffffff) AM_READ( MRA8_NOP )

ADDRESS_MAP_END

static ADDRESS_MAP_START (mac2_writemem, ADDRESS_SPACE_PROGRAM, 16)

	AM_RANGE( 0x00000000, 0x007fffff) AM_WRITE( MWA8_RAM )	/* ram */
	AM_RANGE( 0x00800000, 0x008fffff) AM_WRITE( MWA8_ROM )	/* rom */
	AM_RANGE( 0x00900000, 0x00ffffff) AM_WRITE( MWA8_NOP )

ADDRESS_MAP_END

static void mac2_init_machine( void )
{
	memset(memory_region(REGION_CPU1), 0, 0x800000);
}


static struct MachineDriver machine_driver_mac2 =
{
	/* basic machine hardware */
	{
		{
			CPU_M68020,
			16000000,			/* +/- 16 Mhz */
			mac2_readmem,mac2_writemem,0,0,
			0,0,
		}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,		/* frames per second, vblank duration */
	1,
	mac2_init_machine,
	0,

	/* video hardware */
	640, 480, /* screen width, screen height */
	{ 0, 640-1, 0, 480-1 }, 		/* visible_area */

	0,					/* graphics decode info */
	2, 2,						/* number of colors, colortable size */
	mac_init_palette,				/* convert color prom */

	VIDEO_TYPE_RASTER | VIDEO_UPDATE_BEFORE_VBLANK,
	0,
	mac_vh_start,
	mac_vh_stop,
	mac_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{

	},

	mac_nvram_handler
};

//#define input_ports_mac2 NULL

INPUT_PORTS_START( mac2 )

INPUT_PORTS_END

ROM_START( mac2 )
	ROM_REGION(0x00900000,REGION_CPU1,0) /* for ram, etc */
	ROM_LOAD_WIDE( "256k.rom",  0x800000, 0x40000, NO_DUMP)
ROM_END

COMPX( 1987, mac2,	   0,		 mac2,	   mac2,	 0/*mac2*/,  "Apple Computer",    "Macintosh II",  GAME_NOT_WORKING )

#endif

