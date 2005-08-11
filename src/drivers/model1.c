/*
**      V60 + 68k + 4xTGP
*/

#include "driver.h"
#include "vidhrdw/generic.h"
#include "machine/eeprom.h"
#include "system16.h"
#include "vidhrdw/segaic24.h"
#include "cpu/m68000/m68k.h"
#include "sound/multipcm.h"
#include "sound/2612intf.h"

WRITE16_HANDLER( model1_paletteram_w );
VIDEO_START(model1);
VIDEO_UPDATE(model1);
VIDEO_EOF(model1);
extern UINT16 *model1_display_list0, *model1_display_list1;
extern UINT16 *model1_color_xlat;
READ16_HANDLER( model1_listctl_r );
WRITE16_HANDLER( model1_listctl_w );

READ16_HANDLER( model1_tgp_copro_r );
WRITE16_HANDLER( model1_tgp_copro_w );
READ16_HANDLER( model1_tgp_copro_adr_r );
WRITE16_HANDLER( model1_tgp_copro_adr_w );
READ16_HANDLER( model1_tgp_copro_ram_r );
WRITE16_HANDLER( model1_tgp_copro_ram_w );

static int model1_sound_irq;

void model1_tgp_reset(int swa);

static READ16_HANDLER( io_r )
{
	if(offset < 0x8)
		return readinputport(offset);
	if(offset < 0x10) {
		offset -= 0x8;
		if(offset < 3)
			return readinputport(offset+8) | 0xff00;
		return 0xff;
	}

	logerror("IOR: %02x\n", offset);
	return 0xffff;
}

static READ16_HANDLER( fifoin_status_r )
{
	return 0xffff;
}

static WRITE16_HANDLER( bank_w )
{
	if(ACCESSING_LSB) {
		switch(data & 0xf) {
		case 0x1: // 100000-1fffff data roms banking
			memory_set_bankptr(1, memory_region(REGION_CPU1) + 0x1000000 + 0x100000*((data >> 4) & 0xf));
			logerror("BANK %x\n", 0x1000000 + 0x100000*((data >> 4) & 0xf));
			break;
		case 0x2: // 200000-2fffff data roms banking (unused, all known games have only one bank)
			break;
		case 0xf: // f00000-ffffff program roms banking (unused, all known games have only one bank)
			break;
		}
	}
}


static int last_irq;

static void irq_raise(int level)
{
	//  logerror("irq: raising %d\n", level);
	//  irq_status |= (1 << level);
	last_irq = level;
	cpunum_set_input_line(0, 0, HOLD_LINE);
}

static int irq_callback(int irqline)
{
	return last_irq;
}
// vf
// 1 = fe3ed4
// 3 = fe3f5c
// other = fe3ec8 / fe3ebc

// vr
// 1 = fe02bc
// other = f302a4 / fe02b0

// swa
// 1 = ff504
// 3 = ff54c
// other = ff568/ff574

static void irq_init(void)
{
	cpunum_set_input_line(0, 0, CLEAR_LINE);
	cpu_set_irq_callback(0, irq_callback);
}

extern void tgp_tick(void);
static INTERRUPT_GEN(model1_interrupt)
{
	if (cpu_getiloops())
	{
		irq_raise(1);
		tgp_tick();
	}
	else
	{
		irq_raise(model1_sound_irq);
	}
}

static MACHINE_INIT(model1)
{
	memory_set_bankptr(1, memory_region(REGION_CPU1) + 0x1000000);
	irq_init();
	model1_tgp_reset(!strcmp(Machine->gamedrv->name, "swa") || !strcmp(Machine->gamedrv->name, "wingwar") || !strcmp(Machine->gamedrv->name, "wingwara"));
	if (!strcmp(Machine->gamedrv->name, "swa"))
	{
		model1_sound_irq = 0;
	}
	else
	{
		model1_sound_irq = 3;
	}
}

static READ16_HANDLER( network_ctl_r )
{
	if(offset)
		return 0x40;
	else
		return 0x00;
}

static WRITE16_HANDLER( network_ctl_w )
{
}

static WRITE16_HANDLER(md1_w)
{
	extern int model1_dump;
	COMBINE_DATA(model1_display_list1+offset);
	if(0 && offset)
		return;
	if(1 && model1_dump)
		logerror("TGP: md1_w %x, %04x @ %04x (%x)\n", offset, data, mem_mask, activecpu_get_pc());
}

static WRITE16_HANDLER(md0_w)
{
	extern int model1_dump;
	COMBINE_DATA(model1_display_list0+offset);
	if(0 && offset)
		return;
	if(1 && model1_dump)
		logerror("TGP: md0_w %x, %04x @ %04x (%x)\n", offset, data, mem_mask, activecpu_get_pc());
}

static WRITE16_HANDLER(p_w)
{
	UINT16 old = paletteram16[offset];
	paletteram16_xBBBBBGGGGGRRRRR_word_w(offset, data, mem_mask);
	if(0 && paletteram16[offset] != old)
		logerror("XVIDEO: p_w %x, %04x @ %04x (%x)\n", offset, data, mem_mask, activecpu_get_pc());
}

static UINT16 *mr;
static WRITE16_HANDLER(mr_w)
{
	COMBINE_DATA(mr+offset);
	if(0 && offset == 0x1138/2)
		logerror("MR.w %x, %04x @ %04x (%x)\n", offset*2+0x500000, data, mem_mask, activecpu_get_pc());
}

static UINT16 *mr2;
static WRITE16_HANDLER(mr2_w)
{
	COMBINE_DATA(mr2+offset);
#if 0
	if(0 && offset == 0x6e8/2) {
		logerror("MR.w %x, %04x @ %04x (%x)\n", offset*2+0x400000, data, mem_mask, activecpu_get_pc());
	}
	if(offset/2 == 0x3680/4)
		logerror("MW f80[r25], %04x%04x (%x)\n", mr2[0x3680/2+1], mr2[0x3680/2], activecpu_get_pc());
	if(offset/2 == 0x06ca/4)
		logerror("MW fca[r19], %04x%04x (%x)\n", mr2[0x06ca/2+1], mr2[0x06ca/2], activecpu_get_pc());
	if(offset/2 == 0x1eca/4)
		logerror("MW fca[r22], %04x%04x (%x)\n", mr2[0x1eca/2+1], mr2[0x1eca/2], activecpu_get_pc());
#endif

	// wingwar scene position, pc=e1ce -> d735
	if(offset/2 == 0x1f08/4)
		logerror("MW  8[r10], %f (%x)\n", *(float *)(mr2+0x1f08/2), activecpu_get_pc());
	if(offset/2 == 0x1f0c/4)
		logerror("MW  c[r10], %f (%x)\n", *(float *)(mr2+0x1f0c/2), activecpu_get_pc());
	if(offset/2 == 0x1f10/4)
		logerror("MW 10[r10], %f (%x)\n", *(float *)(mr2+0x1f10/2), activecpu_get_pc());
}

static int to_68k;

static READ16_HANDLER( snd_68k_ready_r )
{
	int sr = cpunum_get_reg(1, M68K_REG_SR);

	if ((sr & 0x0700) > 0x0100)
	{
		cpu_spinuntil_time(TIME_IN_USEC(40));
		return 0;	// not ready yet, interrupts disabled
	}

	return 0xff;
}

static WRITE16_HANDLER( snd_latch_to_68k_w )
{
	while (!snd_68k_ready_r(0, 0))
	{
		cpu_spinuntil_time(TIME_IN_USEC(40));
	}

	to_68k = data;

	cpunum_set_input_line(1, 2, HOLD_LINE);
	// give the 68k time to reply
	cpu_spinuntil_time(TIME_IN_USEC(40));
}

static ADDRESS_MAP_START( model1_mem, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x0fffff) AM_ROM
	AM_RANGE(0x100000, 0x1fffff) AM_ROMBANK(1)
	AM_RANGE(0x200000, 0x2fffff) AM_ROM

	AM_RANGE(0x400000, 0x40ffff) AM_READWRITE(MRA16_RAM, mr2_w) AM_BASE(&mr2)
	AM_RANGE(0x500000, 0x53ffff) AM_READWRITE(MRA16_RAM, mr_w)  AM_BASE(&mr)

	AM_RANGE(0x600000, 0x60ffff) AM_READWRITE(MRA16_RAM, md0_w) AM_BASE(&model1_display_list0)
	AM_RANGE(0x610000, 0x61ffff) AM_READWRITE(MRA16_RAM, md1_w) AM_BASE(&model1_display_list1)
	AM_RANGE(0x680000, 0x680003) AM_READWRITE(model1_listctl_r, model1_listctl_w)

	AM_RANGE(0x700000, 0x70ffff) AM_READWRITE(sys24_tile_r, sys24_tile_w)
	AM_RANGE(0x720000, 0x720001) AM_WRITENOP		// Unknown, always 0
	AM_RANGE(0x740000, 0x740001) AM_WRITENOP		// Horizontal synchronization register
	AM_RANGE(0x760000, 0x760001) AM_WRITENOP		// Vertical synchronization register
	AM_RANGE(0x770000, 0x770001) AM_WRITENOP		// Video synchronization switch
	AM_RANGE(0x780000, 0x7fffff) AM_READWRITE(sys24_char_r, sys24_char_w)

	AM_RANGE(0x900000, 0x903fff) AM_READWRITE(MRA16_RAM, p_w) AM_BASE(&paletteram16)
	AM_RANGE(0x910000, 0x91bfff) AM_RAM  AM_BASE(&model1_color_xlat)

	AM_RANGE(0xc00000, 0xc0003f) AM_READ(io_r) AM_WRITENOP

	AM_RANGE(0xc00040, 0xc00043) AM_READWRITE(network_ctl_r, network_ctl_w)

	AM_RANGE(0xc00200, 0xc002ff) AM_RAM AM_BASE(&generic_nvram16) AM_SIZE(&generic_nvram_size)

	AM_RANGE(0xc40000, 0xc40001) AM_WRITE(snd_latch_to_68k_w)
	AM_RANGE(0xc40002, 0xc40003) AM_READ(snd_68k_ready_r)

	AM_RANGE(0xd00000, 0xd00001) AM_READWRITE(model1_tgp_copro_adr_r, model1_tgp_copro_adr_w)
	AM_RANGE(0xd20000, 0xd20003) AM_WRITE(model1_tgp_copro_ram_w )
	AM_RANGE(0xd80000, 0xd80003) AM_WRITE(model1_tgp_copro_w) AM_MIRROR(0x10)
	AM_RANGE(0xdc0000, 0xdc0003) AM_READ(fifoin_status_r)

	AM_RANGE(0xe00000, 0xe00001) AM_WRITENOP        // Watchdog?  IRQ ack? Always 0x20, usually on irq
	AM_RANGE(0xe00004, 0xe00005) AM_WRITE(bank_w)
	AM_RANGE(0xe0000c, 0xe0000f) AM_WRITENOP

	AM_RANGE(0xfc0000, 0xffffff) AM_ROM
ADDRESS_MAP_END

static ADDRESS_MAP_START( model1_io, ADDRESS_SPACE_IO, 16 )
	AM_RANGE(0xd20000, 0xd20003) AM_READ(model1_tgp_copro_ram_r)
	AM_RANGE(0xd80000, 0xd80003) AM_READ(model1_tgp_copro_r)
ADDRESS_MAP_END

static READ16_HANDLER( m1_snd_68k_latch_r )
{
	return to_68k;
}

static READ16_HANDLER( m1_snd_v60_ready_r )
{
	return 1;
}

static READ16_HANDLER( m1_snd_mpcm0_r )
{
	return MultiPCM_reg_0_r(0);
}

static WRITE16_HANDLER( m1_snd_mpcm0_w )
{
	MultiPCM_reg_0_w(offset, data);
}

static WRITE16_HANDLER( m1_snd_mpcm0_bnk_w )
{
	multipcm_set_bank(0, 0x100000 * (data & 3), 0x100000 * (data & 3));
}

static READ16_HANDLER( m1_snd_mpcm1_r )
{
	return MultiPCM_reg_1_r(0);
}

static WRITE16_HANDLER( m1_snd_mpcm1_w )
{
	MultiPCM_reg_1_w(offset, data);
}

static WRITE16_HANDLER( m1_snd_mpcm1_bnk_w )
{
	multipcm_set_bank(1, 0x100000 * (data & 3), 0x100000 * (data & 3));
}

static READ16_HANDLER( m1_snd_ym_r )
{
	return YM3438_status_port_0_A_r(0);
}

static WRITE16_HANDLER( m1_snd_ym_w )
{
	switch (offset)
	{
		case 0:
			YM3438_control_port_0_A_w(0, data);
			break;

		case 1:
			YM3438_data_port_0_A_w(0, data);
			break;

		case 2:
			YM3438_control_port_0_B_w(0, data);
			break;

		case 3:
			YM3438_data_port_0_B_w(0, data);
			break;
	}
}

static WRITE16_HANDLER( m1_snd_68k_latch1_w )
{
}

static WRITE16_HANDLER( m1_snd_68k_latch2_w )
{
}

static ADDRESS_MAP_START( model1_snd, ADDRESS_SPACE_PROGRAM, 16 )
	AM_RANGE(0x000000, 0x0bffff) AM_ROM
	AM_RANGE(0xc20000, 0xc20001) AM_READWRITE( m1_snd_68k_latch_r, m1_snd_68k_latch1_w )
	AM_RANGE(0xc20002, 0xc20003) AM_READWRITE( m1_snd_v60_ready_r, m1_snd_68k_latch2_w )
	AM_RANGE(0xc40000, 0xc40007) AM_READWRITE( m1_snd_mpcm0_r, m1_snd_mpcm0_w )
	AM_RANGE(0xc40012, 0xc40013) AM_WRITENOP
	AM_RANGE(0xc50000, 0xc50001) AM_WRITE( m1_snd_mpcm0_bnk_w )
	AM_RANGE(0xc60000, 0xc60007) AM_READWRITE( m1_snd_mpcm1_r, m1_snd_mpcm1_w )
	AM_RANGE(0xc70000, 0xc70001) AM_WRITE( m1_snd_mpcm1_bnk_w )
	AM_RANGE(0xd00000, 0xd00007) AM_READWRITE( m1_snd_ym_r, m1_snd_ym_w )
	AM_RANGE(0xf00000, 0xf0ffff) AM_RAM
ADDRESS_MAP_END

static struct MultiPCM_interface m1_multipcm_interface_1 =
{
	REGION_SOUND1
};

static struct MultiPCM_interface m1_multipcm_interface_2 =
{
	REGION_SOUND2
};

INPUT_PORTS_START( vf )
	PORT_START_TAG("AN0")  /* Unused analog port 0 */
	PORT_START_TAG("AN1")  /* Unused analog port 1 */
	PORT_START_TAG("AN2")  /* Unused analog port 2 */
	PORT_START_TAG("AN3")  /* Unused analog port 3 */
	PORT_START_TAG("AN4")  /* Unused analog port 4 */
	PORT_START_TAG("AN5")  /* Unused analog port 5 */
	PORT_START_TAG("AN6")  /* Unused analog port 6 */
	PORT_START_TAG("AN7")  /* Unused analog port 7 */

	PORT_START_TAG("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNKNOWN )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNKNOWN )

	PORT_START_TAG("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(1)

	PORT_START_TAG("IN2")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_JOYSTICK_DOWN ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_JOYSTICK_UP ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_JOYSTICK_RIGHT ) PORT_8WAY PORT_PLAYER(2)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_JOYSTICK_LEFT ) PORT_8WAY PORT_PLAYER(2)
INPUT_PORTS_END

INPUT_PORTS_START( vr )
	PORT_START_TAG("AN0")	/* Steering */
	PORT_BIT( 0xff, 0x80, IPT_PADDLE ) PORT_MINMAX(0,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(3)

	PORT_START_TAG("AN1")	/* Accel / Decel */
	PORT_BIT( 0xff, 0x30, IPT_PEDAL ) PORT_MINMAX(1,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(16)

	PORT_START_TAG("AN2")	/* Brake */
	PORT_BIT( 0xff, 0x30, IPT_PEDAL ) PORT_MINMAX(1,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(16) PORT_PLAYER(2)

	PORT_START_TAG("AN3")  /* Unused analog port 3 */
	PORT_START_TAG("AN4")  /* Unused analog port 4 */
	PORT_START_TAG("AN5")  /* Unused analog port 5 */
	PORT_START_TAG("AN6")  /* Unused analog port 6 */
	PORT_START_TAG("AN7")  /* Unused analog port 7 */

	PORT_START_TAG("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)

	PORT_START_TAG("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON6 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START_TAG("IN2")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( wingwar )
	PORT_START_TAG("AN0")	/* X */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_X ) PORT_MINMAX(0x00,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(4)

	PORT_START_TAG("AN1")	/* Y */
	PORT_BIT( 0xff, 0x80, IPT_AD_STICK_Y ) PORT_MINMAX(0x00,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_REVERSE

	PORT_START_TAG("AN2")	/* Throttle */
	PORT_BIT( 0xff, 0x00, IPT_PEDAL ) PORT_MINMAX(1,0xff) PORT_SENSITIVITY(100) PORT_KEYDELTA(16)

	PORT_START_TAG("AN3")  /* Unused analog port 3 */
	PORT_START_TAG("AN4")  /* Unused analog port 4 */
	PORT_START_TAG("AN5")  /* Unused analog port 5 */
	PORT_START_TAG("AN6")  /* Unused analog port 6 */
	PORT_START_TAG("AN7")  /* Unused analog port 7 */

	PORT_START_TAG("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)

	PORT_START_TAG("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON4 ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_UNUSED )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON5 ) PORT_PLAYER(1)
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_BUTTON6 ) PORT_PLAYER(1)
	PORT_BIT( 0x40, IP_ACTIVE_LOW, IPT_BUTTON7 ) PORT_PLAYER(1)
	PORT_BIT( 0x80, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START_TAG("IN2")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

INPUT_PORTS_START( swa )
	PORT_START_TAG("AN0")	/* X */
	PORT_BIT( 0xff, 127, IPT_AD_STICK_X ) PORT_MINMAX(27,227) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_REVERSE

	PORT_START_TAG("AN1")	/* Y */
	PORT_BIT( 0xff, 127, IPT_AD_STICK_Y ) PORT_MINMAX(27,227) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_REVERSE

	PORT_START_TAG("AN2")	/* Throttle */
	PORT_BIT( 0xff, 228, IPT_PEDAL ) PORT_MINMAX(28,228) PORT_SENSITIVITY(100) PORT_KEYDELTA(16) PORT_REVERSE

	PORT_START_TAG("AN3")  /* Unused analog port 3 */

	PORT_START_TAG("AN4")	/* X */
	PORT_BIT( 0xff, 127, IPT_AD_STICK_X ) PORT_MINMAX(27,227) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_REVERSE PORT_PLAYER(2)

	PORT_START_TAG("AN5")	/* Y */
	PORT_BIT( 0xff, 127, IPT_AD_STICK_Y ) PORT_MINMAX(27,227) PORT_SENSITIVITY(100) PORT_KEYDELTA(4) PORT_PLAYER(2)

	PORT_START_TAG("AN6")  /* Unused analog port 6 */
	PORT_START_TAG("AN7")  /* Unused analog port 7 */

	PORT_START_TAG("IN0")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_COIN1 )
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_COIN2 )
	PORT_SERVICE_NO_TOGGLE(0x04, IP_ACTIVE_LOW)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_SERVICE1 )
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_START1 )
	PORT_BIT( 0x20, IP_ACTIVE_LOW, IPT_START2 )
	PORT_BIT( 0xc0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START_TAG("IN1")
	PORT_BIT( 0x01, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(1)
	PORT_BIT( 0x02, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(1)
	PORT_BIT( 0x04, IP_ACTIVE_LOW, IPT_BUTTON1 ) PORT_PLAYER(2)
	PORT_BIT( 0x08, IP_ACTIVE_LOW, IPT_BUTTON2 ) PORT_PLAYER(2)
	PORT_BIT( 0x10, IP_ACTIVE_LOW, IPT_BUTTON3 ) PORT_PLAYER(1)
	PORT_BIT( 0xe0, IP_ACTIVE_LOW, IPT_UNUSED )

	PORT_START_TAG("IN2")
	PORT_BIT( 0xff, IP_ACTIVE_LOW, IPT_UNUSED )
INPUT_PORTS_END

ROM_START( vf )
	ROM_REGION( 0x1400000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16082.14", 0x200000, 0x80000, CRC(b23f22ee) SHA1(9fd5b5a5974703a60a54de3d2bce4301bfc0e533) )
	ROM_LOAD16_BYTE( "epr-16083.15", 0x200001, 0x80000, CRC(d12c77f8) SHA1(b4aeba8d5f1ab4aec024391407a2cb58ce2e94b0) )

	ROM_LOAD( "epr-16080.4", 0xfc0000, 0x20000, CRC(3662E1A5) SHA1(6bfceb1a7c1c7912679c907f2b7516ae9c7dda67) )
	ROM_LOAD( "epr-16081.5", 0xfe0000, 0x20000, CRC(6DEC06CE) SHA1(7891544456bccd2fc647bccd058945ad50466636) )

	ROM_LOAD16_BYTE( "mpr-16084.6", 0x1000000, 0x80000, CRC(483f453b) SHA1(41a5527be73f5dd1c87b2a8113235bdd247ec049) )
	ROM_LOAD16_BYTE( "mpr-16085.7", 0x1000001, 0x80000, CRC(5fa01277) SHA1(dfa7ddff0a7daf29071431f26b93dd8e8e5793b6) )
	ROM_LOAD16_BYTE( "mpr-16086.8", 0x1100000, 0x80000, CRC(deac47a1) SHA1(3a8016124e4dc579d4aae745d4af1905ad0e4fbd) )
	ROM_LOAD16_BYTE( "mpr-16087.9", 0x1100001, 0x80000, CRC(7a64daac) SHA1(da6a9cad4b0cb2af4299e664c0889f3fbdc25530) )
	ROM_LOAD16_BYTE( "mpr-16088.10", 0x1200000, 0x80000, CRC(fcda2d1e) SHA1(0f7d0f604d429a1da0d1c3f31694520bada49680) )
	ROM_LOAD16_BYTE( "mpr-16089.11", 0x1200001, 0x80000, CRC(39befbe0) SHA1(362c493092cd0536fadee7326ecc7f973e23fb58) )
	ROM_LOAD16_BYTE( "mpr-16090.12", 0x1300000, 0x80000, CRC(90c76831) SHA1(5a3c25f2a131cfbb2ad067bef1ab7b1c95645d41) )
	ROM_LOAD16_BYTE( "mpr-16091.13", 0x1300001, 0x80000, CRC(53115448) SHA1(af798d5b1fcb720d7288a5ac48839d9ace16a2f2) )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-16120.7", 0x00000, 0x20000, CRC(2bff8378) SHA1(854b08ab983e4e98cb666f2f44de9a6829b1eb52) )
	ROM_LOAD16_WORD_SWAP( "epr-16121.8", 0x20000, 0x20000, CRC(ff6723f9) SHA1(53498b8c103745883657dfd6efe27edfd48b356f) )
	ROM_RELOAD( 0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-16122.32", 0x000000, 0x200000, CRC(568bc64e) SHA1(31fd0ef8319efe258011b4621adebb790b620770) )
	ROM_LOAD( "mpr-16123.33", 0x200000, 0x200000, CRC(15d78844) SHA1(37c17e38604cf7004a951408024941cd06b1d93e) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-16124.4", 0x000000, 0x200000, CRC(45520ba1) SHA1(c33e3c12639961016e5fa6b5025d0a67dff28907) )
	ROM_LOAD( "mpr-16125.5", 0x200000, 0x200000, CRC(9b4998b6) SHA1(0418d9b0acf79f35d0f7575c21f1be9a0ea343da) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16096.26", 0x000000, 0x200000, CRC(a92b0bf3) SHA1(fd3adff5f41f0b0be98df548c848eda04fc0da48) )
	ROM_LOAD32_WORD( "mpr-16097.27", 0x000002, 0x200000, CRC(0232955a) SHA1(df934fb6d022032620932571ff5ed176d5dcb017) )
	ROM_LOAD32_WORD( "mpr-16098.28", 0x400000, 0x200000, CRC(cf2e1b84) SHA1(f3d16c72344f7f218a792ce7f1dd7cad910a8c97) )
	ROM_LOAD32_WORD( "mpr-16099.29", 0x400002, 0x200000, CRC(20e46854) SHA1(423d3642bd2f14e68d29029c027b791de2c1ec53) )
	ROM_LOAD32_WORD( "mpr-16100.30", 0x800000, 0x200000, CRC(e13e983d) SHA1(120637caa2404ad4124b676fd6fcd721f30948df) )
	ROM_LOAD32_WORD( "mpr-16101.31", 0x800002, 0x200000, CRC(0dbed94d) SHA1(df1cddcc1d3976816bd786c2d6211a8563f6f690) )
	ROM_LOAD32_WORD( "mpr-16102.32", 0xc00000, 0x200000, CRC(4cb41fb6) SHA1(4a07bfad4f221508de8c931861424dcc5be3f46a) )
	ROM_LOAD32_WORD( "mpr-16103.33", 0xc00002, 0x200000, CRC(526d1c76) SHA1(edc8dafc9261cd0e970c3b50e3c1ca51a32a4cdf) )
ROM_END

ROM_START( vr )
	ROM_REGION( 0x1400000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-14882.14", 0x200000, 0x80000, CRC(547D75AD) SHA1(a57c11966886c37de1d7df131ad60457669231dd) )
	ROM_LOAD16_BYTE( "epr-14883.15", 0x200001, 0x80000, CRC(6BFAD8B1) SHA1(c1f780e456b405abd42d92f4e03e40aad88f8c22) )

	ROM_LOAD( "epr-14878a.4", 0xfc0000, 0x20000, CRC(6D69E695) SHA1(12d3612d3dfd474b8020cdfb8ffc5dcc64e2e1a3) )
	ROM_LOAD( "epr-14879a.5", 0xfe0000, 0x20000, CRC(D45AF9DD) SHA1(48f2bf940c78e3ae4273a56e9f32371d870e41e0) )

	ROM_LOAD16_BYTE( "mpr-14880.6",  0x1000000, 0x80000, CRC(ADC7C208) SHA1(967b6f522011f17fd2821ccbe20bcb6d4680a4a0) )
	ROM_LOAD16_BYTE( "mpr-14881.7",  0x1000001, 0x80000, CRC(E5AB89DF) SHA1(873dea86628457e69f732158e3efb05d133eaa44) )
	ROM_LOAD16_BYTE( "mpr-14884.8",  0x1100000, 0x80000, CRC(6CF9C026) SHA1(f4d717958dba6b6402f5ffe7638fe0bf353b61ed) )
	ROM_LOAD16_BYTE( "mpr-14885.9",  0x1100001, 0x80000, CRC(F65C9262) SHA1(511a22bcfaf199737bfc5a809fd66cb4439dd386) )
	ROM_LOAD16_BYTE( "mpr-14886.10", 0x1200000, 0x80000, CRC(92868734) SHA1(e1dfb134dc3ba7e0b1d40681621e09ac5a222518) )
	ROM_LOAD16_BYTE( "mpr-14887.11", 0x1200001, 0x80000, CRC(10C7C636) SHA1(c77d55460bba4354349e473e129f21afeed05e91) )
	ROM_LOAD16_BYTE( "mpr-14888.12", 0x1300000, 0x80000, CRC(04BFDC5B) SHA1(bb8788a761620d0440a62ae51c3b41f70a04b5e4) )
	ROM_LOAD16_BYTE( "mpr-14889.13", 0x1300001, 0x80000, CRC(C49F0486) SHA1(cc2bb9059c016ba2c4f6e7508bd1687df07b8b48) )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-14870a.7", 0x00000, 0x20000, CRC(919d9b75) SHA1(27be79881cc9a2b5cf37e18f1e2d87251426b428) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-14873.32", 0x000000, 0x200000, CRC(b1965190) SHA1(fc47e9ed4a44d48477bd9a35e42c26508c0f4a0c) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-14876.4", 0x000000, 0x200000, CRC(ba6b2327) SHA1(02285520624a4e612cb4b65510e3458b13b1c6ba) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-14890.26", 0x000000, 0x200000, CRC(dcbe006b) SHA1(195be7fabec405ca1b4e1338d3b8d7bb4a06dd73) )
	ROM_LOAD32_WORD( "mpr-14891.27", 0x000002, 0x200000, CRC(25832b38) SHA1(a8d74538149c92bae661334e327b031eaca2a8f2) )
	ROM_LOAD32_WORD( "mpr-14892.28", 0x400000, 0x200000, CRC(5136f3ba) SHA1(ce8312975764db09bbfa2068b99559a5c1798a36) )
	ROM_LOAD32_WORD( "mpr-14893.29", 0x400002, 0x200000, CRC(1c531ada) SHA1(8b373ac97a3649c64f48eb3d1dd95c5833f330b6) )
	ROM_LOAD32_WORD( "mpr-14894.30", 0x800000, 0x200000, CRC(830a71bc) SHA1(884378e8a5afeb819daf5285d0d205986d566340) )
	ROM_LOAD32_WORD( "mpr-14895.31", 0x800002, 0x200000, CRC(af027ac5) SHA1(523f03d90358ddb7d0e96fd06b9a65cebfc09f24) )
	ROM_LOAD32_WORD( "mpr-14896.32", 0xc00000, 0x200000, CRC(382091dc) SHA1(efa266f0f6bfe36ad1c365e588fff33b01e166dd) )
	ROM_LOAD32_WORD( "mpr-14879.33", 0xc00002, 0x200000, CRC(74873195) SHA1(80705ec577d14570f9bba77cc26766f831c41f42) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-14898.39", 0x000000, 0x80000, CRC(61da2bb6) SHA1(7a12ba522d64a1aeec1ca6f5a87ee063692131f9) )
	ROM_LOAD32_BYTE( "mpr-14899.40", 0x000001, 0x80000, CRC(2cd58bee) SHA1(73defec823de4244a387af55fea7210edc1b314f) )
	ROM_LOAD32_BYTE( "mpr-14900.41", 0x000002, 0x80000, CRC(aa7c017d) SHA1(0fa2b59a8bb5f5907b2b2567e69d11c73b398dc1) )
	ROM_LOAD32_BYTE( "mpr-14901.42", 0x000003, 0x80000, CRC(175b7a9a) SHA1(c86602e771cd49bab425b4ba7926d2f44858bd39) )
ROM_END

ROM_START( vformula )
	ROM_REGION( 0x1400000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr15638.14", 0x200000, 0x80000, CRC(b9db21a2) SHA1(db58c047977f5fc37f278afe7159a78e3fa6c015) )
	ROM_LOAD16_BYTE( "epr15639.15", 0x200001, 0x80000, CRC(4c3796f5) SHA1(1bf312a4999a15fbc5d194627f9c0ad9dbc1f2c0) )

	ROM_LOAD( "epr15623.4", 0xfc0000, 0x20000, CRC(155fa5be) SHA1(a0a3fd8980c52279adbc1c64aa22e42a0b196dd9) )
	ROM_LOAD( "epr15622.5", 0xfe0000, 0x20000, CRC(18007f6f) SHA1(61573742627ec027abd2cc700e79f04da5215bfd) )

	ROM_LOAD16_BYTE( "epr15641.6",  0x1000000, 0x80000, CRC(bf01e4d5) SHA1(53ad9ecd2de2ea1d7b446f9db61352e10a55ea05) )
	ROM_LOAD16_BYTE( "epr15640.7",  0x1000001, 0x80000, CRC(3e6d83dc) SHA1(62aa552a38ee193e0dfab5d1261756fe237db42c) )
	ROM_LOAD16_BYTE( "mpr-14884.8",  0x1100000, 0x80000, CRC(6CF9C026) SHA1(f4d717958dba6b6402f5ffe7638fe0bf353b61ed) )
	ROM_LOAD16_BYTE( "mpr-14885.9",  0x1100001, 0x80000, CRC(F65C9262) SHA1(511a22bcfaf199737bfc5a809fd66cb4439dd386) )
	ROM_LOAD16_BYTE( "mpr-14886.10", 0x1200000, 0x80000, CRC(92868734) SHA1(e1dfb134dc3ba7e0b1d40681621e09ac5a222518) )
	ROM_LOAD16_BYTE( "mpr-14887.11", 0x1200001, 0x80000, CRC(10C7C636) SHA1(c77d55460bba4354349e473e129f21afeed05e91) )
	ROM_LOAD16_BYTE( "mpr-14888.12", 0x1300000, 0x80000, CRC(04BFDC5B) SHA1(bb8788a761620d0440a62ae51c3b41f70a04b5e4) )
	ROM_LOAD16_BYTE( "mpr-14889.13", 0x1300001, 0x80000, CRC(C49F0486) SHA1(cc2bb9059c016ba2c4f6e7508bd1687df07b8b48) )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP( "epr-14870a.7", 0x00000, 0x20000, CRC(919d9b75) SHA1(27be79881cc9a2b5cf37e18f1e2d87251426b428) )

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD( "mpr-14873.32", 0x000000, 0x200000, CRC(b1965190) SHA1(fc47e9ed4a44d48477bd9a35e42c26508c0f4a0c) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD( "mpr-14876.4", 0x000000, 0x200000, CRC(ba6b2327) SHA1(02285520624a4e612cb4b65510e3458b13b1c6ba) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-14890.26", 0x000000, 0x200000, CRC(dcbe006b) SHA1(195be7fabec405ca1b4e1338d3b8d7bb4a06dd73) )
	ROM_LOAD32_WORD( "mpr-14891.27", 0x000002, 0x200000, CRC(25832b38) SHA1(a8d74538149c92bae661334e327b031eaca2a8f2) )
	ROM_LOAD32_WORD( "mpr-14892.28", 0x400000, 0x200000, CRC(5136f3ba) SHA1(ce8312975764db09bbfa2068b99559a5c1798a36) )
	ROM_LOAD32_WORD( "mpr-14893.29", 0x400002, 0x200000, CRC(1c531ada) SHA1(8b373ac97a3649c64f48eb3d1dd95c5833f330b6) )
	ROM_LOAD32_WORD( "mpr-14894.30", 0x800000, 0x200000, CRC(830a71bc) SHA1(884378e8a5afeb819daf5285d0d205986d566340) )
	ROM_LOAD32_WORD( "mpr-14895.31", 0x800002, 0x200000, CRC(af027ac5) SHA1(523f03d90358ddb7d0e96fd06b9a65cebfc09f24) )
	ROM_LOAD32_WORD( "mpr-14896.32", 0xc00000, 0x200000, CRC(382091dc) SHA1(efa266f0f6bfe36ad1c365e588fff33b01e166dd) )
	ROM_LOAD32_WORD( "mpr-14879.33", 0xc00002, 0x200000, CRC(74873195) SHA1(80705ec577d14570f9bba77cc26766f831c41f42) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-14898.39", 0x000000, 0x80000, CRC(61da2bb6) SHA1(7a12ba522d64a1aeec1ca6f5a87ee063692131f9) )
	ROM_LOAD32_BYTE( "mpr-14899.40", 0x000001, 0x80000, CRC(2cd58bee) SHA1(73defec823de4244a387af55fea7210edc1b314f) )
	ROM_LOAD32_BYTE( "mpr-14900.41", 0x000002, 0x80000, CRC(aa7c017d) SHA1(0fa2b59a8bb5f5907b2b2567e69d11c73b398dc1) )
	ROM_LOAD32_BYTE( "mpr-14901.42", 0x000003, 0x80000, CRC(175b7a9a) SHA1(c86602e771cd49bab425b4ba7926d2f44858bd39) )

	ROM_REGION( 0x20000, REGION_USER3, 0 ) /* Comms Board */
	ROM_LOAD( "epr15624.17", 0x00000, 0x20000, CRC(9b3ba315) SHA1(0cd0983cc8b2f2d6b41617d0d0a24cc6c188e62a) )
ROM_END


ROM_START( swa )
	ROM_REGION( 0x1000000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16468.14", 0x200000, 0x80000, CRC(681d03c0) SHA1(4d21e26ce211466d429b84bca69a8147ff31ec6c) )
	ROM_LOAD16_BYTE( "epr-16469.15", 0x200001, 0x80000, CRC(6f281f7c) SHA1(6a9179e48d14838bb2a1a3f63fdd3a68ed009e03) )

	ROM_LOAD( "epr-16467.5", 0xf80000, 0x80000, CRC(605068f5) SHA1(99d7e171ce3353477c282d7567dedb9947206f14) )
	ROM_RELOAD(          0x000000, 0x80000 )
	ROM_RELOAD(          0x080000, 0x80000 )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
        ROM_LOAD16_WORD_SWAP( "epr16470.bin", 0x000000, 0x020000, CRC(7da18cf7) SHA1(bd432d882d217277faee120e2577357a32eb4a6e) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
        ROM_LOAD( "mpr16486.bin", 0x000000, 0x200000, CRC(7df50533) SHA1(f2fb876738e37d70eb9005e5629a9ae89aa413a8) )
        ROM_LOAD( "mpr16487.bin", 0x200000, 0x200000, CRC(31b28dfa) SHA1(bd1ac11bf2f9161f61f8af3b9ff4c2709b7ee700) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
        ROM_LOAD( "mpr16484.bin", 0x000000, 0x200000, CRC(9d4c334d) SHA1(8b4d903f14559fed425d225bb23ccfe8da23cbd3) )
        ROM_LOAD( "mpr16485.bin", 0x200000, 0x200000, CRC(95aadcad) SHA1(4276db655db9834692c3843eb96a3e3a89cb7252) )

	ROM_REGION( 0x20000, REGION_CPU3, 0 ) /* Z80 DSB code */
        ROM_LOAD( "epr16471.bin", 0x000000, 0x020000, CRC(f4ee84a4) SHA1(f12b214e6f195b0e5f49ba9f41d8e54bfcea9acc) )

	ROM_REGION( 0x400000, REGION_SOUND3, 0 ) /* DSB MPEG data */
        ROM_LOAD( "mpr16514.bin", 0x000000, 0x200000, CRC(3175b0be) SHA1(63649d053c8c17ce1746d16d0cc8202be20c302f) )
        ROM_LOAD( "mpr16515.bin", 0x000000, 0x200000, CRC(3114d748) SHA1(9ef090623cdd2a1d06b5d1bc4b9a07ab4eff5b76) )

	ROM_REGION32_LE( 0xc00000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16476.26", 0x000000, 0x200000, CRC(d48609ae) SHA1(8c8686a5c9ca4837447a7f70ed194e2f1882b66d) )
// original dump (which one is right?)
//  ROM_LOAD32_WORD( "mpr-16477.27", 0x000002, 0x200000, CRC(b979b082) SHA1(0c60d259093e987f3856730b57b43bde7e9562e3) )
// new dump
        ROM_LOAD32_WORD( "mpr16477.bin", 0x000002, 0x200000, CRC(971ff194) SHA1(9665ede3ca22885489f1f1b5865ccfac42364206) )
	ROM_LOAD32_WORD( "mpr-16478.28", 0x400000, 0x200000, CRC(80c780f7) SHA1(2f57c5373b02765d302bcd81e24f7b7bc4181387) )
	ROM_LOAD32_WORD( "mpr-16479.29", 0x400002, 0x200000, CRC(e43183b3) SHA1(4e62c67cdf7a6fdac0ded86d5f9e81044b9dea8d) )
	ROM_LOAD32_WORD( "mpr-16480.30", 0x800000, 0x200000, CRC(3185547a) SHA1(9871937372c2c755717802117a3ad39e1a11410e) )
	ROM_LOAD32_WORD( "mpr-16481.31", 0x800002, 0x200000, CRC(ce8d76fe) SHA1(0406f0500d19d6707515627b4143f92a9a5db769) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16472.39", 0x000000, 0x80000, CRC(5a0d7553) SHA1(ba8e08e5a0c6b7fbc10084ad7ad3edf61efb0d70) )
	ROM_LOAD32_BYTE( "mpr-16473.40", 0x000001, 0x80000, CRC(876c5399) SHA1(be7e40c77a385600941f11c24852cd73c71696f0) )
	ROM_LOAD32_BYTE( "mpr-16474.41", 0x000002, 0x80000, CRC(5864a26f) SHA1(be0c22dfff37408f6b401b1970f7fcc6fc7fbcd2) )
	ROM_LOAD32_BYTE( "mpr-16475.42", 0x000003, 0x80000, CRC(b9266be9) SHA1(cf195cd89c9d191b9eb8c5299f8cc154c2b4bd82) )
ROM_END

ROM_START( wingwar )
	ROM_REGION( 0x1300000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr-16951.4", 0xfc0000, 0x20000, CRC(8df5a798) SHA1(ef2756f237933ecf429dab0f362e572eb1965f4d) )
	ROM_RELOAD(          0x000000, 0x20000 )
	ROM_LOAD( "epr-16950.5", 0xfe0000, 0x20000, CRC(841e2195) SHA1(66f465aaf71955496e6f83335f7b836ad1d8c724) )
	ROM_RELOAD(          0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "mpr-16738.6",  0x1000000, 0x80000, CRC(51518ffa) SHA1(e4674ddfed4205957b14e133c6fdf6454872f324) )
	ROM_LOAD16_BYTE( "mpr-16737.7",  0x1000001, 0x80000, CRC(37b1379c) SHA1(98620c324268e1dd906c077ac8a8cd903b9de1f7) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP("epr-16751.7", 0x000000, 0x20000, CRC(23ba5ebc) SHA1(b98aab546c5e980baeedbada4e7472eb4c588260) )
	ROM_LOAD16_WORD_SWAP("epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD("mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD("mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD("mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD("mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )
ROM_END

ROM_START( wingwara )
	ROM_REGION( 0x1300000, REGION_CPU1, 0 ) /* v60 code */
	ROM_LOAD16_BYTE( "epr-16729.14", 0x200000, 0x80000, CRC(7edec2cc) SHA1(3e423a868ca7c8475fbb5bc1a10526e69d94d865) )
	ROM_LOAD16_BYTE( "epr-16730.15", 0x200001, 0x80000, CRC(bab24dee) SHA1(26c95139c1aa7f34b6a5cce39e5bd1dd2ef0dd49) )

	ROM_LOAD( "epr16953.bin", 0xfc0000, 0x20000, CRC(c821a920) SHA1(7fc9ea5d828aac664514fa6d38f708f1ffd26220) )
	ROM_RELOAD(          0x000000, 0x20000 )
	ROM_LOAD( "epr16952.bin", 0xfe0000, 0x20000, CRC(03a3ecc5) SHA1(5c4aa221302b0a0800e1af99a41ab46fe4325184) )
	ROM_RELOAD(          0x020000, 0x20000 )

	ROM_LOAD16_BYTE( "mpr-16738.6",  0x1000000, 0x80000, CRC(51518ffa) SHA1(e4674ddfed4205957b14e133c6fdf6454872f324) )
	ROM_LOAD16_BYTE( "mpr-16737.7",  0x1000001, 0x80000, CRC(37b1379c) SHA1(98620c324268e1dd906c077ac8a8cd903b9de1f7) )
	ROM_LOAD16_BYTE( "mpr-16736.8",  0x1100000, 0x80000, CRC(10b6a025) SHA1(7a4f624ceb7c0b92044a5db8ff55440562ef836b) )
	ROM_LOAD16_BYTE( "mpr-16735.9",  0x1100001, 0x80000, CRC(c82fd198) SHA1(d9e53ae1e14dfc8e84a14c0026ef0b904863bb1b) )
	ROM_LOAD16_BYTE( "mpr-16734.10", 0x1200000, 0x80000, CRC(f76371c1) SHA1(0ff082db3877383d0dd977dc60c932b725e3d164) )
	ROM_LOAD16_BYTE( "mpr-16733.11", 0x1200001, 0x80000, CRC(e105847b) SHA1(8489a6c91fd6d1e9ba81e8eaf36c514da30dccbe) )

	ROM_REGION( 0xc0000, REGION_CPU2, 0 )  /* 68K code */
	ROM_LOAD16_WORD_SWAP("epr17126.bin",0x000000, 0x20000, CRC(50178e40) SHA1(fb01aecfbe4e90adc997de0d45a63c16ef353b37) )
	ROM_LOAD16_WORD_SWAP("epr-16752.8", 0x020000, 0x20000, CRC(6541c48f) SHA1(9341eff160e31a8574b9545fafc1c4059323fa0c) )
	ROM_RELOAD(0x80000, 0x20000)

	ROM_REGION( 0x400000, REGION_SOUND1, 0 ) /* Samples */
	ROM_LOAD("mpr-16753.32", 0x000000, 0x200000, CRC(324a8333) SHA1(960342e08db637c6f72615d49cffd9fb0889620b) )
	ROM_LOAD("mpr-16754.33", 0x200000, 0x200000, CRC(144f3cf5) SHA1(d2f8cc9086affbbc5ef2195272200230f724e5d1) )

	ROM_REGION( 0x400000, REGION_SOUND2, 0 ) /* Samples */
	ROM_LOAD("mpr-16755.4", 0x000000, 0x200000, CRC(4baaf878) SHA1(661d4ea9be6a4952852d0ef94becee7ed42bf4a1) )
	ROM_LOAD("mpr-16756.5", 0x200000, 0x200000, CRC(d9c40672) SHA1(83e6f1156b30888d3a00103f079dc74f4fca8446) )

	ROM_REGION32_LE( 0x1000000, REGION_USER1, 0 ) /* TGP model roms */
	ROM_LOAD32_WORD( "mpr-16743.26", 0x000000, 0x200000, CRC(a710d33c) SHA1(1d0184545b34789ed511caaa25d57db3cd9a8e2f) )
	ROM_LOAD32_WORD( "mpr-16744.27", 0x000002, 0x200000, CRC(de796e1f) SHA1(397efb86a21b178770f29d2464bacf5f893619a0) )
	ROM_LOAD32_WORD( "mpr-16745.28", 0x400000, 0x200000, CRC(905b689c) SHA1(685dec2a65d5b3a386bda0af1bb5ae7e2b73a01a) )
	ROM_LOAD32_WORD( "mpr-16746.29", 0x400002, 0x200000, CRC(163b312e) SHA1(6b45007d6a9d17c8a0b46d81ec84ce9bfefb1695) )
	ROM_LOAD32_WORD( "mpr-16747.30", 0x800000, 0x200000, CRC(7353bb12) SHA1(608c5d561e909b8ba31d53db18e6e199855eaaec) )
	ROM_LOAD32_WORD( "mpr-16748.31", 0x800002, 0x200000, CRC(8ce98d3a) SHA1(1978776a0e2ea817508e30ba232d5f8d9c158f70) )
	ROM_LOAD32_WORD( "mpr-16749.32", 0xc00000, 0x200000, CRC(0e36dc1a) SHA1(4939177a6ac51ca57d0acd118ff14af4f4e438bb) )
	ROM_LOAD32_WORD( "mpr-16750.33", 0xc00002, 0x200000, CRC(e4f0b98d) SHA1(e69de2e5ccea2834fb8326bdd61fc6b517bc60b7) )

	ROM_REGION32_LE( 0x200000, REGION_USER2, 0 ) /* TGP data roms */
	ROM_LOAD32_BYTE( "mpr-16741.39", 0x000000, 0x80000, CRC(84b2ffd8) SHA1(0eba3855d20b88567c6fa08046e12429664d87cb) )
	ROM_LOAD32_BYTE( "mpr-16742.40", 0x000001, 0x80000, CRC(e9cc12bb) SHA1(40c83c968be3b11fad193a00e7b760f074450683) )
	ROM_LOAD32_BYTE( "mpr-16739.41", 0x000002, 0x80000, CRC(6c73e98f) SHA1(7b31e62922ab6d0df97c3ecc52b78e6d086c8635) )
	ROM_LOAD32_BYTE( "mpr-16740.42", 0x000003, 0x80000, CRC(44b31007) SHA1(4bb265fea25a7bbcbb8ab080fdcf09849b18f1de) )
ROM_END

static MACHINE_DRIVER_START( model1 )
	MDRV_CPU_ADD(V60, 16000000)
	MDRV_CPU_PROGRAM_MAP(model1_mem, 0)
	MDRV_CPU_IO_MAP(model1_io, 0)
	MDRV_CPU_VBLANK_INT(model1_interrupt, 2)

	MDRV_CPU_ADD(M68000, 12000000)	// Confirmed 10 MHz on real PCB, run slightly faster here to prevent sync trouble
	MDRV_CPU_PROGRAM_MAP(model1_snd, 0)

	MDRV_FRAMES_PER_SECOND(60)
	MDRV_VBLANK_DURATION(DEFAULT_60HZ_VBLANK_DURATION)

	MDRV_MACHINE_INIT(model1)
	MDRV_NVRAM_HANDLER(generic_0fill)

	MDRV_VIDEO_ATTRIBUTES(VIDEO_TYPE_RASTER | VIDEO_UPDATE_AFTER_VBLANK | VIDEO_RGB_DIRECT)
	MDRV_SCREEN_SIZE(62*8, 48*8)
	MDRV_VISIBLE_AREA(0*8, 62*8-1, 0*8, 48*8-1)
	MDRV_PALETTE_LENGTH(8192)

	MDRV_VIDEO_START(model1)
	MDRV_VIDEO_UPDATE(model1)
	MDRV_VIDEO_EOF(model1)

	MDRV_SPEAKER_STANDARD_STEREO("left", "right")

	MDRV_SOUND_ADD(YM3438, 8000000)
	MDRV_SOUND_ROUTE(0, "left", 0.60)
	MDRV_SOUND_ROUTE(1, "right", 0.60)

	MDRV_SOUND_ADD(MULTIPCM, 8000000)
	MDRV_SOUND_CONFIG(m1_multipcm_interface_1)
	MDRV_SOUND_ROUTE(0, "left", 1.0)
	MDRV_SOUND_ROUTE(1, "right", 1.0)

	MDRV_SOUND_ADD(MULTIPCM, 8000000)
	MDRV_SOUND_CONFIG(m1_multipcm_interface_2)
	MDRV_SOUND_ROUTE(0, "left", 1.0)
	MDRV_SOUND_ROUTE(1, "right", 1.0)
MACHINE_DRIVER_END

GAMEX( 1993, vf,      0, model1, vf,      0, ROT0, "Sega", "Virtua Fighter", GAME_IMPERFECT_GRAPHICS )
GAMEX( 1992, vr,       0, model1, vr,       0, ROT0, "Sega", "Virtua Racing", GAME_NOT_WORKING )
GAMEX( 1993, vformula, vr, model1, vr,       0, ROT0, "Sega", "Virtua Formula", GAME_NOT_WORKING )
GAMEX( 1993, swa,      0, model1, swa,      0, ROT0, "Sega", "Star Wars Arcade", GAME_NOT_WORKING|GAME_NO_SOUND )
GAMEX( 1994, wingwar,  0, model1, wingwar,  0, ROT0, "Sega", "Wing War (US)", GAME_NOT_WORKING )
GAMEX( 1994, wingwara, wingwar, model1, wingwar,  0, ROT0, "Sega", "Wing War", GAME_NOT_WORKING )
