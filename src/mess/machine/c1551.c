#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "driver.h"
#include "c1551.h"

#define VERBOSE_DBG 0      /* general debug messages */
/**************************************************************************
 * Logging
 * call the XXX_LOG with XXX_LOG("info",(errorlog,"%fmt\n",args));
 * where "info" can also be 0 to append .."%fmt",args to a line.
 **************************************************************************/
#define LOG(LEVEL,N,M,A)  \
	if( errorlog && (LEVEL>=N) ){ if( M )fprintf( errorlog,"%11.6f: %-24s",timer_get_time(),(char*)M ); fprintf##A; }

#if VERBOSE_DBG
#define DBG_LOG(n,m,a) LOG(VERBOSE_DBG,n,m,a)
#else
#define DBG_LOG(n,m,a)
#endif

/* tracks 1 to 35
   sectors number from 0 
   each sector holds 256 data bytes
   directory and Bitmap Allocation Memory in track 18
   sector 0:
    0: track# of directory begin (this linkage of sector often used)
	1: sector# of directory begin

    BAM entries (one per track)
     offset 0: # of free sectors
     offset 1: sector 0 (lsb) free to sector 7
	  offset 2: sector 8 to 15
	  offset 3: sector 16 to whatever the number to sectors in track is

  directory sector:
    0,1: track sector of next directory sector
    2, 34, 66, ... : 8 directory entries 

  directory entry:
    0: file type
	(0x = scratched/splat, 8x = alive, Cx = locked
				   where x: 0=DEL, 1=SEQ, 2=PRG, 3=USR, 4=REL)
	1,2: track and sector of file
	 3..18: file name padded with a0
	 19,20: REL files side sector
	 21: REL files record length
	28,29: number of blocks in file
	 ended with illegal track and sector numbers
*/

static void	vc1541_init(int devicenr, CBM_Drive *drive);

static struct {
  int count;
  CBM_Drive *drives[4];
  // whole + computer + drives
  int /*reset, request[6],*/ data[6], clock[6], atn[6];
} serial;


#define MAX_TRACKS 35
static int sectors_per_track[]={
 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21 ,21,
 19, 19, 19, 19, 19, 19, 19,
 18, 18, 18, 18, 18, 18,
 17, 17, 17, 17, 17
};

static int offset[MAX_TRACKS]; // offset of begin of track in d64 file

/* calculates offset to beginning of d64 file for sector beginning */
static int tracksector2offset(int track, int sector)
{
   return offset[track-1]+sector*256;
}

int c1551_d64_open(CBM_Drive *drive, char *imagename)
{
  FILE *in;
  int size;
  
  memset(drive, 0, sizeof(CBM_Drive));
  drive->interface=IEC;
  drive->drive=D64_IMAGE;
  if (!(in=osd_fopen(Machine->gamedrv->name, imagename, 
		     OSD_FILETYPE_IMAGE_R, 0)) ) {
    if (errorlog) fprintf(errorlog, " image %s not found\n",imagename);
    return 1;
  }
  size=osd_fsize(in);
  if ( !(drive->d.d64.image=malloc(size)) ) { osd_fclose(in); return 1; }
  if (size!=osd_fread(in, drive->d.d64.image, size)) { 
    osd_fclose(in); return 1; 
  }
  osd_fclose(in);
  
  if (errorlog) fprintf(errorlog, " image %s loaded\n", imagename);
  
  drive->d.d64.imagename=imagename;
  return 0;
}

int vc1541_d64_open(int devicenr, CBM_Drive *drive, char *imagename)
{
  int ret;
  serial.drives[serial.count++]=drive;
  ret=c1551_d64_open(drive,imagename);
  drive->interface=SERIAL;
  vc1541_init(devicenr,drive);
  return ret;
}

void cbm_drive_close(CBM_Drive *drive)
{
  if (drive->drive==D64_IMAGE) {
    if (drive->d.d64.image) free(drive->d.d64.image);
  }
}

static int compareNames(unsigned char *left, unsigned char *right)
{
  int i;
  for (i=0; i<16;i++) {
    if ( (left[i]=='*')||(right[i]=='*') ) return 1;
    if (left[i]==right[i]) continue;
    if ( (left[i]==0xa0)&&(right[i]==0) ) return 1;
    if ( (right[i]==0xa0)&&(left[i]==0) ) return 1;
    return 0;
  }
  return 1;
}

/* searches program with given name in directory
	delivers -1 if not found
	or pos in image of directory node */
static int find(CBM_Drive *drive, unsigned char *name)
{
  int pos, track, sector, i;
  
  pos=tracksector2offset(18,0);
  track=drive->d.d64.image[pos];
  sector=drive->d.d64.image[pos+1];
  
  while ( (track>=1)&&(track<=35) ) {
    pos=tracksector2offset(track, sector);
    for (i=2; i<256; i+=32) {
      if (drive->d.d64.image[pos+i]&0x80) {
	if ( stricmp((char*)name,(char*)"*")==0) return pos+i;
	if (compareNames(name,drive->d.d64.image+pos+i+3) ) return pos+i;
      }
    }
    track=drive->d.d64.image[pos];
    sector=drive->d.d64.image[pos+1];
  }
  return -1;
}

/* reads file into buffer */
static void readprg(CBM_Drive *c1551, int pos)
{
  int i;
  
  for (i=0; i<16;i++) 
    c1551->d.d64.filename[i]=toupper(c1551->d.d64.image[pos+i+3]);
  
  c1551->d.d64.filename[i]=0;
  
  pos=tracksector2offset(c1551->d.d64.image[pos+1],c1551->d.d64.image[pos+2]);
  

  i=pos; c1551->size=0; while (c1551->d.d64.image[i]!=0) {
    c1551->size+=254;
    i=tracksector2offset(c1551->d.d64.image[i], c1551->d.d64.image[i+1]);
  }
  c1551->size+=c1551->d.d64.image[i+1];
  
  fprintf(errorlog,"size %d",c1551->size);
  
  c1551->buffer=malloc(c1551->size);
  
  c1551->size--;
  fprintf(errorlog,"track: %d sector: %d\n",c1551->d.d64.image[pos+1],
	  c1551->d.d64.image[pos+2]);
  
  for (i=0; i<c1551->size; i+=254) {
    if (i+254<c1551->size) { // not last sector
      memcpy(c1551->buffer+i, c1551->d.d64.image+pos+2, 254);
      pos=tracksector2offset(c1551->d.d64.image[pos+0],
			     c1551->d.d64.image[pos+1]);
      fprintf(errorlog,"track: %d sector: %d\n",c1551->d.d64.image[pos],
	      c1551->d.d64.image[pos+1]);
    } else {
      memcpy(c1551->buffer+i, c1551->d.d64.image+pos+2, c1551->size-i);
    }
  }
}

/* reads directory into buffer */
static void read_directory(CBM_Drive *c1551)
{
  int pos, track, sector, i,j, blocksfree;
  
  c1551->buffer=malloc(8*18*25);
  c1551->size=0;
  
  pos=tracksector2offset(18,0);
  track=c1551->d.d64.image[pos];
  sector=c1551->d.d64.image[pos+1];
  
  blocksfree=0; for (j=1,i=4;j<=35; j++,i+=4) { 
    blocksfree+=c1551->d.d64.image[pos+i]; 
  }
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]=27;
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]='\"';
  for (j=0; j<16; j++)
    c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+0x90+j];
//memcpy(c1551->buffer+c1551->size,c1551->image+pos+0x90, 16);c1551->size+=16;
  c1551->buffer[c1551->size++]='\"';
  c1551->buffer[c1551->size++]=' ';
  c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+162];
  c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+163];
  c1551->buffer[c1551->size++]=' ';
  c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+165];
  c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+166];
  c1551->buffer[c1551->size++]=0;
  
  while ( (track>=1)&&(track<=35) ) {
    pos=tracksector2offset(track, sector);
    for (i=2; i<256; i+=32) {
      if (c1551->d.d64.image[pos+i]&0x80) {
	int len,blocks=c1551->d.d64.image[pos+i+28]
	  +256*c1551->d.d64.image[pos+i+29];
	char dummy[10];
	
	sprintf(dummy,"%d",blocks);
	len=strlen(dummy);
	
	c1551->buffer[c1551->size++]=29-len;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+i+28];
	c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+i+29];
	for (j=4;j>len;j--) c1551->buffer[c1551->size++]=' ';
	c1551->buffer[c1551->size++]='\"';
	for (j=0; j<16; j++)
	  c1551->buffer[c1551->size++]=c1551->d.d64.image[pos+i+3+j];
	c1551->buffer[c1551->size++]='\"';
	c1551->buffer[c1551->size++]=' ';
	switch (c1551->d.d64.image[pos+i]&0x3f) {
	case 0:
	  c1551->buffer[c1551->size++]='D';
	  c1551->buffer[c1551->size++]='E';
	  c1551->buffer[c1551->size++]='L';
	  break;
	case 1:
	  c1551->buffer[c1551->size++]='S';
	  c1551->buffer[c1551->size++]='E';
	  c1551->buffer[c1551->size++]='Q';
	  break;
	case 2:
	  c1551->buffer[c1551->size++]='P';
	  c1551->buffer[c1551->size++]='R';
	  c1551->buffer[c1551->size++]='G';
	  break;
	case 3:
	  c1551->buffer[c1551->size++]='U';
	  c1551->buffer[c1551->size++]='S';
	  c1551->buffer[c1551->size++]='R';
	  break;
	case 4:
	  c1551->buffer[c1551->size++]='R';
	  c1551->buffer[c1551->size++]='E';
	  c1551->buffer[c1551->size++]='L';
	  break;
	}
	c1551->buffer[c1551->size++]=0;
      }
    }
    track=c1551->d.d64.image[pos];
    sector=c1551->d.d64.image[pos+1];
  }
  c1551->buffer[c1551->size++]=14;
  c1551->buffer[c1551->size++]=0;
  c1551->buffer[c1551->size++]=blocksfree&0xff;
  c1551->buffer[c1551->size++]=blocksfree>>8;
  memcpy(c1551->buffer+c1551->size,"BLOCKS FREE", 11);c1551->size+=11;
  c1551->buffer[c1551->size++]=0;
  
  strcpy(c1551->d.d64.filename,"$");
}

static void c1551_d64_command(CBM_Drive *c1551)
{
  unsigned char name[20];
  int j, i, pos;
  
#if 1
  if (errorlog) {
    fprintf(errorlog,"floppycommand:");
    for (i=0; i<c1551->cmdpos;i++) 
      fprintf(errorlog,"%.2x",c1551->cmdbuffer[i]);
    for (i=0; i<c1551->cmdpos;i++) 
      fprintf(errorlog,"%c",c1551->cmdbuffer[i]);
    fprintf(errorlog,"\n");
  }
#endif
  /* i dont know the different commands,
     so i filter unknown commands out */
  i=0; while ( ((c1551->cmdbuffer[i]!=0x20)||(c1551->cmdbuffer[i+1]!=0xf0))
	       && (i<c1551->cmdpos) ) i++;
  
  if (i>=c1551->cmdpos) {
    c1551->i.iec.status=3;
    c1551->state=0;
    c1551->cmdpos=0;
    return;
  }
  for (j=0; j<c1551->cmdpos-i; j++)
    c1551->cmdbuffer[j]=c1551->cmdbuffer[j+i];
  
  c1551->cmdpos-=i;
//  memmove(c1551->cmdbuffer, c1551->cmdbuffer+i, c1551->cmdpos-i);

  if (c1551->cmdbuffer[3]==':')
    for (i=0; i<c1551->cmdpos-7;i++) name[i]=c1551->cmdbuffer[i+4];
  else
    for (i=0; i<c1551->cmdpos-5;i++) name[i]=c1551->cmdbuffer[i+2];
  
  name[i]=0;
  // eventuell mit 0xa0 auffuellen

  c1551->state=READING;c1551->pos=0;

  if ( stricmp((char*)name,(char*)"$")==0 ) {
    read_directory(c1551);
  } else {
    if ( (pos=find(c1551,name))==-1 ) {
      c1551->i.iec.status=3;
      c1551->state=0;
      c1551->cmdpos=0;
      return;
    }
    readprg(c1551,pos);
  }
  c1551->cmdpos=0;
}

#if 0
static int c1551_d64_read(CBM_Drive *c1551)
{
  return 0;
}

static void c1551_d64_status(CBM_Drive *c1551)
{
  if ((c1551->state==READING)&&(c1551->pos>=c1551->size)) {
    c1551->i.iec.status=3;
    c1551->state=0;
  }
}
#endif

int c1551_fs_open(CBM_Drive *c1551)
{
  memset(c1551, 0, sizeof(CBM_Drive));
  c1551->drive=FILESYSTEM;
  c1551->interface=IEC;
  return 0;
}

int vc1541_fs_open(int devicenr, CBM_Drive *drive)
{
  serial.drives[serial.count++]=drive;
  c1551_fs_open(drive);
  drive->interface=SERIAL;
  vc1541_init(devicenr, drive);
  return 0;
}

static void c1551_fs_command(CBM_Drive *c1551)
{
  FILE *fp;
  int read;
  char name[20];
  int j,i;
  
#if 1
  if (errorlog) {
    fprintf(errorlog,"floppycommand:");
    for (i=0; i<c1551->cmdpos;i++) fprintf(errorlog,"%.2x",c1551->cmdbuffer[i]);
    fprintf(errorlog,"\n");
  }
#endif
  
  /* i dont know the different commands,
     so i filter unknown commands out */
  i=0; while ( ((c1551->cmdbuffer[i]!=0x20)||(c1551->cmdbuffer[i+1]!=0xf0))
	       && (i<c1551->cmdpos) ) i++;
  
  if (i>=c1551->cmdpos) {
    c1551->i.iec.status=3;
    c1551->state=0;
    c1551->cmdpos=0;
    return;
  }
  for (j=0; j<c1551->cmdpos-i; j++)
    c1551->cmdbuffer[j]=c1551->cmdbuffer[j+i];
  
  c1551->cmdpos-=i;
//  memmove(c1551->cmdbuffer, c1551->cmdbuffer+i, c1551->cmdpos-i);

  if (c1551->cmdbuffer[3]==':')
    for (i=0; i<c1551->cmdpos-7;i++) name[i]=c1551->cmdbuffer[i+4];
  else
    for (i=0; i<c1551->cmdpos-5;i++) name[i]=c1551->cmdbuffer[i+2];
  
  strcpy(name+i,".prg");
  
  
  c1551->state=READING;c1551->pos=0;
  
  fp = osd_fopen(Machine->gamedrv->name, name, OSD_FILETYPE_IMAGE_R, 0);
  if(fp) {
    c1551->size = osd_fsize(fp);
    c1551->buffer=malloc(c1551->size);
    read=osd_fread(fp,c1551->buffer,c1551->size);
    osd_fclose(fp);
    if (errorlog) fprintf(errorlog,"loading file %s\n",name);
  } else {
    if (errorlog) fprintf(errorlog,"file %s not found\n",name);
    c1551->i.iec.status=3;
    c1551->state=0;
    c1551->cmdpos=0;
    return;
  }
  strcpy(c1551->d.fs.filename,name);
  c1551->cmdpos=0;
}

static int c1551_fs_read(CBM_Drive *c1551)
{
  int data=c1551->buffer[c1551->pos++];
  if (c1551->pos>=c1551->size) {
    c1551->state=0;
    free(c1551->buffer);
  }
  return data;
}

static void c1551_fs_status(CBM_Drive *c1551)
{
  if ((c1551->state==READING)&&(c1551->pos+1>=c1551->size)) {
//  free(c1551->buffer);
  c1551->i.iec.status=3;
//  c1551->read=0;
  }
}

/**

  7.1 Serial bus

   CBM Serial Bus Control Codes

	20	Talk
	3F	Untalk
	40	Listen
	5F	Unlisten
	60	Open Channel
	70	-
	80	-
	90	-
	A0	-
	B0	-
	C0	-
	D0	-
	E0	Close
	F0	Open



	 How the C1541 is called by the C64:

		read (drive 8)
		/28 /f0 filename /3f
		/48 /60 read data /5f
		/28 /e0 /3f

		write (drive 8)
		/28 /f0 filename /3f
		/28 /60 send data /3f
		/28 /e0 /3f

	 I used '/' to denote bytes sent under Attention (ATN low).

	28 == LISTEN command + device number 8
	f0 == secondary addres for OPEN file on channel 0

  Note that there's no acknowledge bit, but timeout/EOI handshake for each
  byte. Check the C64 Kernel for exact description...



 computer master

 for an read file operation dload
 computer write 20 f0 30 3a name 3f 40 60

 for an read operation load
 computer write 20 f0 name 3f 40 60

 0x20 0xe0 0x3f clear error status ?

 floppy drive delivers
 status 3 for file not found
 or filedata ended with status 3

 seldom double handshake with handshake lines
 and data value
 value 00 linehandshake value 0x8
 */
void c1551_write_data(CBM_Drive *c1551, int data)
{
  c1551->i.iec.data=data;
//  DBG_LOG(1,"c1551",(errorlog,"write data %.2x\n",data));
}

int c1551_read_data(CBM_Drive *c1551)
{
  int data=0xff;
  if (c1551->state==READING) {
    data=c1551_fs_read(c1551);
  }
//  DBG_LOG(1,"c1551",(errorlog,"read data %.2x\n",data));
  return data;
}


void c1551_write_handshake(CBM_Drive *c1551, int data)
{
  if (data!=c1551->i.iec.handshakein) {
    if(data)	{
      c1551->i.iec.handshakeout=0;
    } else {
      if (!c1551->state) {
	c1551->cmdbuffer[c1551->cmdpos]=c1551->i.iec.data;
	c1551->cmdpos++;	
//        DBG_LOG(1,"c1551",(errorlog,"taken data %.2x\n", c1551->data));
	if (c1551->i.iec.data==0x60) {
	  if (c1551->drive==D64_IMAGE) c1551_d64_command(c1551);
	  else if (c1551->drive==FILESYSTEM) c1551_fs_command(c1551);
	}
      }
      c1551->i.iec.handshakeout=1;
    }
  }
  c1551->i.iec.handshakein=data;
}


int c1551_read_handshake(CBM_Drive *c1551)
{
  return c1551->i.iec.handshakeout;
}


int c1551_read_status(CBM_Drive *c1551)
{
  int data=0;
  c1551_fs_status(c1551);
  data=c1551->i.iec.status;
  c1551->i.iec.status=0;
  return data;
}

/* must be called before other functions */
void cbm_drive_init(void)
{
  int i;
  
  offset[0]=0;
  for (i=1; i<=35; i++)
    offset[i]=offset[i-1]+sectors_per_track[i-1]*256;

  serial.count=0;
  for (i=0;i<sizeof(serial.atn)/sizeof(int);i++) {
    serial.atn[i]=
      serial.data[i]=
      serial.clock[i]=1;
  }
}

// delivers status for displaying
extern void cbm_drive_status(CBM_Drive *c1551,char *text, int size)
{
  text[0]=0;
#if 0
  if ((c1551->interface=SERIAL)&&(c1551->i.serial.device==8)) {
    sprintf(text,"state:%d %d %d",c1551->i.serial.state,c1551->i.serial.last,
	    c1551->i.serial.broadcast);
    return;
  }
#endif
  if (c1551->state==READING) {
    switch (c1551->drive) {
    case FILESYSTEM:
      sprintf(text,"Disk File %s loading %d",
	      c1551->d.fs.filename,c1551->size-c1551->pos-1);
      break;
    case D64_IMAGE:
      sprintf(text,"Disk (%s) File %s loading %d",
	      c1551->d.d64.imagename,c1551->d.d64.filename, 
	      c1551->size-c1551->pos-1);
      break;
    }
  }
}

static void vc1541_reset_write(CBM_Drive *vc1541, int level)
{
  if (level==0) {
    vc1541->i.serial.data=
      vc1541->i.serial.clock=
      vc1541->i.serial.atn=1;
    vc1541->i.serial.state=0;
  }
}

static void vc1541_init(int devicenr, CBM_Drive *drive)
{
  drive->interface=SERIAL;
  drive->i.serial.device=devicenr;
  vc1541_reset_write(drive,0);
}

static void vc1541_state(CBM_Drive *vc1541)
{
  int oldstate=vc1541->i.serial.state;
  switch (vc1541->i.serial.state) {
  case 0:
    if (!serial.atn[0]) {
      vc1541->i.serial.state=20;
      break;
    }
    if (!serial.clock[0]) {
      vc1541->i.serial.data=0;
      vc1541->i.serial.state++;
      break;
    }
    break;
  case 1:
    if (!serial.atn[0]) {
      vc1541->i.serial.state=20;
      break;
    }
    if (serial.clock[0]) {
      vc1541->i.serial.broadcast=0;
      vc1541->i.serial.data=1;
      vc1541->i.serial.state=10;
      vc1541->i.serial.value=0;
      vc1541->i.serial.time=timer_get_time();
      vc1541->i.serial.last=0;
      break;
    }
    break;

  case 10:
    if (serial.clock[0]) {
      vc1541->i.serial.broadcast=0;
      vc1541->i.serial.data=1;
      vc1541->i.serial.state++;
      vc1541->i.serial.value=0;
      vc1541->i.serial.time=timer_get_time();
      vc1541->i.serial.last=0;
      break;
    }
    break;
  case 11:
    if (!serial.clock[0]) {
      vc1541->i.serial.state=100;
      break;
    }
    if (timer_get_time()-vc1541->i.serial.time>200e-6) {
      vc1541->i.serial.data=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.last=1;
      break;
    }
    break;
  case 12:
    if (timer_get_time()-vc1541->i.serial.time>60e-6) {
      vc1541->i.serial.data=1;
      vc1541->i.serial.state++;
      break;
    }
    break;
  case 13:
    if (!serial.clock[0]) {
      vc1541->i.serial.state=100;
      break;
    }
    break;

  case 20:
#if 0
    if (serial.atn[0]) {
      vc1541->i.serial.time=timer_get_time();
      vc1541->i.serial.state=30;
      break;
    }
#endif
    if (serial.clock[0]) {
      vc1541->i.serial.broadcast=1;
      vc1541->i.serial.data=1;
      vc1541->i.serial.state++;
      vc1541->i.serial.value=0;
      vc1541->i.serial.time=timer_get_time();
      vc1541->i.serial.last=0;
      break;
    }
    break;

  case 21:
    if (!serial.clock[0]) {
      vc1541->i.serial.state=100;
      break;
    }
    if (timer_get_time()-vc1541->i.serial.time>200e-6) {
      //
      vc1541->i.serial.data=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.last=1;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 22:
    if (timer_get_time()-vc1541->i.serial.time>60e-6) {
      vc1541->i.serial.data=1;
      vc1541->i.serial.state++;
      break;
    }
    break;
  case 23:
    if (!serial.clock[0]) {
      vc1541->i.serial.state=100;
      break;
    }
    break;

  case 30: 
    if (serial.clock[0]) {
      vc1541->i.serial.state++;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 31:
    if (timer_get_time()-vc1541->i.serial.time>80e-6) {
      vc1541->i.serial.clock=1;
      vc1541->i.serial.data=1;
      vc1541->i.serial.state++;
      break;
    }
    break;
  case 32:
    if (serial.data[0]) {
      vc1541->i.serial.value=0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.data=(vc1541->i.serial.value&1)?1:0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 33:case 35:case 37:case 39:case 41:case 43:case 45:case 47:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.clock=1;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 34:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&2?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 36:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&4?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 38:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&8?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 40:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&0x10?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 42:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&0x20?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 44:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&0x40?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 46:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=vc1541->i.serial.value&0x80?1:0;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;  
  case 48:
    if (timer_get_time()-vc1541->i.serial.time>20e-6) {
      vc1541->i.serial.data=1;
      vc1541->i.serial.clock=0;
      vc1541->i.serial.state++;
      vc1541->i.serial.time=timer_get_time();
      break;
    }
    break;
  case 49:
    if (!serial.data[0]) {
      vc1541->i.serial.state++;
      break;
    }
    break;

  // bits to byte fitting
  case 100:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?1:0;
      vc1541->i.serial.state++;
    }
    break;
  case 101:case 103:case 105:case 107:case 109:case 111:case 113:
    if (!serial.clock[0]) vc1541->i.serial.state++;
    break;
  case 102:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?2:0;
      vc1541->i.serial.state++;
    }
    break;
  case 104:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?4:0;
      vc1541->i.serial.state++;
    }
    break;
  case 106:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?8:0;
      vc1541->i.serial.state++;
    }
    break;
  case 108:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?0x10:0;
      vc1541->i.serial.state++;
    }
    break;
  case 110:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?0x20:0;
      vc1541->i.serial.state++;
    }
    break;
  case 112:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?0x40:0;
      vc1541->i.serial.state++;
    }
    break;
  case 114:
    if (serial.clock[0]) {
      vc1541->i.serial.value|=serial.data[0]?0x80:0;
      DBG_LOG(1,"serial read",(errorlog,"%s %s %.2x\n",
			       vc1541->i.serial.broadcast?"broad":"",
			       vc1541->i.serial.last?"last":"",
			       vc1541->i.serial.value));
      vc1541->i.serial.state++;
    }
    break;
  case 115:
    if (!serial.clock[0]) {
      if (vc1541->i.serial.last) vc1541->i.serial.state=0;
      else if (vc1541->i.serial.broadcast) vc1541->i.serial.state=20;
      else vc1541->i.serial.state=10;
      vc1541->i.serial.data=0;
      vc1541->i.serial.time=timer_get_time();
    }
    break;
  }
  if (errorlog&&(oldstate!=vc1541->i.serial.state)) 
    fprintf(errorlog,"state %d->%d\n",oldstate,vc1541->i.serial.state);
}

static int vc1541_atn_read(CBM_Drive *vc1541)
{
  vc1541_state(vc1541);
  return vc1541->i.serial.atn;
}

static int vc1541_data_read(CBM_Drive *vc1541)
{
  vc1541_state(vc1541);
  return vc1541->i.serial.data;
}

static int vc1541_clock_read(CBM_Drive *vc1541)
{
  vc1541_state(vc1541);
  return vc1541->i.serial.clock;
}

static void vc1541_data_write(CBM_Drive *vc1541,int level)
{
//	DBG_LOG(1,"serial out",(errorlog,"dataout%s\n",level?"high":"low"));
  vc1541_state(vc1541);
}

static void vc1541_clock_write(CBM_Drive *vc1541,int level)
{
//	DBG_LOG(1,"serial out",(errorlog,"clockout%s\n",level?"high":"low"));
  vc1541_state(vc1541);
}

static void vc1541_atn_write(CBM_Drive *vc1541,int level)
{
//	DBG_LOG(1,"serial out",(errorlog,"atnout%s\n",level?"high":"low"));
  vc1541_state(vc1541);
}


/* bus handling */
void cbm_serial_reset_write(int level)
{
  int i;
  for (i=0;i<serial.count;i++) 
    vc1541_reset_write(serial.drives[i],level);
  // init bus signals
}

int cbm_serial_request_read(void)
{
  /* in c16 not connected */
  return 1;
}

void cbm_serial_request_write(int level)
{
}

int cbm_serial_atn_read(void)
{
  int i;
  serial.atn[0]=serial.atn[1];
  for (i=0;i<serial.count;i++)
    serial.atn[0]&=serial.atn[i+2]=vc1541_atn_read(serial.drives[i]);
  return serial.atn[0];
}

int cbm_serial_data_read(void)
{
  int i;
  serial.data[0]=serial.data[1];
  for (i=0;i<serial.count;i++)
    serial.data[0]&=serial.data[i+2]=vc1541_data_read(serial.drives[i]);
  return serial.data[0];
}

int cbm_serial_clock_read(void)
{
  int i;
  serial.clock[0]=serial.clock[1];
  for (i=0;i<serial.count;i++)
    serial.clock[0]&=serial.clock[i+2]=vc1541_clock_read(serial.drives[i]);
  return serial.clock[0];
}

void cbm_serial_data_write(int level)
{
  int i;
  serial.data[0]=
    serial.data[1]=level;
  // update line
  for (i=0;i<serial.count;i++) serial.data[0]&=serial.data[i+2];
  // inform drives
  for (i=0;i<serial.count;i++)
    vc1541_data_write(serial.drives[i],serial.data[0]);
}

void cbm_serial_clock_write(int level)
{
  int i;
  serial.clock[0]=
    serial.clock[1]=level;
  // update line
  for (i=0;i<serial.count;i++) serial.clock[0]&=serial.clock[i+2];
  // inform drives
  for (i=0;i<serial.count;i++)
    vc1541_clock_write(serial.drives[i],serial.clock[0]);
}

void cbm_serial_atn_write(int level)
{
  int i;
  serial.atn[0]=
    serial.atn[1]=level;
  // update line
  for (i=0;i<serial.count;i++) serial.atn[0]&=serial.atn[i+2];
  // inform drives
  for (i=0;i<serial.count;i++)
    vc1541_atn_write(serial.drives[i],serial.atn[0]);
}
