/***************************************************************************


***************************************************************************/
#include <ctype.h>
#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "mess/machine/vc20.h"
#include "6522via2.h"


static int via1_portb;
//static int via0_porta, via0_portb, via0ca2, via0cb2, via1_porta, via1ca2, via1cb2;

UINT8 *vc20_memory;
UINT8 *vc20_memory_1000;
UINT8 *vc20_memory_9400;

static void via0_irq (int level)
{
	if( errorlog ) fprintf(errorlog, "via0_irq(%d)\n", level);
    cpu_set_nmi_line(0, level);
}

static int via0_read_ca1(int offset)
{
	return KEY_RESTORE ? 0 : 1;
}

static int via0_read_porta(int offset)
{
	int value = 0xff;
	if( JOYSTICK )
		value &= readinputport(0) | JOY_VIA0_IGNORE;
	if( PADDLES )
		value &= readinputport(1);
	return value;
}

static void via1_irq(int level)
{
	if( errorlog ) fprintf(errorlog, "via1_irq(%d)\n", level);
    cpu_set_irq_line(0, M6502_INT_IRQ, level);
}

static int via1_read_porta(int offset)
{
	int value = 0xff;

	if( !(via1_portb & 0x01) )
		value &= KEYBOARD_ROW(0);

	if( !(via1_portb & 0x02) )
		value &= KEYBOARD_ROW(1);

	if( !(via1_portb & 0x04) )
	{
		value &= KEYBOARD_ROW(2);
		if( KEYBOARD_EXTRA & KEY_CURSOR_LEFT )
			value &= ~0x80; /* CURSOR RIGHT */
    }

	if( !(via1_portb & 0x08) )
	{
		value &= KEYBOARD_ROW(3);
		if( KEYBOARD_EXTRA & KEY_CURSOR_UP )
			value &= ~0x80; /* CURSOR DOWN */
		if( KEYBOARD_EXTRA & KEY_SHIFTLOCK )
			value &= ~0x02; /* LEFT SHIFT */
    }

	if( !(via1_portb & 0x10) )
	{
		value &= KEYBOARD_ROW(4);
		if( KEYBOARD_EXTRA & (KEY_CURSOR_LEFT | KEY_CURSOR_UP) )
			value &= ~0x40; /* RIGHT SHIFT */
    }

	if( !(via1_portb & 0x20) )
		value &= KEYBOARD_ROW(5);

	if( !(via1_portb & 0x40) )
		value &= KEYBOARD_ROW(6);

	if( !(via1_portb & 0x80) )
		value &= KEYBOARD_ROW(7);

	if( errorlog ) fprintf(errorlog, "via1_read_porta(%d): portb $%02X -> $%02X\n", offset, via1_portb, value);
    return value;
}

static int via1_read_portb(int offset)
{
	int value = 0xff;
	if( JOYSTICK )
		value &= readinputport(0) | JOY_VIA1_IGNORE;
	return value;
}

static void via1_write_portb(int offset, int data)
{
	if( errorlog ) fprintf(errorlog, "via1_write_portb: $%02X\n", data);
	via1_portb = data;
}

struct via6522_interface via0= {
	via0_read_porta,
	0,//via0_read_portb,
	via0_read_ca1,
	0,//via0_read_cb1,
	0,//via0_read_ca2,
	0,//via0_read_cb2,
	0,//via0_write_porta,
	0,//via0_write_portb,
	0,//via0_write_ca2,
	0,//via0_write_cb2,
	via0_irq
}, via1= {
	via1_read_porta,
	via1_read_portb,
	0,//via1_read_ca1,
	0,//via1_read_cb1,
	0,//via1_read_ca2,
	0,//via1_read_cb2,
	0,//via1_write_porta,
	via1_write_portb,
	0,//via1_write_ca2,
	0,//via1_write_cb2,
	via1_irq
};

/*************************************
 *
 *		Port handlers.
 *
 *************************************/
void vc20_write(int offset, int data)
{
	if (vc20_memory[offset]==data) return;
	vc20_memory[offset]=data;
	vic6560_addr_w(VC20ADDR2VIC6560ADDR(offset),data);
}

void vc20_write_0400(int offset, int data)
{
	if (vc20_memory[0x400+offset]==data) return;
		vc20_memory[0x400+offset]=data;
		vic6560_addr_w(VC20ADDR2VIC6560ADDR(0x0400+offset),data);
}

void vc20_write_1000(int offset, int data)
{
	if (vc20_memory_1000[offset]==data) return;
	vc20_memory_1000[offset]=data;
	vic6560_addr_w(VC20ADDR2VIC6560ADDR(0x1000+offset),data);
}
void vc20_write_9400(int offset, int data)
{
	if (vc20_memory_9400[offset]!=data) {
		vic6560_addr8_w(offset,data);
		vc20_memory_9400[offset]=data|0xf0;
	}
}

int vic6560_dma_read(int offset)
{
	int value=((int)vc20_memory_9400[offset&0x3ff]&0xf)<<8,
		addr=VIC6560ADDR2VC20ADDR(offset);
	if (addr<0x2000) return vc20_memory[addr]|value;
	if (addr<0x9000) return vc20_memory[addr]|value;
	return value|0xff;
}


static void vc20_common_init_machine(void)
{
	int i;

	// rams look like 0xff when powered on (2114)

	//	vc20_memory[0x288]=0xff;// makes ae's graphics look correctly
	//	vc20_memory[0xd]=0xff; // for moneywars
	for (i=0;i<0x400;i+=0x40) memset(vc20_memory+i,i&0x40?0:0xff,0x40);
//	for (i=0x1000;i<0x2000;i+=0x40) memset(vc20_memory+i,i&0x40?0:0xff,0x40);
	for (i=0x9400;i<0x9800;i+=0x40) memset(vc20_memory+i,i&0x40?0:0xff,0x40);
	for (i=0;i<0x400;i++) vc20_memory_9400[i]|=0xf0;

#if 0
	// i think roms look like 0xff
	for (i=0x400;i<0x1000;i++) vc20_memory[i]=0xff;
	for (i=0;i<0x2000;i++) {
		vc20_memory_2000[i]=0xff;
		vc20_memory_4000[i]=0xff;
		vc20_memory_6000[i]=0xff;
		vc20_memory_a000[i]=0xff;
	}
#endif

	if (RAMIN0X0400) {
		install_mem_write_handler(0,0x400,0xfff,vc20_write_0400);
		install_mem_read_handler(0,0x400,0xfff,MRA_RAM);
	} else {
		install_mem_write_handler(0,0x400,0xfff,MWA_NOP);
		install_mem_read_handler(0,0x400,0xfff,MRA_ROM);

	}
	if (RAMIN0X2000) {
		install_mem_write_handler(0,0x2000,0x3fff,MWA_RAM);
		install_mem_read_handler(0,0x2000,0x3fff,MRA_RAM);
	} else {
		install_mem_write_handler(0,0x2000,0x3fff,MWA_NOP);
		install_mem_read_handler(0,0x2000,0x3fff,MRA_ROM);

	}
	if (RAMIN0X4000) {
		install_mem_write_handler(0,0x4000,0x5fff,MWA_RAM);
		install_mem_read_handler(0,0x4000,0x5fff,MRA_RAM);
	} else {
		install_mem_write_handler(0,0x4000,0x5fff,MWA_NOP);
		install_mem_read_handler(0,0x4000,0x5fff,MRA_ROM);

	}
	if (RAMIN0X6000) {
		install_mem_write_handler(0,0x6000,0x7fff,MWA_RAM);
		install_mem_read_handler(0,0x6000,0x7fff,MRA_RAM);
	} else {
		install_mem_write_handler(0,0x6000,0x7fff,MWA_NOP);
		install_mem_read_handler(0,0x6000,0x7fff,MRA_ROM);

	}
	if (RAMIN0XA000) {
		install_mem_write_handler(0,0xa000,0xbfff,MWA_RAM);
		install_mem_read_handler(0,0xa000,0xbfff,MRA_RAM);
	} else {
		install_mem_write_handler(0,0xa000,0xbfff,MWA_NOP);
		install_mem_read_handler(0,0xa000,0xbfff,MRA_ROM);

	}
	via2_config(0,&via0);
	via2_config(1,&via1);
	via2_0_ca1_w(0,via0_read_ca1(0) );

	vc20_rom_load();
#if 0
	for (i = 0; i < Machine->gamedrv->num_of_floppy_drives; i++)
	{
		/* no floppy name given for that drive ? */
		if (!floppy_name[i]) continue;
		if (!floppy_name[i][0]) continue;
		pc_fdc_file[i] = osd_fopen(Machine->gamedrv->name, floppy_name[i], OSD_FILETYPE_IMAGE, 1);
		/* find the sectors/track and bytes/sector values in the boot sector */
		if (!pc_fdc_file[i] && Machine->gamedrv->clone_of)
			pc_fdc_file[i] = osd_fopen(Machine->gamedrv->clone_of->name, floppy_name[i], OSD_FILETYPE_IMAGE, 1);
		if (pc_fdc_file[i])
		{
		//
		}
	 }
#endif
}

void vic20_init_machine(void)
{
	vic6560_init(vic6560_dma_read);
	vc20_common_init_machine();
}

void vc20_init_machine(void)
{
	vic6561_init(vic6560_dma_read);
	vc20_common_init_machine();
}

void vc20_shutdown_machine(void)
{
#if 0
	int i;

	for (i = 0; i < Machine->gamedrv->num_of_floppy_drives; i++)
	{
		if (pc_fdc_file[i]) osd_fclose(pc_fdc_file[i]);
		pc_fdc_file[i] = 0;
	 }
#endif
}

int vc20_rom_load(void)
{
	 int result	= 0;
	 FILE *fp;
	 int size, i, read;
	 char *cp;
	 int addr=0;

	 for (i=0;(i<=2)&&(rom_name[i]!=NULL)&&(strlen(rom_name[i])!=0); i++) {

		 if(!vc20_rom_id(rom_name[i],rom_name[i])) continue;
		 fp = osd_fopen(Machine->gamedrv->name, rom_name[i], OSD_FILETYPE_IMAGE_R, 0);
		 if(!fp) {
			if (errorlog) fprintf(errorlog,"%s file not found\n",rom_name[i]);
			return 1;
		 }

		 size = osd_fsize(fp);

		 if ((cp=strrchr(rom_name[i],'.'))!=NULL) {
			if ((cp[1]!=0)&&(cp[2]=='0')&&(cp[3]==0)) {
				switch(toupper(cp[1])) {
				case 'A': addr=0xa000;break;
				case '2': addr=0x2000;break;
				case '4': addr=0x4000;break;
				case '6': addr=0x6000;break;
				}
			} else {
				if (stricmp(cp,".prg")==0) {
					unsigned short in;
					osd_fread_lsbfirst(fp,&in, 2);
					if (errorlog) fprintf(errorlog,"rom prg %.4x\n",in);
					addr=in;
					size-=2;
				}
			}
		 }
		 if (addr==0) {
			 if (size==0x4000) { // I think rom at 0x4000
				 addr=0x4000;
			 } else {
				addr=0xa000;
			 }
		 }

		 if (errorlog)
			fprintf(errorlog,"loading rom %s at %.4x size:%.4x\n",
						rom_name[i],addr, size);
		 read=osd_fread(fp,vc20_memory+addr,size);
		 osd_fclose(fp);
		 if (read!=size) return 1;
	 }
	 return result;
}

int 	vc20_rom_id(const char *name, const char *gamename)
{
	FILE *romfile;
	unsigned char magic[]={ 0x41,0x30,0x20,0xc3,0xc2,0xcd }; // A0 CBM at 0xa004 (module offset 4)
	unsigned char buffer[sizeof(magic)];
	char *cp;
	int retval;

	if (errorlog) fprintf(errorlog,"vc20_rom_id %s\n",gamename);
	if (!(romfile = osd_fopen (name, name, OSD_FILETYPE_IMAGE_R, 0))) {
		if (errorlog) fprintf(errorlog,"rom %s not found\n",name);
		return 0;
	}

	retval = 0;

	osd_fseek (romfile, 4, SEEK_SET);
	osd_fread (romfile, buffer, sizeof(magic));
	osd_fclose (romfile);

	if (!memcmp(buffer,magic,sizeof(magic))) retval=1;

	if ((cp=strrchr(rom_name[0],'.'))!=NULL) {
		if ((stricmp(cp+1,"a0")==0)
			||(stricmp(cp+1,"20")==0)
			||(stricmp(cp+1,"40")==0)
			||(stricmp(cp+1,"60")==0)
			||(stricmp(cp+1,"bin")==0)
			||(stricmp(cp+1,"rom")==0)
			||(stricmp(cp+1,"prg")==0) ) retval=1;
	}

	if (errorlog) {
		if (retval) fprintf(errorlog,"rom %s recognized\n",name);
		else fprintf(errorlog,"rom %s not recognized\n",name);
	}

	return retval;
}

int vc20_frame_interrupt (void)
{
	via2_0_ca1_w(0, via0_read_ca1(0) );

    return ignore_interrupt ();
}
