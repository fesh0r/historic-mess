/******************************************************************************
	Atari 400/800

    GTIA  graphics television interface adapter

	Juergen Buchmueller, June 1998
******************************************************************************/

#include "driver.h"
#include "cpu/m6502/m6502.h"
#include "mess/machine/atari.h"
#include "mess/vidhrdw/atari.h"

GTIA    gtia;

#define P0 0x01
#define P1 0x02
#define P2 0x04
#define P3 0x08
#define M0 0x10
#define M1 0x20
#define M2 0x40
#define M3 0x80

#define CHECK_GRACTL	0

/**********************************************
 * split a color into hue and luminance values
 **********************************************/
#define SPLIT_HUE_LUM(data,hue,lum) \
    hue = (data & HUE); lum = (data & LUM)

/**********************************************
 * set both color clocks equal for one color
 **********************************************/
#define SETCOL_B(o,d) \
	antic.color_lookup[o] = (Machine->remapped_colortable[d] << 8) | Machine->remapped_colortable[d]

/**********************************************
 * set left color clock for one color
 **********************************************/
#define SETCOL_L(o,d) \
	*((UINT8*)&antic.color_lookup[o] + 0) = Machine->remapped_colortable[d]

/**********************************************
 * set right color clock for one color
 **********************************************/
#define SETCOL_R(o,d) \
	*((UINT8*)&antic.color_lookup[o] + 1) = Machine->remapped_colortable[d]

/**************************************************************
 *
 * Reset GTIA
 * 
 **************************************************************/

void    gtia_reset(void)
{
int i;
    /* reset the GTIA read/write/helper registers */
	for (i = 0; i < 32; i++)
		MWA_GTIA(i,0);
    memset(&gtia.r, 0, sizeof(gtia.r));
	gtia.r.gtia14 = 0xff;
	gtia.r.gtia15 = 0xff;
	gtia.r.gtia16 = 0xff;
	gtia.r.gtia17 = 0xff;
	gtia.r.gtia18 = 0xff;
	gtia.r.gtia19 = 0xff;
	gtia.r.gtia1a = 0xff;
	gtia.r.gtia1b = 0xff;
	gtia.r.gtia1c = 0xff;
	gtia.r.gtia1d = 0xff;
	gtia.r.gtia1e = 0xff;
	gtia.r.cons = 0x07; 	/* console keys */
	SETCOL_B(ILL,0x3e); 	/* bright red */
	SETCOL_B(EOR,0xff); 	/* yellow */
}

/**************************************************************
 *
 * Read GTIA hardware registers
 * 
 **************************************************************/

int MRA_GTIA(int offset)
{
    switch (offset & 31)
    {
		case  0: return gtia.r.m0pf;
		case  1: return gtia.r.m1pf;
		case  2: return gtia.r.m2pf;
		case  3: return gtia.r.m3pf;
		case  4: return gtia.r.p0pf;
		case  5: return gtia.r.p1pf;
		case  6: return gtia.r.p2pf;
		case  7: return gtia.r.p3pf;
		case  8: return gtia.r.m0pl;
		case  9: return gtia.r.m1pl;
		case 10: return gtia.r.m2pl;
		case 11: return gtia.r.m3pl;
		case 12: return gtia.r.p0pl;
		case 13: return gtia.r.p1pl;
		case 14: return gtia.r.p2pl;
		case 15: return gtia.r.p3pl;

		case 16: return gtia.r.but0;
		case 17: return gtia.r.but1;
		case 18: return gtia.r.but2;
		case 19: return gtia.r.but3;

		case 20: return gtia.r.gtia14;
		case 21: return gtia.r.gtia15;
		case 22: return gtia.r.gtia16;
		case 23: return gtia.r.gtia17;
		case 24: return gtia.r.gtia18;
		case 25: return gtia.r.gtia19;
		case 26: return gtia.r.gtia1a;
		case 27: return gtia.r.gtia1b;
		case 28: return gtia.r.gtia1c;
		case 29: return gtia.r.gtia1d;
		case 30: return gtia.r.gtia1e;

		case 31: return gtia.r.cons = (input_port_0_r(0) | gtia.w.cons) & 0x07;
    }
    return 0xff;
}

/**************************************************************
 *
 * Write GTIA hardware registers
 * 
 **************************************************************/

static	int pm_size[4] = {1, 2, 1, 4};

static void recalc_p0(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_PLAYER) == 0 ||
#endif
        gtia.w.grafp0 == 0 ||
		gtia.w.hposp0 < (16 - gtia.h.sizep0) ||
		gtia.w.hposp0 >= 224)
	{
		gtia.h.hposp0 = 0;
		gtia.h.grafp0 = 0;
		gtia.h.usedp &= ~0x10;
	}
	else
	{
		gtia.h.hposp0 = gtia.w.hposp0;
		gtia.h.grafp0 = gtia.w.grafp0;
		gtia.h.usedp |= 0x10;
    }
}

static void recalc_p1(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_PLAYER) == 0 ||
#endif
        gtia.w.grafp1 == 0 ||
		gtia.w.hposp1 < (16 - gtia.h.sizep1) ||
		gtia.w.hposp1 >= 224)
	{
		gtia.h.hposp1 = 0;
		gtia.h.grafp1 = 0;
		gtia.h.usedp &= ~0x20;
	}
	else
	{
		gtia.h.hposp1 = gtia.w.hposp1;
		gtia.h.grafp1 = gtia.w.grafp1;
		gtia.h.usedp |= 0x20;
    }
}

static void recalc_p2(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_PLAYER) == 0 ||
#endif
        gtia.w.grafp2 == 0 ||
		gtia.w.hposp2 < (16 - gtia.h.sizep2) ||
		gtia.w.hposp2 >= 224)
	{
		gtia.h.hposp2 = 0;
		gtia.h.grafp2 = 0;
		gtia.h.usedp &= ~0x40;
	}
	else
	{
		gtia.h.hposp2 = gtia.w.hposp2;
		gtia.h.grafp2 = gtia.w.grafp2;
		gtia.h.usedp |= 0x40;
    }
}

static void recalc_p3(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_PLAYER) == 0 ||
#endif
        gtia.w.grafp3 == 0 ||
		gtia.w.hposp3 < (16 - gtia.h.sizep3) ||
		gtia.w.hposp3 >= 224)
	{
		gtia.h.hposp3 = 0;
		gtia.h.grafp3 = 0;
		gtia.h.usedp &= ~0x80;
	}
	else
	{
		gtia.h.hposp3 = gtia.w.hposp3;
		gtia.h.grafp3 = gtia.w.grafp3;
		gtia.h.usedp |= 0x80;
    }
}

static void recalc_m0(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_MISSILE) == 0 ||
#endif
        (gtia.w.grafm & 0x03) == 0 ||
		gtia.w.hposm0 < (16 - gtia.h.sizem) ||
		gtia.w.hposm0 >= 224)
	{
		gtia.h.hposm0 = 0;
		gtia.h.grafm0 = 0;
		gtia.h.usedm0 = 0;
	}
	else
	{
		gtia.h.hposm0 = gtia.w.hposm0;
		gtia.h.grafm0 = (gtia.w.grafm << 6) & 0xc0;
		gtia.h.usedm0 = (gtia.w.prior & 0x10) ? 0x08 : 0x10;
    }
}

static void recalc_m1(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_MISSILE) == 0 ||
#endif
        (gtia.w.grafm & 0x0c) == 0 ||
		gtia.w.hposm1 < (16 - gtia.h.sizem) ||
		gtia.w.hposm1 >= 224)
	{
		gtia.h.hposm1 = 0;
		gtia.h.grafm1 = 0;
        gtia.h.usedm1 = 0;
	}
	else
	{
		gtia.h.hposm1 = gtia.w.hposm1;
		gtia.h.grafm1 = (gtia.w.grafm << 4) & 0xc0;
		gtia.h.usedm1 = (gtia.w.prior & 0x10) ? 0x08 : 0x20;
    }
}

static void recalc_m2(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_MISSILE) == 0 ||
#endif
        (gtia.w.grafm & 0x30) == 0 ||
		gtia.w.hposm2 < (16 - gtia.h.sizem) ||
		gtia.w.hposm2 >= 224)
	{
		gtia.h.hposm2 = 0;
		gtia.h.grafm2 = 0;
		gtia.h.usedm2 = 0;
	}
	else
	{
		gtia.h.hposm2 = gtia.w.hposm2;
		gtia.h.grafm2 = (gtia.w.grafm << 2) & 0xc0;
		gtia.h.usedm2 = (gtia.w.prior & 0x10) ? 0x08 : 0x40;
	}
}

static void recalc_m3(void)
{
	if (
#if CHECK_GRACTL
        (gtia.w.gractl & GTIA_MISSILE) == 0 ||
#endif
        (gtia.w.grafm & 0xc0) == 0 ||
		gtia.w.hposm3 < (16 - gtia.h.sizem) ||
		gtia.w.hposm3 >= 224)
	{
		gtia.h.hposm3 = 0;
		gtia.h.grafm3 = 0;
		gtia.h.usedm3 = 0;
	}
	else
	{
		gtia.h.hposm3 = gtia.w.hposm3;
		gtia.h.grafm3 = (gtia.w.grafm << 0) & 0xc0;
		gtia.h.usedm3 = (gtia.w.prior & 0x10) ? 0x08 : 0x80;
    }
}

void MWA_GTIA(int offset, int data)
{
/* used for mixing hue/lum of different colors */
static UINT8 lumpm0=0,lumpm1=0,lumpm2=0,lumpm3=0,lumpm4=0;
static UINT8 lumpf1=0,lumpf2=0,lumbk= 0;
static UINT8 huepm0=0,huepm1=0,huepm2=0,huepm3=0,huepm4=0;
static UINT8 huepf1=0,huepf2=0,huebk= 0;

    switch (offset & 31)
    {
		case  0:
			gtia.w.hposp0 = data;
			recalc_p0();
			break;
		case  1:
			gtia.w.hposp1 = data;
			recalc_p1();
            break;
		case  2:
			gtia.w.hposp2 = data;
			recalc_p2();
            break;
		case  3:
			gtia.w.hposp3 = data;
			recalc_p3();
            break;

		case  4:
			gtia.w.hposm0 = data;
			recalc_m0();
            break;
		case  5:
			gtia.w.hposm1 = data;
			recalc_m1();
            break;
		case  6:
			gtia.w.hposm2 = data;
			recalc_m2();
            break;
		case  7:
			gtia.w.hposm3 = data;
			recalc_m3();
            break;

		case  8:
			data &= 3;
			gtia.w.sizep0 = data;
			gtia.h.sizep0 = pm_size[data];
			recalc_p0();
            break;
		case  9:
			data &= 3;
			gtia.w.sizep1 = data;
			gtia.h.sizep1 = pm_size[data];
            recalc_p1();
            break;
		case 10:
			data &= 3;
			gtia.w.sizep2 = data;
			gtia.h.sizep2 = pm_size[data];
            recalc_p2();
            break;
		case 11:
			data &= 3;
			gtia.w.sizep3 = data;
			gtia.h.sizep3 = pm_size[data];
            recalc_p3();
            break;

		case 12:
			data &= 3;
			gtia.w.sizem = data;
            gtia.h.sizem = pm_size[data];
			recalc_m0();
			recalc_m1();
			recalc_m2();
			recalc_m3();
            break;

		case 13:
			gtia.w.grafp0 = data;
			recalc_p0();
			break;
		case 14:
			gtia.w.grafp1 = data;
			recalc_p1();
            break;
		case 15:
			gtia.w.grafp2 = data;
			recalc_p2();
            break;
		case 16:
			gtia.w.grafp3 = data;
			recalc_p3();
            break;

		case 17:
			gtia.w.grafm = data;
			recalc_m0();
			recalc_m1();
			recalc_m2();
			recalc_m3();
            break;

        case 18:    /* color for player/missile #0 */
			if (data == gtia.w.colpm0)
                break;
			gtia.w.colpm0 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpm0 $%02x\n", data);
#endif
            SETCOL_B(PL0,data);     /* set player 0 color */
            SETCOL_B(MI0,data);     /* set missile 0 color */
            SETCOL_B(GT2,data);     /* set GTIA mode 2 color 0 */
            SETCOL_B(P000,data);    /* set player 0 both pixels 0 */
            SETCOL_L(P001,data);    /* set player 0 left pixel 0 */
            SETCOL_R(P010,data);    /* set player 0 right pixel 0 */
            SPLIT_HUE_LUM(data,huepm0,lumpm0);
            data = huepm0 | lumpf1;
            SETCOL_R(P001,data);    /* set player 0 right pixel 1 */
            SETCOL_L(P010,data);    /* set player 0 left pixel 1 */
            SETCOL_B(P011,data);    /* set player 0 both pixels 1 */
            break;
        case 19:    /* color for player/missile #1 */
			if (data == gtia.w.colpm1)
                break;
			gtia.w.colpm1 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpm1 $%02x\n", data);
#endif
            SETCOL_B(PL1,data);     /* set player color 1 */
            SETCOL_B(MI1,data);     /* set missile color 1 */
            SETCOL_B(GT2+1,data);   /* set GTIA mode 2 color 1 */
            SETCOL_B(P100,data);    /* set player 1 both pixels 0 */
            SETCOL_L(P101,data);    /* set player 1 left pixel 0 */
            SETCOL_R(P110,data);    /* set player 1 right pixel 0 */
            SPLIT_HUE_LUM(data,huepm1,lumpm1);
            data = huepm1 | lumpf1;
            SETCOL_R(P101,data);    /* set player 1 right pixel 1 */
            SETCOL_L(P110,data);    /* set player 1 left pixel 1 */
            SETCOL_B(P111,data);    /* set player 1 both pixels 1 */
            break;
        case 20:    /* color for player/missile #2 */
			if (data == gtia.w.colpm2)
                break;
			gtia.w.colpm2 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpm2 $%02x\n", data);
#endif
            SETCOL_B(PL2,data);     /* set player 2 color */
            SETCOL_B(MI2,data);     /* set missile 2 color */
            SETCOL_B(GT2+2,data);   /* set GTIA mode 2 color 2 */
            SETCOL_B(P200,data);    /* set player 2 both pixels 0 */
            SETCOL_L(P201,data);    /* set player 2 left pixel 0 */
            SETCOL_R(P210,data);    /* set player 2 right pixel 0 */
            SPLIT_HUE_LUM(data,huepm2,lumpm2);
            data = huepm2 | lumpf1;
            SETCOL_R(P201,data);    /* set player 2 right pixel 1 */
            SETCOL_L(P210,data);    /* set player 2 left pixel 1 */
            SETCOL_B(P211,data);    /* set player 2 both pixels 1 */
            break;
        case 21:    /* color for player/missile #3 */
			if (data == gtia.w.colpm3)
                break;
			gtia.w.colpm3 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpm3 $%02x\n", data);
#endif
            SETCOL_B(PL3,data);     /* set player 3 color */
            SETCOL_B(MI3,data);     /* set missile 3 color */
            SETCOL_B(GT2+3,data);   /* set GTIA mode 2 color 3 */
            SETCOL_B(P300,data);    /* set player 3 both pixels 0 */
            SETCOL_L(P301,data);    /* set player 3 left pixel 0 */
            SETCOL_R(P310,data);    /* set player 3 right pixel 0 */
            SPLIT_HUE_LUM(data,huepm3,lumpm3);
            data = huepm3 | lumpf1;
            SETCOL_R(P301,data);    /* set player 3 right pixel 1 */
            SETCOL_L(P310,data);    /* set player 3 left pixel 1 */
            SETCOL_B(P311,data);    /* set player 3 both pixels 1 */
            break;
        case 22:    /* playfield color #0 */
			if (data == gtia.w.colpf0)
                break;
			gtia.w.colpf0 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpf0 $%02x\n", data);
#endif
            SETCOL_B(PF0,data);     /* set playfield 0 color */
            SETCOL_B(GT2+4,data);   /* set GTIA mode 2 color 4 */
            break;
        case 23:    /* playfield color #1 */
			if (data == gtia.w.colpf1)
                break;
			gtia.w.colpf1 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpf1 $%02x\n", data);
#endif
            SETCOL_B(PF1,data);     /* set playfield 1 color */
            SETCOL_B(GT2+5,data);   /* set GTIA mode 2 color 5 */
            SPLIT_HUE_LUM(data,huepf1,lumpf1);
            data = huepf2 | lumpf1;
            SETCOL_R(T01,data);     /* set text mode right pixel 1 */
            SETCOL_L(T10,data);     /* set text mode left pixel 1 */
            SETCOL_B(T11,data);     /* set text mode both pixels 1 */
            data = huebk | lumpf1;
            SETCOL_R(G01,data);     /* set graphics mode right pixel 1 */
            SETCOL_L(G10,data);     /* set graphics mode left pixel 1 */
            SETCOL_B(G11,data);     /* set graphics mode both pixels 1 */
            data = huepm0 | lumpf1;
            SETCOL_R(P001,data);    /* set player 0 right pixel 1 */
            SETCOL_L(P010,data);    /* set player 0 left pixel 1 */
            SETCOL_B(P011,data);    /* set player 0 both pixels 1 */
            data = huepm1 | lumpf1;
            SETCOL_R(P101,data);    /* set player 1 right pixel 1 */
            SETCOL_L(P110,data);    /* set player 1 left pixel 1 */
            SETCOL_B(P111,data);    /* set player 1 both pixels 1 */
            data = huepm2 | lumpf1;
            SETCOL_R(P201,data);    /* set player 2 right pixel 1 */
            SETCOL_L(P210,data);    /* set player 2 left pixel 1 */
            SETCOL_B(P211,data);    /* set player 2 both pixels 1 */
            data = huepm3 | lumpf1;
            SETCOL_R(P301,data);    /* set player 3 right pixel 1 */
            SETCOL_L(P310,data);    /* set player 3 left pixel 1 */
            SETCOL_B(P311,data);    /* set player 3 both pixels 1 */
            data = huepm4 | lumpf1;
            SETCOL_R(P401,data);    /* set missiles right pixel 1 */
            SETCOL_L(P410,data);    /* set missiles left pixel 1 */
            SETCOL_B(P411,data);    /* set missiles both pixels 1 */
            break;
        case 24:    /* playfield color #2 */
			if (data == gtia.w.colpf2)
                break;
			gtia.w.colpf2 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpf2 $%02x\n", data);
#endif
            SETCOL_B(PF2,data);     /* set playfield color 2 */
            SETCOL_B(GT2+6,data);   /* set GTIA mode 2 color 6 */
            SETCOL_B(T00,data);     /* set text mode both pixels 0 */
            SETCOL_L(T01,data);     /* set text mode left pixel 0 */
            SETCOL_R(T10,data);     /* set text mode right pixel 0 */
            SPLIT_HUE_LUM(data,huepf2,lumpf2);
            data = huepf2 | lumpf1;
            SETCOL_R(T01,data);     /* set text mode right pixel 1 */
            SETCOL_L(T10,data);     /* set text mode left pixel 1 */
            SETCOL_B(T11,data);     /* set text mode both pixels 1 */
            break;
        case 25:    /* playfield color #3 */
			if (data == gtia.w.colpf3)
                break;
			gtia.w.colpf3 = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colpf3 $%02x\n", data);
#endif
            SETCOL_B(PF3,data);     /* set playfield color 3 */
            SETCOL_B(GT2+7,data);   /* set GTIA mode 2 color 7 */
            SETCOL_B(P400,data);    /* set p/m xor mode both pixels 0 */
            SETCOL_L(P401,data);    /* set p/m xor mode left pixel 0 */
            SETCOL_R(P410,data);    /* set p/m xor mode right pixel 0 */
            SPLIT_HUE_LUM(data,huepm4,lumpm4);
            data = huepm4 | lumpf1;
            SETCOL_R(P401,data);    /* set p/m xor mode right pixel 1 */
            SETCOL_L(P410,data);    /* set p/m xor mode left pixel 1 */
            SETCOL_B(P411,data);    /* set p/m xor mode both pixels 1 */
            break;
        case 26:    /* playfield background */
			if (data == gtia.w.colbk)
                break;
			gtia.w.colbk = data;
#ifdef VERBOSE
            if (errorlog) fprintf(errorlog, "atari colbk  $%02x\n", data);
#endif
            SETCOL_B(PBK,data);     /* set background color */
            SETCOL_B(GT2+8,data);   /* set GTIA mode 2 color 8 */
            SETCOL_B(GT2+9,data);   /* set GTIA mode 2 color 9 */
            SETCOL_B(GT2+10,data);  /* set GTIA mode 2 color 10 */
            SETCOL_B(GT2+11,data);  /* set GTIA mode 2 color 11 */
            SETCOL_B(GT2+12,data);  /* set GTIA mode 2 color 12 */
            SETCOL_B(GT2+13,data);  /* set GTIA mode 2 color 13 */
            SETCOL_B(GT2+14,data);  /* set GTIA mode 2 color 14 */
            SETCOL_B(GT2+15,data);  /* set GTIA mode 2 color 15 */
            SETCOL_B(G00,data);     /* set 2 color graphics both pixels 0 */
            SETCOL_L(G01,data);     /* set 2 color graphics left pixel 0 */
            SETCOL_R(G10,data);     /* set 2 color graphics right pixel 0 */
            SPLIT_HUE_LUM(data,huebk,lumbk);
            data = huebk | lumpf1;
            SETCOL_R(G01,data);     /* set 2 color graphics right pixel 1 */
            SETCOL_L(G10,data);     /* set 2 color graphics left pixel 1 */
            SETCOL_B(G11,data);     /* set 2 color graphics both pixels 1 */
            SETCOL_B(GT1+ 0,(data&HUE)+(0x00&LUM));   /* set GTIA mode 1 HUE + LUM 0..15 */
            SETCOL_B(GT1+ 1,(data&HUE)+(0x11&LUM));
            SETCOL_B(GT1+ 2,(data&HUE)+(0x22&LUM));
            SETCOL_B(GT1+ 3,(data&HUE)+(0x33&LUM));
            SETCOL_B(GT1+ 4,(data&HUE)+(0x44&LUM));
            SETCOL_B(GT1+ 5,(data&HUE)+(0x55&LUM));
            SETCOL_B(GT1+ 6,(data&HUE)+(0x66&LUM));
            SETCOL_B(GT1+ 7,(data&HUE)+(0x77&LUM));
            SETCOL_B(GT1+ 8,(data&HUE)+(0x88&LUM));
            SETCOL_B(GT1+ 9,(data&HUE)+(0x99&LUM));
            SETCOL_B(GT1+10,(data&HUE)+(0xaa&LUM));
            SETCOL_B(GT1+11,(data&HUE)+(0xbb&LUM));
            SETCOL_B(GT1+12,(data&HUE)+(0xcc&LUM));
            SETCOL_B(GT1+13,(data&HUE)+(0xdd&LUM));
            SETCOL_B(GT1+14,(data&HUE)+(0xee&LUM));
            SETCOL_B(GT1+15,(data&HUE)+(0xff&LUM));
            SETCOL_B(GT3+ 0,(data&LUM)+(0x00&HUE));   /* set GTIA mode 3 LUM + HUE 0..15 */
            SETCOL_B(GT3+ 1,(data&LUM)+(0x11&HUE));
            SETCOL_B(GT3+ 2,(data&LUM)+(0x22&HUE));
            SETCOL_B(GT3+ 3,(data&LUM)+(0x33&HUE));
            SETCOL_B(GT3+ 4,(data&LUM)+(0x44&HUE));
            SETCOL_B(GT3+ 5,(data&LUM)+(0x55&HUE));
            SETCOL_B(GT3+ 6,(data&LUM)+(0x66&HUE));
            SETCOL_B(GT3+ 7,(data&LUM)+(0x77&HUE));
            SETCOL_B(GT3+ 8,(data&LUM)+(0x88&HUE));
            SETCOL_B(GT3+ 9,(data&LUM)+(0x99&HUE));
            SETCOL_B(GT3+10,(data&LUM)+(0xaa&HUE));
            SETCOL_B(GT3+11,(data&LUM)+(0xbb&HUE));
            SETCOL_B(GT3+12,(data&LUM)+(0xcc&HUE));
            SETCOL_B(GT3+13,(data&LUM)+(0xdd&HUE));
            SETCOL_B(GT3+14,(data&LUM)+(0xee&HUE));
            SETCOL_B(GT3+15,(data&LUM)+(0xff&HUE));
            break;

		case 27:
			gtia.w.prior = data;
			recalc_m0();
			recalc_m1();
			recalc_m2();
			recalc_m3();
			break;

        case 28:    /* delay until vertical retrace */
			gtia.w.vdelay = data;
			gtia.h.vdelay_m0 = (data >> 0) & 1;
			gtia.h.vdelay_m1 = (data >> 1) & 1;
			gtia.h.vdelay_m2 = (data >> 2) & 1;
			gtia.h.vdelay_m3 = (data >> 3) & 1;
			gtia.h.vdelay_p0 = (data >> 4) & 1;
			gtia.h.vdelay_p1 = (data >> 5) & 1;
			gtia.h.vdelay_p2 = (data >> 6) & 1;
			gtia.h.vdelay_p3 = (data >> 7) & 1;
            break;

		case 29:
			gtia.w.gractl = data;
			recalc_p0();
			recalc_p1();
			recalc_p2();
			recalc_p3();
			recalc_m0();
			recalc_m1();
			recalc_m2();
			recalc_m3();
            break;

        case 30:    /* clear collisions */
#if OPTIMIZE
			if (*(UINT32*)&gtia.r.m0pf || *(UINT32*)&gtia.r.p0pf ||
				*(UINT32*)&gtia.r.m0pl || *(UINT32*)&gtia.r.p0pl)
            {
            int i;
			VIDEO * video = antic.video[antic.scanline];
				*(UINT32*)&gtia.r.m0pf = *(UINT32*)&gtia.r.p0pf =
				*(UINT32*)&gtia.r.m0pl = *(UINT32*)&gtia.r.p0pl = 0;
                for (i = 0; i < TOTAL_LINES; i++)
				{
					video = antic.video[i];
					*(UINT32*)&video->gtia_r.m0pf =
					*(UINT32*)&video->gtia_r.p0pf =
					*(UINT32*)&video->gtia_r.m0pl =
					*(UINT32*)&video->gtia_r.p0pl =
					*(UINT32*)&video->gtia_h.grafp0 =
					*(UINT32*)&video->gtia_h.grafm0 = 0;
                }
				gtia.h.hitclr_frames = 0;
            }
#else
			gtia.r.m0pf = gtia.r.m1pf = gtia.r.m2pf = gtia.r.m3pf =
			gtia.r.p0pf = gtia.r.p1pf = gtia.r.p2pf = gtia.r.p3pf =
			gtia.r.m0pl = gtia.r.m1pl = gtia.r.m2pl = gtia.r.m3pl =
			gtia.r.p0pl = gtia.r.p1pl = gtia.r.p2pl = gtia.r.p3pl = 0;
#endif
			gtia.w.hitclr = data;
            break;
        case 31:    /* write console (speaker) */
			if (data == gtia.w.cons)
				break;
            gtia.w.cons  = data;
			if (gtia.w.cons & 0x08)
				DAC_data_w(0, -120);
			else
				DAC_data_w(0, +120);
            break;
    }
}

static UINT8 pf_collision[256] = {
	0,1,2,0,4,0,0,0,8,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
   15,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static int	pf_prioindex[256] = {
/*			PBK   PF0	PF1 		PF2 					PF3 										   */
/*	   */	0x000,0x100,0x100,0x000,0x200,0x000,0x000,0x000,0x200,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/*	   */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/*	   */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/*	   */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* T00 */	0x400,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* T01 */	0x500,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* T10 */	0x600,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* T11 */	0x700,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* G00 */	0x400,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* G01 */	0x500,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* G10 */	0x600,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* G11 */	0x700,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* GT1 */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* GT2 */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/* GT3 */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,
/*	   */	0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000,0x000
};

void gtia_render(VIDEO *video)
{
int x;
UINT8 *prio = antic.prio_table[gtia.w.prior & 0x3f];
UINT8 *src, *dst;

#ifdef	SHOW_EVERY_SCREEN_BIT
	if (antic.scanline >= 256)
        return;
#else
	if (antic.scanline < VBL_END ||  antic.scanline >= 256)
        return;
#endif

	if (gtia.h.hposp0)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposp0];
		if (gtia.h.grafp0 & 0x80)
			for (i = 0*gtia.h.sizep0; i < 1*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x40)
			for (i = 1*gtia.h.sizep0; i < 2*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x20)
			for (i = 2*gtia.h.sizep0; i < 3*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x10)
			for (i = 3*gtia.h.sizep0; i < 4*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x08)
			for (i = 4*gtia.h.sizep0; i < 5*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x04)
			for (i = 5*gtia.h.sizep0; i < 6*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x02)
			for (i = 6*gtia.h.sizep0; i < 7*gtia.h.sizep0; i++)
				dst[i] |= P0;
		if (gtia.h.grafp0 & 0x01)
			for (i = 7*gtia.h.sizep0; i < 8*gtia.h.sizep0; i++)
				dst[i] |= P0;
	}

    if (gtia.h.hposp1)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposp1];
		if (gtia.h.grafp1 & 0x80)
			for (i = 0*gtia.h.sizep1; i < 1*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x40)
			for (i = 1*gtia.h.sizep1; i < 2*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x20)
			for (i = 2*gtia.h.sizep1; i < 3*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x10)
			for (i = 3*gtia.h.sizep1; i < 4*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x08)
			for (i = 4*gtia.h.sizep1; i < 5*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x04)
			for (i = 5*gtia.h.sizep1; i < 6*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x02)
			for (i = 6*gtia.h.sizep1; i < 7*gtia.h.sizep1; i++)
				dst[i] |= P1;
		if (gtia.h.grafp1 & 0x01)
			for (i = 7*gtia.h.sizep1; i < 8*gtia.h.sizep1; i++)
				dst[i] |= P1;
	}

	if (gtia.h.hposp2)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposp2];
		if (gtia.h.grafp2 & 0x80)
			for (i = 0*gtia.h.sizep2; i < 1*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x40)
			for (i = 1*gtia.h.sizep2; i < 2*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x20)
			for (i = 2*gtia.h.sizep2; i < 3*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x10)
			for (i = 3*gtia.h.sizep2; i < 4*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x08)
			for (i = 4*gtia.h.sizep2; i < 5*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x04)
			for (i = 5*gtia.h.sizep2; i < 6*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x02)
			for (i = 6*gtia.h.sizep2; i < 7*gtia.h.sizep2; i++)
				dst[i] |= P2;
		if (gtia.h.grafp2 & 0x01)
			for (i = 7*gtia.h.sizep2; i < 8*gtia.h.sizep2; i++)
				dst[i] |= P2;
	}

	if (gtia.h.hposp3)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposp3];
		if (gtia.h.grafp3 & 0x80)
			for (i = 0*gtia.h.sizep3; i < 1*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x40)
			for (i = 1*gtia.h.sizep3; i < 2*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x20)
			for (i = 2*gtia.h.sizep3; i < 3*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x10)
			for (i = 3*gtia.h.sizep3; i < 4*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x08)
			for (i = 4*gtia.h.sizep3; i < 5*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x04)
			for (i = 5*gtia.h.sizep3; i < 6*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x02)
			for (i = 6*gtia.h.sizep3; i < 7*gtia.h.sizep3; i++)
				dst[i] |= P3;
		if (gtia.h.grafp3 & 0x01)
			for (i = 7*gtia.h.sizep3; i < 8*gtia.h.sizep3; i++)
				dst[i] |= P3;
	}

	if (gtia.h.hposm0)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposm0];
		if (gtia.h.grafm0 & 0x80)
			for (i = 0*gtia.h.sizem; i < 1*gtia.h.sizem; i++)
				dst[i] |= M0;
		if (gtia.h.grafm0 & 0x40)
			for (i = 1*gtia.h.sizem; i < 2*gtia.h.sizem; i++)
				dst[i] |= M0;
	}

	if (gtia.h.hposm1)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposm1];
		if (gtia.h.grafm1 & 0x80)
			for (i = 0*gtia.h.sizem; i < 1*gtia.h.sizem; i++)
				dst[i] |= M1;
		if (gtia.h.grafm1 & 0x40)
			for (i = 1*gtia.h.sizem; i < 2*gtia.h.sizem; i++)
				dst[i] |= M1;
	}

	if (gtia.h.hposm2)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposm2];
		if (gtia.h.grafm2 & 0x80)
			for (i = 0*gtia.h.sizem; i < 1*gtia.h.sizem; i++)
				dst[i] |= M2;
		if (gtia.h.grafm2 & 0x40)
			for (i = 1*gtia.h.sizem; i < 2*gtia.h.sizem; i++)
				dst[i] |= M2;
	}

	if (gtia.h.hposm3)
	{
	int i;
		dst = &antic.pmbits[gtia.h.hposm3];
		if (gtia.h.grafm3 & 0x80)
			for (i = 0*gtia.h.sizem; i < 1*gtia.h.sizem; i++)
				dst[i] |= M3;
		if (gtia.h.grafm3 & 0x40)
			for (i = 1*gtia.h.sizem; i < 2*gtia.h.sizem; i++)
				dst[i] |= M3;
	}

	src = antic.pmbits + PMOFFSET;
	dst = antic.cclock + PMOFFSET - antic.hscrol_old;

    for (x = 0; x < HWIDTH*4; x++, src++, dst++)
	{
		if (*src)		/* here is one... */
		{
		UINT8 pm, pc, pf;
			/* get the player/missile combination bits and reset the buffer */
			pm = *src;
			*src = 0;
			/* get the current playfield color */
            pc = *dst;
			pf = pf_collision[pc];
			if (pm&P0) { gtia.r.p0pf |= pf; gtia.r.p0pl |= pm&(   P1|P2|P3); }
			if (pm&P1) { gtia.r.p1pf |= pf; gtia.r.p1pl |= pm&(P0|	 P2|P3); }
			if (pm&P2) { gtia.r.p2pf |= pf; gtia.r.p2pl |= pm&(P0|P1|	P3); }
			if (pm&P3) { gtia.r.p3pf |= pf; gtia.r.p3pl |= pm&(P0|P1|P2   ); }
			if (pm&M0) { gtia.r.m0pf |= pf; gtia.r.m0pl |= pm&(P0|P1|P2|P3); }
            if (pm&M1) { gtia.r.m1pf |= pf; gtia.r.m1pl |= pm&(P0|P1|P2|P3); }
            if (pm&M2) { gtia.r.m2pf |= pf; gtia.r.m2pl |= pm&(P0|P1|P2|P3); }
            if (pm&M3) { gtia.r.m3pf |= pf; gtia.r.m3pl |= pm&(P0|P1|P2|P3); }
            /* color with higher priority? change playfield */
			pc = prio[pf_prioindex[pc] | pm];
			if (pc) *dst = pc;
		}
	}
}

/*************  ANTIC mode 0F : GTIA mode 1 ********************
 * graphics mode 8x1:16 (32/40/48 byte per line)
 ***************************************************************/
#define GTIA1(s) COPY4(dst, antic.pf_gtia1[video->data[s]])

void gtia_mode_1_32(VIDEO *video)
{
	PREPARE_GFXG1(32);
	REP32(GTIA1);
	POST_GFX(32);
}
void gtia_mode_1_40(VIDEO *video)
{
	PREPARE_GFXG1(40);
	REP40(GTIA1);
	POST_GFX(40);
}
void gtia_mode_1_48(VIDEO *video)
{
	PREPARE_GFXG1(48);
	REP48(GTIA1);
	POST_GFX(48);
}

/*************  ANTIC mode 0F : GTIA mode 2 ********************
 * graphics mode 8x1:16 (32/40/48 byte per line)
 ***************************************************************/
#define GTIA2(s) COPY4(dst, antic.pf_gtia2[video->data[s]])

void gtia_mode_2_32(VIDEO *video)
{
	PREPARE_GFXG2(32);
	REP32(GTIA2);
	POST_GFX(32);
}
void gtia_mode_2_40(VIDEO *video)
{
	PREPARE_GFXG2(40);
	REP40(GTIA2);
	POST_GFX(40);
}
void gtia_mode_2_48(VIDEO *video)
{
	PREPARE_GFXG2(48);
	REP48(GTIA2);
	POST_GFX(48);
}

/*************  ANTIC mode 0F : GTIA mode 3 ********************
 * graphics mode 8x1:16 (32/40/48 byte per line)
 ***************************************************************/
#define GTIA3(s) COPY4(dst, antic.pf_gtia3[video->data[s]])

void gtia_mode_3_32(VIDEO *video)
{
	PREPARE_GFXG3(32);
	REP32(GTIA3);
	POST_GFX(32);
}
void gtia_mode_3_40(VIDEO *video)
{
	PREPARE_GFXG3(40);
	REP40(GTIA3);
	POST_GFX(40);
}
void gtia_mode_3_48(VIDEO *video)
{
	PREPARE_GFXG3(48);
	REP48(GTIA3);
	POST_GFX(48);
}

