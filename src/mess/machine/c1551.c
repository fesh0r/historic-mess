#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "driver.h"
#include "c1551.h"

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

int c1551_d64_open(c1551_Drive *drive, char *imagename)
{
   FILE *in;
   int size;

   memset(drive, 0, sizeof(c1551_Drive));
	drive->type=C1551_D64_IMAGE;
	if (!(in=osd_fopen(Machine->gamedrv->name, imagename, OSD_FILETYPE_IMAGE_R, 0)) ) {
		if (errorlog) fprintf(errorlog, " image %s not found\n",imagename);
		return 1;
	}
	size=osd_fsize(in);
	if ( !(drive->image=malloc(size)) ) { osd_fclose(in); return 1; }
	if (size!=osd_fread(in, drive->image, size)) { osd_fclose(in); return 1; }
	osd_fclose(in);

	if (errorlog) fprintf(errorlog, " image %s loaded\n", imagename);
   return 0;
}

void c1551_close(c1551_Drive *drive)
{
    if (drive->image) free(drive->image);
}

static int compareNames(unsigned char *left, unsigned char *right)
{
	int i;
	for (i=0; i<16;i++) {
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
static int find(c1551_Drive *drive, unsigned char *name)
{
	int pos, track, sector, i;

	pos=tracksector2offset(18,0);
	track=drive->image[pos];
	sector=drive->image[pos+1];

	while ( (track>=1)&&(track<=35) ) {
		pos=tracksector2offset(track, sector);
		for (i=2; i<256; i+=32) {
			if (drive->image[pos+i]&0x80) {
				if ( stricmp((char*)name,(char*)"*")==0) return pos+i;
				if (compareNames(name,drive->image+pos+i+3) ) return pos+i;
			}
		}
		track=drive->image[pos];
		sector=drive->image[pos+1];
	}
	return -1;
}

/* reads file into buffer */
static void readprg(c1551_Drive *c1551, int pos)
{
	int i;

	pos=tracksector2offset(c1551->image[pos+1],c1551->image[pos+2]);

	i=pos; c1551->size=0; while (c1551->image[i]!=0) {
		c1551->size+=254;i=tracksector2offset(c1551->image[i], c1551->image[i+1]);
	}
	c1551->size+=c1551->image[i+1];

	fprintf(errorlog,"size %d",c1551->size);

	c1551->buffer=malloc(c1551->size);

	c1551->size--;
	fprintf(errorlog,"track: %d sector: %d\n",c1551->image[pos+1],c1551->image[pos+2]);

	for (i=0; i<c1551->size; i+=254) {
		if (i+254<c1551->size) { // not last sector
			memcpy(c1551->buffer+i, c1551->image+pos+2, 254);
			pos=tracksector2offset(c1551->image[pos+0],c1551->image[pos+1]);
			fprintf(errorlog,"track: %d sector: %d\n",c1551->image[pos],c1551->image[pos+1]);
		} else {
			memcpy(c1551->buffer+i, c1551->image+pos+2, c1551->size-i);
		}
	}
}

/* reads directory into buffer */
static void read_directory(c1551_Drive *c1551)
{
	int pos, track, sector, i,j, blocksfree;

	c1551->buffer=malloc(8*18*25);
	c1551->size=0;

	pos=tracksector2offset(18,0);
	track=c1551->image[pos];
	sector=c1551->image[pos+1];

	blocksfree=0; for (j=1,i=4;j<=35; j++,i+=4) { blocksfree+=c1551->image[pos+i]; }
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=27;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]='\"';
	for (j=0; j<16; j++)
		c1551->buffer[c1551->size++]=c1551->image[pos+0x90+j];
//	memcpy(c1551->buffer+c1551->size,c1551->image+pos+0x90, 16);c1551->size+=16;
	c1551->buffer[c1551->size++]='\"';
	c1551->buffer[c1551->size++]=' ';
	c1551->buffer[c1551->size++]=c1551->image[pos+162];
	c1551->buffer[c1551->size++]=c1551->image[pos+163];
	c1551->buffer[c1551->size++]=' ';
	c1551->buffer[c1551->size++]=c1551->image[pos+165];
	c1551->buffer[c1551->size++]=c1551->image[pos+166];
	c1551->buffer[c1551->size++]=0;

	while ( (track>=1)&&(track<=35) ) {
		pos=tracksector2offset(track, sector);
		for (i=2; i<256; i+=32) {
			if (c1551->image[pos+i]&0x80) {
				int len,blocks=c1551->image[pos+i+28]+256*c1551->image[pos+i+29];
				char dummy[10];

				sprintf(dummy,"%d",blocks);
				len=strlen(dummy);

				c1551->buffer[c1551->size++]=29-len;
				c1551->buffer[c1551->size++]=0;
				c1551->buffer[c1551->size++]=c1551->image[pos+i+28];
				c1551->buffer[c1551->size++]=c1551->image[pos+i+29];
				for (j=4;j>len;j--) c1551->buffer[c1551->size++]=' ';
				c1551->buffer[c1551->size++]='\"';
				for (j=0; j<16; j++)
					c1551->buffer[c1551->size++]=c1551->image[pos+i+3+j];
				c1551->buffer[c1551->size++]='\"';
				c1551->buffer[c1551->size++]=' ';
				switch (c1551->image[pos+i]&0x3f) {
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
		track=c1551->image[pos];
		sector=c1551->image[pos+1];
	}
	c1551->buffer[c1551->size++]=14;
	c1551->buffer[c1551->size++]=0;
	c1551->buffer[c1551->size++]=blocksfree&0xff;
	c1551->buffer[c1551->size++]=blocksfree>>8;
	memcpy(c1551->buffer+c1551->size,"BLOCKS FREE", 11);c1551->size+=11;
	c1551->buffer[c1551->size++]=0;
}

static void c1551_d64_command(c1551_Drive *c1551)
{
	unsigned char name[20];
	int j, i, pos;

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
		c1551->status=3;
		c1551->read=0;
		c1551->cmdpos=0;
		return;
	}
	for (j=0; j<c1551->cmdpos-i; j++)
		c1551->cmdbuffer[j]=c1551->cmdbuffer[j+i];

	c1551->cmdpos-=i;
//	memmove(c1551->cmdbuffer, c1551->cmdbuffer+i, c1551->cmdpos-i);

	if (c1551->cmdbuffer[3]==':')
		for (i=0; i<c1551->cmdpos-7;i++) name[i]=c1551->cmdbuffer[i+4];
	else
		for (i=0; i<c1551->cmdpos-5;i++) name[i]=c1551->cmdbuffer[i+2];

	name[i]=0;
	// eventuell mit 0xa0 auffuellen

	c1551->read=1;c1551->readpos=0;

	if ( stricmp((char*)name,(char*)"$")==0 ) {
		read_directory(c1551);
	} else {
		if ( (pos=find(c1551,name))==-1 ) {
			c1551->status=3;
			c1551->read=0;
			c1551->cmdpos=0;
			return;
		}
		readprg(c1551,pos);
	}
	c1551->cmdpos=0;
}

#if 0
static int c1551_d64_read(c1551_Drive *c1551)
{
	return 0;
}

static void c1551_d64_status(c1551_Drive *c1551)
{
	if ((c1551->read)&&(c1551->readpos>=c1551->size)) { 
		c1551->status=3; 
		c1551->read=0; 
	} 
}
#endif

int c1551_fs_open(c1551_Drive *c1551)
{
   memset(c1551, 0, sizeof(c1551_Drive));
   c1551->type=C1551_FILESYSTEM;
   return 0;
}

static void c1551_fs_command(c1551_Drive *c1551) 
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
		c1551->status=3; 
		c1551->read=0; 
		c1551->cmdpos=0; 
		return; 
	} 
	for (j=0; j<c1551->cmdpos-i; j++) 
		c1551->cmdbuffer[j]=c1551->cmdbuffer[j+i]; 
 
	c1551->cmdpos-=i; 
//	memmove(c1551->cmdbuffer, c1551->cmdbuffer+i, c1551->cmdpos-i); 
 
	if (c1551->cmdbuffer[3]==':') 
		for (i=0; i<c1551->cmdpos-7;i++) name[i]=c1551->cmdbuffer[i+4]; 
	else 
		for (i=0; i<c1551->cmdpos-5;i++) name[i]=c1551->cmdbuffer[i+2]; 
 
	strcpy(name+i,".prg"); 
 
 
	c1551->read=1;c1551->readpos=0; 
 
	fp = osd_fopen(Machine->gamedrv->name, name, OSD_FILETYPE_IMAGE_R, 0); 
	if(fp) { 
		c1551->size = osd_fsize(fp);
		c1551->buffer=malloc(c1551->size); 
		read=osd_fread(fp,c1551->buffer,c1551->size); 
		osd_fclose(fp); 
		if (errorlog) fprintf(errorlog,"loading file %s\n",name); 
	} else { 
		if (errorlog) fprintf(errorlog,"file %s not found\n",name); 
		c1551->status=3; 
		c1551->read=0; 
		c1551->cmdpos=0; 
		return; 
	} 

	c1551->cmdpos=0; 
} 

static int c1551_fs_read(c1551_Drive *c1551)
{
	int data=c1551->buffer[c1551->readpos++];
	if (c1551->readpos>=c1551->size) {
		c1551->read=0;
		free(c1551->buffer);
	}
	return data;

}

static void c1551_fs_status(c1551_Drive *c1551)
{
	if ((c1551->read)&&(c1551->readpos+1>=c1551->size)) {
//		free(c1551->buffer);
		c1551->status=3;
//		c1551->read=0;
	}
}

/** 
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
void c1551_write_data(c1551_Drive *c1551, int data) 
{ 
	c1551->data=data; 
//	DBG_LOG(1,"c1551",(errorlog,"write data %.2x\n",data)); 
} 
 
int c1551_read_data(c1551_Drive *c1551) 
{ 
	int data=0xff;  
	if (c1551->read) {
		data=c1551_fs_read(c1551);
	}
//	DBG_LOG(1,"c1551",(errorlog,"read data %.2x\n",data)); 
	return data; 
} 
 
 
void c1551_write_handshake(c1551_Drive *c1551, int data) 
{ 
	if (data!=c1551->handshakein) { 
		if(data)	{ 
			c1551->handshakeout=0; 
		} else { 
			if (!c1551->read) { 
				c1551->cmdbuffer[c1551->cmdpos]=c1551->data;
				c1551->cmdpos++; 
 
//				DBG_LOG(1,"c1551",(errorlog,"taken data %.2x\n", c1551->data)); 
				if (c1551->data==0x60) { 
					if (c1551->type==C1551_D64_IMAGE) c1551_d64_command(c1551);
					else if (c1551->type==C1551_FILESYSTEM) c1551_fs_command(c1551); 
				}
			} 
			c1551->handshakeout=1; 
		} 
	} 
	c1551->handshakein=data; 
} 
 
 
int c1551_read_handshake(c1551_Drive *c1551) 
{ 
	return c1551->handshakeout; 
} 
 
 
int c1551_read_status(c1551_Drive *c1551) 
{ 
	int data=0; 
	c1551_fs_status(c1551);
	data=c1551->status; 
	c1551->status=0; 
	return data; 
} 

/* must be called before other functions */
void c1551_init(void)
{
   int i;

   offset[0]=0;
   for (i=1; i<=35; i++) 
	offset[i]=offset[i-1]+sectors_per_track[i-1]*256;
}

