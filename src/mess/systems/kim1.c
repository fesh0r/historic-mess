/******************************************************************************
	KIM-1

    MESS Driver

	Juergen Buchmueller, Oct 1999
******************************************************************************/

#include "driver.h"
#include "artwork.h"
#include "cpu/m6502/m6502.h"
#include "vidhrdw/generic.h"
//#include "mess/machine/kim1.h"
//#include "mess/vidhrdw/kim1.h"

#define VERBOSE 1

#if VERBOSE
#define LOG(x)	if( errorlog ) fprintf x
#else
#define LOG(x)	/* x */
#endif

/******************************************************************************
	KIM-1 memory map

	range	  short 	description
	0000-03FF RAM
	1400-16FF ???
	1700-173F 6530-003	see 6530
	1740-177F 6530-002	see 6530
	1780-17BF RAM		internal 6530-003
	17C0-17FF RAM		internal 6530-002
	1800-1BFF ROM		internal 6530-003
	1C00-1FFF ROM		internal 6530-002

	6530
	offset	R/W short	purpose
	0		X	DRA 	Data register A
	1		X	DDRA	Data direction register A
	2		X	DRB 	Data register B
	3		X	DDRB	Data direction register B
	4		W	CNT1T	Count down from value, divide by 1, disable IRQ
	5		W	CNT8T	Count down from value, divide by 8, disable IRQ
	6		W	CNT64T	Count down from value, divide by 64, disable IRQ
			R	LATCH	Read current counter value, disable IRQ
	7		W	CNT1KT	Count down from value, divide by 1024, disable IRQ
			R	STATUS	Read counter statzs, bit 7 = 1 means counter overflow
	8		X	DRA 	Data register A
	9		X	DDRA	Data direction register A
	A		X	DRB 	Data register B
	B		X	DDRB	Data direction register B
	C		W	CNT1I	Count down from value, divide by 1, enable IRQ
	D		W	CNT8I	Count down from value, divide by 8, enable IRQ
	E		W	CNT64I	Count down from value, divide by 64, enable IRQ
			R	LATCH	Read current counter value, enable IRQ
	F		W	CNT1KI	Count down from value, divide by 1024, enable IRQ
            R   STATUS  Read counter statzs, bit 7 = 1 means counter overflow

	6530-002 (U2)
		DRA bit write				read
		---------------------------------------------
			0-6 segments A-G		key columns 1-7
			7	PTR 				KBD

		DRB bit write				read
		---------------------------------------------
			0	PTR 				-/-
			1-4 dec 1-3 key row 1-3
					4	RW3
					5-9 7-seg	1-6
	6530-003 (U3)
		DRA bit write				read
		---------------------------------------------
			0-7 bus PA0-7			bus PA0-7

		DRB bit write				read
		---------------------------------------------
			0-7 bus PB0-7			bus PB0-7

******************************************************************************/

typedef struct {
	UINT8	dria;			/* Data register A input */
	UINT8	droa;			/* Data register A output */
	UINT8	ddra;			/* Data direction register A; 1 bits = output */
	UINT8	drib;			/* Data register B input */
	UINT8	drob;			/* Data register B output */
	UINT8	ddrb;			/* Data direction register B; 1 bits = output */
    UINT8   irqen;          /* IRQ enabled ? */
	UINT8	state;			/* current timer state (bit 7) */
	double	clock;			/* 100000/1(,8,64,1024) */
	void	*timer; 		/* timer callback */
}	M6530;

static M6530 m6530[2];

static struct artwork *kim1_backdrop;

int m6530_003_r(int offset);
int m6530_002_r(int offset);
int kim1_mirror_r(int offset);

void m6530_003_w(int offset, int data);
void m6530_002_w(int offset, int data);
void kim1_mirror_w(int offset, int data);

INLINE void m6530_timer_cb(int chip);

static struct MemoryReadAddress readmem[] =
{
	{ 0x0000, 0x03ff, MRA_RAM },
	{ 0x1700, 0x173f, m6530_003_r },
	{ 0x1740, 0x177f, m6530_002_r },
	{ 0x1780, 0x17bf, MRA_RAM },
	{ 0x17c0, 0x17ff, MRA_RAM },
	{ 0x1800, 0x1bff, MRA_ROM },
	{ 0x1c00, 0x1fff, MRA_ROM },
	{ 0x2000, 0xffff, kim1_mirror_r },
    {-1}
};

static struct MemoryWriteAddress writemem[] =
{
	{ 0x0000, 0x03ff, MWA_RAM },
	{ 0x1700, 0x173f, m6530_003_w },
	{ 0x1740, 0x177f, m6530_002_w },
	{ 0x1780, 0x17bf, MWA_RAM },
	{ 0x17c0, 0x17ff, MWA_RAM },
	{ 0x1800, 0x1bff, MWA_ROM },
	{ 0x1c00, 0x1fff, MWA_ROM },
	{ 0x2000, 0xffff, kim1_mirror_w },
    {-1}
};

INPUT_PORTS_START( kim1 )
	PORT_START			/* IN0 keys row 0 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "0.6: 0",        KEYCODE_0,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "0.5: 1",        KEYCODE_1,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "0.4: 2",        KEYCODE_2,      IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "0.3: 3",        KEYCODE_3,      IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "0.2: 4",        KEYCODE_4,      IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "0.1: 5",        KEYCODE_5,      IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "0.0: 6",        KEYCODE_6,      IP_JOY_NONE )
	PORT_START			/* IN1 keys row 1 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "1.6: 7",        KEYCODE_7,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "1.5: 8",        KEYCODE_8,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "1.4: 9",        KEYCODE_9,      IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "1.3: A",        KEYCODE_A,      IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "1.2: B",        KEYCODE_B,      IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "1.1: C",        KEYCODE_C,      IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "1.0: D",        KEYCODE_D,      IP_JOY_NONE )
	PORT_START			/* IN2 keys row 2 */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "2.6: E",        KEYCODE_E,      IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "2.5: F",        KEYCODE_F,      IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_UNKNOWN, "2.4: AD (F1)",  KEYCODE_F1,     IP_JOY_NONE )
	PORT_BITX(0x08, 0x08, IPT_UNKNOWN, "2.3: DA (F2)",  KEYCODE_F2,     IP_JOY_NONE )
	PORT_BITX(0x04, 0x04, IPT_UNKNOWN, "2.2: +  (CR)",  KEYCODE_ENTER,  IP_JOY_NONE )
	PORT_BITX(0x02, 0x02, IPT_UNKNOWN, "2.1: GO (F5)",  KEYCODE_F5,     IP_JOY_NONE )
	PORT_BITX(0x01, 0x01, IPT_UNKNOWN, "2.0: PC (F6)",  KEYCODE_F6,     IP_JOY_NONE )
	PORT_START			/* IN3 STEP and RESET keys, MODE switch */
	PORT_BIT (0x80, 0x00, IPT_UNUSED )
	PORT_BITX(0x40, 0x40, IPT_UNKNOWN, "sw1: ST (F7)",  KEYCODE_F7,     IP_JOY_NONE )
	PORT_BITX(0x20, 0x20, IPT_UNKNOWN, "sw2: RS (F3)",  KEYCODE_F3,     IP_JOY_NONE )
	PORT_BITX(0x10, 0x10, IPT_DIPSWITCH_NAME | IPF_TOGGLE, "sw3: SS (NumLock)", KEYCODE_NUMLOCK, IP_JOY_NONE )
	PORT_DIPSETTING( 0x00, "single step")
	PORT_DIPSETTING( 0x10, "run")
	PORT_BIT (0x08, 0x00, IPT_UNUSED )
	PORT_BIT (0x04, 0x00, IPT_UNUSED )
	PORT_BIT (0x02, 0x00, IPT_UNUSED )
	PORT_BIT (0x01, 0x00, IPT_UNUSED )
INPUT_PORTS_END

void kim1_init_machine(void)
{
	UINT8 *RAM = memory_region(REGION_CPU1);

	/* setup RAM IRQ vector */
    if( RAM[0x17fa] == 0x00 && RAM[0x17fb] == 0x00 )
	{
		RAM[0x17fa] = 0x00;
		RAM[0x17fb] = 0x1c;
	}
	/* setup RAM NMI vector */
    if( RAM[0x17fe] == 0x00 && RAM[0x17ff] == 0x00 )
	{
		RAM[0x17fe] = 0x00;
		RAM[0x17ff] = 0x1c;
    }

    /* reset the 6530 */
    memset(&m6530, 0, sizeof(m6530));

	m6530[0].dria = 0xff;
    m6530[0].drib = 0xff;
	m6530[0].clock = (double)1000000/1;
	m6530[0].timer = timer_pulse(TIME_IN_HZ(256 * m6530[0].clock / 256 / 256), 0, m6530_timer_cb);

    m6530[1].dria = 0xff;
	m6530[1].drib = 0xff;
	m6530[1].clock = (double)1000000/1;
	m6530[1].timer = timer_pulse(TIME_IN_HZ(256 * m6530[1].clock / 256 / 256), 1, m6530_timer_cb);
}

int kim1_rom_load(void)
{
    const char magic[] = "KIM1";
    char buff[4];
    void *file;
    int i;

    for( i = 0; i < MAX_CASSETTE; i++ )
    {
		if( rom_name[i] == NULL )
			continue;
		file = osd_fopen(Machine->gamedrv->name, rom_name[i], OSD_FILETYPE_IMAGE_RW, 0);
        if( file )
        {
			UINT16 addr, size;
			UINT8 id, *RAM = memory_region(REGION_CPU1);
			osd_fread(file, buff, sizeof(buff));
			if( memcmp(buff, magic, sizeof(buff)) )
			{
				LOG((errorlog, "kim1_rom_load: magic '%s' not found\n", magic));
				return 1;
			}
			osd_fread_lsbfirst(file, &addr, 2);
			osd_fread_lsbfirst(file, &size, 2);
			osd_fread(file, &id, 1);
			LOG((errorlog, "kim1_rom_load: $%04X $%04X $%02X\n", addr, size, id));
			while( size-- > 0 )
				osd_fread(file, &RAM[addr++], 1);
			osd_fclose(file);
        }
    }
	return 0;
}

int kim1_rom_id(const char *name, const char *gamename)
{
    const char magic[] = "KIM1";
    char buff[4];
    void *file;
	file = osd_fopen(gamename, name, OSD_FILETYPE_IMAGE_RW, 0);
    if( file )
    {
		osd_fread(file, buff, sizeof(buff));
		if( memcmp(buff, magic, sizeof(buff)) == 0 )
		{
			LOG((errorlog, "kim1_rom_id: magic '%s' found\n", magic));
            return 0;
		}
    }
    return 1;
}

void kim1_init_colors(unsigned char *palette, unsigned short *colortable,const unsigned char *color_prom)
{
	char backdrop_name[1024];
	int i, nextfree;

	/* try to load a backdrop for the machine */
	sprintf(backdrop_name, "%s.png", Machine->gamedrv->name);

    /* initialize 16 colors with shades of red */
    for( i = 0; i < 16; i++ )
    {
        palette[3*i+0] = (i+1) * (i+1) -1;
		palette[3*i+1] = (i+1) * (i+1) / 4;
        palette[3*i+2] = 0;

        colortable[2*i+0] = 1;
        colortable[2*i+1] = i;
    }

    palette[3*16+0] = 0;
    palette[3*16+1] = 0;
    palette[3*16+2] = 0;

    palette[3*17+0] = 30;
    palette[3*17+1] = 30;
    palette[3*17+2] = 30;

	palette[3*18+0] = 90;
	palette[3*18+1] = 90;
	palette[3*18+2] = 90;

	palette[3*19+0] = 50;
	palette[3*19+1] = 50;
	palette[3*19+2] = 50;

    palette[3*20+0] =255;
    palette[3*20+1] =255;
    palette[3*20+2] =255;

    colortable[2*16+0*4+0] = 17;
    colortable[2*16+0*4+1] = 18;
    colortable[2*16+0*4+2] = 19;
    colortable[2*16+0*4+3] = 20;

    colortable[2*16+1*4+0] = 17;
	colortable[2*16+1*4+1] = 17;
	colortable[2*16+1*4+2] = 19;
    colortable[2*16+1*4+3] = 15;

	nextfree = 21;

	if( (kim1_backdrop = artwork_load(backdrop_name, nextfree, Machine->drv->total_colors - nextfree)) != NULL )
	{
		LOG((errorlog,"backdrop %s successfully loaded\n", backdrop_name));
		memcpy(&palette[nextfree*3], kim1_backdrop->orig_palette, kim1_backdrop->num_pens_used * 3 * sizeof(unsigned char));
    }
	else
	{
		LOG((errorlog,"no backdrop loaded\n"));
	}
}

int kim1_vh_start(void)
{
	videoram_size = 6 * 2 + 24;
	videoram = malloc(videoram_size);
	if( !videoram )
		return 1;
	if( kim1_backdrop )
        backdrop_refresh(kim1_backdrop);
    if( generic_vh_start() != 0 )
        return 1;

    return 0;
}

void kim1_vh_stop(void)
{
	if( kim1_backdrop )
		artwork_free(kim1_backdrop);
	kim1_backdrop = NULL;
    if( videoram )
		free(videoram);
	videoram = NULL;
	generic_vh_stop();
}

void kim1_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
	int x, y;

    if( full_refresh )
	{
        osd_mark_dirty(0, 0, bitmap->width, bitmap->height, 0);
		memset(videoram, 0x0f, videoram_size);
    }
	if( kim1_backdrop )
		copybitmap(bitmap, kim1_backdrop->artwork, 0, 0, 0, 0, NULL, TRANSPARENCY_NONE, 0);

    for( x = 0; x < 6; x++ )
	{
		int sy = 408;
		int sx = Machine->drv->screen_width - 212 + x * 30 + ((x >= 4) ? 6 : 0);
		drawgfx(bitmap, Machine->gfx[0],
				videoram[2*x+0], videoram[2*x+1],
				0, 0, sx, sy, &Machine->drv->visible_area, TRANSPARENCY_PEN, 0);
		osd_mark_dirty(sx,sy,sx+15,sy+31,1);
    }

    for( y = 0; y < 6; y++ )
	{
		int sy = 516 + y * 36;

        for( x = 0; x < 4; x++ )
		{
            static int layout[6][4] =
            {
				{ 22, 19, 21, 23},
				{ 16, 17, 20, 18},
				{ 12, 13, 14, 15},
				{  8,  9, 10, 11},
				{  4,  5,  6,  7},
				{  0,  1,  2,  3}
            };
			int sx = Machine->drv->screen_width - 182 + x * 37;
			int color, code = layout[y][x];

			color = ( readinputport(code/7) & (0x40 >> (code%7)) ) ? 0 : 1;

			videoram[6*2 + code] = color;
			drawgfx(bitmap, Machine->gfx[1],
				layout[y][x], color,
				0, 0, sx, sy, &Machine->drv->visible_area,
				TRANSPARENCY_NONE, 0);
			osd_mark_dirty(sx,sy,sx+23,sy+17,1);
        }
	}

}

int kim1_interrupt(void)
{
	int i;

	for( i = 0; i < 6; i++ )
	{
		if( videoram[i*2+1] > 0 )
			videoram[i*2+1] -= 1;
	}

    return ignore_interrupt();
}

INLINE int m6530_r(int chip, int offset)
{
	int data = 0xff;
    switch( offset )
    {
	case 0: case 8: /* Data register A */
		if( chip == 1 )
		{
			int which = ((m6530[1].drob & m6530[1].ddrb) >> 1) & 0x0f;
			switch( which )
			{
			case 0: /* key row 1 */
				m6530[1].dria = input_port_0_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'0',
						(m6530[1].dria&0x20)?'.':'1',
						(m6530[1].dria&0x10)?'.':'2',
						(m6530[1].dria&0x08)?'.':'3',
						(m6530[1].dria&0x04)?'.':'4',
						(m6530[1].dria&0x02)?'.':'5',
						(m6530[1].dria&0x01)?'.':'6'));
				break;
			case 1: /* key row 2 */
				m6530[1].dria = input_port_1_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'7',
						(m6530[1].dria&0x20)?'.':'8',
						(m6530[1].dria&0x10)?'.':'9',
						(m6530[1].dria&0x08)?'.':'A',
						(m6530[1].dria&0x04)?'.':'B',
						(m6530[1].dria&0x02)?'.':'C',
						(m6530[1].dria&0x01)?'.':'D'));
				break;
			case 2: /* key row 3 */
				m6530[1].dria = input_port_2_r(0);
				LOG((errorlog, "read keybd(%d): %c%c%c%c%c%c%c\n",
						which,
						(m6530[1].dria&0x40)?'.':'E',
						(m6530[1].dria&0x20)?'.':'F',
						(m6530[1].dria&0x10)?'.':'a',
						(m6530[1].dria&0x08)?'.':'d',
						(m6530[1].dria&0x04)?'.':'+',
						(m6530[1].dria&0x02)?'.':'g',
						(m6530[1].dria&0x01)?'.':'p'));
				break;
			case 3: 	/* WR4?? */
				m6530[1].dria = 0xff;
                break;
			default:
                m6530[1].dria = 0xff;
				LOG((errorlog, "read DRA(%d) $ff\n", which));
			}
        }
        data = (m6530[chip].dria & ~m6530[chip].ddra) | (m6530[chip].droa & m6530[chip].ddra);
		LOG((errorlog, "m6530(%d) DRA   read : $%02x\n", chip, data));
		break;
	case 1: case 9: 	/* Data direction register A */
		data = m6530[chip].ddra;
		LOG((errorlog, "m6530(%d) DDRA  read : $%02x\n", chip, data));
		break;
	case 2: case 10:	/* Data register B */
		data = (m6530[chip].drib & ~m6530[chip].ddrb) | (m6530[chip].drob & m6530[chip].ddrb);
		LOG((errorlog, "m6530(%d) DRB   read : $%02x\n", chip, data));
		break;
	case 3: case 11:	/* Data direction register B */
		data = m6530[chip].ddrb;
		LOG((errorlog, "m6530(%d) DDRB  read : $%02x\n", chip, data));
		break;
	case 4: case 12:	/* Timer count read (not supported?) */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 5: case 13:	/* Timer count read (not supported?) */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
        m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 6: case 14:	/* Timer count read */
		data = (int)(256 * timer_timeleft(m6530[chip].timer) / TIME_IN_HZ(m6530[chip].clock));
        m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) TIMR  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
	case 7: case 15:	/* Timer status read */
		data = m6530[chip].state;
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		LOG((errorlog, "m6530(%d) STAT  read : $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		break;
    }
	return data;
}

int m6530_003_r(int offset) { return m6530_r(0,offset); }
int m6530_002_r(int offset) { return m6530_r(1,offset); }

int kim1_mirror_r(int offset)
{
	return cpu_readmem16(offset & 0x1fff);
}

INLINE void m6530_timer_cb(int chip)
{
	LOG((errorlog, "m6530(%d) timer expired\n", chip));
	m6530[chip].state |= 0x80;
	if( m6530[chip].irqen ) /* with IRQ? */
		cpu_set_irq_line(0, 0, ASSERT_LINE);
}

void m6530_w(int chip, int offset, int data)
{
	switch( offset )
    {
	case 0: case 8: 	/* Data register A */
		LOG((errorlog, "m6530(%d) DRA  write: $%02x\n", chip, data));
		m6530[chip].droa = data;
		if( chip == 1 )
		{
			int which = (m6530[1].drob & m6530[1].ddrb) >> 1;
			switch( which )
			{
			case 0: 	/* key row 1 */
				break;
			case 1: 	/* key row 2 */
				break;
			case 2: 	/* key row 3 */
				break;
			case 3: 	/* WR4?? */
				break;
			/* write LED # 1-6 */
			case 4: case 5: case 6: case 7: case 8: case 9:
				if( data & 0x80 )
				{
					LOG((errorlog, "write 7seg(%d): %c%c%c%c%c%c%c\n",
							which+1-4,
							(data&0x01)?'a':'.',
							(data&0x02)?'b':'.',
							(data&0x04)?'c':'.',
							(data&0x08)?'d':'.',
							(data&0x10)?'e':'.',
							(data&0x20)?'f':'.',
							(data&0x40)?'g':'.'));
					videoram[(which-4)*2+0] = data & 0x7f;
					videoram[(which-4)*2+1] = 15;
				}
			}
		}
		break;
	case 1: case 9: 	/* Data direction register A */
		LOG((errorlog, "m6530(%d) DDRA  write: $%02x\n", chip, data));
		m6530[chip].ddra = data;
		break;
	case 2: case 10:	/* Data register B */
		LOG((errorlog, "m6530(%d) DRB   write: $%02x\n", chip, data));
		m6530[chip].drob = data;
        if( chip == 1 )
        {
			int which = m6530[1].ddrb & m6530[1].drob;
			if( (which & 0x3f) == 0x27 )
			{
				/* This is the cassette output port */
				LOG((errorlog, "write cassette port: %d\n", (which & 0x80) ? 1 : 0));
				DAC_signed_data_w(0, (which & 0x80) ? 255 : 0);
			}
		}
        break;
	case 3: case 11:	/* Data direction register B */
		LOG((errorlog, "m6530(%d) DDRB  write: $%02x\n", chip, data));
		m6530[chip].ddrb = data;
		break;
	case 4: case 12:	/* Timer 1 start */
		LOG((errorlog, "m6530(%d) TMR1  write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
		if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 1;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
		break;
	case 5: case 13:	/* Timer 8 start */
		LOG((errorlog, "m6530(%d) TMR8  write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 8;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
	case 6: case 14:	/* Timer 64 start */
		LOG((errorlog, "m6530(%d) TMR64 write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 64;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
	case 7: case 15:	/* Timer 1024 start */
		LOG((errorlog, "m6530(%d) TMR1K write: $%02x%s\n", chip, data, (offset & 8) ? " (IRQ)":""));
		m6530[chip].state &= ~0x80;
		m6530[chip].irqen = (offset & 8) ? 1 : 0;
        if( m6530[chip].timer )
			timer_remove(m6530[chip].timer);
		m6530[chip].clock = (double)1000000 / 1024;
		m6530[chip].timer = timer_pulse(TIME_IN_HZ((data+1) * m6530[chip].clock / 256 / 256), chip, m6530_timer_cb);
        break;
    }
}

void m6530_003_w(int offset, int data) { m6530_w(0,offset,data); }
void m6530_002_w(int offset, int data) { m6530_w(1,offset,data); }

void kim1_mirror_w(int offset, int data)
{
	cpu_writemem16(offset & 0x1fff, data);
}

static struct GfxLayout led_layout =
{
	18, 24, 	/* 16 x 24 LED 7segment displays */
	128,		/* 128 codes */
	1,			/* 1 bit per pixel */
	{ 0 },		/* no bitplanes */
	{ 0, 1, 2, 3, 4, 5, 6, 7,
	  8, 9,10,11,12,13,14,15,
	 16,17 },
	{ 0*24, 1*24, 2*24, 3*24,
	  4*24, 5*24, 6*24, 7*24,
	  8*24, 9*24,10*24,11*24,
	 12*24,13*24,14*24,15*24,
	 16*24,17*24,18*24,19*24,
	 20*24,21*24,22*24,23*24,
	 24*24,25*24,26*24,27*24,
	 28*24,29*24,30*24,31*24 },
	24 * 24,	/* every LED code takes 32 times 18 (aligned 24) bit words */
};

static struct GfxLayout key_layout =
{
	24, 18, 	/* 24 * 18 keyboard icons */
	24, 		/* 24  codes */
	2,			/* 2 bit per pixel */
	{ 0, 1 },	/* two bitplanes */
	{ 0*2, 1*2, 2*2, 3*2, 4*2, 5*2, 6*2, 7*2,
	  8*2, 9*2,10*2,11*2,12*2,13*2,14*2,15*2,
	 16*2,17*2,18*2,19*2,20*2,21*2,22*2,23*2 },
	{ 0*24*2, 1*24*2, 2*24*2, 3*24*2, 4*24*2, 5*24*2, 6*24*2, 7*24*2,
	  8*24*2, 9*24*2,10*24*2,11*24*2,12*24*2,13*24*2,14*24*2,15*24*2,
	 16*24*2,17*24*2 },
	18 * 24 * 2,	/* every icon takes 18 rows of 24 * 2 bits */
};

static struct GfxDecodeInfo gfxdecodeinfo[] =
{
	{ 1, 0, &led_layout, 0, 16 },
	{ 2, 0, &key_layout, 16*2, 2 },
	{ -1 } /* end of array */
};

static struct DACinterface dac_interface =
{
	1,			/* number of DACs */
	{ 100 } 	/* volume */
};

static struct MachineDriver machine_driver =
{
	/* basic machine hardware */
	{
		{
			CPU_M6502,
			1000000,	/* 1 MHz */
			readmem,writemem,0,0,
			kim1_interrupt, 1
        }
	},
	/* frames per second, VBL duration */
	60, DEFAULT_60HZ_VBLANK_DURATION,
	1,				/* single CPU */
	kim1_init_machine,
	NULL,			/* stop machine */

	/* video hardware (well, actually there was no video ;) */
	600, 768, { 0, 600 - 1, 0, 768 - 1},
	gfxdecodeinfo,
	256*3,
	256,
	kim1_init_colors,		/* convert color prom */

	VIDEO_TYPE_RASTER,		/* video flags */
	0,						/* obsolete */
	kim1_vh_start,
	kim1_vh_stop,
	kim1_vh_screenrefresh,

	/* sound hardware */
	0,0,0,0,
	{
        {
			SOUND_DAC,
			&dac_interface
		}
    }
};

ROM_START(kim1)
	ROM_REGIONX(0x10000,REGION_CPU1)
		ROM_LOAD("6530-003.bin",    0x1800, 0x0400, 0xa2a56502)
		ROM_LOAD("6530-002.bin",    0x1c00, 0x0400, 0x2b08e923)
	ROM_REGIONX(128 * 24 * 3,REGION_GFX1)
		/* space filled with 7segement graphics by kim1_rom_decode */
	ROM_REGIONX( 24 * 18 * 3 * 2,REGION_GFX2)
		/* space filled with key icons by kim1_rom_decode */
ROM_END

void kim1_rom_decode(void)
{
	UINT8 *dst;
    int x, y, i;

	static char *seg7 =
		"....aaaaaaaaaaaaa." \
		"...f.aaaaaaaaaaa.b" \
		"...ff.aaaaaaaaa.bb" \
		"...fff.........bbb" \
		"...fff.........bbb" \
		"...fff.........bbb" \
		"..fff.........bbb." \
		"..fff.........bbb." \
		"..fff.........bbb." \
		"..ff...........bb." \
		"..f.ggggggggggg.b." \
		"..gggggggggggggg.." \
		".e.ggggggggggg.c.." \
		".ee...........cc.." \
		".eee.........ccc.." \
		".eee.........ccc.." \
		".eee.........ccc.." \
		"eee.........ccc..." \
		"eee.........ccc..." \
		"eee.........ccc..." \
		"ee.ddddddddd.cc..." \
		"e.ddddddddddd.c..." \
		".ddddddddddddd...." \
		"..................";


	static char *keys[24] = {
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaccccaaaaaaa" \
		"aaaaaaaccaaaccaccaaaaaaa" \
		"aaaaaaaccaaccaaccaaaaaaa" \
		"aaaaaaaccaccaaaccaaaaaaa" \
		"aaaaaaaccccaaaaccaaaaaaa" \
		"aaaaaaacccaaaaaccaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaacccaaaaaaaaaaa" \
		"aaaaaaaaaccccaaaaaaaaaaa" \
		"aaaaaaaaccaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccaaaaaaaaaaaa" \
		"aaaaaaacccaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaccccccaaaaaaaa" \
		"aaaaaaaaaaccccccaaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaacaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaacccaaaaaaaaa" \
		"aaaaaaaaaaaccccaaaaaaaaa" \
		"aaaaaaaaaaccaccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaccaaaccaaaaaaaaa" \
		"aaaaaaaccaaaaccaaaaaaaaa" \
		"aaaaaacccccccccccaaaaaaa" \
		"aaaaaacccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaccccaaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccccaaaccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaacccaaaaaaa" \
		"aaaaaaaaaaaaacccaaaaaaaa" \
		"aaaaaaaaaaaacccaaaaaaaaa" \
		"aaaaaaaaaaacccaaaaaaaaaa" \
		"aaaaaaaaaacccaaaaaaaaaaa" \
		"aaaaaaaaacccaaaaaaaaaaaa" \
		"aaaaaaaacccaaaaaaaaaaaaa" \
		"aaaaaaacccaaaaaaaaaaaaaa" \
		"aaaaaacccaaaaaaaaaaaaaaa" \
		"aaaaaacccaaaaaaaaaaaaaaa" \
		"aaaaaaccaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaacccaaacccaaaaaaa" \
		"aaaaaaaacccccccccaaaaaaa" \
		"aaaaaaaaaaccccaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaaaaaaaaaaccaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaaccccaaaaaaaaaa" \
		"aaaaaaaaaccccccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaaccaaccaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaccaaaaccaaaaaaaa" \
		"aaaaaaacccaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaacccaaaaaacccaaaaaa" \
		"aaaaaaccaaaaaaaaccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaacccccccccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaccccccccaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaacccccccaaaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaaccaaaacccaaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaaaccaaaaaaa" \
		"aaaaaaaccaaaaacccaaaaaaa" \
		"aaaaaaaccaaaaaccaaaaaaaa" \
		"aaaaaaaccaaaacccaaaaaaaa" \
		"aaaaaaaccccccccaaaaaaaaa" \
		"aaaaaaacccccccaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccccccccccaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccccccaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaccaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaccaaaaacccccaaaaaa" \
		"aaaaaaccaaaaaccccccaaaaa" \
		"aaaaaccccaaaaccaacccaaaa" \
		"aaaaaccccaaaaccaaaccaaaa" \
		"aaaaccccccaaaccaaaaccaaa" \
		"aaaaccaaccaaaccaaaaccaaa" \
		"aaaaccccccaaaccaaaaccaaa" \
		"aaaccccccccaaccaaaaccaaa" \
		"aaacccaacccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaccaaaa" \
		"aacccaaaacccaccaacccaaaa" \
		"aaccaaaaaaccaccccccaaaaa" \
		"aaccaaaaaaccacccccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaacccccaaaaaaaccaaaaaaa" \
		"aaaccccccaaaaaaccaaaaaaa" \
		"aaaccaacccaaaaccccaaaaaa" \
		"aaaccaaaccaaaaccccaaaaaa" \
		"aaaccaaaaccaaccccccaaaaa" \
		"aaaccaaaaccaaccaaccaaaaa" \
		"aaaccaaaaccaaccccccaaaaa" \
		"aaaccaaaaccaccccccccaaaa" \
		"aaaccaaaaccacccaacccaaaa" \
		"aaaccaaaaccaccaaaaccaaaa" \
		"aaaccaaaccaaccaaaaccaaaa" \
		"aaaccaacccacccaaaacccaaa" \
		"aaaccccccaaccaaaaaaccaaa" \
		"aaacccccaaaccaaaaaaccaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaccccccccccccccaaaaa" \
		"aaaaaccccccccccccccaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaccaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaccccaaaaaaccccaaaaa" \
		"aaaaccccccaaaaccccccaaaa" \
		"aaacccaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaaaacccaaaacccaa" \
		"aaaccaaaaaaaccaaaaaaccaa" \
		"aaaccacccccaccaaaaaaccaa" \
		"aaaccacccccaccaaaaaaccaa" \
		"aaaccaaaaccaccaaaaaaccaa" \
		"aaaccaaaaccacccaaaacccaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaacccaacccaacccaacccaaa" \
		"aaaacccccccaaaccccccaaaa" \
		"aaaaacccaccaaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaccccccaaaaaaccccaaaaa" \
		"aaacccccccaaaaccccccaaaa" \
		"aaaccaaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccacccaaaaaaaaa" \
		"aaaccaaacccaccaaaaaaaaaa" \
		"aaacccccccaaccaaaaaaaaaa" \
		"aaaccccccaaaccaaaaaaaaaa" \
		"aaaccaaaaaaaccaaaaaaaaaa" \
		"aaaccaaaaaaacccaaaaaaaaa" \
		"aaaccaaaaaaaaccaaaaccaaa" \
		"aaaccaaaaaaaacccaacccaaa" \
		"aaaccaaaaaaaaaccccccaaaa" \
		"aaaccaaaaaaaaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaccccaaaaccccccccaaa" \
		"aaaaccccccaaaccccccccaaa" \
		"aaacccaacccaaaaaccaaaaaa" \
		"aaaccaaaaccaaaaaccaaaaaa" \
		"aaaccaaaaaaaaaaaccaaaaaa" \
		"aaacccaaaaaaaaaaccaaaaaa" \
		"aaaacccccaaaaaaaccaaaaaa" \
		"aaaaacccccaaaaaaccaaaaaa" \
		"aaaaaaaacccaaaaaccaaaaaa" \
		"aaaaaaaaaccaaaaaccaaaaaa" \
		"aaaccaaaaccaaaaaccaaaaaa" \
		"aaacccaacccaaaaaccaaaaaa" \
		"aaaaccccccaaaaaaccaaaaaa" \
		"aaaaaccccaaaaaaaccaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaccccccaaaaaaccccaaaaa" \
		"aaacccccccaaaaccccccaaaa" \
		"aaaccaaacccaacccaacccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaaccaaccaaaaaaaaa" \
		"aaaccaaacccaacccaaaaaaaa" \
		"aaacccccccaaaacccccaaaaa" \
		"aaaccccccaaaaaacccccaaaa" \
		"aaaccaacccaaaaaaaacccaaa" \
		"aaaccaaacccaaaaaaaaccaaa" \
		"aaaccaaaaccaaccaaaaccaaa" \
		"aaaccaaaacccacccaacccaaa" \
		"aaaccaaaaaccaaccccccaaaa" \
		"aaaccaaaaaccaaaccccaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa" \
		"aaaaaaaaaaaaaaaaaaaaaaaa",

		"........................" \
		"........................" \
		".bbbbbbbbbbbbbbbbbbbbbb." \
		".baaaaaaaaaaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".baccccccccaaaaaaaaaaab." \
		".bbbbbbbbbbbbbbbbbbbbbb." \
		"........................" };

    dst = memory_region(REGION_GFX1);
	memset(dst, 0, 128 * 24 * 24 / 8);
    for( i = 0; i < 128; i++ )
    {
		for( y = 0; y < 24; y++ )
		{
			for( x = 0; x < 18; x++ )
			{
				switch( seg7[y*18+x] )
                {
					case 'a': if( i &  1 ) *dst |= 0x80 >> (x & 7); break;
					case 'b': if( i &  2 ) *dst |= 0x80 >> (x & 7); break;
					case 'c': if( i &  4 ) *dst |= 0x80 >> (x & 7); break;
					case 'd': if( i &  8 ) *dst |= 0x80 >> (x & 7); break;
					case 'e': if( i & 16 ) *dst |= 0x80 >> (x & 7); break;
					case 'f': if( i & 32 ) *dst |= 0x80 >> (x & 7); break;
					case 'g': if( i & 64 ) *dst |= 0x80 >> (x & 7); break;
                }
				if( (x & 7) == 7 ) dst++;
            }
			dst++;
		}
	}

	dst = memory_region(2);
	memset(dst, 0, 24 * 18 * 24 / 8);
	for( i = 0; i < 24; i++ )
    {
		for( y = 0; y < 18; y++ )
		{
			for( x = 0; x < 24; x++ )
			{
				switch( keys[i][y*24+x] )
				{
					case 'a': *dst |= 0x80 >> ((x & 3) * 2); break;
					case 'b': *dst |= 0x40 >> ((x & 3) * 2); break;
					case 'c': *dst |= 0xc0 >> ((x & 3) * 2); break;
				}
				if( (x & 3) == 3 ) dst++;
            }
		}
    }
}

static const char *kim1_file_extensions[] = { "kim",0 };

struct GameDriver kim1_driver =
{
	__FILE__,
	0,
	"kim1",
	"KIM-1",
	"1975",
	"Commodore/MOS",
	"Juergen Buchmueller",
	0,
	&machine_driver,
	0,

	rom_kim1,	/* ROM_LOAD structures */
	kim1_rom_load,
	kim1_rom_id,
	kim1_file_extensions,	/*	default file extensions */
	1,	/* number of ROM slots */
	4,	/* number of floppy drives supported */
	0,	/* number of hard drives supported */
	0,	/* number of cassette drives supported */
	kim1_rom_decode,
	0,
	0,
	0,	/* sound_prom */

	input_ports_kim1,

	0, 0, 0,
	ORIENTATION_DEFAULT,

	0, 0,
};

