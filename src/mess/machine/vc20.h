#ifndef __VC20_H_
#define __VC20_H_

#include <stdio.h>
#include "osd_cpu.h"
#include "driver.h"
#include "vidhrdw/generic.h"
#include "mess/machine/vc20tape.h"
#include "mess/machine/c1551.h"
#include "mess/vidhrdw/vic6560.h"

/* enable and set level for verbosity of the various parts of emulation */
#define VERBOSE_DBG 0       /* general debug messages */

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

#if VERBOSE_DBG
#define VIC_LOG(n,m,a) LOG(VERBOSE_DBG,n,m,a)
#else
#define VIC_LOG(n,m,a)
#endif

#if VERBOSE_DBG
#define VIA_LOG(n,m,a) LOG(VERBOSE_DBG,n,m,a)
#else
#define VIA_LOG(n,m,a)
#endif

#if VERBOSE_DBG
#define SND_LOG(n,m,a) LOG(VERBOSE_DBG,n,m,a)
#else
#define SND_LOG(n,m,a)
#endif

/**
  joystick (4 directions switches, 1 button),
  paddles (2) button, 270 degree wheel,
  and lightpen are all connected to the gameport connector
  lightpen to joystick button
  paddle 1 button to joystick left
  paddle 2 button to joystick right
 */

#define JOYSTICK (readinputport(14)&0x80)
#define PADDLES (readinputport(14)&0x40)
#define LIGHTPEN (readinputport(14)&0x20)
#define LIGHTPEN_POINTER (LIGHTPEN&&(readinputport(14)&0x10))
#define LIGHTPEN_X_VALUE (readinputport(16)&~1) // effectiv resolution
#define LIGHTPEN_Y_VALUE (readinputport(17)&~1) // effectiv resolution
#define LIGHTPEN_BUTTON (LIGHTPEN&&(readinputport(15)&0x80))

#define DATASSETTE (readinputport(14)&0x8)
#define DATASSETTE_TONE (readinputport(14)&4)

#define SERIAL8ON (readinputport(14)&2)
#define SERIAL9ON (readinputport(14)&1)

// macros to access keys
#define JOY_VIA0_IGNORE 0x80
#define JOY_VIA1_IGNORE ~0x80

#define PADDLE1_VALUE   readinputport(2)
#define PADDLE2_VALUE	readinputport(3)

#define KEYBOARD_ROW(n) readinputport(4+n)
#define KEYBOARD_EXTRA	readinputport(12)

#define DATASSETTE_PLAY		(readinputport(12)&4)
#define DATASSETTE_RECORD	(readinputport(12)&2)
#define DATASSETTE_STOP		(readinputport(12)&1)

#define KEY_SHIFTLOCK	0x80
#define KEY_RESTORE 	0x40
#define KEY_CURSOR_UP	0x20
#define KEY_CURSOR_LEFT 0x10

// macros to access the dip switches
#define EXPANSION (readinputport(13)&7)
#define EXP_3K 1
#define EXP_8K 2
#define EXP_16K 3
#define EXP_32K 4
#define EXP_CUSTOM 5
#define RAMIN0X0400 ((EXPANSION==EXP_3K)\
							||((EXPANSION==EXP_CUSTOM)&&(readinputport(13)&8)) )
#define RAMIN0X2000 ((EXPANSION==EXP_8K)||(EXPANSION==EXP_16K)\
							||(EXPANSION==EXP_32K)\
							||((EXPANSION==EXP_CUSTOM)&&(readinputport(13)&0x10)) )
#define RAMIN0X4000 ((EXPANSION==EXP_16K)||(EXPANSION==EXP_32K)\
							||((EXPANSION==EXP_CUSTOM)&&(readinputport(13)&0x20)) )
#define RAMIN0X6000 ((EXPANSION==EXP_32K)\
							||((EXPANSION==EXP_CUSTOM)&&(readinputport(13)&0x40)) )
#define RAMIN0XA000 ((EXPANSION==EXP_32K)\
							||((EXPANSION==EXP_CUSTOM)&&(readinputport(13)&0x80)) )


#define VC20ADDR2VIC6560ADDR(a) (((a)>0x8000)?((a)&0x1fff):((a)|0x2000))
#define VIC6560ADDR2VC20ADDR(a) (((a)>0x2000)?((a)&0x1fff):((a)|0x8000))

extern UINT8 *vc20_memory;
extern UINT8 *vc20_memory_9400;
extern CBM_Drive vc20_drive8, vc20_drive9;

extern void vc20_write_9400(int offset, int data);

// split for more performance
// VIC reads bits 8 till 11
extern int vic6560_dma_read_color(int offset);
// VIC reads bits 0 till 7
extern int vic6560_dma_read(int offset);

extern int  vc20_rom_load(void);
extern int  vc20_rom_id(const char *name, const char *gamename);

extern void vc20_driver_init(void);
extern void vic20_driver_init(void);
extern void vc20_driver_shutdown(void);

extern void vc20_init_machine(void);
extern void vc20_shutdown_machine(void);

extern int  vc20_frame_interrupt(void);

#endif
