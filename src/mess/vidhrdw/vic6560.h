#ifndef __VIC6560_H_
#define __VIC6560_H_
/***************************************************************************

  MOS video interface chip 6560, 6561

***************************************************************************/

/*
	if you need this chip in another mame/mess emulation than let it me know
	 I will split this from the vc20 driver
	 peter.trauner@jk.uni-linz.ac.at
	 16. november 1999
	look at mess/systems/vc20.c and mess/machine/vc20.c
	 on how to use it
 */

/*
	2 Versions
		6560 NTSC
		6561 PAL
	14 bit addr bus
	12 bit data bus
	(16 8 bit registers)
	alternates with MOS 6502 on the address bus
	fetch 8 bit characternumber and 4 bit color
	high bit of 4 bit color value determines:
	 0: 2 color mode
	 1: 4 color mode
	than fetch characterbitmap for characternumber
	 2 color mode:
	  set bit in characterbitmap gives pixel in color of the lower 3 color bits
	  cleared bit gives pixel in backgroundcolor
	 4 color mode:
	  2 bits in the characterbitmap are viewed together
	  00: backgroundcolor
	  11: colorram
	  01: helpercolor
	  10: framecolor
	advance to next character in videorram until line is full
	repeat this 8 or 16 lines, before moving to next line in videoram
	screen ratio ntsc, pal 4/3

	pal version:
	 can contain greater visible areas
	 expects other sync position (so ntsc modules may be displayed at
	  the upper left corner of the tv screen)
	 pixel ratio seems to be different on pal and ntsc
*/


/*
	commodore vic20 notes
	6560 adress line 13 is connected inverted to address line 15 of the board
	1 K 4 bit ram at 0x9400 is additional connected as 4 higher bits
	of the 6560 (colorram) without decoding the 6560 address line a8..a13
*/

#include <stdio.h>
#include "osd_cpu.h"
#include "driver.h"
#include "vidhrdw/generic.h"

typedef int bool;
#ifndef true
#define true 1
#endif
#ifndef false
#define false 0
#endif

// call to init videodriver
// pal version
// dma_read: videochip fetched 12 bit data from system bus
extern void vic6560_init(int(*dma_read)(int));
extern void vic6561_init(int(*dma_read)(int));

// internal
extern bool vic6560_pal;

#define VIC6560_VRETRACERATE 60
#define VIC6561_VRETRACERATE 50

#define VREFRESHINLINES 28

// to be inserted in MachineDriver-Structure
/* the following values depend on the VIC clock,
	but to achieve TV-frequency the clock must have a fix frequency
	only estimations */
#define VIC6560_HSIZE	210 // 206
#define VIC6560_VSIZE	(261-VREFRESHINLINES)
#define VIC6561_HSIZE	253
#define VIC6561_VSIZE	(312-VREFRESHINLINES)
#define VIC6560_CLOCK	(14318181/14)
#define VIC6561_CLOCK	(4433618/4)

#define VIC656X_CLOCK	(vic6560_pal?VIC6561_CLOCK:VIC6560_CLOCK)

extern int	vic6560_vh_start(void);
extern void vic6560_vh_stop(void);
extern void vic6560_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh);
extern unsigned char vic6560_palette[16*3];
// to be inserted in GameDriver-Structure
extern struct CustomSound_interface vic6560_sound_interface;

// to be called when writting to port
extern void vic6560_port_w(int offset, int data);
// to be called when reading from port
extern int vic6560_port_r(int offset);

// to call when memory, which is readable by the vic is changed
extern void vic6560_addr_w(int offset, int data);
// inform about writes to the databits 8 till 11 on the 6560 vic
// to call when special 4 bit memory for the vic is changed
extern void vic6560_addr8_w(int offset, int data);

// private area

/* from sndhrdw/pc.c */
extern int  vic6560_custom_start(const struct MachineSound *driver);
extern void vic6560_custom_stop(void);
extern void vic6560_custom_update(void);
extern void vic6560_soundport_w(int mode,int data);

extern UINT8 vic6560[16];
#endif
