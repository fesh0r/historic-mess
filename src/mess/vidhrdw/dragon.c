#include "driver.h"
#include "cpu/m6809/m6809.h"
#include "machine/6821pia.h"

#define MAX_VRAM 6144

extern int dragon_cart_inserted;
extern UINT8 *dragon_tape;
extern int dragon_tapesize;
UINT8 *dragon_ram;

static void d_pia1_pb_w(int offset, int data);
static void d_pia1_pa_w(int offset, int data);
static int d_pia1_cb1_r(int offset);
static int d_pia0_ca1_r(int offset);
static int d_pia0_pa_r(int offset);
static int d_pia1_pa_r(int offset);
static void d_pia0_pb_w(int offset, int data);
static void d_pia1_cb2_w(int offset, int data);
static void d_pia0_cb2_w(int offset, int data);
static void d_pia1_ca2_w(int offset, int data);
static void d_pia0_ca2_w(int offset, int data);
static void d_pia0_irq_b(int state);

static void coco_pia1_ca2_w(int offset, int data);

static struct pia6821_interface dragon_pia_0_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ d_pia0_pa_r, 0, d_pia0_ca1_r, 0, 0, 0,
	/*outputs: A/B,CA/B2	   */ 0, d_pia0_pb_w, d_pia0_ca2_w, d_pia0_cb2_w,
	/*irqs	 : A/B			   */ 0, d_pia0_irq_b
};

static struct pia6821_interface dragon_pia_1_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ d_pia1_pa_r, 0, 0, d_pia1_cb1_r, 0, 0,
	/*outputs: A/B,CA/B2	   */ d_pia1_pa_w, d_pia1_pb_w, d_pia1_ca2_w, d_pia1_cb2_w,
	/*irqs	 : A/B			   */ 0, 0
};

static struct pia6821_interface coco_pia_1_intf =
{
	/*inputs : A/B,CA/B1,CA/B2 */ d_pia1_pa_r, 0, 0, d_pia1_cb1_r, 0, 0,
	/*outputs: A/B,CA/B2	   */ d_pia1_pa_w, d_pia1_pb_w, coco_pia1_ca2_w, d_pia1_cb2_w,
	/*irqs	 : A/B			   */ 0, 0
};

static UINT8 pia1_pb, sound_mux, tape_motor;
static UINT8 joystick_axis, joystick;
static int display_offset, d_dac;
static UINT8 *dirtybuffer, *ptape;
static int sam_vdg_mode, pia_vdg_mode;
static struct GfxElement *dfont;

/***************************************************************************
  Start the video hardware emulation.
***************************************************************************/

struct GfxElement *build_dragon_font(void)
{
	static unsigned char fontdata8x12[] =
	{
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1a, 0x2a, 0x2a, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x14, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x1c, 0x12, 0x12, 0x3c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x20, 0x20, 0x20, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3c, 0x12, 0x12, 0x12, 0x12, 0x12, 0x3c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x3e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x20, 0x20, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1e, 0x20, 0x20, 0x26, 0x22, 0x22, 0x1e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x3e, 0x22, 0x22, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x08, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x02, 0x02, 0x02, 0x02, 0x22, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x24, 0x28, 0x30, 0x28, 0x24, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x3e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x36, 0x2a, 0x2a, 0x22, 0x22, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x32, 0x2a, 0x26, 0x22, 0x22, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x22, 0x22, 0x22, 0x22, 0x22, 0x3e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x20, 0x20, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x22, 0x2a, 0x24, 0x1a, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3c, 0x22, 0x22, 0x3c, 0x28, 0x24, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x10, 0x08, 0x04, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x14, 0x14, 0x08, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x22, 0x2a, 0x2a, 0x36, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x14, 0x22, 0x22, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x22, 0x22, 0x14, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x3e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x38, 0x20, 0x20, 0x20, 0x20, 0x20, 0x38, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x20, 0x20, 0x10, 0x08, 0x04, 0x02, 0x02, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x1c, 0x2a, 0x08, 0x08, 0x08, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x10, 0x3e, 0x10, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x08, 0x08, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x14, 0x14, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x14, 0x14, 0x36, 0x00, 0x36, 0x14, 0x14, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x1e, 0x20, 0x1c, 0x02, 0x3c, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x32, 0x32, 0x04, 0x08, 0x10, 0x26, 0x26, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x28, 0x28, 0x10, 0x2a, 0x24, 0x1a, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x18, 0x18, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x10, 0x20, 0x20, 0x20, 0x10, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x1c, 0x3e, 0x1c, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x08, 0x08, 0x3e, 0x08, 0x08, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x10, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x30, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x02, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x24, 0x24, 0x24, 0x24, 0x24, 0x18, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x08, 0x18, 0x08, 0x08, 0x08, 0x08, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x1c, 0x20, 0x20, 0x3e, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x02, 0x0c, 0x02, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x04, 0x0c, 0x14, 0x3e, 0x04, 0x04, 0x04, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x20, 0x3c, 0x02, 0x02, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x20, 0x20, 0x3c, 0x22, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x3e, 0x02, 0x04, 0x08, 0x10, 0x20, 0x20, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1c, 0x22, 0x22, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x1c, 0x22, 0x22, 0x1e, 0x02, 0x02, 0x1c, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x18, 0x00, 0x18, 0x18, 0x08, 0x10, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0x00, 0x3e, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x10, 0x08, 0x04, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x18, 0x24, 0x04, 0x08, 0x08, 0x00, 0x08, 0x00, 0x00,

		/* Semigraphics */
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
		0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 
		0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 
		0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0x00, 0x00, 0x00, 0x00, 
		0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0x00, 0x00, 0x00, 0x00, 
		0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff, 0xff, 0xff, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x0f, 0x0f, 0x0f, 0x0f, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0xf0, 0xf0, 0xf0, 
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

		0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,
		0x00,0x00,0x00,0x00,0x00,0x00, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
		0x00,0x00,0x00,0x00,0x00,0x00, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
		0x00,0x00,0x00,0x00,0x00,0x00, 0xff,0xff,0xff,0xff,0xff,0xff,
		0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0x00,0x00,0x00,0x00,0x00,0x00,
		0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
		0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
		0x0f,0x0f,0x0f,0x0f,0x0f,0x0f, 0xff,0xff,0xff,0xff,0xff,0xff,
		0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0x00,0x00,0x00,0x00,0x00,0x00,
		0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
		0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
		0xf0,0xf0,0xf0,0xf0,0xf0,0xf0, 0xff,0xff,0xff,0xff,0xff,0xff,
		0xff,0xff,0xff,0xff,0xff,0xff, 0x00,0x00,0x00,0x00,0x00,0x00,
		0xff,0xff,0xff,0xff,0xff,0xff, 0x0f,0x0f,0x0f,0x0f,0x0f,0x0f,
		0xff,0xff,0xff,0xff,0xff,0xff, 0xf0,0xf0,0xf0,0xf0,0xf0,0xf0,
		0xff,0xff,0xff,0xff,0xff,0xff, 0xff,0xff,0xff,0xff,0xff,0xff
	};

	static struct GfxLayout fontlayout8x12 =
	{
		8,12,	/* 8*12 characters */
		64+64+16,	 /* 64+64+16 characters */
		1,	/* 1 bit per pixel */
		{ 0 },
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },	/* straightforward layout */
		{ 0*8, 1*8, 2*8, 3*8, 4*8, 5*8, 6*8, 7*8, 8*8, 9*8, 10*8, 11*8},
		8*12	/* every char takes 12 consecutive bytes */
	};
	struct GfxElement *font;
	static unsigned short colortable[2];
	font = decodegfx(fontdata8x12,&fontlayout8x12);
	if (font)
	{
		/* colortable will be set at run time */
		memset(colortable,0,sizeof(colortable));
		font->colortable = colortable;
		font->total_colors = 2;
	}
	return font;
}

static int generic_vh_start(void)
{
	display_offset = 0;
	ptape = dragon_tape;
	if ((dfont = build_dragon_font())==NULL)
		return 1; 

	if ((dirtybuffer = malloc (MAX_VRAM))==NULL)
		return 1;
	memset(dirtybuffer,1,MAX_VRAM);

	pia_config(0, PIA_STANDARD_ORDERING | PIA_8BIT, &dragon_pia_0_intf);
	return 0;
}

int dragon_vh_start(void)
{
	if (generic_vh_start())
		return 1;

	pia_config(1, PIA_STANDARD_ORDERING | PIA_8BIT, &dragon_pia_1_intf);
	pia_reset();

	/* allow short tape leader */
	ROM[0xbdfc] = 4;

	return 0;
}

static int generic_coco_vh_start(void)
{
	if (generic_vh_start())
		return 1;

	pia_config(1, PIA_STANDARD_ORDERING | PIA_8BIT, &coco_pia_1_intf);
	pia_reset();
	return 0;
}

int coco_vh_start(void)
{
	if (generic_coco_vh_start())
		return 1;

	/* allow short tape leader */
	ROM[0xa791 + 0x8000] = 0xff;
	return 0;
}

void coco3_palette_w(int offset, int data)
{
	paletteram[offset] = data;

	palette_change_color(offset,
						 (((data >> 4) & 2) | ((data >> 2) & 1)) * 0x55,
						 (((data >> 3) & 2) | ((data >> 1) & 1)) * 0x55,
						 (((data >> 2) & 2) | ((data >> 0) & 1)) * 0x55);
}

int coco3_vh_start(void)
{
	static int initial_palette[] = {
		022, 066, 011, 044, 077, 055, 033, 064,
		000, 022, 000, 077, 000, 022, 000, 064
	};
	int i;

	if (generic_coco_vh_start()) {
		paletteram = NULL;
		return 1;
	}

	/* allow short tape leader */
	ROM[0xa791 + 0x78000] = 0xff;

	paletteram = malloc(sizeof(initial_palette));
	if (!paletteram)
		return 1;

	for (i = 0; i < (sizeof(initial_palette) / sizeof(initial_palette[0])); i++)
		coco3_palette_w(i, initial_palette[i]);
	return 0;
}

void dragon_vh_stop(void)
{
	free (dirtybuffer);
	if (dragon_tape) free(dragon_tape);
}

void coco3_vh_stop(void)
{
	dragon_vh_stop();
	if (paletteram) {
		free(paletteram);
		paletteram = NULL;
	}
}

static void generic_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh, const int *metapalette, UINT8 *vrambase)
{
	int x, y, c, fg, bg, offset, text_color;
	UINT8 *vram, *p1, *p2, *db;
	
	/* clear vblank */
	pia_0_cb1_w (0, 0);
	
	if (full_refresh)
		memset(dirtybuffer,1,MAX_VRAM);
	
	vram = &vrambase[display_offset];
	db = dirtybuffer;
	
	if (pia_vdg_mode & 0x10)
	{
		if (pia_vdg_mode & 0x02)
		{
			/* resolution graphics modes */
			fg = Machine->pens[metapalette[pia_vdg_mode & 0x1 ? 11: 9]];
			bg = Machine->pens[metapalette[pia_vdg_mode & 0x1 ? 10: 8]];
			switch (sam_vdg_mode >> 1)
			{
			case 0x2:
				for (y = 0; y < 192; y++)
					for (x = 0; x < 16; x++)
					{
						if (*db)
						{
							for (c = 0; c < 8; c++)
								if (*vram & (1<<(7-c)))
									bitmap->line[y][x*16+c*2] = bitmap->line[y][x*16+c*2+1] = fg;
								else
									bitmap->line[y][x*16+c*2] = bitmap->line[y][x*16+c*2+1] = bg;
							*db = 0;
							osd_mark_dirty (x*16, y, x*16+15, y, 0);
						}
						vram++;
						db++;
					}
				break;
			case 0x3:
				for (y = 0; y < 192; y++)
					for (x = 0; x < 32; x++)
					{
						if (*db)
						{
							for (c = 0; c < 8; c++)
								if (*vram & (1<<(7-c)))
									bitmap->line[y][x*8+c] = fg;
								else
									bitmap->line[y][x*8+c] = bg;
							*db = 0;
							osd_mark_dirty (x*8, y, x*8+7, y, 0);
						}
						vram++;
						db++;
					}
				break;
			default:
				if (errorlog) fprintf (errorlog, "Resolution mode not supported\n");
				break;
			}
		}
		else
		{
			/* color graphics modes */
			offset = pia_vdg_mode & 0x1 ? 4: 0;
			switch (sam_vdg_mode >> 1)
			{
			case 0x2:
				for (y = 0; y < 96; y++)
					for (x = 0; x < 32; x++)
					{
						if (*db)
						{
							p1 = &bitmap->line[y*2][x*8];
							p2 = &bitmap->line[y*2+1][x*8];
							c = Machine->pens[metapalette[offset+(*vram>>6)]];
							*p1++ = c; *p1++ = c; *p2++ = c; *p2++ = c;
							c = Machine->pens[metapalette[offset+((*vram>>4)&0x3)]];
							*p1++ = c; *p1++ = c; *p2++ = c; *p2++ = c;
							c = Machine->pens[metapalette[offset+((*vram>>2)&0x3)]];
							*p1++ = c; *p1++ = c; *p2++ = c; *p2++ = c;
							c = Machine->pens[metapalette[offset+(*vram & 0x3)]];
							*p1++ = c; *p1++ = c; *p2++ = c; *p2++ = c;
							*db = 0;
							osd_mark_dirty (x*8, y*2, x*8+7, y*2+1, 0);
						}
						vram++;
						db++;
					}
				break;
			case 0x3:
				for (y = 0; y < 192; y++)
					for (x = 0; x < 32; x++)
					{
						if (*db)
						{
							p1 = &bitmap->line[y][x*8];
							c = Machine->pens[metapalette[offset+(*vram>>6)]];
							*p1++ = c; *p1++ = c;
							c = Machine->pens[metapalette[offset+((*vram>>4)&0x3)]];
							*p1++ = c; *p1++ = c;
							c = Machine->pens[metapalette[offset+((*vram>>2)&0x3)]];
							*p1++ = c; *p1++ = c;
							c = Machine->pens[metapalette[offset+(*vram & 0x3)]];
							*p1++ = c; *p1++ = c;
							*db = 0;
							osd_mark_dirty (x*8, y, x*8+7, y, 0);
						}
						vram++;
						db++;
					}
				break;
			default:
				if (errorlog) fprintf (errorlog, "Color mode not supported\n");
				break;
			}

		}
	}
	else
	{
		/* Text / semigraphic */
		if (pia_vdg_mode & 0x02)
		{
			offset = pia_vdg_mode & 0x1 ? 4: 0;
			for (y=0; y<16; y++)
				for (x=0; x<32; x++)
				{
					if (*db)
					{
						dfont->colortable[0] = Machine->pens[metapalette[8]];
						dfont->colortable[1] = Machine->pens[metapalette[((*vram >> 6) & 0x3) + offset]];
						drawgfx(bitmap,dfont,64+(*vram &0x3f),0,0,0, x*8, y*12, 0,TRANSPARENCY_NONE,0);
						*db=0;
						osd_mark_dirty (x*8, y*12, x*8+7, y*12+11, 0);
					}
					vram++;
					db++;
				}
		}
		else
		{
			if (pia_vdg_mode & 0x01)
				text_color = 15;
			else
				text_color = 13;

			for (y=0; y<16; y++)
				for (x=0; x<32; x++)
				{
					if (*db)
					{
						if (*vram & 0x80)
						{
							dfont->colortable[0] = Machine->pens[metapalette[8]];
							dfont->colortable[1] = Machine->pens[metapalette[(*vram >> 4) & 0x7]];

							drawgfx(bitmap,dfont,(*vram & 0x0f)+128,0,0,0,
									x*8, y*12, 0,TRANSPARENCY_NONE,0);
						}
						else
						{
							if (*vram & 0x40)
							{
								dfont->colortable[0] = Machine->pens[metapalette[text_color]];
								dfont->colortable[1] = Machine->pens[metapalette[text_color-1]];
							}
							else
							{
								dfont->colortable[1] = Machine->pens[metapalette[text_color]];
								dfont->colortable[0] = Machine->pens[metapalette[text_color-1]];
							}

							drawgfx(bitmap,dfont,*vram & 0x3f,0,0,0,
									x*8, y*12, 0,TRANSPARENCY_NONE,0);
						}
						*db=0;
						osd_mark_dirty (x*8, y*12, x*8+7, y*12+11, 0);
					}
					vram++;
					db++;
				}
		}
	}
}

void dragon_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	static int dragon_metapalette[] = {
		1, 2, 3, 4, 5, 6, 7, 8, 0, 1, 0, 5, 0, 1, 0, 8
	};
	generic_vh_screenrefresh(bitmap, full_refresh, dragon_metapalette, dragon_ram);
}

extern unsigned char *RAM;

void coco3_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	static int coco3_metapalette[] = {
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
	};

	if (palette_recalc())
		memset(dirtybuffer,1,MAX_VRAM);

	generic_vh_screenrefresh(bitmap, full_refresh, coco3_metapalette, &RAM[0x70000]);
}

int dragon_interrupt(void)
{
	pia_0_cb1_w (0, 1);
	return ignore_interrupt();
}

static void d_pia1_pb_w(int offset, int data)
{
	if ((data >> 3) != pia_vdg_mode)
	{
		pia_vdg_mode = data >> 3;
		memset(dirtybuffer,1,MAX_VRAM);
	}

}

static void d_pia1_pa_w(int offset, int data)
{
	d_dac = data & 0xfa;
	if (sound_mux)
		DAC_data_w(0,d_dac);
}

static int d_pia0_ca1_r(int offset)
{
	return 0; 
}

static int d_pia1_cb1_r(int offset)
{
	return dragon_cart_inserted;
}

static void d_pia1_cb2_w(int offset, int data)
{
	sound_mux = data;
}

static void d_pia0_cb2_w(int offset, int data)
{
	joystick = data;
}

static void d_pia1_ca2_w(int offset, int data)
{
	if (tape_motor ^ data)
	{
		/* speed up tape reading */
		dragon_ram[0x0093] = 2;	 
		dragon_ram[0x0092] = 3;

		if (data == 0)
		{
			ptape--;
			ptape[0] = 0x55; /* insert sync byte */
		}
		tape_motor = data;
	}
}

static void coco_pia1_ca2_w(int offset, int data)
{
	if (tape_motor ^ data)
	{
		/* speed up tape reading */
		dragon_ram[0x008f] = 3;	 
		dragon_ram[0x0091] = 2;	 

		if (data == 0)
		{
			ptape--;
			ptape[0] = 0x55; /* insert sync byte */
		}
		tape_motor = data;
	}
}

static void d_pia0_ca2_w(int offset, int data)
{
	joystick_axis = data;
}

static int d_pia0_pa_r(int offset)
{
	int porta=0x7f;

	if ((input_port_0_r(0) | pia1_pb) != 0xff) porta &= ~0x01;
	if ((input_port_1_r(0) | pia1_pb) != 0xff) porta &= ~0x02;
	if ((input_port_2_r(0) | pia1_pb) != 0xff) porta &= ~0x04;
	if ((input_port_3_r(0) | pia1_pb) != 0xff) porta &= ~0x08;
	if ((input_port_4_r(0) | pia1_pb) != 0xff) porta &= ~0x10;
	if ((input_port_5_r(0) | pia1_pb) != 0xff) porta &= ~0x20;
	if ((input_port_6_r(0) | pia1_pb) != 0xff) porta &= ~0x40;
	if (d_dac <= (joystick_axis? input_port_8_r(0): input_port_7_r(0)))
		porta |= 0x80;
	porta &= ~input_port_9_r(0);

	return porta;
}

static int d_pia1_pa_r(int offset)
{
	static int bit=7, bitc=0, *state;
	static int lo[]={1,1,0,0};
	static int hi[]={1,0};

	if (ptape && tape_motor)
	{
		if (bitc == 0)
		{
			if (bit < 0)
			{
				bit = 7;
				if (ptape - dragon_tape < dragon_tapesize)
					ptape++;
			}

			if ((*ptape >> (7-bit)) & 0x01)
			{
				state = hi;
				bitc = 2;
			}
			else
			{
				state = lo;
				bitc = 4;
			}
			bit--;
		}
		bitc--;
		return (state[bitc]);
	}
	return 1;
}

static void d_pia0_pb_w(int offset, int data)
{
	pia1_pb = data;
}

static void d_pia0_irq_b(int state)
{
	cpu_set_irq_line(0, M6809_IRQ_LINE, state);
}

void dragon_sam_display_offset(int offset, int data)
{
	UINT16 d_offset = display_offset;

	if (offset & 0x01)
		d_offset |= 0x01 << (offset/2 + 9);
	else
		d_offset &= ~(0x01 << (offset/2 + 9));

	if (d_offset != display_offset)
	{
		memset(dirtybuffer,1,MAX_VRAM);
		display_offset = d_offset;
	}
}

void dragon_sam_vdg_mode(int offset, int data)
{
	if (offset & 0x01)
		sam_vdg_mode |= 0x01 << (offset/2);
	else
		sam_vdg_mode &= ~(0x01 << (offset/2));
}

static void generic_ram_w(int offset, int data, UINT8 *vrambase)
{
	if ((offset >= display_offset) && (offset < display_offset + MAX_VRAM))
		if (vrambase[offset] != data)
			dirtybuffer[offset - display_offset] = 1;
}

void dragon_ram_w (int offset, int data)
{
	generic_ram_w(offset, data, dragon_ram);
	dragon_ram[offset] = data;
}

static void coco3_ram_w(int offset, int data, int mmuslot)
{
	extern int coco3_mmu[];

	offset += coco3_mmu[mmuslot] * 0x2000;
	if (offset >= 0x70000)
		generic_ram_w(offset - 0x70000, data, &RAM[0x70000]);
	RAM[offset] = data;
}

void coco3_ram_b1_w (int offset, int data)
{
	coco3_ram_w(offset, data, 0);
}
void coco3_ram_b2_w (int offset, int data)
{
	coco3_ram_w(offset, data, 1);
}
void coco3_ram_b3_w (int offset, int data)
{
	coco3_ram_w(offset, data, 2);
}
void coco3_ram_b4_w (int offset, int data)
{
	coco3_ram_w(offset, data, 3);
}
void coco3_ram_b5_w (int offset, int data)
{
	coco3_ram_w(offset, data, 4);
}
void coco3_ram_b6_w (int offset, int data)
{
	coco3_ram_w(offset, data, 5);
}
void coco3_ram_b7_w (int offset, int data)
{
	coco3_ram_w(offset, data, 6);
}
void coco3_ram_b8_w (int offset, int data)
{
	coco3_ram_w(offset, data, 7);
}

int coco3_gime_r(int offset)
{
	/* Dummy for now */
	return 0;
}

void coco3_gime_w(int offset, int data)
{
	/* Dummy for now */
}

