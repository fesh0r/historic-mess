#ifndef __VC20_H_
#define __VC20_H_

#include <stdio.h>
#include "osd_cpu.h"
#include "driver.h"
#include "vidhrdw/generic.h"
#include "mess/vidhrdw/ted7360.h"

// dirtybuffer when disabled, something faster but no rasterlineeffects
#define RASTERLINEBASED

/* enable and set level for verbosity of the various parts of emulation */

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


/* damned c16 joystick
 * connected to the keyboard latch
 * without! selection (also activ) */
#define JOYSTICK1_PORT (input_port_7_r(0)&0x80)

#define JOYSTICK2_PORT (input_port_7_r(0)&0x40)

#define JOYSTICK_2_LEFT	(JOYSTICK2_PORT&&(input_port_1_r(0)&0x80))
#define JOYSTICK_2_RIGHT	(JOYSTICK2_PORT&&(input_port_1_r(0)&0x40))
#define JOYSTICK_2_UP		(JOYSTICK2_PORT&&(input_port_1_r(0)&0x20))
#define JOYSTICK_2_DOWN	(JOYSTICK2_PORT&&(input_port_1_r(0)&0x10))
#define JOYSTICK_2_BUTTON (JOYSTICK2_PORT&&(input_port_1_r(0)&8))

#define JOYSTICK_1_LEFT	(JOYSTICK1_PORT&&(input_port_0_r(0)&0x80))
#define JOYSTICK_1_RIGHT	(JOYSTICK1_PORT&&(input_port_0_r(0)&0x40))
#define JOYSTICK_1_UP		(JOYSTICK1_PORT&&(input_port_0_r(0)&0x20))
#define JOYSTICK_1_DOWN	(JOYSTICK1_PORT&&(input_port_0_r(0)&0x10))
#define JOYSTICK_1_BUTTON (JOYSTICK1_PORT&&(input_port_0_r(0)&8))

// macros to access keys
#define JOYSTICK_LEFT	(JOYSTICK_1_LEFT||JOYSTICK_2_LEFT)
#define JOYSTICK_RIGHT	(JOYSTICK_1_RIGHT||JOYSTICK_2_RIGHT)
#define JOYSTICK_UP		(JOYSTICK_1_UP||JOYSTICK_2_UP)
#define JOYSTICK_DOWN	(JOYSTICK_1_DOWN||JOYSTICK_2_DOWN)
#define JOYSTICK_BUTTON1 (JOYSTICK_1_BUTTON)
#define JOYSTICK_BUTTON2 (JOYSTICK_2_BUTTON)


#define KEY_ESC (input_port_2_r(0)&0x8000)
#define KEY_1 (input_port_2_r(0)&0x4000)
#define KEY_2 (input_port_2_r(0)&0x2000)
#define KEY_3 (input_port_2_r(0)&0x1000)
#define KEY_4 (input_port_2_r(0)&0x800)
#define KEY_5 (input_port_2_r(0)&0x400)
#define KEY_6 (input_port_2_r(0)&0x200)
#define KEY_7 (input_port_2_r(0)&0x100)
#define KEY_8 (input_port_2_r(0)&0x80)
#define KEY_9 (input_port_2_r(0)&0x40)
#define KEY_0 (input_port_2_r(0)&0x20)
#define KEY_LEFT (input_port_2_r(0)&0x10)
#define KEY_RIGHT (input_port_2_r(0)&8)
#define KEY_UP (input_port_2_r(0)&4)
#define KEY_DOWN (input_port_2_r(0)&2)
#define KEY_DEL (input_port_2_r(0)&1)

#define KEY_CTRL (input_port_3_r(0)&0x8000)
#define KEY_Q (input_port_3_r(0)&0x4000)
#define KEY_W (input_port_3_r(0)&0x2000)
#define KEY_E (input_port_3_r(0)&0x1000)
#define KEY_R (input_port_3_r(0)&0x800)
#define KEY_T (input_port_3_r(0)&0x400)
#define KEY_Y (input_port_3_r(0)&0x200)
#define KEY_U (input_port_3_r(0)&0x100)
#define KEY_I (input_port_3_r(0)&0x80)
#define KEY_O (input_port_3_r(0)&0x40)
#define KEY_P (input_port_3_r(0)&0x20)
#define KEY_ATSIGN (input_port_3_r(0)&0x10)
#define KEY_PLUS (input_port_3_r(0)&8)
#define KEY_MINUS (input_port_3_r(0)&4)
#define KEY_HOME (input_port_3_r(0)&2)
#define KEY_STOP (input_port_3_r(0)&1)

#define KEY_SHIFTLOCK (input_port_4_r(0)&0x8000)
#define KEY_A (input_port_4_r(0)&0x4000)
#define KEY_S (input_port_4_r(0)&0x2000)
#define KEY_D (input_port_4_r(0)&0x1000)
#define KEY_F (input_port_4_r(0)&0x800)
#define KEY_G (input_port_4_r(0)&0x400)
#define KEY_H (input_port_4_r(0)&0x200)
#define KEY_J (input_port_4_r(0)&0x100)
#define KEY_K (input_port_4_r(0)&0x80)
#define KEY_L (input_port_4_r(0)&0x40)
#define KEY_SEMICOLON (input_port_4_r(0)&0x20)
#define KEY_COLON (input_port_4_r(0)&0x10)
#define KEY_ASTERIX (input_port_4_r(0)&8)
#define KEY_RETURN (input_port_4_r(0)&4)
#define KEY_CBM (input_port_4_r(0)&2)
#define KEY_LEFT_SHIFT (input_port_4_r(0)&1)


#define KEY_Z (input_port_5_r(0)&0x8000)
#define KEY_X (input_port_5_r(0)&0x4000)
#define KEY_C (input_port_5_r(0)&0x2000)
#define KEY_V (input_port_5_r(0)&0x1000)
#define KEY_B (input_port_5_r(0)&0x800)
#define KEY_N (input_port_5_r(0)&0x400)
#define KEY_M (input_port_5_r(0)&0x200)
#define KEY_COMMA (input_port_5_r(0)&0x100)
#define KEY_POINT (input_port_5_r(0)&0x80)
#define KEY_SLASH (input_port_5_r(0)&0x40)
#define KEY_RIGHT_SHIFT (input_port_5_r(0)&0x20)
#define KEY_POUND (input_port_5_r(0)&0x10)
#define KEY_EQUALS (input_port_5_r(0)&8)
#define KEY_SPACE (input_port_5_r(0)&4)
#define KEY_F1 (input_port_5_r(0)&2)
#define KEY_F2 (input_port_5_r(0)&1)

#define KEY_F3 (input_port_6_r(0)&0x8000) 
#define KEY_HELP (input_port_6_r(0)&0x4000) 

#define KEY_SHIFT (KEY_LEFT_SHIFT||KEY_RIGHT_SHIFT||KEY_SHIFTLOCK)

#define DIPMEMORY (input_port_7_r(0)&3)
#define MEMORY16K (0)
#define MEMORY32K (2)
#define MEMORY64K (3)

#define IEC8ON (input_port_7_r(0)&0x20)
#define IEC9ON (input_port_7_r(0)&0x10)


extern UINT8 *c16_memory;

extern void c16_m7501_port_w(int offset, int data);
extern int c16_m7501_port_r(int offset);

extern void c16_6551_port_w(int offset, int data);
extern int c16_6551_port_r(int offset);

extern void c16_6529b_port_w(int offset, int data);
extern int c16_6529b_port_r(int offset);

extern void c16_6529_port_w(int offset, int data);
extern int c16_6529_port_r(int offset);

extern void c16_iec9_port_w(int offset, int data);
extern int c16_iec9_port_r(int offset);

extern void c16_iec8_port_w(int offset, int data);
extern int c16_iec8_port_r(int offset);

extern void c16_select_roms(int offset, int data);
extern void c16_switch_to_rom(int offset, int data);
extern void c16_switch_to_ram(int offset, int data);

extern void c16_write_0002(int offset, int data);
//extern void c16_write_4000(int offset, int data);
//extern void c16_write_8000(int offset, int data);
//extern void c16_write_c000(int offset, int data);
//extern void c16_write_ff40(int offset, int data);

extern int c16_read_0000(int offset);
extern int c16_read_4000(int offset);
extern int c16_read_8000(int offset);
extern int c16_read_c000(int offset);
extern int c16_read_fc00(int offset);
extern int c16_read_ff40(int offset);

// ted reads
extern int ted7360_dma_read(int offset);
extern int c16_read_keyboard(void);

extern void c16_driver_init(void);
extern void plus4_driver_init(void);
extern int  c16_rom_load(void);
extern int  c16_rom_id(const char *name, const char *gamename);
extern void c16_init_machine(void);
extern void c16_shutdown_machine(void);
extern void c16_interrupt(int);
extern int  c16_frame_interrupt(void);

#endif
