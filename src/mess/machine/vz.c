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

extern char frame_message[32];
extern int frame_time;

int vz_latch = 0;

#define TRKSIZE_VZ	0x9a0	/* arbitrary (actually from analyzing format) */
#define TRKSIZE_FM	3172	/* size of a standard FM mode track */

static void *vz_fdc_file[2] = {NULL, NULL};
static UINT8 vz_track_x2[2] = {80, 80};
static UINT8 vz_fdc_wrprot[2] = {0x80, 0x80};
static UINT8 vz_fdc_status = 0;
static UINT8 vz_fdc_data[TRKSIZE_FM];
static int vz_data;
static int vz_fdc_edge = 0;
static int vz_fdc_bits = 8;
static int vz_drive = -1;
static int vz_fdc_start = 0;
static int vz_fdc_write = 0;
static int vz_fdc_offs = 0;
static int vz_fdc_latch = 0;

static void common_init_machine(void)
{
	/* Install DOS ROM ? */
    if( readinputport(0) & 0x40 )
    {
		void *dos;

		memset(vz_fdc_data, 0, TRKSIZE_FM);

        dos = osd_fopen(Machine->gamedrv->name, "vzdos.rom", OSD_FILETYPE_IMAGE_R, OSD_FOPEN_READ);
		if( dos )
        {
            int i;
			osd_fread(dos, &ROM[0x4000], 0x2000);
			osd_fclose(dos);
            for( i = 0; i < Machine->gamedrv->num_of_floppy_drives; i++ )
            {
                if( floppy_name[i][0] )
                {
					/* first try to open existing image RW */
					vz_fdc_wrprot[i] = 0x00;
					vz_fdc_file[i] = osd_fopen(Machine->gamedrv->name, floppy_name[i], OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_RW_CREATE);
					/* failed? */
                    if( !vz_fdc_file[i] )
					{
						/* try to open existing image RO */
                        vz_fdc_wrprot[i] = 0x80;
						vz_fdc_file[i] = osd_fopen(Machine->gamedrv->name, floppy_name[i], OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_READ);
					}
					/* failed? */
                    if( !vz_fdc_file[i] )
					{
						/* create new image RW */
                        vz_fdc_wrprot[i] = 0x00;
                        vz_fdc_file[i] = osd_fopen(Machine->gamedrv->name, floppy_name[i], OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_RW_CREATE);
                    }
                }
            }
        }
    }
}

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
	common_init_machine();
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
	common_init_machine();
}

void vz_shutdown_machine(void)
{
	int i;
	for( i = 0; i < Machine->gamedrv->num_of_floppy_drives; i++ )
	{
		if( vz_fdc_file[i] )
			osd_fclose(vz_fdc_file[i]);
		vz_fdc_file[i] = NULL;
	}
}

int vz_rom_id(const char *name, const char *gamename)
{
    const char magic_basic[] = "VZF0";
    const char magic_mcode[] = "  \000\000";
    char buff[4];
    void *file;

    file = osd_fopen(gamename, name, OSD_FILETYPE_IMAGE_RW, OSD_FOPEN_READ);
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

static void vz_get_track(void)
{
	sprintf(frame_message, "#%d get track %02d", vz_drive, vz_track_x2[vz_drive]/2);
	frame_time = 30;
    /* drive selected or and image file ok? */
	if( vz_drive >= 0 && vz_fdc_file[vz_drive] != NULL )
	{
		int size, offs;
		size = TRKSIZE_VZ;
		offs = TRKSIZE_VZ * vz_track_x2[vz_drive]/2;
		osd_fseek(vz_fdc_file[vz_drive], offs, SEEK_SET);
		size = osd_fread(vz_fdc_file[vz_drive], vz_fdc_data, size);
		LOG((errorlog,"get track @$%05x $%04x bytes\n", offs, size));
    }
	vz_fdc_offs = 0;
	vz_fdc_write = 0;
}

static void vz_put_track(void)
{
    /* drive selected and image file ok? */
	if( vz_drive >= 0 && vz_fdc_file[vz_drive] != NULL )
	{
		int size, offs;
		offs = TRKSIZE_VZ * vz_track_x2[vz_drive]/2;
		osd_fseek(vz_fdc_file[vz_drive], offs + vz_fdc_start, SEEK_SET);
		size = osd_fwrite(vz_fdc_file[vz_drive], &vz_fdc_data[vz_fdc_start], vz_fdc_write);
		LOG((errorlog,"put track @$%05X+$%X $%04X/$%04X bytes\n", offs, vz_fdc_start, size, vz_fdc_write));
    }
}

#define PHI0(n) (((n)>>0)&1)
#define PHI1(n) (((n)>>1)&1)
#define PHI2(n) (((n)>>2)&1)
#define PHI3(n) (((n)>>3)&1)

int vz_fdc_r(int offset)
{
    int data = 0xff;
    switch( offset )
    {
    case 1: /* data (read-only) */
        if( vz_fdc_bits > 0 )
        {
            if( vz_fdc_status & 0x80 )
                vz_fdc_bits--;
            data = (vz_data >> vz_fdc_bits) & 0xff;
#if 0
            LOG((errorlog,"vz_fdc_r bits %d%d%d%d%d%d%d%d\n",
                (data>>7)&1,(data>>6)&1,(data>>5)&1,(data>>4)&1,
                (data>>3)&1,(data>>2)&1,(data>>1)&1,(data>>0)&1 ));
#endif
        }
        if( vz_fdc_bits == 0 )
        {
            vz_data = vz_fdc_data[vz_fdc_offs];
            LOG((errorlog,"vz_fdc_r %d : data ($%04X) $%02X\n", offset, vz_fdc_offs, vz_data));
            if( vz_fdc_status & 0x80 )
            {
                vz_fdc_bits = 8;
                vz_fdc_offs = ++vz_fdc_offs % TRKSIZE_FM;
            }
            vz_fdc_status &= ~0x80;
        }
        break;
    case 2: /* polling (read-only) */
        /* fake */
        if( vz_drive >= 0 )
			vz_fdc_status |= 0x80;
        data = vz_fdc_status;
        break;
    case 3: /* write protect status (read-only) */
        if( vz_drive >= 0 )
            data = vz_fdc_wrprot[vz_drive];
        LOG((errorlog,"vz_fdc_r %d : write_protect $%02X\n", offset, data));
        break;
    }
    return data;
}

void vz_fdc_w(int offset, int data)
{
	int drive;

    switch( offset )
	{
	case 0: /* latch (write-only) */
		drive = (data & 0x10) ? 0 : (data & 0x80) ? 1 : -1;
		if( drive != vz_drive )
		{
			vz_drive = drive;
			if( vz_drive >= 0 )
				vz_get_track();
        }
        if( vz_drive >= 0 )
        {
			if( (PHI0(data) && !(PHI1(data) || PHI2(data) || PHI3(data)) && PHI1(vz_fdc_latch)) ||
				(PHI1(data) && !(PHI0(data) || PHI2(data) || PHI3(data)) && PHI2(vz_fdc_latch)) ||
				(PHI2(data) && !(PHI0(data) || PHI1(data) || PHI3(data)) && PHI3(vz_fdc_latch)) ||
				(PHI3(data) && !(PHI0(data) || PHI1(data) || PHI2(data)) && PHI0(vz_fdc_latch)) )
            {
				if( vz_track_x2[vz_drive] > 0 )
					vz_track_x2[vz_drive]--;
				LOG((errorlog,"vz_fdc_w(%d) $%02X drive %d: stepout track #%2d.%d\n", offset, data, vz_drive, vz_track_x2[vz_drive]/2,5*(vz_track_x2[vz_drive]&1)));
				if( (vz_track_x2[vz_drive] & 1) == 0 )
					vz_get_track();
            }
            else
			if( (PHI0(data) && !(PHI1(data) || PHI2(data) || PHI3(data)) && PHI3(vz_fdc_latch)) ||
				(PHI1(data) && !(PHI0(data) || PHI2(data) || PHI3(data)) && PHI0(vz_fdc_latch)) ||
				(PHI2(data) && !(PHI0(data) || PHI1(data) || PHI3(data)) && PHI1(vz_fdc_latch)) ||
				(PHI3(data) && !(PHI0(data) || PHI1(data) || PHI2(data)) && PHI2(vz_fdc_latch)) )
            {
				if( vz_track_x2[vz_drive] < 2*40 )
					vz_track_x2[vz_drive]++;
				LOG((errorlog,"vz_fdc_w(%d) $%02X drive %d: stepin track #%2d.%d\n", offset, data, vz_drive, vz_track_x2[vz_drive]/2,5*(vz_track_x2[vz_drive]&1)));
				if( (vz_track_x2[vz_drive] & 1) == 0 )
					vz_get_track();
            }
            if( (data & 0x40) == 0 )
			{
				vz_data <<= 1;
				if( (vz_fdc_latch ^ data) & 0x20 )
					vz_data |= 1;
                if( (vz_fdc_edge ^= 1) == 0 )
                {
					if( --vz_fdc_bits == 0 )
					{
						UINT8 value = 0;
						vz_data &= 0xffff;
						if( vz_data & 0x4000 ) value |= 0x80;
						if( vz_data & 0x1000 ) value |= 0x40;
						if( vz_data & 0x0400 ) value |= 0x20;
						if( vz_data & 0x0100 ) value |= 0x10;
						if( vz_data & 0x0040 ) value |= 0x08;
						if( vz_data & 0x0010 ) value |= 0x04;
						if( vz_data & 0x0004 ) value |= 0x02;
						if( vz_data & 0x0001 ) value |= 0x01;
						LOG((errorlog,"vz_fdc_w(%d) data($%04X) $%02X <- $%02X ($%04X)\n", offset, vz_fdc_offs, vz_fdc_data[vz_fdc_offs], value, vz_data));
						vz_fdc_data[vz_fdc_offs] = value;
						vz_fdc_offs = ++vz_fdc_offs % TRKSIZE_FM;
						vz_fdc_write++;
						vz_fdc_bits = 8;
					}
                }
            }
			/* change of write signal? */
            if( (vz_fdc_latch ^ data) & 0x40 )
            {
                /* falling edge? */
                if ( vz_fdc_latch & 0x40 )
                {
                    sprintf(frame_message, "#%d put track %02d", vz_drive, vz_track_x2[vz_drive]/2);
					frame_time = 30;
                    vz_fdc_start = vz_fdc_offs;
                    vz_fdc_edge = 0;
                }
                else
                {
                    /* data written to track before? */
                    if( vz_fdc_write )
                        vz_put_track();
                }
                vz_fdc_bits = 8;
                vz_fdc_write = 0;
            }
        }
        vz_fdc_latch = data;
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
	if( (vz_latch ^ data) & 0x18 )
	{
		extern int bitmap_dirty;
        bitmap_dirty = 1;
        if( (vz_latch ^ data) & 0x10 )
            LOG((errorlog, "vz_latch_w: change background %d", (data>>4)&1));
        if( (vz_latch ^ data) & 0x08 )
			LOG((errorlog, "vz_latch_w: change mode to %s", (data&0x08)?"gfx":"text"));
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


