#ifndef __VECTOR__
#define __VECTOR__

#define VECTOR_TEAM \
	"-* Vector Heads *-\n" \
	"Brad Oliver\n" \
	"Aaron Giles\n" \
	"Bernd Wiebelt\n" \
	"Allard van der Bas\n" \
	"Al Kossow (VECSIM)\n" \
	"Hedley Rainnie (VECSIM)\n" \
	"Eric Smith (VECSIM)\n" \
	"Neil Bradley (technical advice)\n" \
	"Andrew Caldwell (anti-aliasing)\n" \
	"- *** -\n"

#define MAX_POINTS 5000	/* Maximum # of points we can queue in a vector list */

#define MAX_PIXELS 200000  /* Maximum of pixels we can remember */

#define TRANSLUCENCY 1	/* translucent vectors  */

extern unsigned char *vectorram;
extern int vectorram_size;

int  vector_vh_start (void);
void vector_vh_stop (void);
void vector_vh_update(struct osd_bitmap *bitmap,int full_refresh);
void vector_clear_list (void);
void vector_draw_to (int x2, int y2, int col, int intensity, int dirty);
void vector_add_point (int x, int y, int color, int intensity);
void vector_add_clip (int minx, int miny, int maxx, int maxy);
void vector_set_shift (int shift);

#endif
