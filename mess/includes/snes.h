#ifndef _SNES_H_
#define _SNES_H_

/* Useful defines */
#define SNES_SCR_WIDTH	256		/* 32 characters 8 pixels wide */
#define SNES_SCR_HEIGHT	240		/* Can be either 224 of 240 height, so specify greatest value (maybe we'll have switching later on) */
#define FIXED_COLOUR	256		/* Position in cgram for the fixed colour */
/* Defines for Memory-Mapped registers */
#define INIDISP			0x2100
#define OBSEL			0x2101
#define OAMADDL			0x2102
#define OAMADDH			0x2103
#define OAMDATA			0x2104
#define BGMODE			0x2105	/* abcdefff = abcd: bg4-1 tile size | e: BG3 high priority | f: mode */
#define MOSAIC			0x2106	/* xxxxabcd = x: pixel size | abcd: affects bg 1-4 */
#define BG1SC			0x2107
#define BG2SC			0x2108
#define BG3SC			0x2109
#define BG4SC			0x210A
#define BG12NBA			0x210B
#define BG34NBA			0x210C
#define BG1HOFS			0x210D
#define BG1VOFS			0x210E
#define BG2HOFS			0x210F
#define BG2VOFS			0x2110
#define BG3HOFS			0x2111
#define BG3VOFS			0x2112
#define BG4HOFS			0x2113
#define BG4VOFS			0x2114
#define VMAIN			0x2115	/* i---ffrr = i: Increment timing | f: Full graphic | r: increment rate */
#define VMADDL			0x2116	/* aaaaaaaa = a: LSB of vram address */
#define VMADDH			0x2117	/* aaaaaaaa = a: MSB of vram address */
#define VMDATAL			0x2118	/* dddddddd = d: data to be written */
#define VMDATAH			0x2119	/* dddddddd = d: data to be written */
#define M7SEL			0x211A	/* ab----yx = a: screen over | y: vertical flip | x: horizontal flip */
#define M7A				0x211B
#define M7B				0x211C
#define M7C				0x211D
#define M7D				0x211E
#define M7X				0x211F
#define M7Y				0x2120
#define CGADD			0x2121
#define CGDATA			0x2122
#define W12SEL			0x2123
#define W34SEL			0x2124
#define WOBJSEL			0x2125
#define WH0				0x2126
#define WH1				0x2127
#define WH2				0x2128
#define WH3				0x2129
#define WBGLOG			0x212A
#define WOBJLOG			0x212B
#define TM				0x212C
#define TS				0x212D
#define TMW				0x212E
#define TSW				0x212F
#define CGWSEL			0x2130
#define CGADSUB			0x2131
#define COLDATA			0x2132
#define SETINI			0x2133
#define MPYL			0x2134
#define MPYM			0x2135
#define MPYH			0x2136
#define SLHV			0x2137
#define ROAMDATA		0x2138
#define RVMDATAL		0x2139
#define RVMDATAH		0x213A
#define RCGDATA			0x213B
#define OPHCT			0x213C
#define OPVCT			0x213D
#define STAT77			0x213E
#define STAT78			0x213F
#define APU00			0x2140
#define APU01			0x2141
#define APU02			0x2142
#define APU03			0x2143
#define WMDATA			0x2180
#define WMADDL			0x2181
#define WMADDM			0x2182
#define WMADDH			0x2183
#define OLDJOY1			0x4016
#define OLDJOY2			0x4017
#define NMITIMEN		0x4200
#define WRIO			0x4201
#define WRMPYA			0x4202
#define WRMPYB			0x4203
#define WRDIVL			0x4204
#define WRDIVH			0x4205
#define WRDVDD			0x4206
#define HTIMEL			0x4207
#define HTIMEH			0x4208
#define VTIMEL			0x4209
#define VTIMEH			0x420A
#define MDMAEN			0x420B
#define HDMAEN			0x420C
#define MEMSEL			0x420D
#define RDNMI			0x4210
#define TIMEUP			0x4211
#define HVBJOY			0x4212
#define RDIO			0x4213
#define RDDIVL			0x4214
#define RDDIVH			0x4215
#define RDMPYL			0x4216
#define RDMPYH			0x4217
#define JOY1L			0x4218
#define JOY1H			0x4219
#define JOY2L			0x421A
#define JOY2H			0x421B
#define JOY3L			0x421C
#define JOY3H			0x421D
#define JOY4L			0x421E
#define JOY4H			0x421F
#define DMAP0			0x4300
#define BBAD0			0x4301
#define A1T0L			0x4302
#define A1T0H			0x4303
#define A1B0			0x4304
#define DAS0L			0x4305
#define DAS0H			0x4306
#define DSAB0			0x4307
#define A2A0L			0x4308
#define A2A0H			0x4309
#define NTRL0			0x430A
#define DMAP1			0x4310
#define BBAD1			0x4311
#define A1T1L			0x4312
#define A1T1H			0x4313
#define A1B1			0x4314
#define DAS1L			0x4315
#define DAS1H			0x4316
#define DSAB1			0x4317
#define A2A1L			0x4318
#define A2A1H			0x4319
#define NTRL1			0x431A
#define DMAP2			0x4320
#define BBAD2			0x4321
#define A1T2L			0x4322
#define A1T2H			0x4323
#define A1B2			0x4324
#define DAS2L			0x4325
#define DAS2H			0x4326
#define DSAB2			0x4327
#define A2A2L			0x4328
#define A2A2H			0x4329
#define NTRL2			0x432A
#define DMAP3			0x4330
#define BBAD3			0x4331
#define A1T3L			0x4332
#define A1T3H			0x4333
#define A1B3			0x4334
#define DAS3L			0x4335
#define DAS3H			0x4336
#define DSAB3			0x4337
#define A2A3L			0x4338
#define A2A3H			0x4339
#define NTRL3			0x433A
#define DMAP4			0x4340
#define BBAD4			0x4341
#define A1T4L			0x4342
#define A1T4H			0x4343
#define A1B4			0x4344
#define DAS4L			0x4345
#define DAS4H			0x4346
#define DSAB4			0x4347
#define A2A4L			0x4348
#define A2A4H			0x4349
#define NTRL4			0x434A
#define DMAP5			0x4350
#define BBAD5			0x4351
#define A1T5L			0x4352
#define A1T5H			0x4353
#define A1B5			0x4354
#define DAS5L			0x4355
#define DAS5H			0x4356
#define DSAB5			0x4357
#define A2A5L			0x4358
#define A2A5H			0x4359
#define NTRL5			0x435A
#define DMAP6			0x4360
#define BBAD6			0x4361
#define A1T6L			0x4362
#define A1T6H			0x4363
#define A1B6			0x4364
#define DAS6L			0x4365
#define DAS6H			0x4366
#define DSAB6			0x4367
#define A2A6L			0x4368
#define A2A6H			0x4369
#define NTRL6			0x436A
#define DMAP7			0x4370
#define BBAD7			0x4371
#define A1T7L			0x4372
#define A1T7H			0x4373
#define A1B7			0x4374
#define DAS7L			0x4375
#define DAS7H			0x4376
#define DSAB7			0x4377
#define A2A7L			0x4378
#define A2A7H			0x4379
#define NTRL7			0x437A
/* Defines for sound DSP */
#define DSP_V0_VOLL		0x00
#define DSP_V0_VOLR		0x01
#define DSP_V0_PITCHL	0x02
#define DSP_V0_PITCHH	0x03
#define DSP_V0_SOURCE	0x04
#define DSP_V0_ADSR1	0x05
#define DSP_V0_ADSR2	0x06
#define DSP_V0_GAIN		0x07
#define DSP_V0_ENVX		0x08
#define DSP_V0_OUTX		0x09
#define DSP_V1_VOLL		0x10
#define DSP_V1_VOLR		0x11
#define DSP_V1_PITCHL	0x12
#define DSP_V1_PITCHH	0x13
#define DSP_V1_SOURCE	0x14
#define DSP_V1_ADSR1	0x15
#define DSP_V1_ADSR2	0x16
#define DSP_V1_GAIN		0x17
#define DSP_V1_ENVX		0x18
#define DSP_V1_OUTX		0x19
#define DSP_V2_VOLL		0x20
#define DSP_V2_VOLR		0x21
#define DSP_V2_PITCHL	0x22
#define DSP_V2_PITCHH	0x23
#define DSP_V2_SOURCE	0x24
#define DSP_V2_ADSR1	0x25
#define DSP_V2_ADSR2	0x26
#define DSP_V2_GAIN		0x27
#define DSP_V2_ENVX		0x28
#define DSP_V2_OUTX		0x29
#define DSP_V3_VOLL		0x30
#define DSP_V3_VOLR		0x31
#define DSP_V3_PITCHL	0x32
#define DSP_V3_PITCHH	0x33
#define DSP_V3_SOURCE	0x34
#define DSP_V3_ADSR1	0x35
#define DSP_V3_ADSR2	0x36
#define DSP_V3_GAIN		0x37
#define DSP_V3_ENVX		0x38
#define DSP_V3_OUTX		0x39
#define DSP_V4_VOLL		0x40
#define DSP_V4_VOLR		0x41
#define DSP_V4_PITCHL	0x42
#define DSP_V4_PITCHH	0x43
#define DSP_V4_SOURCE	0x44
#define DSP_V4_ADSR1	0x45
#define DSP_V4_ADSR2	0x46
#define DSP_V4_GAIN		0x47
#define DSP_V4_ENVX		0x48
#define DSP_V4_OUTX		0x49
#define DSP_V5_VOLL		0x50
#define DSP_V5_VOLR		0x51
#define DSP_V5_PITCHL	0x52
#define DSP_V5_PITCHH	0x53
#define DSP_V5_SOURCE	0x54
#define DSP_V5_ADSR1	0x55
#define DSP_V5_ADSR2	0x56
#define DSP_V5_GAIN		0x57
#define DSP_V5_ENVX		0x58
#define DSP_V5_OUTX		0x59
#define DSP_V6_VOLL		0x60
#define DSP_V6_VOLR		0x61
#define DSP_V6_PITCHL	0x62
#define DSP_V6_PITCHH	0x63
#define DSP_V6_SOURCE	0x64
#define DSP_V6_ADSR1	0x65
#define DSP_V6_ADSR2	0x66
#define DSP_V6_GAIN		0x67
#define DSP_V6_ENVX		0x68
#define DSP_V6_OUTX		0x69
#define DSP_V7_VOLL		0x70
#define DSP_V7_VOLR		0x71
#define DSP_V7_PITCHL	0x72
#define DSP_V7_PITCHH	0x73
#define DSP_V7_SOURCE	0x74
#define DSP_V7_ADSR1	0x75
#define DSP_V7_ADSR2	0x76
#define DSP_V7_GAIN		0x77
#define DSP_V7_ENVX		0x78
#define DSP_V7_OUTX		0x79
#define DSP_MVOLL		0x0C
#define DSP_MVOLR		0x1C
#define DSP_EVOLL		0x2C
#define DSP_EVOLR		0x3C
#define DSP_KON			0x4C
#define DSP_KOF			0x5C
#define DSP_FLG			0x6C
#define DSP_ENDX		0x7C
#define DSP_EFB			0x0D
#define DSP_PMON		0x2D
#define DSP_NOV			0x3D
#define DSP_EOV			0x4D
#define DSP_DIR			0x5D
#define DSP_ESA			0x6D
#define DSP_EDL			0x7D
#define DSP_FIR_C0		0x0F
#define DSP_FIR_C1		0x1F
#define DSP_FIR_C2		0x2F
#define DSP_FIR_C3		0x3F
#define DSP_FIR_C4		0x4F
#define DSP_FIR_C5		0x5F
#define DSP_FIR_C6		0x6F
#define DSP_FIR_C7		0x7F

extern MACHINE_INIT( snes );
extern MACHINE_STOP( snes );
extern VIDEO_UPDATE( snes );
extern READ_HANDLER( snes_r_bank );
extern READ_HANDLER( snes_r_mirror );
extern READ_HANDLER( snes_r_io );
extern WRITE_HANDLER( snes_w_bank );
extern WRITE_HANDLER( snes_w_mirror );
extern WRITE_HANDLER( snes_w_io );
extern int snes_load_rom(int id);
extern void snes_scanline_interrupt(void);
extern void snes_gdma( UINT8 channels );
extern void snes_hdma_init(void);
extern void snes_hdma(void);
extern void snes_refresh_scanline( UINT16 curline );

/* Video related */
extern UINT8  *snes_vram;		/* Video RAM (Should be 16-bit, but it's easier this way) */
extern UINT16 *snes_cgram;		/* Colour RAM */
extern UINT8  *snes_oam;		/* Object Attribute Memory (Should be 16-bit, but it's easier this way) */
extern UINT8  *snes_ram;		/* Main memory */
extern UINT16 bg_hoffset[4];	/* Background horizontal scroll offsets */
extern UINT16 bg_voffset[4];	/* Background vertical scroll offsets */
extern UINT16 mode7_data[6];	/* Data for mode7 matrix calculation */
extern UINT8  obj_size[2];		/* Objects sizes */

/* Sound related */
extern UINT8 *spc_ram;			/* SPC main memory */
extern UINT8 spc_port_in[4];
extern UINT8 spc_port_out[4];
extern READ_HANDLER ( spc_r_io );
extern WRITE_HANDLER ( spc_w_io );
extern int snes_sh_start( const struct MachineSound *driver );
extern void snes_sh_update( int param, INT16 **buffer, int length );

/* Just until we get sound working */
extern UINT8 SPCSkipper(void);

#endif /* _SNES_H_ */