#ifndef __C1551_H_
#define __C1551_H_

/* must be called before other functions */
void c1551_init(void);

/* data for one drive */
typedef struct {
#define C1551_D64_IMAGE 0
#define C1551_FILESYSTEM 1
   int type;

   int handshakein, handshakeout; 
   int data; 
   int status; 

   int cmdbuffer[32], cmdpos; 

   int read;  //flag reading active
   int readpos, size;
   unsigned char *buffer; 

   unsigned char *image; // d64 image
} c1551_Drive;

/* open an d64 image */
int c1551_d64_open(c1551_Drive *drive, char *imagename);

/* load *.prg files directy from filesystem (rom directory) */
int c1551_fs_open(c1551_Drive *drive);

void c1551_close(c1551_Drive *drive);

void c1551_write_data(c1551_Drive *c1551, int data);
int c1551_read_data(c1551_Drive *c1551);
void c1551_write_handshake(c1551_Drive *c1551, int data);
int c1551_read_handshake(c1551_Drive *c1551);
int c1551_read_status(c1551_Drive *c1551);

#endif
