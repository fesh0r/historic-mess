/***************************************************************************


***************************************************************************/
#include <ctype.h>
#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "mess/machine/vc20.h"
#include "mess/machine/c1551.h"
#include "mess/machine/6522via.h"

CBM_Drive vc20_drive8, vc20_drive9;

static UINT8 keyboard[8]= { 0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff };
static int via1_portb, via0_ca2;
//static int via0_porta, via0_portb, via0cb2, via1_porta, via1ca2, via1cb2;

UINT8 *vc20_memory;
UINT8 *vc20_memory_9400;

/** via 0 addr 0x9110
 ca1 restore key (low)
 ca2 cassette motor on
 pa0 serial clock in
 pa1 serial data in
 pa2 also joy 0 in
 pa3 also joy 1 in
 pa4 also joy 2 in
 pa5 also joybutton/lightpen in
 pa6 cassette switch in
 pa7 inverted serial atn out
 pa2 till pa6, port b, cb1, cb2 userport
 irq connected to m6502 nmi
*/
static void via0_irq (int level)
{
  cpu_set_nmi_line(0, level);
}

static int via0_read_ca1(int offset)
{
  return (KEYBOARD_EXTRA&KEY_RESTORE) ? 0 : 1;
}

static int via0_read_ca2(int offset)
{
  DBG_LOG(1,"tape",(errorlog,"motor read %d\n", via0_ca2));
  return via0_ca2;
}

static void via0_write_ca2(int offset, int data)
{
  via0_ca2=data?1:0;
  vc20_tape_motor(via0_ca2);
}

static int via0_read_porta(int offset)
{
  int value = 0xff;
  if( JOYSTICK )
    value &= readinputport(0) | JOY_VIA0_IGNORE;
  if( PADDLES )
    value &= readinputport(1) | JOY_VIA0_IGNORE;
  // to short to be recognized normally
  // should be reduced to about 1 or 2 microseconds
  //  if(LIGHTPEN_BUTTON) value&=~0x20;
  if( !cbm_serial_clock_read()) value&=~1;
  if( !cbm_serial_data_read()) value&=~2;
  DBG_LOG(1,"serial in",(errorlog,"%s %s\n",value&1?"clock":"", 
			 value&2?"data":""));
  if( !vc20_tape_switch() ) value &=~0x40;
  return value;
}

static void via0_write_porta(int offset, int data)
{
  cbm_serial_atn_write(!(data&0x80));
  DBG_LOG(1,"serial out",(errorlog,"atn %s\n",!(data&0x80)?"high":"low"));
}

/* via 1 addr 0x9120
 port a input from keyboard (low key pressed in line of matrix
 port b select lines of keyboard matrix (low)
	  pb7 also joystick right in! (low)
	  pb3 also cassette write
 ca1 cassette read
 ca2 inverted serial clk out
 cb1 serial srq in
 cb2 inverted serial data out
 irq connected to m6502 irq
 */
static void via1_irq(int level)
{
  cpu_set_irq_line(0, M6502_INT_IRQ, level);
}

static int via1_read_porta(int offset)
{
  int value = 0xff;
  
  if( !(via1_portb & 0x01) )
    value &= keyboard[0];
  
  if( !(via1_portb & 0x02) )
    value &= keyboard[1];

  if( !(via1_portb & 0x04) )
    value &= keyboard[2];
  
  if( !(via1_portb & 0x08) )
    value &= keyboard[3];

  if( !(via1_portb & 0x10) )
    value &= keyboard[4];
  
  if( !(via1_portb & 0x20) )
    value &= keyboard[5];

  if( !(via1_portb & 0x40) )
    value &= keyboard[6];

  if( !(via1_portb & 0x80) )
    value &= keyboard[7];
  
  return value;
}

static int via1_read_ca1(int offset)
{
  return vc20_tape_read();
}

static void via1_write_ca2(int offset, int data)
{
  cbm_serial_clock_write(!data);
  DBG_LOG(1,"serial out",(errorlog,"clock %s\n",!data?"high":"low"));
}

static int via1_read_portb(int offset)
{
  int value =0xff;
  if(JOYSTICK )
    value &= readinputport(0) | JOY_VIA1_IGNORE;
  if(PADDLES )
    value &= readinputport(1) | JOY_VIA1_IGNORE;
  
  return value;
}

static void via1_write_portb(int offset, int data)
{
//  if( errorlog ) fprintf(errorlog, "via1_write_portb: $%02X\n", data);
vc20_tape_write(data&8?1:0);
via1_portb = data;
}

static int via1_read_cb1(int offset)
{
  DBG_LOG(1,"serial in",(errorlog,"request read\n"));
  return cbm_serial_request_read();
}

static void via1_write_cb2(int offset, int data)
{
  DBG_LOG(1,"serial out",(errorlog,"data %s\n",!data?"high":"low"));
  cbm_serial_data_write(!data);
}

struct via6522_interface via0= {
  via0_read_porta,
  0,//via0_read_portb,
  via0_read_ca1,
  0,//via0_read_cb1,
  via0_read_ca2,
  0,//via0_read_cb2,
  via0_write_porta,
  0,//via0_write_portb,
  via0_write_ca2,
  0,//via0_write_cb2,
  via0_irq
}, via1= {
  via1_read_porta,
  via1_read_portb,
  via1_read_ca1,
  via1_read_cb1,
  0,//via1_read_ca2,
  0,//via1_read_cb2,
  0,//via1_write_porta,
  via1_write_portb,
  via1_write_ca2,
  via1_write_cb2,
  via1_irq
};

void vc20_write_9400(int offset, int data)
{
  vc20_memory_9400[offset]=data|0xf0;
}


int vic6560_dma_read_color(int offset)
{
  return vc20_memory_9400[offset&0x3ff];
}

int vic6560_dma_read(int offset)
{
  return vc20_memory[VIC6560ADDR2VC20ADDR(offset)];
}

static void vc20_common_driver_init(void)
{
  int i,j;
  
// vc20_memory[0x288]=0xff;// makes ae's graphics look correctly
// vc20_memory[0xd]=0xff; // for moneywars
  // 2114 poweron ? 64 x 0xff, 64x 0, and so on
  for (i=0;i<0x400;i+=0x40) {
    memset(vc20_memory+i,i&0x40?0:0xff,0x40);
    memset(vc20_memory_9400+i,0xf0|(i&0x40?0:0xf),0x40);
  }
// for (i=0x1000;i<0x2000;i+=0x40) memset(vc20_memory+i,i&0x40?0:0xff,0x40);

  // i think roms look like 0xff
  memset(vc20_memory+0x400,0xff,0x1000-0x400);
  memset(vc20_memory+0x2000,0xff,0x6000);
  memset(vc20_memory+0xa000,0xff,0x2000);

  cbm_drive_init();
  for (i = 0, j=0; i < Machine->gamedrv->num_of_floppy_drives; i++) {
    int rc;
    char *cp;
    /* no floppy name given for that drive ? */
    if (!options.floppy_name[i]) continue;
    if (!options.floppy_name[i][0]) continue;
    if ((cp=strrchr(options.floppy_name[0],'.'))!=NULL) {
      if (stricmp(cp+1,"d64")==0) {
        if (j==0) 
          rc=vc1541_d64_open(8,&vc20_drive8, options.floppy_name[i]);
        else 
          rc=vc1541_d64_open(9,&vc20_drive9, options.floppy_name[i]);
        if (rc==0) j++;
      }
    }
  }

  if (j==0) vc1541_fs_open(8,&vc20_drive8);
//  if (j<=1) vc1541_fs_open(9,&vc20_drive9);
        
  vc20_tape_open(cassette_name[0], via_1_ca1_w);
  via_config(0,&via0);
  via_config(1,&via1);

  vc20_rom_load();
}

// currently not used, but when time comes
void vc20_driver_shutdown(void)
{
  vc20_tape_close();
//  cbm_drive_close(&vc20_drive8);
//  cbm_drive_close(&vc20_drive9);
}

void vc20_driver_init(void)
{
  vc20_common_driver_init();
  vic6561_init(vic6560_dma_read, vic6560_dma_read_color);
}

void vic20_driver_init(void)
{
  vc20_common_driver_init();
  vic6560_init(vic6560_dma_read, vic6560_dma_read_color);
}

void vc20_init_machine(void)
{
  if (RAMIN0X0400) {
    install_mem_write_handler(0,0x400,0xfff,MWA_RAM);
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
  cbm_serial_reset_write(0);
  via_reset();
  via_0_ca1_w(0,via0_read_ca1(0) );
}

void vc20_shutdown_machine(void)
{
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
    fp = osd_fopen(Machine->gamedrv->name, rom_name[i], 
		   OSD_FILETYPE_IMAGE_R, 0);
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

int vc20_rom_id(const char *name, const char *gamename)
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
  via_0_ca1_w(0, via0_read_ca1(0) );
  keyboard[0] = KEYBOARD_ROW(0);
  
  keyboard[1] = KEYBOARD_ROW(1);
  
  keyboard[2] = KEYBOARD_ROW(2);
  if( KEYBOARD_EXTRA & KEY_CURSOR_LEFT )
    keyboard[2] &= ~0x80; /* CURSOR RIGHT */
  
  
  keyboard[3] = KEYBOARD_ROW(3);
  if( KEYBOARD_EXTRA & KEY_CURSOR_UP )
    keyboard[3] &= ~0x80; /* CURSOR DOWN */
  if( KEYBOARD_EXTRA & KEY_SHIFTLOCK )
    keyboard[3] &= ~0x02; /* LEFT SHIFT */

  keyboard[4] = KEYBOARD_ROW(4);
  if( KEYBOARD_EXTRA & (KEY_CURSOR_LEFT | KEY_CURSOR_UP) )
    keyboard[4] &= ~0x40; /* RIGHT SHIFT */
  
  keyboard[5] = KEYBOARD_ROW(5);
  
  keyboard[6] = KEYBOARD_ROW(6);
  
  keyboard[7] = KEYBOARD_ROW(7);

  vc20_tape_config(DATASSETTE,DATASSETTE_TONE);
  vc20_tape_buttons(DATASSETTE_PLAY,DATASSETTE_RECORD,DATASSETTE_STOP);
  osd_led_w(1/*KB_CAPSLOCK_FLAG*/,(KEYBOARD_EXTRA&KEY_SHIFTLOCK)?1:0);
  
  return ignore_interrupt ();
}
