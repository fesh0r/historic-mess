/***************************************************************************

  machine.c

  Functions to emulate general aspects of the machine (RAM, ROM, interrupts,
  I/O ports)

***************************************************************************/

#include "driver.h"
#include "cpu/m6809/m6809.h"

int dragon_cart_inserted;
UINT8 *dragon_tape;
int dragon_tapesize;
UINT8 *dragon_rom;

/* from vidhrdw/dragon.c */
extern void coco3_ram_b1_w (int offset, int data);
extern void coco3_ram_b2_w (int offset, int data);
extern void coco3_ram_b3_w (int offset, int data);
extern void coco3_ram_b4_w (int offset, int data);
extern void coco3_ram_b5_w (int offset, int data);
extern void coco3_ram_b6_w (int offset, int data);
extern void coco3_ram_b7_w (int offset, int data);
extern void coco3_ram_b8_w (int offset, int data);

static int generic_rom_load(UINT8 *rambase, UINT8 *rombase)
{
	void *fp;

	typedef struct {
		UINT8 length_lsb;
		UINT8 length_msb;
		UINT8 start_lsb;
		UINT8 start_msb;
	} pak_header;

	fp = NULL;
	dragon_cart_inserted = 0;
	dragon_tape = NULL;

	if (strlen(rom_name[0])==0)
	{
		if (errorlog) fprintf(errorlog,"No image specified!\n");
	}
	else if (!(fp = osd_fopen (Machine->gamedrv->name, rom_name[0], OSD_FILETYPE_IMAGE_R, 0)))
	{
		if (errorlog) fprintf(errorlog,"Unable to locate image: %s\n",rom_name[0]);
		return 1;
	}

	if (fp!=NULL)
	{
		/* Is this a .PAK or a tape? */
		if ((strlen(rom_name[0]) < 4) || stricmp(&rom_name[0][strlen(rom_name[0])-4], ".PAK")) {
			/* Tape */
			dragon_tapesize = osd_fsize(fp);
			if ((dragon_tape = (UINT8 *)malloc(dragon_tapesize)) == NULL)
			{
				if (errorlog) fprintf(errorlog,"Not enough memory.\n");
				return 1;
			}
			osd_fread (fp, dragon_tape, dragon_tapesize);
		}
		else {
			/* PAK file */

			/* PAK files have the following format:
			 *
			 * length		(two bytes, little endian)
			 * base address (two bytes, little endian, typically 0xc000)
			 * ...data...
			 */
			int paklength;
			int pakstart;
			pak_header header;

			if (osd_fread(fp, &header, sizeof(header)) < sizeof(header))
			{
				if (errorlog) fprintf(errorlog,"Could not fully read PAK.\n");
				return 1;
			}

			paklength = (((int) header.length_msb) << 8) | (int) header.length_lsb;
			pakstart = (((int) header.start_msb) << 8) | (int) header.start_lsb;

			/* Since PAK files allow the flexibility of loading anywhere in
			 * the base RAM or ROM, we have to do tricks because in MESS's
			 * memory, RAM and ROM may be separated, hense this function's
			 * two parameters.
			 */

			/* Get the RAM portion */
			if (pakstart < 0x8000) {
				int ram_paklength;

				ram_paklength = (paklength > (0x8000 - pakstart)) ? (0x8000 - pakstart) : paklength;
				if (ram_paklength) {
					if (osd_fread(fp, &rambase[pakstart], ram_paklength) < ram_paklength)
					{
						if (errorlog) fprintf(errorlog,"Could not fully read PAK.\n");
						return 1;
					}
					pakstart += ram_paklength;
					paklength -= ram_paklength;
				}
			}

			/* Get the ROM portion */
			if (paklength) {
				if (osd_fread(fp, &rombase[pakstart - 0x8000], paklength) < paklength)
				{
					if (errorlog) fprintf(errorlog,"Could not fully read PAK.\n");
					return 1;
				}
				dragon_cart_inserted = 1;
			}

			/* One thing I _don_t_ do yet is set the program counter properly... */
		}
	}

	return 0;
}

int dragon_rom_load(void)
{
	return generic_rom_load(&ROM[0], &ROM[0x8000]);
}

int coco_rom_load(void)
{
	return generic_rom_load(&ROM[0], &ROM[0x10000]);
}

int coco3_rom_load(void)
{
	return generic_rom_load(&ROM[0x70000], &ROM[0x80000]);
}

int dragon_mapped_irq_r(int offset)
{
	return dragon_rom[0x3ff0 + offset];
}

void coco_speedctrl_w(int offset, int data)
{
	/* The infamous speed up poke. However, I can't seem to implement
	 * it. I would be doing the following if it were legal */
/*	Machine->drv->cpu[0].cpu_clock = ((offset & 1) + 1) * 894886; */
}

/***************************************************************************
  MMU
***************************************************************************/

extern unsigned char *RAM;

void coco_enable_64k_w(int offset, int data)
{
	if (offset) {
		cpu_setbank(1, &RAM[0x8000]);
		cpu_setbankhandler_w(1, MWA_BANK1);
	}
	else {
		cpu_setbank(1, dragon_rom);
		cpu_setbankhandler_w(1, MWA_ROM);
	}
}

/* Coco 3 */

int coco3_enable_64k;
int coco3_mmu[8];

static void coco3_mmu_update(int offset)
{
	typedef void (*writehandler)(int wh_offset, int data);

	static writehandler handlers[] = {
		coco3_ram_b1_w, coco3_ram_b2_w,
		coco3_ram_b3_w, coco3_ram_b4_w,
		coco3_ram_b5_w, coco3_ram_b6_w,
		coco3_ram_b7_w, coco3_ram_b8_w
	};

	cpu_setbank(offset + 1, &RAM[coco3_mmu[offset] * 0x2000]);
	cpu_setbankhandler_w(offset + 1, handlers[offset]);
}

void coco3_exposerom(void)
{
	extern int coco3_gimereg[];
	int mode;

	mode = coco3_gimereg[0] & 3;

	cpu_setbank(5, &dragon_rom[(mode != 3) ? 0x0000 : 0x8000]);
	cpu_setbankhandler_w(5, MWA_ROM);
	cpu_setbank(6, &dragon_rom[(mode != 3) ? 0x2000 : 0xa000]);
	cpu_setbankhandler_w(6, MWA_ROM);
	cpu_setbank(7, &dragon_rom[(mode == 2) ? 0x4000 : 0xc000]);
	cpu_setbankhandler_w(7, MWA_ROM);
	cpu_setbank(8, &dragon_rom[(mode == 2) ? 0x6000 : 0xe000]);
	cpu_setbankhandler_w(8, MWA_ROM);
}

int coco3_mmu_r(int offset)
{
	return coco3_mmu[offset];
}

void coco3_mmu_w(int offset, int data)
{

	data &= 0x3f;
	coco3_mmu[offset] = data;

	if ((offset < 4) || coco3_enable_64k)
		coco3_mmu_update(offset);
}

void coco3_enable_64k_w(int offset, int data)
{
	coco3_enable_64k = offset;

	if (offset) {
		coco3_mmu_update(4);
		coco3_mmu_update(5);
		coco3_mmu_update(6);
		coco3_mmu_update(7);
	}
	else {
		coco3_exposerom();
	}
}

/***************************************************************************
  Machine Initialization
***************************************************************************/

static void generic_init_machine(void)
{
	if (dragon_cart_inserted)
		cpu_set_irq_line(0, M6809_FIRQ_LINE, ASSERT_LINE);
}

void dragon_init_machine(void)
{
	generic_init_machine();
	dragon_rom = &ROM[0x8000];
}

void coco_init_machine(void)
{
	generic_init_machine();
	dragon_rom = &ROM[0x10000];
	coco_enable_64k_w(0, 0);
}

void coco3_init_machine(void)
{
	int i;

	generic_init_machine();
	dragon_rom = &ROM[0x80000];

	coco3_enable_64k_w(0, 0);
	for (i = 0; i < 7; i++)
		coco3_mmu_w(i, 56 + i);
}

/***************************************************************************
  Disk Controller
***************************************************************************/

static void *coco_disk_fd;
static int coco_disk_counter;
static int coco_disk_haltenable;
static int coco_disk_reading = 0;
static int coco_disk_writing = 0;

static void close_sector(void)
{
	if (coco_disk_fd) {
		osd_fclose(coco_disk_fd);
		coco_disk_fd = NULL;
	}
	coco_disk_reading = 0;
	coco_disk_writing = 0;
}

static void check_counter(void)
{
	if (--coco_disk_counter == 0) {
		if (coco_disk_haltenable) {
			m6809_set_nmi_line(1);
			m6809_set_nmi_line(0);
		}
		close_sector();
	}
}

static int open_sector(int drive, int track, int sector)
{
	close_sector();

	coco_disk_fd = osd_fopen(Machine->gamedrv->name,floppy_name[drive],OSD_FILETYPE_IMAGE_RW,0);
	if (!coco_disk_fd)
	{
		if (errorlog) fprintf(errorlog,"Couldn't open image.\n");
		return 1;
	}

	if (osd_fseek(coco_disk_fd,track * 0x1200 + ((sector-1) * 0x100),SEEK_CUR)!=0)
	{
		if (errorlog) fprintf(errorlog,"Couldn't find track/sector %d/%d.\n", track, sector);
		osd_fclose(coco_disk_fd);
		coco_disk_fd = NULL;
		return 1;
	}

	coco_disk_counter = 256;
	return 0;
}

static int sector_read(int *data)
{
	UINT8 d;

	if (!coco_disk_fd || (osd_fread(coco_disk_fd,&d,1) <1))
	{
		if (errorlog) fprintf(errorlog,"Couldn't read data from disk\n");
		return 1;
	}

	*data = d;
	check_counter();
	return 0;
}

static int sector_write(int data)
{
	UINT8 d;

	d = data;
	if (!coco_disk_fd || (osd_fwrite(coco_disk_fd,&d,1) <1))
	{
		if (errorlog) fprintf(errorlog,"Couldn't read data from disk\n");
		return 1;
	}

	check_counter();
	return 0;
}

/***************************************************************************/

typedef struct {
	int motor_on;
	int track;
	int sector;
} coco_drive;

static coco_drive coco_drives[4];

static coco_drive *find_drive(void)
{
	int i;
	for (i = 0; i < (sizeof(coco_drives) / sizeof(coco_drives[0])); i++) {
		if (coco_drives[i].motor_on)
			return &coco_drives[i];
	}
	return NULL;
}

/***************************************************************************/

static int coco_disk_status = 0;
static int coco_disk_datareg;

static void set_track(int track)
{
	coco_drive *d;

	d = find_drive();
	if (d) {
		d->track = track;
		coco_disk_status = track ? 0 : (1 << 2);
	}
	else {
		coco_disk_status = 0x80;
	}
}

int coco_disk_r(int offset)
{
	coco_drive *d;

	switch(offset) {
	case 8:
		/* status register */
		return coco_disk_status;
	case 9:
		/* track register */
		d = find_drive();
		return d ? d->track : 0;
	case 10:
		/* sector register */
		d = find_drive();
		return d ? d->sector : 0;
	case 11:
		/* data register */
		if (coco_disk_reading) {
			int data;
			coco_disk_status = sector_read(&data) ? 0x80 : (coco_disk_counter ? 0x02 : 0x00);
			return data;
		}
		break;
	default:
		if (errorlog) fprintf(errorlog,"Bad disk register read: offset=%d\n",offset);
	}
	return 0;
}

void coco_disk_w(int offset, int data)
{
	coco_drive *d;

	switch(offset) {
	case 0:
		/* DSKREG - the control register
		 *
		 * Bit
		 *	7 halt enable flag
		 *	6 drive select #3
		 *	5 density flag (0=single, 1=double)
		 *	4 write precompensation
		 *	3 drive motor activation
		 *	2 drive select #2
		 *	1 drive select #1
		 *	0 drive select #0
		 */
		d = find_drive();

		if (data & 0x01) {
			coco_drives[0].motor_on = (data & 0x08) ? 1 : 0;
		}
		if (data & 0x02) {
			coco_drives[1].motor_on = (data & 0x08) ? 1 : 0;
		}
		if (data & 0x04) {
			coco_drives[2].motor_on = (data & 0x08) ? 1 : 0;
		}
		if (data & 0x40) {
			coco_drives[3].motor_on = (data & 0x08) ? 1 : 0;
		}

		/* If we are selecting a different drive, close out this operation */
		if (d != find_drive()) {
			close_sector();
		}

		coco_disk_haltenable = (data & 0x80) ? 1 : 0;
		break;
	case 8:
		/* command register */
		switch(data) {
		case 0x03:	/* restore */
			set_track(0);
			break;
		case 0x14:	/* seek */
		case 0x17:	/* seek */
			set_track(coco_disk_datareg);
			break;
		case 0x23:	/* step */
			break;
		case 0x43:	/* step in */
			break;
		case 0x53:	/* step out */
			break;
		case 0x80:	/* read sector */
		case 0xa0:	/* write sector */
			d = find_drive();
			if (!d) {
				coco_disk_status = 0x80;
				return;
			}
			if (open_sector(d - coco_drives, d->track, d->sector)) {
				coco_disk_status = 0x80;
				return;
			}
			coco_disk_reading = (data == 0x80);
			coco_disk_writing = (data == 0xa0);
			coco_disk_status = 0x02;
			break;
		case 0xc0:	/* read address */
			break;
		case 0xe4:	/* read track */
			break;
		case 0xf4:	/* write track */
			break;
		case 0xd0:	/* force interrupt */
			break;
		default:
			if (errorlog) fprintf(errorlog,"Bad disk command: command=%i\n",data);
		}
		break;
	case 9:
		/* track register */
		break;
	case 10:
		/* sector register */
		d = find_drive();
		if (!d) {
			coco_disk_status = 0x80;
			return;
		}
		d->sector = data;
		break;
	case 11:
		/* data register */
		if (coco_disk_writing) {
			coco_disk_status = sector_write(data) ? 0x80 : (coco_disk_counter ? 0x02 : 0x00);
			return;
		}
		coco_disk_datareg = data;
		break;
	default:
		if (errorlog) fprintf(errorlog,"Bad disk register write: offset=%i data=%i\n",offset,data);
	}
}
