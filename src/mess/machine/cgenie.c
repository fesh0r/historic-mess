/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine
  (RAM, ROM, interrupts, I/O ports)

***************************************************************************/

#include <stdarg.h>
#include "driver.h"
#include "cpu/z80/z80.h"
#include "vidhrdw/generic.h"
#include "mess/vidhrdw/m6845.h"
#include "mess/machine/wd179x.h"

#define AYWriteReg(chip,port,value) \
	AY8910_control_port_0_w(0,port);  \
	AY8910_write_port_0_w(0,value)

UINT8 *fontram;

int cgenie_tv_mode = -1;
static int port_ff = 0xff;

#define CGENIE_DRIVE_INFO

/********************************************************
   abbreviations used:
   GPL  Granules Per Lump
   GAT  Granule Allocation Table
   GATL GAT Length
   GATM GAT Mask
   DDGA Disk Directory Granule Allocation
*********************************************************/
typedef struct
{
    UINT8 DDSL;      /* Disk Directory Start Lump (lump number of GAT) */
    UINT8 GATL;      /* # of UINT8s used in the Granule Allocation Table sector */
    UINT8 STEPRATE;  /* step rate and some with SD/DD flag ... */
    UINT8 TRK;       /* number of tracks */
    UINT8 SPT;       /* sectors per track (both heads!) */
    UINT8 GATM;      /* number of used bits per UINT8 in the GAT sector */
    UINT8 P7;        /* ???? always zero */
    UINT8 FLAGS;     /* ???? some flags (SS/DS bit 6) */
    UINT8 GPL;       /* Sectors per granule (always 5) */
    UINT8 DDGA;      /* Disk Directory Granule allocation (number of driectory granules) */
}   PDRIVE;

static PDRIVE pd_list[12] = {
    /* CMD"<0=A" 40 tracks, single sided, single density */
    {0x14, 0x28, 0x07, 0x28, 0x0A, 0x02, 0x00, 0x00, 0x05, 0x02},
    /* CMD"<0=B" 40 tracks, double sided, single density */
    {0x14, 0x28, 0x07, 0x28, 0x14, 0x04, 0x00, 0x40, 0x05, 0x04},
    /* CMD"<0=C" 40 tracks, singled sided, double density */
    {0x18, 0x30, 0x53, 0x28, 0x12, 0x03, 0x00, 0x03, 0x05, 0x03},
    /* CMD"<0=D" 40 tracks, double sided, double density */
    {0x18, 0x30, 0x53, 0x28, 0x24, 0x06, 0x00, 0x43, 0x05, 0x06},
    /* CMD"<0=E" 40 tracks, single sided, single density */
    {0x14, 0x28, 0x07, 0x28, 0x0A, 0x02, 0x00, 0x04, 0x05, 0x02},
    /* CMD"<0=F" 40 tracks, double sided, single density */
    {0x14, 0x28, 0x07, 0x28, 0x14, 0x04, 0x00, 0x44, 0x05, 0x04},
    /* CMD"<0=G" 40 tracks, single sided, double density */
    {0x18, 0x30, 0x53, 0x28, 0x12, 0x03, 0x00, 0x07, 0x05, 0x03},
    /* CMD"<0=H" 40 tracks, double sided, double density */
    {0x18, 0x30, 0x53, 0x28, 0x24, 0x06, 0x00, 0x47, 0x05, 0x06},
    /* CMD"<0=I" 80 tracks, single sided, single density */
    {0x28, 0x50, 0x07, 0x50, 0x0A, 0x02, 0x00, 0x00, 0x05, 0x02},
    /* CMD"<0=J" 80 tracks, double sided, single density */
    {0x28, 0x50, 0x07, 0x50, 0x14, 0x04, 0x00, 0x40, 0x05, 0x04},
    /* CMD"<0=K" 80 tracks, single sided, double density */
    {0x30, 0x60, 0x53, 0x50, 0x12, 0x03, 0x00, 0x03, 0x05, 0x03},
    /* CMD"<0=L" 80 tracks, double sided, double density */
    {0x30, 0x60, 0x53, 0x50, 0x24, 0x06, 0x00, 0x43, 0x05, 0x06},
};

#define IRQ_TIMER       0x80
#define IRQ_FDC         0x40
static UINT8 irq_status = 0;

static UINT8 first_fdc_access[4] = {1, 1, 1, 1};
static UINT8 motor_drive = 0;
static short motor_count = 0;
static PDRIVE pdrive[4];
static UINT8 tracks[4] = {0,};
static UINT8 heads[4] = {0,};
static UINT8 s_p_t[4] = {0,};
static UINT8 head[4] = {0,};
static short dir_sector[4] = {0,};
static short dir_length[4] = {0,};

/* current tape file handles */
static void *tape_put_file = 0;
static void *tape_get_file = 0;

/* tape buffer for the first eight bytes at write (to extract a filename) */
static UINT8 tape_buffer[9];

/* file offset within tape file */
static int tape_count = 0;

/* number of sync and data bits that were written */
static int put_bit_count = 0;

/* number of sync and data bits to read */
static int get_bit_count = 0;

/* sync and data bits mask */
static int tape_bits = 0;

/* time in cycles for the next bit at read */
static int tape_time = 0;

/* flag if writing to tape detected the sync header A5 already */
static int in_sync = 0;

/* cycle count at last output port change */
static int put_cycles = 0;

/* cycle count at last input port read */
static int get_cycles = 0;

/* a prototype to be called from cgenie_stop_machine */
static void tape_put_close(void);


void cgenie_init_machine(void)
{
	osd_fdc_init();

	/* reset the AY8910 to be quiet, since the cgenie BIOS doesn't */
	AYWriteReg(0, 0, 0);
	AYWriteReg(0, 1, 0);
	AYWriteReg(0, 2, 0);
	AYWriteReg(0, 3, 0);
	AYWriteReg(0, 4, 0);
	AYWriteReg(0, 5, 0);
	AYWriteReg(0, 6, 0);
	AYWriteReg(0, 7, 0x3f);
	AYWriteReg(0, 8, 0);
	AYWriteReg(0, 9, 0);
	AYWriteReg(0, 10, 0);

	/* wipe out color RAM */
	memset(memory_region(REGION_CPU1)+0x0f000, 0x00, 0x0400);
	/* wipe out font RAM */
	memset(memory_region(REGION_CPU1)+0x0f400, 0xff, 0x0400);



	if (input_port_0_r(0) & 0x80)
	{
		if (errorlog)
			fprintf(errorlog, "cgenie floppy discs enabled\n");
		wd179x_init(1);
		first_fdc_access[0] = 1;
		first_fdc_access[1] = 1;
		first_fdc_access[2] = 1;
		first_fdc_access[3] = 1;
	}
	else
	{
		if (errorlog)
			fprintf(errorlog, "cgenie floppy discs disabled\n");
		wd179x_init(0);
	}

	/* copy DOS ROM, if enabled or wipe out that memory area */
	if (input_port_0_r(0) & 0x40)
	{
		if (errorlog)
			fprintf(errorlog, "cgenie DOS enabled\n");
		memcpy(&memory_region(REGION_CPU1)[0x0c000],
			   &memory_region(REGION_CPU1)[0x10000], 0x2000);
	}
	else
	{
		if (errorlog)
			fprintf(errorlog, "cgenie DOS disabled\n");
		memset(&memory_region(REGION_CPU1)[0x0c000], 0x00, 0x2000);
	}

	/* copy EXT ROM, if enabled or wipe out that memory area */
	if (input_port_0_r(0) & 0x20)
	{
		if (errorlog)
			fprintf(errorlog, "cgenie EXT enabled\n");
		memcpy(&memory_region(REGION_CPU1)[0x0e000],
			   &memory_region(REGION_CPU1)[0x12000], 0x1000);
	}
	else
	{
		if (errorlog)
			fprintf(errorlog, "cgenie EXT disabled\n");
		memset(&memory_region(REGION_CPU1)[0x0e000], 0x00, 0x1000);
	}

}

void cgenie_stop_machine(void)
{
	tape_put_close();
}

int cgenie_cmd_load(void *cmd)
{
	UINT8 *buff = malloc(65536), *s, *d;
	unsigned size, block_len, block_ofs;
	UINT8 data;

	if (!buff)
		return 7;

	size = osd_fread(cmd, buff, 65536);
	s = buff;
	while (size > 3)
	{
		data = *s++;
		switch (data)
		{
			case 0x01:		   /* CMD file header */
			case 0x07:		   /* another type of CMD file header */
			case 0x3c:		   /* CAS file header */
				block_len = *s++;
			/* on CMD files size <= 2 means size + 256 */
				if ((data != 0x3c) && (block_len <= 2))
					block_len += 256;
				block_ofs = *s++;
				block_ofs += 256 * *s++;
				d = &memory_region(REGION_CPU1)[block_ofs];
				if (data != 0x3c)
					block_len -= 2;
				size -= 4;
				if (errorlog)
					fprintf(errorlog, "cgenie_cmd_load %d at %04X\n", block_len, block_ofs);
				while (block_len && size)
				{
					if (block_ofs >= 0x4000)
					{
						*d++ = *s++;
					}
					else
					{
						d++;
						s++;
					}
					block_ofs++;
					block_len--;
					size--;
				}
				if (data == 0x3c)
					s++;
				break;
			case 0x02:
				block_len = *s++;
				size -= 1;
			case 0x78:
				memory_region(REGION_CPU1)[0x115] = 0xc3;
				memory_region(REGION_CPU1)[0x116] = *s++;
				memory_region(REGION_CPU1)[0x117] = *s++;
				size -= 3;
				break;
			default:
				size--;
		}
	}
	free(buff);
	return 0;
}

int cgenie_rom_load(void)
{
	int result = 0, i;
	UINT8 *gfx;
	void *rom, *cmd;
	char filename[200];

	/* Initialize memory */
	memset(&ROM[0x4000], 0xff, 0xc000);

	/* Initialize the character generator */
	gfx = memory_region(2);

	/* Initialize some patterns to be displayed in graphics mode */
	for (i = 0; i < 256; i++)
		memset(gfx + i * 8, i, 8);

	strcpy(filename, "newe000.rom");
	rom = osd_fopen(Machine->gamedrv->name, filename, OSD_FILETYPE_IMAGE_R, 0);
	if (rom)
	{
		if (errorlog)
			fprintf(errorlog, "%s found '%s' ROM\n", Machine->gamedrv->name, filename);
		osd_fread(rom, &ROM[0x12000], 0x1000);
		osd_fclose(rom);
	}
	else
	{
		if (errorlog)
			fprintf(errorlog, "%s optional ROM image '%s' not found\n", Machine->gamedrv->name, filename);
	}

	/* if a CAS or CMD name is given */
	if( rom_name[0] )
	{
		cmd = osd_fopen(Machine->gamedrv->name, rom_name[0], OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_READ);
		if (cmd)
		{
			result = cgenie_cmd_load(cmd);
			osd_fclose(cmd);
		}
	}

	return result;
}

int cgenie_rom_id(const char *name, const char *gamename)
{
	/* This driver cannot ID ROMs */
	return 0;
}


/*************************************
 *
 *              Tape emulation.
 *
 *************************************/

/*******************************************************************
 * tape_put_byte
 * write next data byte to virtual tape. After collecting the first
 * nine bytes try to extract the kind of data and filename.
 *******************************************************************/
static void tape_put_byte(UINT8 value)
{
	if (tape_count < 9)
	{
		tape_buffer[tape_count++] = value;
		if (tape_count == 9)
		{
		/* BASIC tape ? */
			if (tape_buffer[1] != 0x55 || tape_buffer[8] != 0x3c)
			{
				char filename[12 + 1];
				sprintf(filename, "basic%c.cas", tape_buffer[1]);
				tape_put_file = osd_fopen(Machine->gamedrv->name, filename, OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_RW);
				osd_fwrite(tape_put_file, tape_buffer, 9);
			}
			else
			/* SYSTEM tape ? */
			if (tape_buffer[1] == 0x55 && tape_buffer[8] == 0x3c)
			{
				char filename[12 + 1];
				sprintf(filename, "%-6.6s.cas", tape_buffer + 2);
				tape_put_file = osd_fopen(Machine->gamedrv->name, filename, OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_RW);
				osd_fwrite(tape_put_file, tape_buffer, 9);
			}
		}
	}
	else
	{
		if (tape_put_file)
			osd_fwrite(tape_put_file, &value, 1);
	}
}

/*******************************************************************
 * tape_put_close
 * eventuall flush output buffer and close an open
 * virtual tape output file.
 *******************************************************************/
static void tape_put_close(void)
{
	/* file open ? */
	if (tape_put_file)
	{
		if (put_bit_count)
		{
			UINT8 value;
			while (put_bit_count < 16)
			{
				tape_bits <<= 1;
				put_bit_count++;
			}
			value = 0;
			if (tape_bits & 0x8000)
				value |= 0x80;
			if (tape_bits & 0x2000)
				value |= 0x40;
			if (tape_bits & 0x0800)
				value |= 0x20;
			if (tape_bits & 0x0200)
				value |= 0x10;
			if (tape_bits & 0x0080)
				value |= 0x08;
			if (tape_bits & 0x0020)
				value |= 0x04;
			if (tape_bits & 0x0008)
				value |= 0x02;
			if (tape_bits & 0x0002)
				value |= 0x01;
			tape_put_byte(value);
		}
		osd_fclose(tape_put_file);
	}
	tape_count = 0;
	tape_put_file = 0;
}

/*******************************************************************
 * tape_put_bit
 * port FF tape status bit changed. Figure out what to do with it ;-)
 *******************************************************************/
static void tape_put_bit(void)
{
	int now_cycles = cpu_gettotalcycles();
	int diff = now_cycles - put_cycles;
	int limit = 12 * (memory_region(REGION_CPU1)[0x4310] + memory_region(REGION_CPU1)[0x4311]);
	UINT8 value;

	/* overrun since last write ? */
	if (diff > 4000)
	{
		/* reset tape output */
		tape_put_close();
		put_bit_count = tape_bits = in_sync = 0;
	}
	else
	{
		/* change within time for a 1 bit ? */
		if (diff < limit)
		{
			tape_bits = (tape_bits << 1) | 1;
			switch (in_sync)
			{
				case 0:		   /* look for sync AA */
					if ((tape_bits & 0xffff) == 0xcccc)
						in_sync = 1;
					break;
				case 1:		   /* look for sync 66 */
					if ((tape_bits & 0xffff) == 0x3c3c)
					{
						in_sync = 2;
						put_bit_count = 16;
					}
					break;
				case 2:		   /* count 1 bit */
					put_bit_count += 1;
			}
		}
		/* no change within time indicates a 0 bit */
		else
		{
			tape_bits <<= 2;
			switch (in_sync)
			{
				case 0:		   /* look for sync AA */
					if ((tape_bits & 0xffff) == 0xcccc)
						in_sync = 1;
					break;
				case 1:		   /* look for sync 66 */
					if ((tape_bits & 0xffff) == 0x3c3c)
					{
						in_sync = 2;
						put_bit_count = 16;
					}
					break;
				case 2:		   /* count 2 bits */
					put_bit_count += 2;
			}
		}

		if (errorlog)
			fprintf(errorlog, "%4d %4d %d bits %04X\n", diff, limit, in_sync, tape_bits & 0xffff);

		/* collected 8 sync plus 8 data bits ? */
		if (put_bit_count >= 16)
		{
			/* extract data bits to value */
			value = 0;
			if (tape_bits & 0x8000)
				value |= 0x80;
			if (tape_bits & 0x2000)
				value |= 0x40;
			if (tape_bits & 0x0800)
				value |= 0x20;
			if (tape_bits & 0x0200)
				value |= 0x10;
			if (tape_bits & 0x0080)
				value |= 0x08;
			if (tape_bits & 0x0020)
				value |= 0x04;
			if (tape_bits & 0x0008)
				value |= 0x02;
			if (tape_bits & 0x0002)
				value |= 0x01;
			put_bit_count -= 16;
			tape_bits = 0;
			tape_put_byte(value);
		}
	}
/* remember the cycle count of this write */
	put_cycles = now_cycles;
}

/*******************************************************************
 * tape_get_byte
 * read next byte from input tape image file.
 * the first 32 bytes are faked to be sync header AA.
 *******************************************************************/
static void tape_get_byte(void)
{
	UINT8 value;

	if (tape_get_file)
	{
		if (tape_count < 32)
			value = 0xaa;
		else
			osd_fread(tape_get_file, &value, 1);
		tape_bits |= 0xaaaa;
		if (value & 0x80)
			tape_bits ^= 0x4000;
		if (value & 0x40)
			tape_bits ^= 0x1000;
		if (value & 0x20)
			tape_bits ^= 0x0400;
		if (value & 0x10)
			tape_bits ^= 0x0100;
		if (value & 0x08)
			tape_bits ^= 0x0040;
		if (value & 0x04)
			tape_bits ^= 0x0010;
		if (value & 0x02)
			tape_bits ^= 0x0004;
		if (value & 0x01)
			tape_bits ^= 0x0001;
		get_bit_count = 16;
		tape_count++;
	}
}

/*******************************************************************
 * tape_get_open
 * Open a virtual tape image file for input. Look for a special
 * header and skip leading description, if present.
 * The filename is taken from BASIC input buffer at 41E8 ff.
 *******************************************************************/
static void tape_get_open(void)
{
#define TAPE_HEADER     "Colour Genie - Virtual Tape File"
	if (!tape_get_file)
	{
		char filename[12 + 1];
		char buffer[sizeof(TAPE_HEADER)];
		/* TODO: remove this */
		unsigned char *RAM = memory_region(REGION_CPU1);

		sprintf(filename, "%-6.6s.cas", RAM + 0x41e8);
		if( errorlog )
			fprintf(errorlog, "tape_get_open filename %s\n", filename);
		tape_get_file = osd_fopen(Machine->gamedrv->name, filename, OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_READ);
		if( tape_get_file )
		{
			osd_fread(tape_get_file, buffer, sizeof(TAPE_HEADER));
			if( strncmp(buffer, TAPE_HEADER, sizeof(TAPE_HEADER) - 1) == 0 )
			{
				UINT8 data;
				/* skip data until zero byte */
				do
				{
					osd_fread(tape_get_file, &data, 1);
				} while (data);
			}
			else
			{
				/* seek back to start of tape */
				osd_fseek(tape_get_file, 0, SEEK_SET);
			}
		}
		tape_count = 0;
	}
}

static void tape_get_bit(void)
{
	int now_cycles = cpu_gettotalcycles();
	int limit = 10 * memory_region(REGION_CPU1)[0x4312];
	int diff = now_cycles - get_cycles;

	/* overrun since last read ? */
	if (diff >= 4000)
	{
		if (tape_get_file)
		{
			osd_fclose(tape_get_file);
			tape_get_file = 0;
		}
		get_bit_count = tape_bits = tape_time = 0;
	}
	else
	/* check what he will get for input */
	{
		/* count down cycles */
		tape_time -= diff;

		/* time for the next sync or data bit ? */
		if (tape_time <= 0)
		{
		/* approx time for a bit */
			tape_time += limit;
		/* need to read get new data ? */
			if (--get_bit_count <= 0)
			{
				tape_get_open();
				tape_get_byte();
			}
		/* shift next sync or data bit to bit 16 */
			tape_bits <<= 1;
			if (tape_bits & 0x10000)
				port_ff ^= 1;
		}
	}
	/* remember the cycle count of this read */
	get_cycles = now_cycles;
}

/*************************************
 *
 *              Port handlers.
 *
 *************************************/

/* used bits on port FF */
#define FF_CAS  0x01		   /* tape output signal */
#define FF_BGD0 0x04		   /* background color enable */
#define FF_CHR1 0x08		   /* charset 0xc0 - 0xff 1:fixed 0:defined */
#define FF_CHR0 0x10		   /* charset 0x80 - 0xbf 1:fixed 0:defined */
#define FF_CHR  (FF_CHR0 | FF_CHR1)
#define FF_FGR  0x20		   /* 1: "hi" resolution graphics, 0: text mode */
#define FF_BGD1 0x40		   /* background color select 1 */
#define FF_BGD2 0x80		   /* background color select 2 */
#define FF_BGD  (FF_BGD0 | FF_BGD1 | FF_BGD2)

void cgenie_port_ff_w(int offset, int data)
{
	int port_ff_changed = port_ff ^ data;

	if (port_ff_changed & FF_CAS)	/* casette port changed ? */
	{
		/* virtual tape ? */
		if (input_port_0_r(0) & 0x10)
			tape_put_bit();
		else
		{
			int dac_data = 0;

            if (data & FF_CAS)
				dac_data = 127;
			DAC_data_w(0, dac_data);
		}
	}

	/* background bits changed ? */
	if (port_ff_changed & FF_BGD)
	{
		unsigned char r, g, b;

		if (data & FF_BGD0)
		{
			r = 112; g = 0;  b = 112;
		}
		else
		{
			if( cgenie_tv_mode == 0 )
			{
				switch (data & (FF_BGD1 + FF_BGD2))
				{
					case FF_BGD1:
						r = 112; g = 40;  b = 32;
						break;
					case FF_BGD2:
						r = 40;  g = 112; b = 32;
						break;
					case FF_BGD1 + FF_BGD2:
						r = 72;  g = 72; b = 72;
						break;
					default:
						r = 0;	 g = 0;  b = 0;
						break;
				}
			}
			else
			{
				r = 15; g = 15; b = 15;
			}
		}
		osd_modify_pen(0, r, g, b);
		osd_modify_pen(Machine->pens[0], r, g, b);
	}

	/* character mode changed ? */
	if( port_ff_changed & FF_CHR )
	{
		m6845_font_offset[2] = (data & FF_CHR0) ? 0x00 : 0x80;
		m6845_font_offset[3] = (data & FF_CHR1) ? 0x00 : 0x80;
		if ((port_ff_changed & FF_CHR) == FF_CHR)
			m6845_invalidate_range(0x80, 0xff);
		else if ((port_ff_changed & FF_CHR) == FF_CHR0)
			m6845_invalidate_range(0x80, 0xbf);
		else
			m6845_invalidate_range(0xc0, 0xff);
	}

	/* graphics mode changed ? */
	if (port_ff_changed & FF_FGR)
	{
		m6845_invalidate_range(0x00, 0xff);
		m6845_mode_select(data & FF_FGR);
	}

	port_ff = data;
}

int cgenie_port_ff_r(int offset)
{
	/* virtual tape ? */

	if (input_port_0_r(0) & 0x10)
		tape_get_bit();

	return port_ff;
}

int cgenie_port_xx_r(int offset)
{
	return 0xff;
}

/*************************************
 *                                   *
 *      Memory handlers              *
 *                                   *
 *************************************/

static UINT8 psg_a_out = 0x00;
static UINT8 psg_b_out = 0x00;
static UINT8 psg_a_inp = 0x00;
static UINT8 psg_b_inp = 0x00;

int cgenie_psg_port_a_r(int port)
{
	return psg_a_inp;
}

int cgenie_psg_port_b_r(int port)
{
	if (psg_a_out < 0xd0)
	{
		/* comparator value */
		psg_b_inp = 0x00;
		if (input_port_9_r(0) > psg_a_out)
			psg_b_inp |= 0x80;
		if (input_port_10_r(0) > psg_a_out)
			psg_b_inp |= 0x40;
		if (input_port_11_r(0) > psg_a_out)
			psg_b_inp |= 0x20;
		if (input_port_12_r(0) > psg_a_out)
			psg_b_inp |= 0x10;
	}
	else
	{
		/* read keypad matrix */
		psg_b_inp = 0xFF;
		if (!(psg_a_out & 0x01))
			psg_b_inp &= ~(input_port_13_r(0) & 15);
		if (!(psg_a_out & 0x02))
			psg_b_inp &= ~(input_port_13_r(0) / 16);
		if (!(psg_a_out & 0x04))
			psg_b_inp &= ~(input_port_14_r(0) & 15);
		if (!(psg_a_out & 0x08))
			psg_b_inp &= ~(input_port_14_r(0) / 16);
		if (!(psg_a_out & 0x10))
			psg_b_inp &= ~(input_port_15_r(0) & 15);
		if (!(psg_a_out & 0x20))
			psg_b_inp &= ~(input_port_15_r(0) / 16);
	}
	return psg_b_inp;
}

void cgenie_psg_port_a_w(int port, int val)
{
	psg_a_out = val;
}

void cgenie_psg_port_b_w(int port, int val)
{
	psg_b_out = val;
}

int cgenie_status_r(int offset)
{
	/* If the floppy isn't emulated, return 0 */
	if ((readinputport(0) & 0x80) == 0)
		return 0;
	return wd179x_status_r(offset);
}

int cgenie_track_r(int offset)
{
	/* If the floppy isn't emulated, return 0xff */
	if ((readinputport(0) & 0x80) == 0)
		return 0xff;
    return wd179x_track_r(offset);
}

int cgenie_sector_r(int offset)
{
	/* If the floppy isn't emulated, return 0xff */
	if ((readinputport(0) & 0x80) == 0)
        return 0xff;
    return wd179x_sector_r(offset);
}

int cgenie_data_r(int offset)
{
	/* If the floppy isn't emulated, return 0xff */
	if ((readinputport(0) & 0x80) == 0)
        return 0xff;
    return wd179x_data_r(offset);
}

void cgenie_command_w(int offset, int data)
{
	/* If the floppy isn't emulated, return immediately */
	if ((readinputport(0) & 0x80) == 0)
		return;
    wd179x_command_w(offset, data);
}

void cgenie_track_w(int offset, int data)
{
	/* If the floppy isn't emulated, ignore the write */
	if ((readinputport(0) & 0x80) == 0)
        return;
    wd179x_track_w(offset, data);
}

void cgenie_sector_w(int offset, int data)
{
	/* If the floppy isn't emulated, ignore the write */
    if ((readinputport(0) & 0x80) == 0)
        return;
    wd179x_sector_w(offset, data);
}

void cgenie_data_w(int offset, int data)
{
	/* If the floppy isn't emulated, ignore the write */
    if ((readinputport(0) & 0x80) == 0)
        return;
    wd179x_data_w(offset, data);
}

int cgenie_irq_status_r(int offset)
{
int result = irq_status;

	irq_status &= ~(IRQ_TIMER | IRQ_FDC);
	return result;
}

int cgenie_timer_interrupt(void)
{
	if ((irq_status & IRQ_TIMER) == 0)
	{
		irq_status |= IRQ_TIMER;
		cpu_cause_interrupt(0, 0);
		return 0;
	}
	return ignore_interrupt();
}

int cgenie_fdc_interrupt(void)
{
	if ((irq_status & IRQ_FDC) == 0)
	{
		irq_status |= IRQ_FDC;
		cpu_cause_interrupt(0, 0);
		return 0;
	}
	return ignore_interrupt();
}

void cgenie_fdc_callback(int event)
{
	switch (event)
	{
		case WD179X_IRQ_CLR:
			irq_status &= ~IRQ_FDC;
			break;
		case WD179X_IRQ_SET:
			cgenie_fdc_interrupt();
			break;
	}
}

void cgenie_motor_w(int offset, int data)
{
	UINT8 drive = 255;
	void *file;

	if (errorlog)
		fprintf(errorlog, "cgenie motor_w $%02X\n", data);

	if (data & 1)
		drive = 0;
	if (data & 2)
		drive = 1;
	if (data & 4)
		drive = 2;
	if (data & 8)
		drive = 3;

	if (drive > 3)
		return;

	/* no floppy name given for that drive ? */
	if (!floppy_name[drive])
		return;

	/* mask head select bit */
	head[drive] = (data >> 4) & 1;

	/* currently selected drive */
	motor_drive = drive;

	/* let it run about 5 seconds */
	motor_count = 5 * 60;

	/* select the drive / head */
	file = wd179x_select_drive(drive, head[drive], cgenie_fdc_callback, floppy_name[drive]);

	if (!file)
		return;

	if (file == REAL_FDD)
	{
		PDRIVE *pd = (PDRIVE *)memory_region(REGION_CPU1) + 0x5a71 + drive * sizeof(PDRIVE);
		/* changed pdrive parameters for drive ? */
		if (memcmp(&pdrive[drive], pd, sizeof(PDRIVE)))
		{
			/* copy them and set new geometry */
			memcpy(&pdrive[drive], pd, sizeof(PDRIVE));
			tracks[drive] = pd->TRK;
			heads[drive] = (pd->SPT > 18) ? 2 : 1;
			s_p_t[drive] = pd->SPT / heads[drive];
			dir_sector[drive] = pd->DDSL * pd->GATM * pd->GPL + pd->SPT;
			dir_length[drive] = pd->DDGA * pd->GPL;
			wd179x_set_geometry(drive, tracks[drive], heads[drive], s_p_t[drive], 256, dir_sector[drive], dir_length[drive]);
        }
		return;
	}

	/* drive selected for the first time ? */
	if (first_fdc_access[drive])
	{
		int i, j, dir_offset;
		UINT8 buff[16];

		first_fdc_access[drive] = 0;

		/* determine geometry from disk contents */

		for (i = 0; i < 12; i++)
		{
			osd_fseek(file, pd_list[i].SPT * 256, SEEK_SET);
			osd_fread(file, buff, 16);
			/* find an entry with matching DDSL */
			if (buff[0] != 0x00 || buff[1] != 0xfe || buff[2] != pd_list[i].DDSL)
				continue;
			dir_sector[drive] = pd_list[i].DDSL * pd_list[i].GATM * pd_list[i].GPL + pd_list[i].SPT;
			dir_length[drive] = pd_list[i].DDGA * pd_list[i].GPL;
			/* scan directory for DIR/SYS or NCW1983/JHL files */
			/* look into sector 2 and 3 first entry relative to DDSL */
			for (j = 16; j < 32; j += 8)
			{
				dir_offset = dir_sector[drive] * 256 + j * 32;
				if (osd_fseek(file, dir_offset, SEEK_SET) < 0)
					break;
				if (osd_fread(file, buff, 16) != 16)
					break;
				if (!strncmp((char*)buff + 5, "DIR     SYS", 11) ||
					!strncmp((char*)buff + 5, "NCW1983 JHL", 11))
				{
					tracks[drive] = pd_list[i].TRK;
					heads[drive] = (pd_list[i].SPT > 18) ? 2 : 1;
					s_p_t[drive] = pd_list[i].SPT / heads[drive];
					dir_sector[drive] = pd_list[i].DDSL * pd_list[i].GATM * pd_list[i].GPL + pd_list[i].SPT;
					dir_length[drive] = pd_list[i].DDGA * pd_list[i].GPL;
					wd179x_set_geometry(drive, tracks[drive], heads[drive], s_p_t[drive], 256, dir_sector[drive], dir_length[drive]);
					memcpy(memory_region(REGION_CPU1) + 0x5A71 + drive * sizeof(PDRIVE), &pd_list[i], sizeof(PDRIVE));
					return;
				}
			}
		}
	}
}

/*************************************
 *      Keyboard                     *
 *************************************/
int cgenie_keyboard_r(int offset)
{
	int result = 0;

	if( setup_active() || onscrd_active() )
		return result;

    if (offset & 1)
		result |= input_port_1_r(0);
	if (offset & 2)
		result |= input_port_2_r(0);
	if (offset & 4)
		result |= input_port_3_r(0);
	if (offset & 8)
		result |= input_port_4_r(0);
	if (offset & 16)
		result |= input_port_5_r(0);
	if (offset & 32)
		result |= input_port_6_r(0);
	if (offset & 64)
		result |= input_port_7_r(0);
	if (offset & 128)
		result |= input_port_8_r(0);

	return result;
}

/*************************************
 *      Video RAM                    *
 *************************************/

int cgenie_videoram_r(int offset)
{
	return videoram[offset];
}

void cgenie_videoram_w(int offset, int data)
{
	/* write to video RAM */
	if (data == videoram[offset])
		return;				   /* no change */
	videoram[offset] = data;
	dirtybuffer[offset] = 1;
}

void cgenie_dos_rom_w(int offset, int data)
{
	/* write to dos ROM area */
	if( input_port_0_r(0) & 0x40 )
		return;
	/* write RAM at c000-dfff */
	memory_region(REGION_CPU1)[0x0c000 + offset] = data;
}

void cgenie_ext_rom_w(int offset, int data)
{
	/* write to extra ROM area */
	if( input_port_0_r(0) & 0x20 )
		return;
	/* write RAM at e000-efff */
	memory_region(REGION_CPU1)[0x0e000 + offset] = data;
}

int cgenie_colorram_r(int offset)
{
	return colorram[offset] | 0xf0;
}

void cgenie_colorram_w(int offset, int data)
{
	int a;

/* only bits 0 to 3 */
	data &= 15;
/* nothing changed ? */
	if (data == colorram[offset])
		return;

/* set new value */
	colorram[offset] = data;
/* make offset relative to video frame buffer offset */
	offset = (offset + (m6845_get_register(12) << 8) + m6845_get_register(13)) & 0x3ff;
/* mark every 1k of the frame buffer dirty */
	for (a = offset; a < 0x4000; a += 0x400)
		dirtybuffer[a] = 1;
}

int cgenie_fontram_r(int offset)
{
	return fontram[offset];
}

void cgenie_fontram_w(int offset, int data)
{
	UINT8 *dp;
	int code;

	if (data == fontram[offset])
		return;				   /* no change */

/* store data */
	fontram[offset] = data;

/* convert eight pixels */
	dp = &Machine->gfx[0]->gfxdata[(256 * 8 + offset) * Machine->gfx[0]->width];
	dp[0] = (data & 0x80) ? 1 : 0;
	dp[1] = (data & 0x40) ? 1 : 0;
	dp[2] = (data & 0x20) ? 1 : 0;
	dp[3] = (data & 0x10) ? 1 : 0;
	dp[4] = (data & 0x08) ? 1 : 0;
	dp[5] = (data & 0x04) ? 1 : 0;
	dp[6] = (data & 0x02) ? 1 : 0;
	dp[7] = (data & 0x01) ? 1 : 0;

/* invalidate related character */
	code = 0x80 + offset / 8;
	m6845_invalidate_range(code, code);
}

/*************************************
 *
 *		Interrupt handlers.
 *
 *************************************/

int cgenie_frame_interrupt(void)
{
	if( cgenie_tv_mode != (input_port_0_r(0) & 0x10) )
	{
		cgenie_tv_mode = input_port_0_r(0) & 0x10;
        memset(dirtybuffer, 1, videoram_size);
		/* force setting of background color */
		port_ff ^= FF_BGD0;
		cgenie_port_ff_w(0, port_ff ^ FF_BGD0);
    }

	if (motor_count)
	{
		if (!--motor_count)
			wd179x_stop_drive();
	}

	return 0;
}

void cgenie_nmi_generate(int param)
{
	cpu_cause_interrupt(0, Z80_NMI_INT);
}
