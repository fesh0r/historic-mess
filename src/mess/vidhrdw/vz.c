/***************************************************************************
	vz.c

    video hardware
	Juergen Buchmueller <pullmoll@t-online.de>, Dec 1999

****************************************************************************/

#include "driver.h"
#include "vidhrdw/generic.h"

/* from machine/vz.c */
extern int vz_latch;

void vz_vh_screenrefresh(struct osd_bitmap *bitmap, int full_refresh)
{
    int offs;

    if( full_refresh )
        memset(dirtybuffer, 0xff, videoram_size);

    if( vz_latch & 0x08 )
    {
		int color = (vz_latch & 0x10) ? 1 : 0;
        for( offs = 0; offs < videoram_size; offs++ )
        {
            if( dirtybuffer[offs] )
            {
                int sx, sy, code;
				sy = (offs / 32) * 3;
				sx = (offs % 32) * 8;
                code = videoram[offs];
                drawgfx(
					bitmap,Machine->gfx[1],code,color,0,0,sx,sy,
                    &Machine->drv->visible_area,TRANSPARENCY_NONE,0);
				osd_mark_dirty(sx,sy,sx+7,sy+2,0);
                dirtybuffer[offs] = 0;
            }
        }
    }
    else
    {
        for( offs = 0; offs < 32*16; offs++ )
        {
            if( dirtybuffer[offs] || dirtybuffer[offs+0x200] )
            {
                int sx, sy, code, color;
				sy = (offs / 32) * 12;
				sx = (offs % 32) * 8;
                code = videoram[offs];
				if( vz_latch & 0x10 )
					color = (code & 0x80) ? ((code >> 4) & 3) + 5 : 9;
                else
					color = (code & 0x80) ? ((code >> 4) & 3) + 1 : 0;
                drawgfx(
					bitmap,Machine->gfx[0],code,color,0,0,sx,sy,
                    &Machine->drv->visible_area,TRANSPARENCY_NONE,0);
				osd_mark_dirty(sx,sy,sx+7,sy+11,0);
                dirtybuffer[offs] = 0;
            }
        }
    }
}


