/***************************************************************************
	vz.c

    machine driver
	Juergen Buchmueller <pullmoll@t-online.de>, Dec 1999

	TODO:
		Finish hard disk support, once somebody dumped the DOS ROM.
		Change loading .vz images from ROMs to cassette.
		Printer and RS232 support.

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

#define VERBOSE 1

#if VERBOSE
#define LOG(x)	if( errorlog ) fprintf x
#else
#define LOG(x)	/* x */
#endif

int vz_latch = 0;

void *vz_floppy_file[2] = {NULL, NULL};
UINT8 vz_floppy_data[256];
UINT8 vz_floppy_count = 0;
UINT8 vz_floppy_latch = 0;

void vz200_init_machine(void)
{
    if( readinputport(0) & 0x80 )
    {
        install_mem_read_handler(0, 0x9000, 0xcfff, MRA_RAM);
        install_mem_write_handler(0, 0x9000, 0xcfff, MWA_RAM);
    }
    else
    {
        install_mem_read_handler(0, 0x9000, 0xcfff, MRA_NOP);
        install_mem_write_handler(0, 0x9000, 0xcfff, MWA_NOP);
    }
	/* Install DOS ROM ? */
	if( readinputport(0) & 0x40 )
	{
		void *rom = osd_fopen(Machine->gamedrv->name, "vzdos.rom", OSD_FILETYPE_IMAGE_R, 0);
		if( rom )
		{
			osd_fread(rom, &ROM[0x4000], 0x2000);
			osd_fclose(rom);
        }
	}
}

void vz300_init_machine(void)
{
    if( readinputport(0) & 0x80 )
    {
        install_mem_read_handler(0, 0xb800, 0xf7ff, MRA_RAM);
        install_mem_write_handler(0, 0xb800, 0xf7ff, MWA_RAM);
    }
    else
    {
        install_mem_read_handler(0, 0xb800, 0xf7ff, MRA_NOP);
        install_mem_write_handler(0, 0xb800, 0xf7ff, MWA_NOP);
    }
	/* Install DOS ROM ? */
	if( readinputport(0) & 0x40 )
	{
		void *rom = osd_fopen(Machine->gamedrv->name, "vzdos.rom", OSD_FILETYPE_IMAGE_R, 0);
		if( rom )
		{
			osd_fread(rom, &ROM[0x4000], 0x2000);
			osd_fclose(rom);
        }
    }
}

void vz_shutdown_machine(void)
{
	int i;
	for( i = 0; i < 2; i++ )
	{
		if( vz_floppy_file[i] )
			osd_fclose(vz_floppy_file[i]);
		vz_floppy_file[i] = NULL;
	}
}

int vz_rom_id(const char *name, const char *gamename)
{
    const char magic_basic[] = "VZF0";
    const char magic_mcode[] = "  \000\000";
    char buff[4];
    void *file;
    file = osd_fopen(gamename, name, OSD_FILETYPE_IMAGE_RW, 0);
    if( file )
    {
        osd_fread(file, buff, sizeof(buff));
        if( memcmp(buff, magic_basic, sizeof(buff)) == 0 )
        {
            LOG((errorlog, "vz_rom_id: BASIC magic '%s' found\n", magic_basic));
            return 0;
        }
        if( memcmp(buff, magic_mcode, sizeof(buff)) == 0 )
        {
            LOG((errorlog, "vz_rom_id: MCODE magic '%s' found\n", magic_mcode));
            return 0;
        }
    }
    return 1;
}

int vz_floppy_r(int offset)
{
    int data = 0xff;
	switch( offset )
	{
	case 0: /* latch (write-only) */
		break;
	case 1: /* data (read-only? I don't believe the docs here.. :) */
		data = vz_floppy_data[vz_floppy_count++];
        break;
	case 2: /* polling (read-only) */
		/* fake */
		data &= (cpu_getscanline() & 1) ? ~0x00 : ~0x80;
		break;
	case 3: /* write protect status (read-only) */
		data &= ~0x80;
        break;
	}
	LOG((errorlog,"vz_floppy_r %d $%02X '%c'\n", offset, data, (data >= 32 && data < 128) ? data : '.'));
    return data;
}

void vz_floppy_w(int offset, int data)
{
	LOG((errorlog,"vz_floppy_w %d $%02X '%c'\n", offset, data, (data >= 32 && data < 128) ? data : '.'));
    switch( offset )
	{
	case 0: /* latch (write-only) */
		vz_floppy_latch = data;
		break;
	case 1: /* data (read-write, I'm pretty sure :) */
		if( vz_floppy_latch & 0x20 )	/* write data high ? */
		{
			vz_floppy_data[vz_floppy_count++] = data;
			if( !vz_floppy_count )
			{
				/* write the sector */
			}
		}
		else
		if( !(vz_floppy_latch & 0x40) ) /* write request low? */
		{
			/* check commands and read sector, prepare write sector etc. */
		}
        break;
    }
}

int vz_joystick_r(int offset)
{
    int data = 0xff;

    /* Joystick enabled? */
    if( readinputport(0) & 0x20 )
    {
        if( !(offset & 1) )
            data &= readinputport(10);
        if( !(offset & 2) )
            data &= readinputport(11);
        if( !(offset & 4) )
            data &= readinputport(12);
        if( !(offset & 8) )
            data &= readinputport(13);
    }

    return data;
}

#define KEY_INV 0x80
#define KEY_RUB 0x40
#define KEY_LFT 0x20
#define KEY_DN  0x10
#define KEY_RGT 0x08
#define KEY_BSP 0x04
#define KEY_UP  0x02
#define KEY_INS 0x01

int vz_keyboard_r(int offset)
{
    int data = 0xff;

    if( !(offset & 0x01) )
    {
        data &= readinputport(1);
    }
    if( !(offset & 0x02) )
    {
        data &= readinputport(2);
        /* Joystick disabled ? */
        if( !(readinputport(0) & 0x20) )
			/* extra keys pressed? */
			if( readinputport(9) != 0xff )
                data &= ~0x04;
    }
    if( !(offset & 0x04) )
    {
        data &= readinputport(3);
    }
    if( !(offset & 0x08) )
    {
        data &= readinputport(4);
    }
    if( !(offset & 0x10) )
    {
        data &= readinputport(5);
        /* Joystick disabled ? */
        if( !(readinputport(0) & 0x20) )
		{
			int key = readinputport(9);
            /* easy cursor keys */
			data &= key | ~(KEY_UP|KEY_DN|KEY_LFT|KEY_RGT);
			/* backspace does cursor left too */
			if( !(key & KEY_BSP) )
				data &= ~KEY_LFT;
		}
    }
    if( !(offset & 0x20) )
    {
        data &= readinputport(6);
    }
    if( !(offset & 0x40) )
    {
        data &= readinputport(7);
    }
    if( !(offset & 0x80) )
    {
        data &= readinputport(8);
		/* Joystick disabled ? */
        if( !(readinputport(0) & 0x20) )
		{
			int key = readinputport(9);
			if( !(key & KEY_INV) )
				data &= ~0x04;
			if( !(key & KEY_RUB) )
                data &= ~0x10;
			if( !(key & KEY_INS) )
				data &= ~0x02;
        }
    }

    if( cpu_getscanline() >= 16*12 )
        data &= ~0x80;

    /* cassette input would be bit 5 (0x40) */

    return data;
}

/*************************************************
 * bit  function
 * 7-6  not assigned
 * 5    speaker B
 * 4    VDC background 0 green, 1 orange
 * 3    VDC display mode 0 text, 1 graphics
 * 2    cassette out (MSB)
 * 1    cassette out (LSB)
 * 0    speaker A
 ************************************************/
void vz_latch_w(int offset, int data)
{
    int dac = 0;

    LOG((errorlog, "vz_latch_w $%02X\n", data));
    /* dirty all if the mode or the background color are toggled */
	if( (vz_latch ^ data) & 0x10 )
	{
		LOG((errorlog, "vz_latch_w: change background %d", (data>>4)&1));
		memset(dirtybuffer, 1, videoram_size);
	}
	if( (vz_latch ^ data) & 0x08 )
	{
		LOG((errorlog, "vz_latch_w: change mode to %s", (data&0x08)?"gfx":"text"));
		memset(dirtybuffer, 1, videoram_size);
    }
    vz_latch = data;

	/* cassette output bits */
    dac = (vz_latch & 0x06) * 8;

	/* speaker B push */
    if( vz_latch & 0x20 )
        dac += 48;
	/* speaker B pull */
    if( vz_latch & 0x01 )
        dac -= 48;

    DAC_signed_data_w(0, dac);
}


