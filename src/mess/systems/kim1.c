/******************************************************************************
	KIM-1

    MESS Driver

	Juergen Buchmueller, Oct 1999
******************************************************************************/

#include "driver.h"
#include "artwork.h"
#include "cpu/m6502/m6502.h"
#include "vidhrdw/generic.h"
//#include "mess/machine/kim1.h"
//#include "mess/vidhrdw/kim1.h"

#define LUNAR_LANDER	1
#define MUSICBOX		0
#define HYPER_TAPE		0

#define VERBOSE 1

#if VERBOSE
#define LOG(x)	if( errorlog ) fprintf x
#else
#define LOG(x)	/* x */
#endif

/******************************************************************************
	KIM-1 memory map

	range	  short 	description
	0000-03FF RAM
	1400-16FF ???
	1700-173F 6530-003	see 6530
	1740-177F 6530-002	see 6530
	1780-17BF RAM		internal 6530-003
	17C0-17FF RAM		internal 6530-002
	1800-1BFF ROM		internal 6530-003
	1C00-1FFF ROM		internal 6530-002

	6530
	offset	R/W short	purpose
	0		X	DRA 	Data register A
	1		X	DDRA	Data direction register A
	2		X	DRB 	Data register B
	3		X	DDRB	Data direction register B
	4		W	CNT1T	Count down from value, divide by 1, disable IRQ
	5		W	CNT8T	Count down from value, divide by 8, disable IRQ
	6		W	CNT64T	Count down from value, divide by 64, disable IRQ
			R	LATCH	Read current counter value, disable IRQ
	7		W	CNT1KT	Count down from value, divide by 1024, disable IRQ
			R	STATUS	Read counter statzs, bit 7 = 1 means counter overflow
	8		X	DRA 	Data register A
	9		X	DDRA	Data direction register A
	A		X	DRB 	Data register B
	B		X	DDRB	Data direction register B
	C		W	CNT1I	Count down from value, divide by 1, enable IRQ
	D		W	CNT8I	Count down from value, divide by 8, enable IRQ
	E		W	CNT64I	Count down from value, divide by 64, enable IRQ
			R	LATCH	Read current counter value, enable IRQ
	F		W	CNT1KI	Count down from value, divide by 1024, enable IRQ
            R   STATUS  Read counter statzs, bit 7 = 1 means counter overflow

	6530-002 (U2)
		DRA bit write				read
		---------------------------------------------
			0-6 segments A-G		key columns 1-7
			7	PTR 				KBD

		DRB bit write				read
		---------------------------------------------
			0	PTR 				-/-
			1-4 dec 1-3 key row 1-3
					4	RW3
					5-9 7-seg	1-6
	6530-003 (U3)
		DRA bit write				read
		---------------------------------------------
			0-7 bus PA0-7			bus PA0-7

		DRB bit write				read
		---------------------------------------------
			0-7 bus PB0-7			bus PB0-7

******************************************************************************/

typedef struct {
	UINT8	dria;			/* Data register A input */
	UINT8	droa;			/* Data register A output */
	UINT8	ddra;			/* Data direction register A; 1 bits = output */
	UINT8	drib;			/* Data register B input */
	UINT8	drob;			/* Data register B output */
	UINT8	ddrb;			/* Data direction register B; 1 bits = output */
    UINT8   irqen;          /* IRQ enabled ? */
	UINT8	state;			/* current timer state (bit 7) */
	double	clock;			/* 100000/1(,8,64,1024) */
	void	*timer; 		/* timer callback */
}	M6530;

static M6530 m6530[2];

static struct artwork *kim1_backdrop;

int m6530_003_r(int offset);
int m6530_002_r(int offset);
int kim1_mirror_r(int offset);

void m6530_003_w(int offset, int data);
void m6530_002_w(int offset, int data);
void kim1_mirror_w(int offset, int data);

INLINE void m6530_timer_cb(int chip);

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x1700, 0x173f, m6530_003_r },
	{ 0x1740, 0x177f, m6530_002_r },
	{ 0x1780, 0x17bf, MRA_RAM },
	{ 0x17c0, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1bff, MRA_ROM },
	{ 0x1c00, 0x1fff, MRA_ROM },
	{ 0x2000, 0xffff, kim1_mirror_r },
    {-1}
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x1700, 0x173f, m6530_003_w },
	{ 0x1740, 0x177f, m6530_002_w },
	{ 0x1780, 0x17bf, MWA_RAM },
	{ 0x17c0, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1bff, MWA_ROM },
	{ 0x1c00, 0x1fff, MWA_ROM },
	{ 0x2000, 0xffff, kim1_mirror_w },
    {-1}
};

INPUT_PORTS_START( kim1 )
	PORT_START			/* IN0 keys row 0 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "0.6: 0",        KEYCODE_0,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "0.5: 1",        KEYCODE_1,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "0.4: 2",        KEYCODE_2,      IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "0.3: 3",        KEYCODE_3,      IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "0.2: 4",        KEYCODE_4,      IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "0.1: 5",        KEYCODE_5,      IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "0.0: 6",        KEYCODE_6,      IP_JOY_NONE )
	PORT_START			/* IN1 keys row 1 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "1.6: 7",        KEYCODE_7,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "1.5: 8",        KEYCODE_8,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "1.4: 9",        KEYCODE_9,      IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "1.3: A",        KEYCODE_A,      IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "1.2: B",        KEYCODE_B,      IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "1.1: C",        KEYCODE_C,      IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "1.0: D",        KEYCODE_D,      IP_JOY_NONE )
	PORT_START			/* IN2 keys row 2 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "2.6: E",        KEYCODE_E,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "2.5: F",        KEYCODE_F,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "2.4: AD (F1)",  KEYCODE_F1,     IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "2.3: DA (F2)",  KEYCODE_F2,     IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "2.2: +  (CR)",  KEYCODE_ENTER,  IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "2.1: GO (F5)",  KEYCODE_F5,     IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "2.0: PC (F6)",  KEYCODE_F6,     IP_JOY_NONE )
	PORT_START			/* IN3 STEP and RESET keys, MODE switch */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "sw1: ST (F7)",  KEYCODE_F7,     IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "sw2: RS (F3)",  KEYCODE_F3,     IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "sw3: SS (NumLock)", KEYCODE_NUMLOCK, IP_JOY_NONE )
	PORT_DIPSETTING( 0x00, "single step")
	PORT_DIPSETTING( 0x10, "run")
	PORT_BIT (0x08, 0x00, IPT_UNUSED )
	PORT_BIT (0x04, 0x00, IPT_UNUSED )
	PORT_BIT (0x02, 0x00, IPT_UNUSED )
	PORT_BIT (0x01, 0x00, IPT_UNUSED )
INPUT_PORTS_END

void kim1_init_machine(void)
{
	UINT8 *RAM = Machine->memory_region[0];

	/* setup RAM IRQ vector */
    if( RAM[0x17fa] == 0x00 && RAM[0x17fb] == 0x00 )
	{
		RAM[0x17fa] = 0x00;
		RAM[0x17fb] = 0x1c;
	}
	/* setup RAM NMI vector */
    if( RAM[0x17fe] == 0x00 && RAM[0x17ff] == 0x00 )
	{
		RAM[0x17fe] = 0x00;
		RAM[0x17ff] = 0x1c;
    }

    /* reset the 6530 */
    memset(&m6530, 0, sizeof(m6530));

	m6530[0].dria = 0xff;
    m6530[0].drib = 0xff;
	m6530[0].clock = (double)1000000/1;
	m6530[0].timer = timer_pulse(TIME_IN_HZ(256 * m6530[0].clock / 256 / 256), 0, m6530_timer_cb);

    m6530[1].dria = 0xff;
	m6530[1].drib = 0xff;
	m6530[1].clock = (double)1000000/1;
	m6530[1].timer = timer_pulse(TIME_IN_HZ(256 * m6530[1].clock / 256 / 256), 1, m6530_timer_cb);

#if LUNAR_LANDER
/*
 * LUNAR LANDER
 *	 Jim Butterfield
 *
 * Description
 *		This program starts at 0200.  When started, you will find
 * yourself at 4500 feet and falling.  The thrust on your machine
 * is set to low; so you will pick up speed due to the force of
 * gravity.
 *	   You can look at your fuel at any time by pressing the
 * "F" button.  Your fuel (initially 800 pounds) will be shown
 * in the first four digits of the KIM display.
 *	   The last two digits of the KIM display always show
 * your rate of ascent of descent.	"A" restores altitude.
 *	   Set your thrust by pressing buttons 1 through 9.
 * Warning: button 0 turns you motor off, and it will not
 * reignite!  A thrust of 1, minimum, burns very little fuel;
 * but gravity will be pulling your craft down faster and
 * faster.	A thrust of 9, maximum, overcomes gravity
 * and reduces your rate of descent very sharply  A thrust of 5
 * exactly counterbalances gravity; you will continue to descend
 * (or ascend) at a constant rate.	If you run out of fuel,
 * your thrust controls will become inoperative.
 *
 * Suggestions for a safe flight:
 *	 [1] Conserve fuel at the beginning by pressing 1.	You
 *		 begin to pick up speed downwards.
 *	 [2] When your rate of descent gets up to the 90's, you're
 *		 falling fast enough.  Press 5 to steady the rate.
 *	 [3] When your altitude reaches about 1500 feet, you'll
 *		 need to slow down.  Press 9 and slow down fast.
 *	 [4] When your rate of descent has dropped to 15 or 20,
 *		 steady the craft by pressing 5 or 6.  Now you're on
 *		 your own.
 * MESS:
 *	 To execute enter 0 2 0 0 and GO (Enter) after startup.
 *
 */
	memcpy(&RAM[0x200], "\xA2\x0D\xBD\xCC\x02\x95\xD5\xCA\x10\xF8\xA2\x05\xA0\x01\xF8\x18", 16);
	memcpy(&RAM[0x210], "\xB5\xD5\x75\xD7\x95\xD5\xCA\x88\x10\xF6\xB5\xD8\x10\x02\xA9\x99", 16);
	memcpy(&RAM[0x220], "\x75\xD5\x95\xD5\xCA\x10\xE5\xA5\xD5\x10\x0D\xA9\x00\x85\xE2\xA2", 16);
	memcpy(&RAM[0x230], "\x02\x95\xD5\x95\xDB\xCA\x10\xF9\x38\xA5\xE0\xE5\xDD\x85\xE0\xA2", 16);
	memcpy(&RAM[0x240], "\x01\xB5\xDE\xE9\x00\x95\xDE\xCA\x10\xF7\xB0\x0C\xA9\x00\xA2\x03", 16);
	memcpy(&RAM[0x250], "\x95\xDD\xCA\x10\xFB\x20\xBD\x02\xA5\xDE\xA6\xDF\x09\xF0\xA4\xE1", 16);
	memcpy(&RAM[0x260], "\xF0\x20\xF0\x9C\xF0\xA4\xA2\xFE\xA0\x5A\x18\xA5\xD9\x69\x05\xA5", 16);
	memcpy(&RAM[0x270], "\xD8\x69\x00\xB0\x04\xA2\xAD\xA0\xDE\x98\xA4\xE2\xF0\x04\xA5\xD5", 16);
	memcpy(&RAM[0x280], "\xA6\xD6\x85\xFB\x86\xFA\xA5\xD9\xA6\xD8\x10\x05\x38\xA9\x00\xE5", 16);
	memcpy(&RAM[0x290], "\xD9\x85\xF9\xA9\x02\x85\xE3\xD8\x20\x1F\x1F\x20\x6A\x1F\xC9\x13", 16);
	memcpy(&RAM[0x2A0], "\xF0\xC0\xB0\x03\x20\xAD\x02\xC6\xE3\xD0\xED\xF0\xB7\xC9\x0A\x90", 16);
	memcpy(&RAM[0x2B0], "\x05\x49\x0F\x85\xE1\x60\xAA\xA5\xDD\xF0\xFA\x86\xDD\xA5\xDD\x38", 16);
	memcpy(&RAM[0x2C0], "\xF8\xE9\x05\x85\xDC\xA9\x00\xE9\x00\x85\xDB\x60\x45\x01\x00\x99", 16);
	memcpy(&RAM[0x2D0], "\x81\x00\x99\x97\x02\x08\x00\x00\x01\x01", 10);

	RAM[0x00f1] = 0x00; /* ????? */

    RAM[0x17f5] = 0x00; /* program start address: 0x0200 */
	RAM[0x17f6] = 0x02;
	RAM[0x17f7] = 0xda; /* program length: 0x00da */
	RAM[0x17f8] = 0x00;
	RAM[0x17f9] = 0x01; /* program ID: 1 */
#endif

#if MUSICBOX
#if 0
    memcpy(&RAM[0x000],"\xFB\x18\xFE\xFF\x44\x51\xE6\xE6\x66\x5A\x51\x4C\xC4\xC4\xC4\xD1", 16);
	memcpy(&RAM[0x010],"\xBD\xBD\xBD\x00\x44\xBD\x00\x44\x3D\x36\x33\x2D\xA8\x80\x80\x33", 16);
	memcpy(&RAM[0x020],"\x44\xB3\x80\x80\x44\x51\xC4\x80\x80\x5A\x51\xE6\x80\x80\xFA\xFE", 16);
	memcpy(&RAM[0x030],"\x00\xFB\x28\x5A\x5A\x51\x48\x5A\x48\xD1\x5A\x5A\x51\x48\xDA\xE0", 16);
	memcpy(&RAM[0x040],"\x5A\x5A\x51\x48\x44\x48\x51\x5A\x60\x79\x6C\x60\xDA\xDA\xFA\xFE", 16);
	memcpy(&RAM[0x050],"\xFF\x5A\x5A\x5A\x5A\x5A\x5A\x66\x72\x79\xE6\xE6\x80\x00\x56\x56", 16);
	memcpy(&RAM[0x060],"\x56\x56\x56\x56\x5A\x66\xF2\x80\x80\x4C\x4B\x4C\x4C\x4C\x4C\x56", 16);
	memcpy(&RAM[0x070],"\x5A\x56\x4C\x00\xCh\x44\x4C\x56\x5A\x5A\x56\x5A\x66\x56\x6A\x66", 16);
	memcpy(&RAM[0x080],"\xF2\x80\xFE\x00\x00\x72\x5A\xCC\x72\x5A\xCC\x72\x5A\xCC\x80\xB8", 16);
	memcpy(&RAM[0x090],"\x80\x4C\x56\x5A\x56\x5A\xE6\xF2\x80\xFA\xFF\x00", 12);
#else
	memcpy(&RAM[0x000],"\xFE\x00\x56\x52\xAD\xAF\xAD\xAF\xAD\xFC\x06\xAF\xFC\x02\xFE\xFF", 16);
	memcpy(&RAM[0x010],"\x2F\x29\x26\x24\x2F\x29\xAA\x32\xA9\xFC\x06\xAF\xFC\x02\xFE\x00", 16);
	memcpy(&RAM[0x020],"\x56\x52\xAD\xAF\x5D\xAF\xAD\xFC\x06\xAF\xFC\x02\xFE\xFF\x39\x40", 16);
	memcpy(&RAM[0x030],"\x44\x39\x2F\xAF\x29\x2F\x39\xA9\xB0\x80\xFE\x00\x56\x52\xAD\xAF", 16);
	memcpy(&RAM[0x040],"\xAD\xAF\x0D\xFC\x06\xAF\xFC\x02\xFE\xFF\x2F\x29\x26\x24\x2F\x29", 16);
	memcpy(&RAM[0x050],"\xAA\x32\xA9\xAF\xB0\x80\x2F\x29\x24\x2F\x29\xA4\x2F\x29\x2F\x24", 16);
	memcpy(&RAM[0x060],"\x2F\x29\xAA\x2F\x29\x2F\x2A\x2F\x29\xAQ\x32\xA9\xAF\x80\x80\xFA", 16);
	memcpy(&RAM[0x070],"\xFF\x00", 2);
#endif
    memcpy(&RAM[0x200],"\xA2\x05\xBD\x86\x02\x95\xE0\xCA\x10\xF8\xA9\xBF\x8D\x43\x17\xA0", 16);
	memcpy(&RAM[0x210],"\x00\xB1\xE4\xE6\xE4\xC9\xFA\xD0\x04\x00\xEA\xF0\xED\x90\x0B\xE9", 16);
	memcpy(&RAM[0x220],"\xFB\xAA\xB1\xE4\xE6\xE4\x95\xE0\xB0\xE0\xA6\xE0\x86\xE7\xA6\xE1", 16);
	memcpy(&RAM[0x230],"\xA8\x30\x02\xA2\x01\x86\xE6\x29\x7F\x85\xE9\xF0\x02\x85\xEA\xA5", 16);
	memcpy(&RAM[0x240],"\xE9\x25\xE3\xF0\x04\xE6\xEA\xC6\xE9\xA6\xE9\xA9\xA7\x20\x5D\x02", 16);
	memcpy(&RAM[0x250],"\x30\xB8\xA6\xEA\xA9\x27\x20\x5D\x02\x30\xAF\x10\xE2\xA4\xE2\x84", 16);
	memcpy(&RAM[0x260],"\xEB\x86\xEC\xE0\x00\xD0\x08\xA6\xEC\xC6\xEB\xD0\xF6\xF0\x16\x8D", 16);
	memcpy(&RAM[0x270],"\x42\x17\xCA\xC6\xE8\xD0\xEC\xC6\xE7\xD0\xE8\xA4\xE0\x84\xE7\xC6", 16);
	memcpy(&RAM[0x280],"\xE6\xD0\xE0\xA9\xFF\x60\x30\x02\x01\xFF\x00\x00", 12);
#endif

#if HYPER_TAPE
    memcpy(&RAM[0x100],"\xA9\xAD\x8D\xEC\x17\x20\x32\x19\xA9\x27\x85\xF5\xA9\xBF\x8D\x43", 16);
	memcpy(&RAM[0x110],"\x17\xA2\x64\xA9\x16\x20\x61\x01\xA9\x2A\x20\x88\x01\xAD\xF9\x17", 16);
	memcpy(&RAM[0x120],"\x20\x70\x01\xAD\xF5\x17\x20\x6D\x01\xAD\xF6\x17\x20\x6D\x01\x20", 16);
	memcpy(&RAM[0x130],"\xEC\x17\x20\x6D\x01\x20\xEA\x19\xAD\xED\x17\xCD\xF7\x17\xAD\xEE", 16);
	memcpy(&RAM[0x140],"\x17\xED\xF8\x17\x90\xE9\xA9\x2F\x20\x88\x01\xAD\xE7\x17\x20\x70", 16);
	memcpy(&RAM[0x150],"\x01\xAD\xE8\x17\x20\x70\x01\xA2\x02\xA9\x04\x20\x61\x01\x4C\x5C", 16);
	memcpy(&RAM[0x160],"\x18\x86\xF1\x48\x20\x88\x01\x68\xC6\xF1\xD0\xF7\x60\x20\x4C\x19", 16);
	memcpy(&RAM[0x170],"\x48\x4A\x4A\x4A\x4A\x20\x7D\x01\x68\x20\x7D\x01\x60\x29\x0F\xC9", 16);
	memcpy(&RAM[0x180],"\x0A\x18\x30\x02\x69\x07\x69\x30\xA0\x07\x84\xF2\xA0\x02\x84\xF3", 16);
	memcpy(&RAM[0x190],"\xBE\xBE\x01\x48\x2C\x47\x17\x10\xFB\xB9\xBF\x01\x8D\x44\x17\xA5", 16);
	memcpy(&RAM[0x1A0],"\xF5\x49\x80\x8D\x42\x17\x85\xF5\xCA\xD0\xE9\x68\xC6\xF3\xF0\x05", 16);
	memcpy(&RAM[0x1B0],"\x30\x07\x4A\x90\xDB\xA0\x00\xF0\xD7\xC6\xF2\x10\xCF\x60\x02\xC3", 16);
	memcpy(&RAM[0x1C0],"\x03\x7E", 2);
#endif
}

int id_rom(const char *name, const char * gamename)
{
	return 0;
}

void kim1_init_colors(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	char backdrop_name[1024];
	int i, nextfree;

	/* try to load a backdrop for the machine */
	sprintf(backdrop_name, "%s.png", Machine->gamedrv->name);

    /* initialize 16 colors with shades of red */
    for( i = 0; i < 16; i++ )
    {
        palette[3*i+0] = (i+1) * (i+1) -1;
		palette[3*i+1] = (i+1) * (i+1) / 4;
        palette[3*i+2] = 0;

        colortable[2*i+0] = 1;
        colortable[2*i+1] = i;
    }

    palette[3*16+0] = 0;
    palette[3*16+1] = 0;
    palette[3*16+2] = 0;

    palette[3*17+0] = 30;
    palette[3*17+1] = 30;
    palette[3*17+2] = 30;

	palette[3*18+0] = 90;
	palette[3*18+1] = 90;
	palette[3*18+2] = 90;

	palette[3*19+0] = 50;
	palette[3*19+1] = 50;
	palette[3*19+2] = 50;

    palette[3*20+0] =255;
    palette[3*20+1] =255;
    palette[3*20+2] =255;

    colortable[2*16+0*4+0] = 17;
    colortable[2*16+0*4+1] = 18;
    colortable[2*16+0*4+2] = 19;
    colortable[2*16+0*4+3] = 20;

    colortable[2*16+1*4+0] = 17;
	colortable[2*16+1*4+1] = 17;
	colortable[2*16+1*4+2] = 19;
    colortable[2*16+1*4+3] = 15;

	nextfree = 21;

	if( (kim1_backdrop = artwork_load(backdrop_name, nextfree, Machine->drv->total_colors - nextfree)) != NULL )
	{
		LOG((errorlog,"backdrop %s successfully loaded\n", backdrop_name));
		memcpy(&palette[nextfree*3], kim1_backdrop->orig_palette, kim1_backdrop->num_pens_used * 3 * sizeof(unsigned char));
    }
	else
	{
		LOG((errorlog,"no backdrop loaded\n"));
	}
}

int kim1_vh_start(void)
{
	videoram_size = 6 * 2 + 24;
	videoram = malloc(videoram_size);
	if( !videoram )
		return 1;
	if( kim1_backdrop )
        backdrop_refresh(kim1_backdrop);
    if( generic_vh_start() != 0 )
        return 1;

    return 0;
}

void kim1_vh_stop(void)
{
	if( kim1_backdrop )
		artwork_free(kim1_backdrop);
	kim1_backdrop = NULL;
    if( videoram )
		free(videoram);
	videoram = NULL;
	generic_vh_stop();
}

void kim1_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int x, y;

    if( full_refresh )
	{
        osd_mark_dirty(0, 0, bitmap->width, bitmap->height, 0);
		memset(videoram, 0x0f, videoram_size);
    }
	if( kim1_backdrop )
		copybitmap(bitmap, kim1_backdrop->artwork, 0, 0, 0, 0, NULL, TRANSPARENCY_NONE, 0);

    for( x = 0; x < 6; x++ )
	{
		int sy = 408;
		int sx = Machine->drv->screen_width - 212 + x * 30 + ((x >= 4) ? 6 : 0);
		drawgfx(bitmap, Machine->gfx[0],
				videoram[2*x+0], videoram[2*x+1],
				0, 0, sx, sy, &Machine->drv->visible_area, TRANSPARENCY_PEN, 0);
		osd_mark_dirty(sx,sy,sx+15,sy+31,1);
    }

    for( y = 0; y < 6; y++ )
	{
		int sy = 516 + y * 36;

        for( x = 0; x < 4; x++ )
		{
            static int layout[6][4] =
            {
				{ 22, 19, 21, 23},
				{ 16, 17, 20, 18},
				{ 12, 13, 14, 15},
				{  8,  9, 10, 11},
				{  4,  5,  6,  7},
				{  0,  1,  2,  3}
            };
			int sx = Machine->drv->screen_width - 182 + x * 37;
			int color, code = layout[y][x];

			color = ( readinputport(code/7) & (0x40 >> (code%7)) ) ? 0 : 1;

			videoram[6*2 + code] = color;
			drawgfx(bitmap, Machine->gfx[1],
				layout[y][x], color,
				0, 0, sx, sy, &Machine->drv->visible_area,
				TRANSPARENCY_NONE, 0);
			osd_mark_dirty(sx,sy,sx+23,sy+17,1);
        }
	}

}

int kim1_interrupt(void)
{
	int i;

	for( i = 0; i < 6; i++ )
	{
		if( videoram[i*2+1] > 0 )
			videoram[i*2+1] -= 1;
	}

    return ignore_interrupt();
}

INLINE int m6530_r(int chip, int offset)
{
	int data = 0xff;
    switch( offset )
    {
	case 0: case 8: /* Data register A */
		if( chip == 1 )
		{
			int which = ((m6530[1].drob & m6530[1].ddrb) >> 1) & 0x0f;
			switch( which )
			{
			case 0: /* key row 1 */
				m6530[1].dria = input_port_0_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'0',
						(m6530[1].dria&0x20)?'.':'1',
						(m6530[1].dria&0x10)?'.':'2',
						(m6530[1].dria&0x08)?'.':'3',
						(m6530[1].dria&0x04)?'.':'4',
						(m6530[1].dria&0x02)?'.':'5',
						(m6530[1].dria&0x01)?'.':'6'));
				break;
			case 1: /* key row 2 */
				m6530[1].dria = input_port_1_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'7',
						(m6530[1].dria&0x20)?'.':'8',
						(m6530[1].dria&0x10)?'.':'9',
						(m6530[1].dria&0x08)?'.':'A',
						(m6530[1].dria&0x04)?'.':'B',
						(m6530[1].dria&0x02)?'.':'C',
						(m6530[1].dria&0x01)?'.':'D'));
				break;
			case 2: /* key row 3 */
				m6530[1].dria = input_port_2_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'E',
						(m6530[1].dria&0x20)?'.':'F',
						(m6530[1].dria&0x10)?'.':'a',
						(m6530[1].dria&0x08)?'.':'d',
						(m6530[1].dria&0x04)?'.':'+',
						(m6530[1].dria&0x02)?'.':'g',
						(m6530[1].dria&0x01)?'.':'p'));
				break;
			case 3: 	/* WR4?? */
				m6530[1].dria = 0xff;
                break;
			default:
                m6530[1].dria = 0xff;
				LOG((errorlog, "read DRA(%d) $ff\n", which));
			}
        }
        data = (m6530[chip].dria & ~m6530[chip].ddra) | (m6530[chip].droa & m6530[chip].ddra);
		LOG((errorlog, "m6530(%d) DRA   read : $%02x\n", chip, data));
		break;
	case 1: case 9: 	/* Data direction register A */
		data = m6530[chip].ddra;
		LOG((errorlog, "m6530(%d) DDRA  read : $%02x\n", chip, data));
		break;
	case 2: case 10:	/* Data register B */
		data = (m6530[chip].drib & ~m6530[chip].ddrb) | (m6530[chip].drob & m6530[chip].ddrb);
		LOG((errorlog, "m6530(%d) DRB   read : $%02x\n", chip, data));
		break;
	case 3: case 11:	/* Data direction register B */
		data = m6530[chip].ddrb;
		LOG((errorlog, "m6530(%d) DDRB  read : $%02x\n", chip, data));
		break;
	case 4: case 12:	/* Timer count read (not supported?) */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 5: case 13:	/* Timer count read (not supported?) */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
        m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 6: case 14:	/* Timer count read */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
        m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 7: case 15:	/* Timer status read */
		data = m6530[chip].state;
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) STAT  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
    }
	return data;
}

int m6530_003_r(int offset) { return m6530_r(0,offset); }
int m6530_002_r(int offset) { return m6530_r(1,offset); }

int kim1_mirror_r(int offset)
{
	return cpu_readmem16(offset & 0x1fff);
}

INLINE void m6530_timer_cb(int chip)
{
	LOG((errorlog, "m6530(%d) timer expired\n", chip));
	m6530[chip].state |= 0x80;
	if( m6530[chip].irqen ) /* with IRQ? */
		cpu_set_irq_line(0, 0, ASSERT_LINE);
}

void m6530_w(int chip, int offset, int data)
{
	switch( offset )
    {
	case 0: case 8: 	/* Data register A */
		LOG((errorlog, "m6530(%d) DRA  write: $%02x\n", chip, data));
		m6530[chip].droa = data;
		if( chip == 1 )
		{
			int which = (m6530[1].drob & m6530[1].ddrb) >> 1;
			switch( which )
			{
			case 0: 	/* key row 1 */
				break;
			case 1: 	/* key row 2 */
				break;
			case 2: 	/* key row 3 */
				break;
			case 3: 	/* WR4?? */
				break;
			/* write LED # 1-6 */
			case 4: case 5: case 6: case 7: case 8: case 9:
				if( data & 0x80 )
				{
					LOG((errorlog, "write 7seg(%d): %c%c%c%c%c%c%c\n",
							which+1-4,
							(data&0x01)?'a':'.',
							(data&0x02)?'b':'.',
							(data&0x04)?'c':'.',
							(data&0x08)?'d':'.',
							(data&0x10)?'e':'.',
							(data&0x20)?'f':'.',
							(data&0x40)?'g':'.'));
					videoram[(which-4)*2+0] = data & 0x7f;
					videoram[(which-4)*2+1] = 15;
				}
			}
		}
		break;
	case 1: case 9: 	/* Data direction register A */
		LOG((errorlog, "m6530(%d) DDRA  write: $%02x\n", chip, data));
		m6530[chip].ddra = data;
		break;
	case 2: case 10:	/* Data register B */
		LOG((errorlog, "m6530(%d) DRB   write: $%02x\n", chip, data));
		m6530[chip].drob = data;
        if( chip == 1 )
        {
			int which = m6530[1].ddrb & m6530[1].drob;
			if( (which & 0x3f) == 0x27 )
			{
				/* This is the cassette output port */
				LOG((errorlog, "write cassette port: %d\n", (which & 0x80) ? 1 : 0));
				DAC_signed_data_w(0, (which & 0x80) ? 255 : 0);
			}
		}
        break;
	case 3: case 11:	/* Data direction register B */
		LOG((errorlog, "m6530(%d) DDRB  write: $%02x\n", chip, data));
		m6530[chip].ddrb = data;
		break;
	case 4: case 12:	/* Timer 1 start */
		LOG((errorlog, "m6530(%d) TMR1  write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 1;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
		break;
	case 5: case 13:	/* Timer 8 start */
		LOG((errorlog, "m6530(%d) TMR8  write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 8;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
	case 6: case 14:	/* Timer 64 start */
		LOG((errorlog, "m6530(%d) TMR64 write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 64;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
	case 7: case 15:	/* Timer 1024 start */
		LOG((errorlog, "m6530(%d) TMR1K write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 1024;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
    }
}

void m6530_003_w(int offset, int data) { m6530_w(0,offset,data); }
void m6530_002_w(int offset, int data) { m6530_w(1,offset,data); }

void kim1_mirror_w(int offset, int data)
{
	cpu_writemem16(offset & 0x1fff, data);
}

static struct GfxLayout led_layout =
{
	18, 24, 	/* 16 x 24 LED 7segment displays */
	128,		/* 128 codes */
	1,			/* 1 bit per pixel */
	{ 0 },		/* no bitplanes */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8, 9,10,11,12,13,14,15,
	 16,17 },
	{ 0*24, 1*24, 2*24, 3*24,
	  4*24, 5*24, 6*24, 7*24,
	  8*24, 9*24,10*24,11*24,
	 12*24,13*24,14*24,15*24,
	 16*24,17*24,18*24,19*24,
	 20*24,21*24,22*24,23*24,
	 24*24,25*24,26*24,27*24,
	 28*24,29*24,30*24,31*24 },
	24 * 24,	/* every LED code takes 32 times 18 (aligned 24) bit words */
};

static struct GfxLayout key_layout =
{
	24, 18, 	/* 24 * 18 keyboard icons */
	24, 		/* 24  codes */
	2,			/* 2 bit per pixel */
	{ 0, 1 },	/* two bitplanes */
	{ 0*2, 1*2, 2*2, 3*2, 4*2, 5*2, 6*2, 7*2,
	  8*2, 9*2,10*2,11*2,12*2,13*2,14*2,15*2,
	 16*2,17*2,18*2,19*2,20*2,21*2,22*2,23*2 },
	{ 0*24*2, 1*24*2, 2*24*2, 3*24*2, 4*24*2, 5*24*2, 6*24*2, 7*24*2,
	  8*24*2, 9*24*2,10*24*2,11*24*2,12*24*2,13*24*2,14*24*2,15*24*2,
	 16*24*2,17*24*2 },
	18 * 24 * 2,	/* every icon takes 18 rows of 24 * 2 bits */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0, &led_layout, 0, 16 },
	{ 2, 0, &key_layout, 16*2, 2 },
	{ -1 } /* end of array */
};

static struct DACinterface dac_interface =
{
	1,			/* number of DACs */
	{ 100 } 	/* volume */
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1000000,	/* 1 MHz */
			0,
			readmem,writemem,0,0,
			kim1_interrupt, 1
        }
	},
	/* frames per second, VBL duration */
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,				/* single CPU */
	kim1_init_machine,
	NULL,			/* stop machine */

	/* video hardware (well, actually there was no video ;) */
	600, 768, { 0, 600 - 1, 0, 768 - 1},
	gfxdecodeinfo,
	256*3,
	256,
	kim1_init_colors,		/* convert color prom */

	VIDEO_TYPE_RASTER,		/* video flags */
	0,						/* obsolete */
	kim1_vh_start,
	kim1_vh_stop,
	kim1_vh_screenrefresh,

	/* sound hardware */
	0,NULL,NULL,NULL,
	{
        {
			SOUND_DAC,
			&dac_interface
		}
    }
};

ROM_START(kim1)
	ROM_REGION(0x10000)
		ROM_LOAD("6530-003.bin",    0x1800, 0x0400, 0xa2a56502)
		ROM_LOAD("6530-002.bin",    0x1c00, 0x0400, 0x2b08e923)
	ROM_REGION(128 * 24 * 3)
		/* space filled with 7segement graphics by kim1_rom_decode */
	ROM_REGION( 24 * 18 * 3 * 2)
		/* space filled with key icons by kim1_rom_decode */
ROM_END

void kim1_rom_decode(void)
{
	UINT8 *dst;
    int x, y, i;

	static char *seg7 =
		"....aaaaaaaaaaaaa." \
		"...f.aaaaaaaaaaa.b" \
		"...ff.aaaaaaaaa.bb" \
		"...fff.........bbb" \
		"...fff.........bbb" \
		"...fff.........bbb" \
		"..fff.........bbb." \
		"..fff.........bbb." \
		"..fff.........bbb." \
		"..ff...........bb." \
		"..f.ggggggggggg.b." \
		"..gggggggggggggg.." \
		".e.ggggggggggg.c.." \
		".ee...........cc.." \
		".eee.........ccc.." \
		".eee.........ccc.." \
		".eee.........ccc.." \
		"eee.........ccc..." \
		"eee.........ccc..." \
		"eee.........ccc..." \
		"ee.ddddddddd.cc..." \
		"e.ddddddddddd.c..." \
		".ddddddddddddd...." \
		"..................";


	static char *keys[24] = {
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaccccaaaaaaa" \
		"aaaaaaaccaaaccaccaaaaaaa" \
		"aaaaaaaccaaccaaccaaaaaaa" \
		"aaaaaaaccaccaaaccaaaaaaa" \
		"aaaaaaaccccaaaaccaaaaaaa" \
		"aaaaaaacccaaaaaccaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaacccaaaaaaaaaaa" \
		"aaaaaaaaaccccaaaaaaaaaaa" \
		"aaaaaaaaccaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccaaaaaaaaaaaa" \
		"aaaaaaacccaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaccccccaaaaaaaa" \
		"aaaaaaaaaaccccccaaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaacaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaacccaaaaaaaaa" \
		"aaaaaaaaaaaccccaaaaaaaaa" \
		"aaaaaaaaaaccaccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaccaaaccaaaaaaaaa" \
		"aaaaaaaccaaaaccaaaaaaaaa" \
		"aaaaaacccccccccccaaaaaaa" \
		"aaaaaacccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaccccaaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccccaaaccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaacccaaaaaaaa" \
		"aaaaaaaaaaaacccaaaaaaaaa" \
		"aaaaaaaaaaacccaaaaaaaaaa" \
		"aaaaaaaaaacccaaaaaaaaaaa" \
		"aaaaaaaaacccaaaaaaaaaaaa" \
		"aaaaaaaacccaaaaaaaaaaaaa" \
		"aaaaaaacccaaaaaaaaaaaaaa" \
		"aaaaaacccaaaaaaaaaaaaaaa" \
		"aaaaaacccaaaaaaaaaaaaaaa" \
		"aaaaaaccaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaacccaaacccaaaaaaa" \
		"aaaaaaaacccccccccaaaaaaa" \
		"aaaaaaaaaaccccaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaacccaaaaaacccaaaaaa" \
		"aaaaaaccaaaaaaaaccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaacccccccaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaaccaaaacccaaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccaaaacccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaccaaaaacccccaaaaaa" \
		"aaaaaaccaaaaaccccccaaaaa" \
		"aaaaaccccaaaaccaacccaaaa" \
		"aaaaaccccaaaaccaaaccaaaa" \
		"aaaaccccccaaaccaaaaccaaa" \
		"aaaaccaaccaaaccaaaaccaaa" \
		"aaaaccccccaaaccaaaaccaaa" \
		"aaaccccccccaaccaaaaccaaa" \
		"aaacccaacccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaccaaaa" \
		"aacccaaaacccaccaacccaaaa" \
		"aaccaaaaaaccaccccccaaaaa" \
		"aaccaaaaaaccacccccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaacccccaaaaaaaccaaaaaaa" \
		"aaaccccccaaaaaaccaaaaaaa" \
		"aaaccaacccaaaaccccaaaaaa" \
		"aaaccaaaccaaaaccccaaaaaa" \
		"aaaccaaaaccaaccccccaaaaa" \
		"aaaccaaaaccaaccaaccaaaaa" \
		"aaaccaaaaccaaccccccaaaaa" \
		"aaaccaaaaccaccccccccaaaa" \
		"aaaccaaaaccacccaacccaaaa" \
		"aaaccaaaaccaccaaaaccaaaa" \
		"aaaccaaaccaaccaaaaccaaaa" \
		"aaaccaacccacccaaaacccaaa" \
		"aaaccccccaaccaaaaaaccaaa" \
		"aaacccccaaaccaaaaaaccaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaccccccccccccccaaaaa" \
		"aaaaaccccccccccccccaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaccccaaaaaaccccaaaaa" \
		"aaaaccccccaaaaccccccaaaa" \
		"aaacccaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaaaacccaaaacccaa" \
		"aaaccaaaaaaaccaaaaaaccaa" \
		"aaaccacccccaccaaaaaaccaa" \
		"aaaccacccccaccaaaaaaccaa" \
		"aaaccaaaaccaccaaaaaaccaa" \
		"aaaccaaaaccacccaaaacccaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaacccaacccaacccaacccaaa" \
		"aaaacccccccaaaccccccaaaa" \
		"aaaaacccaccaaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaccccccaaaaaaccccaaaaa" \
		"aaacccccccaaaaccccccaaaa" \
		"aaaccaaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccacccaaaaaaaaa" \
		"aaaccaaacccaccaaaaaaaaaa" \
		"aaacccccccaaccaaaaaaaaaa" \
		"aaaccccccaaaccaaaaaaaaaa" \
		"aaaccaaaaaaaccaaaaaaaaaa" \
		"aaaccaaaaaaacccaaaaaaaaa" \
		"aaaccaaaaaaaaccaaaaccaaa" \
		"aaaccaaaaaaaacccaacccaaa" \
		"aaaccaaaaaaaaaccccccaaaa" \
		"aaaccaaaaaaaaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaccccaaaaccccccccaaa" \
		"aaaaccccccaaaccccccccaaa" \
		"aaacccaacccaaaaaccaaaaaa" \
		"aaaccaaaaccaaaaaccaaaaaa" \
		"aaaccaaaaaaaaaaaccaaaaaa" \
		"aaacccaaaaaaaaaaccaaaaaa" \
		"aaaacccccaaaaaaaccaaaaaa" \
		"aaaaacccccaaaaaaccaaaaaa" \
		"aaaaaaaacccaaaaaccaaaaaa" \
		"aaaaaaaaaccaaaaaccaaaaaa" \
		"aaaccaaaaccaaaaaccaaaaaa" \
		"aaacccaacccaaaaaccaaaaaa" \
		"aaaaccccccaaaaaaccaaaaaa" \
		"aaaaaccccaaaaaaaccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaccccccaaaaaaccccaaaaa" \
		"aaacccccccaaaaccccccaaaa" \
		"aaaccaaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaaaaaaa" \
		"aaaccaaacccaacccaaaaaaaa" \
		"aaacccccccaaaacccccaaaaa" \
		"aaaccccccaaaaaacccccaaaa" \
		"aaaccaacccaaaaaaaacccaaa" \
		"aaaccaaacccaaaaaaaaccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaacccacccaacccaaa" \
		"aaaccaaaaaccaaccccccaaaa" \
		"aaaccaaaaaccaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"........................" \
		"........................" \
		".bbbbbbbbbbbbbbbbbbbbbb." \
		".baaaaaaaaaaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".bbbbbbbbbbbbbbbbbbbbbb." \
		"........................" };

    dst = Machine->memory_region[1];
	memset(dst, 0, 128 * 24 * 24 / 8);
    for( i = 0; i < 128; i++ )
    {
		for( y = 0; y < 24; y++ )
		{
			for( x = 0; x < 18; x++ )
			{
				switch( seg7[y*18+x] )
                {
					case 'a': if( i &  1 ) *dst |= 0x80 >> (x & 7); break;
					case 'b': if( i &  2 ) *dst |= 0x80 >> (x & 7); break;
					case 'c': if( i &  4 ) *dst |= 0x80 >> (x & 7); break;
					case 'd': if( i &  8 ) *dst |= 0x80 >> (x & 7); break;
					case 'e': if( i & 16 ) *dst |= 0x80 >> (x & 7); break;
					case 'f': if( i & 32 ) *dst |= 0x80 >> (x & 7); break;
					case 'g': if( i & 64 ) *dst |= 0x80 >> (x & 7); break;
                }
				if( (x & 7) == 7 ) dst++;
            }
			dst++;
		}
	}

	dst = Machine->memory_region[2];
	memset(dst, 0, 24 * 18 * 24 / 8);
	for( i = 0; i < 24; i++ )
    {
		for( y = 0; y < 18; y++ )
		{
			for( x = 0; x < 24; x++ )
			{
				switch( keys[i][y*24+x] )
				{
					case 'a': *dst |= 0x80 >> ((x & 3) * 2); break;
					case 'b': *dst |= 0x40 >> ((x & 3) * 2); break;
					case 'c': *dst |= 0xc0 >> ((x & 3) * 2); break;
				}
				if( (x & 3) == 3 ) dst++;
            }
		}
    }
}

struct GameDriver kim1_driver =
{
	__FILE__,
	0,
	"kim1",
	"KIM-1",
	"1975",
	"Commodore/MOS",
	"Juergen Buchmueller",
	0,
	&machine_driver,
	0,

	rom_kim1,	/* ROM_LOAD structures */
	0,
	id_rom,
	0,	/*  default file extensions */
	1,	/* number of ROM slots */
	4,	/* number of floppy drives supported */
	0,	/* number of hard drives supported */
	0,	/* number of cassette drives supported */
	kim1_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	input_ports_kim1,

	0, NULL, NULL,
	ORIENTATION_DEFAULT,

	0, 0,
};

