/***************************************************************************

Sega Genesis/MegaDrive Memory Map (preliminary)

Main CPU (68k)

$000000 - $3fffff : Cartridge ROM
$400000 - $9fffff : Reserved by Sega (unused?)
$a00000 - $a0ffff : shared RAM w/Z80
$a10000 - $a10fff : IO
$a11000 - $a11fff : Controls
	$a11000 : memory mode (16-bit)
	$a11100 : Z80 bus request (16)
	$a11200 : Z80 reset (16)
$c00000 - $dfffff : Video Display Processor (VDP)
	$c00000 : data (16-bit, mirrored)
	$c00004 : control (16, m)
	$c00008 : hv counter (16, m)
	$c00011 : psg 76489 (8)
$ff0000 - $ffffff : RAM

Interrupts:
	level 6 - vertical
	level 4 - horizontal
	level 2 - external

Sound CPU (Z80)

$0000 - $1fff : RAM
$2000 - $3fff : Reserved (RAM?)
$4001 - $4003 : ym2612 registers
$6000         : bank register
$7f11         : psg 76489
$8000 - $ffff : shared RAM w/68k



***************************************************************************/
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#include "driver.h"
#include "vidhrdw/generic.h"

#include "mess/machine/genesis.h"
#include "mess/vidhrdw/genesis.h"
#include "sound/2612intf.h"

#define ARMLong(x) (((x << 24) | (((unsigned long) x) >> 24) | (( x & 0x0000ff00) << 8) | (( x & 0x00ff0000) >> 8)))

extern int data_width;

int deb = 0;


unsigned int	z80_68000_latch			= 0;
unsigned int	z80_latch_bitcount		= 0;

unsigned char cartridge_ram[0x1000]; /* any cartridge RAM */

void genesis_videoram1_w(int offset,int data);

/* machine/genesis.c */
void genesis_init_machine (void);
int genesis_interrupt (void);
void genesis_ram_w (int offset,int data);
int genesis_ram_r (int offset);


void genesis_background_w(int offset,int data);
void genesis_vh_convert_color_prom(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom);

int genesis_s_interrupt(void);

void YM2612_68000_w(int offset, int data);
int YM2612_68000_r(int offset);


extern int z80running;

#if LSB_FIRST
	#define BYTE_XOR(a) ((a) ^ 1)

#else
	#define BYTE_XOR(a) (a)

#endif


int genesis_vdp_76489_r(int offset)
{
  return 0;
}
void genesis_vdp_76489_w(int offset, int data)
{
  SN76496_0_w(0, data);
}

void genesis_ramlatch_w(int offset, int data) /* note value will be meaningless unless all bits are correctly set in */
{
  	if (offset !=0 ) return;
	if (errorlog && !z80running) fprintf(errorlog, "undead Z80 latch write!\n");
  //	cpu_halt(0,0);
	if (z80_latch_bitcount == 0) z80_68000_latch = 0;
/*	if (errorlog) fprintf(errorlog, "latch update\n");*/
  	z80_68000_latch = z80_68000_latch | ((( ((unsigned char)data) & 0x01) << (15+z80_latch_bitcount)));
 	if (errorlog) fprintf(errorlog, "value %x written to latch\n", data);
	z80_latch_bitcount++;
	if (z80_latch_bitcount == 9)
	{
	   //	cpu_halt(0,1);
		z80_latch_bitcount = 0;
		if (errorlog) fprintf (errorlog, "latch set, value %x\n", z80_68000_latch);
	}
}
void genesis_s_68000_ram_w (int offset, int data)
{
	unsigned int address = (z80_68000_latch) + (offset & 0x7fff);
	if (errorlog && !z80running) fprintf(errorlog, "undead Z80->68000 write!\n");
	if (z80_latch_bitcount != 0 && errorlog) fprintf(errorlog, "writing whilst latch being set!\n");

  	if (address > 0xff0000) genesis_sharedram[BYTE_XOR(offset)] = data;
	if (errorlog) fprintf (errorlog, "z80 poke to address %x\n", address);
}
int genesis_s_68000_ram_r (int offset)
{
	int address = (z80_68000_latch) + (offset & 0x7fff);

if (errorlog && !z80running) fprintf(errorlog, "undead Z80->68000 read!\n");

	if (z80_latch_bitcount != 0 && errorlog) fprintf(errorlog, "reading whilst latch being set!\n");

	if (errorlog) fprintf (errorlog, "z80 read from address %x\n", address);

	/* Read the data out of the 68k ROM */
	if (address < 0x400000) return Machine->memory_region[0][BYTE_XOR(address)];
	/* else read the data out of the 68k RAM */
 	else if (address > 0xff0000) return genesis_sharedram[BYTE_XOR(offset)];

	return -1;
}

void genesis_soundram_w (int offset,int data)
{
	if (z80running && errorlog) fprintf(errorlog, "Z80 written whilst running!\n");
	if (errorlog) fprintf(errorlog,"68000->z80 sound write, %x to %x\n", data, offset);

	if (LOWER_BYTE_ACCESS(data)) genesis_soundram[offset+1] = data & 0xff;
	if (UPPER_BYTE_ACCESS(data)) genesis_soundram[offset] = (data >> 8) & 0xff;
}

int genesis_soundram_r (int offset)
{
	if (z80running && errorlog) fprintf(errorlog, "Z80 read whilst running!\n");
	if (errorlog) fprintf(errorlog, "soundram_r returning %x\n",(genesis_soundram[offset] << 8) + genesis_soundram[offset+1]);
	return (genesis_soundram[offset] << 8) + genesis_soundram[offset+1];

}

void genesis_sharedram_w (int offset,int data)
{
   	COMBINE_WORD_MEM(&genesis_sharedram[offset], data);
}

int genesis_sharedram_r (int offset)
{
	return READ_WORD(&genesis_sharedram[offset]);
}






static struct MemoryReadAddress genesis_readmem[] =
{
	{ 0x000000, 0x3fffff, MRA_ROM },
	{ 0xff0000, 0xffffff, MRA_BANK2}, /* RAM */
	{ 0xc00014, 0xfeffff, MRA_NOP },

   	{ 0xc00000, 0xc00003, genesis_vdp_data_r },
	{ 0xc00004, 0xc00007, genesis_vdp_ctrl_r },
	{ 0xc00008, 0xc0000b, genesis_vdp_hv_r },
	{ 0xc00010, 0xc00013, MRA_NOP /*would be genesis_vdp_76489_r*/ },
	{ 0xa11204, 0xbfffff, MRA_NOP },

	{ 0xa11000, 0xa11203, genesis_ctrl_r },
	{ 0xa10000, 0xa1001f, genesis_io_r },
	{ 0xa00000, 0xa01fff, genesis_soundram_r },
  	{ 0xa04000, 0xa04003, YM2612_68000_r },



/*	{ 0x200000, 0x200fff, cartridge_ram_r},*/
	{ -1 }  /* end of table */
};


static struct MemoryWriteAddress genesis_writemem[] =
{
	{ 0xff0000, 0xffffff, MWA_BANK2, /*genesis_sharedram_w*/ },
   	{ 0xd00000, 0xd03fff, genesis_videoram1_w, &videoram, &videoram_size }, /*this is just a fake */
	{ 0xc00014, 0xcfffff, MWA_NOP },
	{ 0xc00000, 0xc00003, genesis_vdp_data_w },
	{ 0xc00004, 0xc00007, genesis_vdp_ctrl_w },
	{ 0xc00008, 0xc0000b, genesis_vdp_hv_w },
   	{ 0xc00010, 0xc00013, genesis_vdp_76489_w },
	{ 0xa11204, 0xbfffff, MWA_NOP },
	{ 0xa11000, 0xa11203, genesis_ctrl_w },
	{ 0xa10000, 0xa1001f, genesis_io_w},
	{ 0xa07f10, 0xa07f13, genesis_vdp_76489_w },
	{ 0xa06000, 0xa06003, genesis_ramlatch_w },
	{ 0xa00000, 0xa01fff, genesis_soundram_w },
	{ 0xa04000, 0xa04003, YM2612_68000_w },
/*	{ 0x200000, 0x200fff, cartridge_ram_w },	*/
	{ 0x000000, 0x3fffff, MWA_ROM },
	{ -1 }  /* end of table */
};




static struct MemoryReadAddress genesis_s_readmem[] =
{
 	{ 0x0000, 0x1fff, MRA_BANK1, /*&genesis_soundram*//*genesis_soundram_r*/ },
    { 0x4000, 0x4000, YM2612_status_port_0_A_r },
	{ 0x4001, 0x4001, YM2612_read_port_0_r },
	{ 0x4002, 0x4002, YM2612_status_port_0_B_r },
//	{ 0x4003, 0x4003, YM2612_3_r },
	{ 0x8000, 0xffff, genesis_s_68000_ram_r },
	{ 0x7f11, 0x7f11, genesis_vdp_76489_r },
 	{ -1 }  /* end of table */
};

static struct MemoryWriteAddress genesis_s_writemem[] =
{
	{ 0x0000, 0x1fff, MWA_BANK1 /*genesis_soundram_w*/ },
  	{ 0x4000, 0x4000, YM2612_control_port_0_A_w },
	{ 0x4001, 0x4001, YM2612_data_port_0_A_w },
	{ 0x4002, 0x4002, YM2612_control_port_0_B_w },
	{ 0x4003, 0x4003, YM2612_data_port_0_B_w },
	{ 0x6000, 0x6000, genesis_ramlatch_w },
   	{ 0x7f11, 0x7f11, SN76496_0_w },
   	{ 0x8000, 0xffff, genesis_s_68000_ram_w },
  	{ -1 }  /* end of table */
};

static struct IOWritePort writeport[] =
{
	{ 0x7f, 0x7f, SN76496_0_w },
	{ -1 }	/* end of table */
};



INPUT_PORTS_START( genesis )
	PORT_START	/* IN0 player 1 controller */
	PORT_BIT(	0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP )
	PORT_BIT(	0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN )
	PORT_BIT(	0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT )
	PORT_BIT(	0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT )
	PORT_BITX(	0x10, IP_ACTIVE_LOW, IPT_BUTTON2,	"Player 1 B",       KEYCODE_X,      IP_JOY_NONE )
	PORT_BITX(	0x20, IP_ACTIVE_LOW, IPT_BUTTON3,	"Player 1 C",       KEYCODE_C,      IP_JOY_NONE )

	PORT_START	/* IN1 playter 1 controller, part 2 */
	PORT_BITX(	0x10, IP_ACTIVE_LOW, IPT_BUTTON1,	"Player 1 A",       KEYCODE_Z,      IP_JOY_NONE )

	PORT_BITX(	0x20, IP_ACTIVE_LOW, IPT_START1,	"Player 1 Start",   KEYCODE_LSHIFT, IP_JOY_NONE )

	PORT_START	/* IN2 player 2 controller */

	PORT_BIT(	0x01, IP_ACTIVE_LOW, IPT_JOYSTICK_UP	| IPF_PLAYER2 )
	PORT_BIT(	0x02, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN	| IPF_PLAYER2 )
	PORT_BIT(	0x04, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT	| IPF_PLAYER2 )
	PORT_BIT(	0x08, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT	| IPF_PLAYER2 )
	PORT_BITX(	0x10, IP_ACTIVE_LOW, IPT_BUTTON2		| IPF_PLAYER2,	"Player 2 B",       KEYCODE_O,      IP_JOY_NONE )
	PORT_BITX(	0x20, IP_ACTIVE_LOW, IPT_BUTTON3		| IPF_PLAYER2,	"Player 2 C",       KEYCODE_P,      IP_JOY_NONE )

	PORT_START	/* IN3 player 2 controller, part 2 */
	PORT_BITX(	0x10, IP_ACTIVE_LOW, IPT_BUTTON1		| IPF_PLAYER2,	"Player 2 A",       KEYCODE_I,      IP_JOY_NONE )

	PORT_BITX(	0x20, IP_ACTIVE_LOW, IPT_START1 		| IPF_PLAYER2,	"Player 2 Start",   KEYCODE_U,      IP_JOY_NONE )




	PORT_START	/* IN4 - finternal switches, and fake 'Auto' */

	PORT_DIPNAME( 0x03, 0x00, "Country",0 )

	PORT_DIPSETTING(    0x00, "Auto" )

	PORT_DIPSETTING(    0x01, "USA" )

	PORT_DIPSETTING(    0x02, "Japan" )

	PORT_DIPSETTING(    0x03, "Europe" )
INPUT_PORTS_END



/* Genesis doesn't have a color PROM, it uses VDP 'CRAM' to generate colours */
/* and change them during the game. */

static struct SN76496interface sn76496_interface =
{
	1,	/* 1 chip */
	{53693100/10.40},
	{ 255*2, 255*2 }
};

static struct YM2612interface ym2612_interface =
{
	1,			/* 1 chip */
	53693100 / 7,
	{ 0x7fffffff,0x7fffffff },
	{ 0 },
};




/* list of file extensions */
static const char *genesis_file_extensions[] =
{
	"smd",
	"bin",
	0       /* end of array */
};


static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M68000,
			53693100 /7 ,	/* 8 Mhz..ish */
			0, /* number of memory regions */
			genesis_readmem,genesis_writemem,0,0, /* zeros are ioport read/write */
			genesis_interrupt,1	/* up to 224 interrupts per frame */
		},
	 	{
	 		CPU_Z80 | CPU_AUDIO_CPU,
	 		53693100 / 15, /* 4 Mhz..ish */
	 		1,
	 		genesis_s_readmem,genesis_s_writemem,0,writeport,
	 		genesis_s_interrupt,1
	 	}
	},
	60, DEFAULT_REAL_60HZ_VBLANK_DURATION,	/* frames per second, vblank duration */
	10,	/* 80 CPU slices per frame */
	genesis_init_machine,
	0,
	/* video hardware */
	40*8, 28*8, { 0*8, 40*8-1, 0*8, 28*8-1 },
	0,/*gfxdecodeinfo,*/
	64,64/sizeof(unsigned short), /* genesis uses 4 color schemes of 16 colors each, 0 of each bank is transparent*/
	genesis_vh_convert_color_prom,

	VIDEO_TYPE_RASTER | VIDEO_MODIFIES_PALETTE,
	0,
	genesis_vh_start,
	genesis_vh_stop,
	genesis_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
		{
 			SOUND_YM2612,
			&ym2612_interface
		},
		{
			SOUND_SN76496,
			&sn76496_interface
		}
	}
};


/***************************************************************************

  Game driver(s)

***************************************************************************/

struct GameDriver genesis_driver =
{
	__FILE__,
	0,
	"genesis",
	"Sega Megadrive/Genesis",
	"1988",
	"Sega",
	"Gareth S. Long\n\n\nIn Memory Of Haruki Ikeda",
	0,
	&machine_driver,
	0,

	0,
	genesis_load_rom,
	genesis_id_rom,
	genesis_file_extensions,
	1,	/* number of ROM slots */
	0,	/* number of floppy drives supported */
	0,	/* number of hard drives supported */
	0,	/* number of cassette drives supported */
	0, 0,
	0,
	0,

	input_ports_genesis,

	0, 0, 0,   /* colors, palette, colortable */
	ORIENTATION_DEFAULT,
	0, 0,
};

