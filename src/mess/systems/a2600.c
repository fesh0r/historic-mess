/***************************************************************************

  a2600.c

  Driver file to handle emulation of the Atari 2600.


 Contains the addresses of the 2600 hardware

 TIA *Write* Addresses (6 bit)

	VSYNC	0x00    Vertical Sync Set-Clear
	VBLANK	0x01    Vertical Blank Set-Clear
	WSYNC	0x02    Wait for Horizontal Blank
	RSYNC	0x03    Reset Horizontal Sync Counter
	NUSIZ0	0x04    Number-Size player/missle 0
	NUSIZ1	0x05    Number-Size player/missle 1
	COLUP0	0x06    Color-Luminance Player 0
	COLUP1	0x07    Color-Luminance Player 1
	COLUPF	0x08    Color-Luminance Playfield
	COLUBK	0x09    Color-Luminance BackGround
	CTRLPF	0x0A    Control Playfield, Ball, Collisions
	REFP0	0x0B    Reflection Player 0
	REFP1	0x0C    Reflection Player 1
	PF0	    0x0D    Playfield Register Byte 0
	PF1	    0x0E    Playfield Register Byte 1
	PF2	    0x0F    Playfield Register Byte 2
	RESP0	0x10    Reset Player 0
	RESP1	0x11    Reset Player 0
	RESM0	0x12    Reset Missle 0
	RESM1	0x13    Reset Missle 1
	RESBL	0x14    Reset Ball
	AUDC0	0x15    Audio Control 0
	AUDC1	0x16    Audio Control 1
	AUDF0	0x17    Audio Frequency 0
	AUDF1	0x18    Audio Frequency 1
	AUDV0	0x19    Audio Volume 0
	AUDV1	0x1A    Audio Volume 1
	GRP0	0x1B    Graphics Register Player 0
	GRP1	0x1C    Graphics Register Player 0
	ENAM0	0x1D    Graphics Enable Missle 0
	ENAM1	0x1E    Graphics Enable Missle 1
	ENABL	0x1F    Graphics Enable Ball
	HMP0	0x20    Horizontal Motion Player 0
	HMP1	0x21    Horizontal Motion Player 0
	HMM0	0x22    Horizontal Motion Missle 0
	HMM1	0x23    Horizontal Motion Missle 1
	HMBL	0x24    Horizontal Motion Ball
	VDELP0	0x25    Vertical Delay Player 0
	VDELP1	0x26    Vertical Delay Player 1
	VDELBL	0x27    Vertical Delay Ball
	RESMP0	0x28    Reset Missle 0 to Player 0
	RESMP1	0x29    Reset Missle 1 to Player 1
	HMOVE	0x2A    Apply Horizontal Motion
	HMCLR	0x2B    Clear Horizontal Move Registers
	CXCLR	0x2C    Clear Collision Latches


 TIA *Read* Addresses
                                  bit 6  bit 7
	CXM0P	0x0    Read Collision M0-P1  M0-P0
	CXM1P	0x1                   M1-P0  M1-P1
	CXP0FB	0x2                   P0-PF  P0-BL
	CXP1FB	0x3                   P1-PF  P1-BL
	CXM0FB	0x4                   M0-PF  M0-BL
	CXM1FB	0x5                   M1-PF  M1-BL
	CXBLPF	0x6                   BL-PF  -----
	CXPPMM	0x7                   P0-P1  M0-M1
	INPT0	0x8     Read Pot Port 0
	INPT1	0x9     Read Pot Port 1
	INPT2	0xA     Read Pot Port 2
	INPT3	0xB     Read Pot Port 3
	INPT4	0xC     Read Input (Trigger) 0
	INPT5	0xD     Read Input (Trigger) 1


 RIOT Addresses

	RAM	    0x80 - 0xff           RAM 0x0180-0x01FF

	SWCHA	0x280   Port A data rwegister (joysticks)
	SWACNT	0x281   Port A data direction register (DDR)
	SWCHB	0x282   Port B data (Console Switches)
	SWBCNT	0x283   Port B DDR
	INTIM	0x284   Timer Output

	TIM1T	0x294   set 1 clock interval
	TIM8T	0x295   set 8 clock interval
	TIM64T	0x296   set 64 clock interval
	T1024T	0x297   set 1024 clock interval
                      these are also at 0x380-0x397

	ROM	0xE000	 To FFFF,0x1000-1FFF

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "cpu/m6502/m6502.h"
#include "sound/tiaintf.h"
#include "machine/6821pia.h"
#include "sound/hc55516.h"

/* vidhrdw/a2600.c */
extern int a2600_vh_start(void);
extern void a2600_vh_stop(void);
extern void a2600_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
//extern int a2600_interrupt(void);

/* machine/a2600.c */
extern int a2600_TIA_r(int offset);
extern void a2600_TIA_w(int offset, int data);
extern void a2600_init_machine(void);
extern void a2600_stop_machine(void);
extern int  a2600_id_rom (const char *name, const char *gamename);
extern int  a2600_load_rom(void);

/* exidy sound hardware for RIOT */
void exidy_shriot_w(int offset,int data);
int exidy_shriot_r(int offset);
void hc55516_digit_clock_clear_w(int num, int data)
{   /* Dummy function body */
}
void hc55516_clock_set_w(int num, int data)
{	/* Dummy function body to use MAME core */
}

static struct MemoryReadAddress readmem[] =
{

	{ 0x0000, 0x000D, a2600_TIA_r },
	{ 0x0080, 0x00FF, MRA_RAM },
	{ 0x0180, 0x01FF, MRA_RAM },		/* mirrored? */
	{ 0x1000, 0x1FFF, MRA_ROM },		/* change based on bank switch? */
	{ 0xE000, 0xFFFF, MRA_ROM },		/* change based on bank switch? */
	{ 0x0280, 0x0284, exidy_shriot_r },	/* mirrored? */
	{ 0x0380, 0x0384, exidy_shriot_r },	/* mirrored? */
    { 0xFFFC, 0xFFFF, MRA_ROM },	    /* mirrored ROM? */
    { -1 }  /* end of table */
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x002C, a2600_TIA_w },
	{ 0x0080, 0x00FF, MWA_RAM },
	{ 0x0180, 0x01FF, MWA_RAM },		/* mirrored? */
    { 0x0280, 0x0284, exidy_shriot_w },	/* mirrored? */
	{ 0x0380, 0x0384, exidy_shriot_w },	/* mirrored? */
	{ -1 }  /* end of table */
};


INPUT_PORTS_START( a2600 )
	PORT_START      /* IN0 DONE!*/
	PORT_BIT ( 0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP | IPF_PLAYER2 )
	PORT_BIT ( 0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN | IPF_PLAYER2 )
	PORT_BIT ( 0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT | IPF_PLAYER2 )
	PORT_BIT ( 0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT | IPF_PLAYER2 )
	PORT_BIT ( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_UP)
	PORT_BIT ( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN)
	PORT_BIT ( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT)
	PORT_BIT ( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT)

	PORT_START      /* IN1 */
    PORT_BIT ( 0x01, IP_ACTIVE_HIGH, IPT_BUTTON1 | IPF_PLAYER2 )
    PORT_BIT ( 0x02, IP_ACTIVE_HIGH, IPT_BUTTON1 )
    PORT_BIT ( 0x04, IP_ACTIVE_HIGH, IPT_BUTTON2 | IPF_PLAYER2 )
    PORT_BIT ( 0x08, IP_ACTIVE_HIGH, IPT_BUTTON2 )
	PORT_BIT ( 0xF0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START      /* IN2 */
	PORT_BIT (0x7F, IP_ACTIVE_LOW, IPT_UNUSED)
	PORT_BIT (0x80, IP_ACTIVE_HIGH, IPT_VBLANK)

	PORT_START      /* IN3 */
	PORT_BITX( 0x01, IP_ACTIVE_LOW, IPT_UNKNOWN, "Reset", KEYCODE_R, IP_JOY_DEFAULT)
	PORT_BITX( 0x02, IP_ACTIVE_LOW, IPT_UNKNOWN, "Start", KEYCODE_S, IP_JOY_DEFAULT)
	PORT_BIT ( 0xFC, IP_ACTIVE_LOW, IPT_UNUSED)

INPUT_PORTS_END




static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ -1 } /* end of array */
};

static struct TIAinterface tia_interface =
{
	31400,
	255,
    TIA_DEFAULT_GAIN,
};


static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
            1190000,        /* 1.19Mhz */
			readmem,writemem

		}
	},
    60, DEFAULT_REAL_60HZ_VBLANK_DURATION,
	1,
    a2600_init_machine, /* init_machine */
    a2600_stop_machine, /* stop_machine */

	/* video hardware */
    228,262, {0,227,37,35+192},
	gfxdecodeinfo,
	0 /*total colours*/,
	0 /*colour_table_length*/,
	0 /*init palette */,

	VIDEO_TYPE_RASTER,
	0,
    a2600_vh_start,
    a2600_vh_stop,
    a2600_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
			SOUND_TIA,
			&tia_interface
		}

    }

};


/* list of file extensions */
static const char *a2600_file_extensions[] =
{
	"bin",
	0       /* end of array */
};


/***************************************************************************

  Game driver

***************************************************************************/


ROM_START( a2600 )
	ROM_REGIONX( 0x10000, REGION_CPU1 ) /* 6502 memory */
ROM_END


struct GameDriver a2600_driver =
{
	__FILE__,
	0,
	"a2600",
	"Atari 2600 - VCS",
	"19??",
	"Atari",
	"Ben Bruscella, Dan Boris",
	0,
	&machine_driver,
	0,

    rom_a2600,
    a2600_load_rom,
    a2600_id_rom,
	a2600_file_extensions,
	1,      /* number of ROM slots */
	0,      /* number of floppy drives supported */
	0,      /* number of hard drives supported */
	0,      /* number of cassette drives supported */
	0, 0,
	0,
	0,      /* sound_prom */

	input_ports_a2600,

    0, 0, 0,
	ORIENTATION_DEFAULT,

	0, 0,
};
