/***************************************************************************

  jupiter.c

  Functions to emulate the video hardware of the Jupiter Ace.

***************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

unsigned char *jupiter_scrnram;
unsigned char *jupiter_dirty;
unsigned char *jupiter_charram;


int jupiter_vh_start(void)

{

jupiter_scrnram = malloc(0x300);
if (!jupiter_scrnram) {
  return 1;
}

jupiter_dirty = malloc(0x400);
if (!jupiter_dirty) {
  free(jupiter_scrnram);
  return 1;
}
memset (jupiter_dirty, 1, 0x400);

jupiter_charram = malloc(0x400);
if (!jupiter_charram) {
  free(jupiter_dirty);
  free(jupiter_scrnram);
  return 1;
}

return (0);

}

void    jupiter_vh_stop(void) {

free(jupiter_scrnram);
free(jupiter_dirty);
free(jupiter_charram);

}

void jupiter_vh_charram_w(int offset, int data) {
  jupiter_charram[offset] = data;
}

int jupiter_vh_charram_r(int offset) {
  return(jupiter_charram[offset]);
}


void jupiter_vh_scrnram_w(int offset,int data) {
  jupiter_dirty[offset] = 1;
  jupiter_scrnram[offset] = data;
}

int jupiter_vh_scrnram_r(int offset) {
  return(jupiter_scrnram[offset]);
}

void jupiter_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)

{

unsigned	char	*dp, charpat;
		int	xcoord, ycoord, xbit, ybit;
		int	bitcnt/*, count*/;

/*if (full_refresh) for (count = 0; count < 0x400; jupiter_dirty[count++]);*/
for (ycoord = 0; ycoord < 24; ycoord++) {
  for (xcoord = 0; xcoord < 32; xcoord++) {
    if (jupiter_dirty[ycoord * 32 + xcoord]) {
      for (ybit = 0; ybit < 8; ybit++) {
	charpat = jupiter_charram[(jupiter_scrnram[ycoord * 32 + xcoord] & 0x7f) * 8 + ybit];
	dp = (*(*Machine).gfx[0]).gfxdata + ybit * 8;
	memset (dp, 0, 8);
	if (jupiter_scrnram[ycoord * 32 + xcoord] & 0x80) {
	  for (xbit = 0, bitcnt = 128; xbit < 8; xbit++, bitcnt >>= 1) {
	    if (!(charpat & bitcnt)) dp[xbit] = 1;
	  }
	} else {
	  for (xbit = 0, bitcnt = 128; xbit < 8; xbit++, bitcnt >>= 1) {
	    if (charpat & bitcnt) dp[xbit] = 1;
	  }
	}
      }
      drawgfx (bitmap, Machine->gfx[0], 0, 1, 0, 0, xcoord * 8, ycoord * 8, 0,
      									TRANSPARENCY_NONE, 0);
      jupiter_dirty[ycoord * 32 + xcoord] = 0;
    }
  }
}

}

