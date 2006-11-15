/**********************************************************************

  Copyright (C) Antoine Mine' 2006

  Thomson 8-bit computers

**********************************************************************/

#include "math.h"
#include "driver.h"
#include "timer.h"
#include "state.h"
#include "cpu/m6809/m6809.h"
#include "machine/6821pia.h"
#include "machine/mc6846.h"
#include "machine/thomson.h"
#include "vidhrdw/thomson.h"


#define VERBOSE 0


#if VERBOSE
#define LOG(x)	logerror x
#else
#define LOG(x)
#endif

#define PRINT(x) mame_printf_info x


/* One GPL is what is drawn in 1 us by the video system in the active window.
   Most of the time, it corresponds to a 8-pixel wide horizontal span.
   For some TO8/9/9+/MO6 modes, it can be 4-pixel or 16-pixel wide.
   There are always 40 GPLs in the active width, and it is always defined by
   two bytes in video memory (0x2000 bytes appart).
*/

#define THOM_GPL_PER_LINE 40


/****************** dynamic screen size *****************/

static UINT16 thom_bwidth;
static UINT16 thom_bheight;
/* border size */

static UINT8  thom_hires;
/* 0 = low res: 320x200 active area (faster)
   1 = hi res:  640x400 active area (can represent 640x200 video mode)
 */

static int thom_update_screen_size( void )
{
  UINT8 p = readinputport( THOM_INPUT_VCONFIG );
  int new_w, new_h, changed = 0;
  switch ( p & 3 ) {
  case 0:  thom_bwidth = 56; thom_bheight = 47; break; /* as in original */
  case 1:  thom_bwidth = 16; thom_bheight = 16; break; /* small */
  default: thom_bwidth =  0; thom_bheight =  0; break; /* none */
  }
  thom_hires = ( p & 4 ) ? 1 : 0;

  new_w = ( 320 + thom_bwidth * 2 ) * ( thom_hires + 1 ) - 1;
  new_h = ( 200 + thom_bheight * 2 ) * (thom_hires + 1 ) - 1;
  if ( ( Machine->screen[0].visarea.max_x != new_w ) ||
       ( Machine->screen[0].visarea.max_y != new_h ) ) 
    changed = 1;
  video_screen_set_visarea( 0, 0, new_w, 0, new_h );

  return changed;
}



/*********************** video timing ******************************/

/* we use our own video timing to precisely cope with VBLANK and HBLANK */

static mame_timer* thom_video_timer; /* time elapsed from begining of frame */

/* elapsed time from begining of frame, in us */
INLINE unsigned thom_video_elapsed ( void )
{
  unsigned u;
  mame_time elapsed = mame_timer_timeelapsed( thom_video_timer );
  u = SUBSECONDS_TO_DOUBLE( elapsed.subseconds ) / TIME_IN_USEC( 1 );
  if ( u >= 19968 ) u = 19968;
  return u;
}

struct thom_vsignal thom_get_vsignal ( void )
{
  struct thom_vsignal v;
  int gpl = thom_video_elapsed() - 64 * THOM_BORDER_HEIGHT - 7;
  if ( gpl < 0 ) gpl += 19968;

  v.inil = ( gpl & 63 ) <= 40;

  v.init = gpl < (64 * THOM_ACTIVE_HEIGHT - 24);

  v.lt3 = ( gpl & 8 ) ? 1 : 0;

  v.line = gpl >> 6;

  v.count = v.line * 320 + ( gpl & 63 ) * 8;

  return v;
}


/************************** lightpen *******************************/

struct thom_vsignal thom_get_lightpen_vsignal ( int xdec, int ydec, int xdec2 )
{
  struct thom_vsignal v;
  int x = readinputport( THOM_INPUT_LIGHTPEN     );
  int y = readinputport( THOM_INPUT_LIGHTPEN + 1 );
  int gpl;

  if ( thom_hires ) { x /= 2; y /= 2; }
  x += xdec - thom_bwidth;
  y += ydec - thom_bheight;

  gpl = (x >> 3) + y * 64;
  if ( gpl < 0 ) gpl += 19968;
  
  v.inil = (gpl & 63) <= 41;

  v.init = (gpl <= 64 * THOM_ACTIVE_HEIGHT - 24);

  v.lt3 = ( gpl & 8 ) ? 1 : 0; 

  v.line = y;

  if ( v.inil && v.init )
    v.count = 
      ( gpl >> 6 ) * 320 +  /* line */
      ( gpl & 63 ) *   8 +  /* gpl inside line */
      ( (x + xdec2) & 7 );  /* pixel inside gpl */
  else v.count = 0;

  return v;
}


/* number of lightpen call-backs per frame */
static int thom_lightpen_nb;

/* called thom_lightpen_nb times */
static mame_timer *thom_lightpen_timer;

/* lightpen callback function to call from timer */
static void (*thom_lightpen_cb) ( int );

void thom_set_lightpen_callback ( int nb, void (*cb) ( int step ) )
{
  LOG (( "%f thom_set_lightpen_callback called\n", timer_get_time() ));
  thom_lightpen_nb = nb;
  thom_lightpen_cb = cb;
}

static void thom_lightpen_step ( int step )
{
  if ( thom_lightpen_cb ) thom_lightpen_cb( step );
  if ( step < thom_lightpen_nb )
    timer_adjust( thom_lightpen_timer, TIME_IN_USEC( 64 ), step + 1, 0 );
}


/***************** scan-line oriented engine ***********************/

/* This code, common for all Thomson machines, emulates the TO8
   video hardware, with its 16-colors chosen among 4096, 9 video modes,
   and 4 video pages. Moreover, it emulates changing the palette several times
   per frame to simulate more than 16 colors per frame (and the same for mode 
   and page switchs).

   TO7, TO7/70 and MO5 video harware are much simpler (8 or 16 fixed colors,
   one mode and one video page). Although the three are different, thay can all
   be emulated by the TO8 video hardware.
   Thus, we use the same TO8-emulation code to deal with these simpler
   hardware (although it is somewhat of an overkill).
 */


/* ---------------- state & state changes ---------------- */

UINT8* thom_vram; /* pointer to video memory */

static mame_timer* thom_scanline_timer; /* scan-line udpate */

static UINT16 thom_last_pal[16];   /* palette at last scanline start */
static UINT16 thom_pal[16];        /* current palette */
static UINT8  thom_pal_changed;    /* whether pal != old_pal */
static UINT8  thom_border_index;   /* current border color index */

/* border color at each scanline, including top and bottom border
   (-1 means unchanged)
*/
static  INT16 thom_border[THOM_TOTAL_HEIGHT+1];

/* active area, update one scan-line at a time every 64us,
   then blitted in VIDEO_UPDATE
 */
static UINT16 thom_vbody[640*200];

static UINT8 thom_vmode; /* current vide mode */
static UINT8 thom_vpage; /* current video page */

/* this stores the video mode & page at each GPL in the last line, to cope with
   (-1 means unchanged)
 */
static INT16 thom_vmodepage[41];
static UINT8 thom_vmodepage_changed;

/* one dirty flag for each video memory line */
static UINT8 thom_vmem_dirty[205];

/* set to 1 if undirty scanlines need to be redrawn due to other video state 
   changes */
static UINT8 thom_vstate_dirty;
static UINT8 thom_vstate_last_dirty;


/* either the border index or its palette entry has changed */
static void thom_border_changed( void )
{
  unsigned l = (thom_video_elapsed() + 53) >> 6;
  if ( l > THOM_TOTAL_HEIGHT ) l = THOM_TOTAL_HEIGHT;
  thom_border[ l ] = thom_pal[ thom_border_index ];
}

/* the video mode or page has changed */
static void thom_gplinfo_changed( void )
{
  unsigned l = thom_video_elapsed() - THOM_BORDER_HEIGHT * 64 - 7;
  unsigned y = l >> 6;
  unsigned x = l & 63;
  int modepage = ((int)thom_vmode << 8) | thom_vpage;
  if ( y >= 200 || x>= 40 ) thom_vmodepage[ 40 ] = modepage;
  else thom_vmodepage[ x ] = modepage;
  thom_vmodepage_changed = 1;
  thom_vstate_dirty = 1;
}

void thom_set_border_color ( unsigned index )
{
  assert( index < 16 );
  if ( index != thom_border_index ) {
    thom_border_index = index;
    thom_border_changed();
  }
}

void thom_set_palette ( unsigned index, UINT16 color )
{
  assert( index < 16 );
  if ( color != 0x1000 ) color &= 0xfff;
  if ( thom_pal[ index ] == color ) return;
  thom_pal[ index ] = color;
  if ( index == thom_border_index ) thom_border_changed();
  thom_pal_changed = 1;
  thom_vstate_dirty = 1;
}

void thom_set_video_mode ( unsigned mode )
{
  assert( mode < THOM_VMODE_NB );
  if ( mode != thom_vmode ) {
    thom_vmode = mode;
    thom_gplinfo_changed();
  }
}

void thom_set_video_page ( unsigned page )
{
  assert( page < THOM_NB_PAGES );
  if ( page != thom_vpage ) {
    thom_vpage = page;
    thom_gplinfo_changed();
  }
}


/* -------------- drawing --------------- */

typedef void ( *thom_scandraw ) ( UINT8* vram, UINT16* dst, UINT16* pal, 
				  int org, int len );

#define UPDATE( name, res )					\
  static void name##_scandraw_##res ( UINT8* vram, UINT16* dst,	UINT16* pal, \
				      int org, int len )		\
  {									\
    unsigned gpl;							\
    vram += org;							\
    dst += org * res;							\
    for ( gpl = 0; gpl < len; gpl++, dst += res, vram++ ) {		\
      UINT8 rama = vram[ 0      ];					\
      UINT8 ramb = vram[ 0x2000 ];				

#define UPDATE_HI( name )  UPDATE( name, 16 )
#define UPDATE_LOW( name ) UPDATE( name,  8 )

#define END_UPDATE } }


/* 320x200, 16-colors, constraints: 2 distinct colors per GPL (8 pixels) */
/* this also works for TO7, assuming the 2 top bits of each color byte are 1 */
UPDATE_HI( to770 )
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ ((ramb & 7) | ((ramb>>4) & 8)) ^ 8 ] ];
  c[1] = Machine->pens[ pal[ ((ramb >> 3) & 15) ^ 8 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( to770 )
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ ((ramb & 7) | ((ramb>>4) & 8)) ^ 8 ] ];
  c[1] = Machine->pens[ pal[ ((ramb >> 3) & 15) ^ 8 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[ 7 - i ] = c[ rama & 1 ];
}
END_UPDATE

/* as above, different (more logical but TO7-incompatible) color encoding */
UPDATE_HI( mo5 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ ramb & 15 ] ];
  c[1] = Machine->pens[ pal[ ramb >> 4 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( mo5 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ ramb & 15 ] ];
  c[1] = Machine->pens[ pal[ ramb >> 4 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[ 7 - i ] = c[ rama & 1 ];
}
END_UPDATE

/* as to770, but with pastel color bit unswitched */
UPDATE_HI( to9 )
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ (ramb & 7) | ((ramb>>4) & 8) ] ];
  c[1] = Machine->pens[ pal[ (ramb >> 3) & 15 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( to9 )
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ (ramb & 7) | ((ramb>>4) & 8) ] ];
  c[1] = Machine->pens[ pal[ (ramb >> 3) & 15 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[ 7 - i ] = c[ rama & 1 ];
}
END_UPDATE

/* 320x200, 4-colors, no constraints */
UPDATE_HI( bitmap4 ) 
{
  int i;
  pen_t c[2][2];
  c[0][0] = Machine->pens[ pal[ 0 ] ];
  c[0][1] = Machine->pens[ pal[ 1 ] ];
  c[1][0] = Machine->pens[ pal[ 2 ] ];
  c[1][1] = Machine->pens[ pal[ 3 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1, ramb >>= 1 )
    dst[ 15 - i ] =  dst[ 14 - i ] = c[ rama & 1 ] [ ramb & 1 ];
}
END_UPDATE
UPDATE_LOW( bitmap4 ) 
{
  int i;
  pen_t c[2][2];
  c[0][0] = Machine->pens[ pal[ 0 ] ];
  c[0][1] = Machine->pens[ pal[ 1 ] ];
  c[1][0] = Machine->pens[ pal[ 2 ] ];
  c[1][1] = Machine->pens[ pal[ 3 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1, ramb >>= 1 )
    dst[ 7 - i ] = c[ rama & 1 ] [ ramb & 1 ];
}
END_UPDATE

/* as above, but using 2-bit pixels instead of 2 planes of 1-bit pixels  */
UPDATE_HI( bitmap4alt ) 
{
  int i;
  pen_t c[4];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 1 ] ];
  c[2] = Machine->pens[ pal[ 2 ] ];
  c[3] = Machine->pens[ pal[ 3 ] ];
  for ( i = 0; i < 8; i += 2, ramb >>= 2 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ ramb & 3 ];
  for ( i = 0; i < 8; i += 2, rama >>= 2 )
    dst[ 7 - i ] = dst[ 6 - i ] = c[ rama & 3 ];
}
END_UPDATE
UPDATE_LOW( bitmap4alt ) 
{
  int i;
  pen_t c[4];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 1 ] ];
  c[2] = Machine->pens[ pal[ 2 ] ];
  c[3] = Machine->pens[ pal[ 3 ] ];
  for ( i = 0; i < 4; i++, ramb >>= 2 )
    dst[ 7 - i ] = c[ ramb & 3 ];
  for ( i = 0; i < 4; i++, rama >>= 2 )
    dst[ 3 - i ] = c[ rama & 3 ];
}
END_UPDATE

/* 160x200, 16-colors, no constraints */
UPDATE_HI( bitmap16 ) 
{
  dst[ 0] = dst[ 1] = dst[ 2] = dst[ 3] = Machine->pens[ pal[ rama >> 4 ] ];
  dst[ 4] = dst[ 5] = dst[ 6] = dst[ 7] = Machine->pens[ pal[ rama & 15 ] ];
  dst[ 8] = dst[ 9] = dst[10] = dst[11] = Machine->pens[ pal[ ramb >> 4 ] ];
  dst[12] = dst[13] = dst[14] = dst[15] = Machine->pens[ pal[ ramb & 15 ] ];
}
END_UPDATE
UPDATE_LOW( bitmap16 ) 
{
  dst[0] = dst[1] = Machine->pens[ pal[ rama >> 4 ] ];
  dst[2] = dst[3] = Machine->pens[ pal[ rama & 15 ] ];
  dst[4] = dst[5] = Machine->pens[ pal[ ramb >> 4 ] ];
  dst[6] = dst[7] = Machine->pens[ pal[ ramb & 15 ] ];
}
END_UPDATE

/* 640x200 (80 text column), 2-colors, no constraints */
UPDATE_HI( mode80 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 1 ] ];
  for ( i = 0; i < 8; i++, ramb >>= 1 )
    dst[ 15 - i ] = c[ ramb & 1 ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[  7 - i ] = c[ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( mode80 ) 
{
  /* merge pixels */
  int i;
  pen_t c[4];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = c[2] = c[3] = Machine->pens[ pal[ 1 ] ];
  for ( i = 0; i < 4; i++, ramb >>= 2 )
    dst[ 7 - i ] = c[ ramb & 3 ];
  for ( i = 0; i < 4; i++, rama >>= 2 )
    dst[ 3 - i ] = c[ rama & 3 ];
}
END_UPDATE

/* as above, but TO9 flavor */
UPDATE_HI( mode80_to9 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 6 ] ];
  for ( i = 0; i < 8; i++, ramb >>= 1 )
    dst[ 15 - i ] = c[ ramb & 1 ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[  7 - i ] = c[ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( mode80_to9 ) 
{
  /* merge pixels */
  int i;
  pen_t c[4];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = c[2] = c[3] = Machine->pens[ pal[ 6 ] ];
  for ( i = 0; i < 4; i++, ramb >>= 2 )
    dst[ 7 - i ] = c[ ramb & 3 ];
  for ( i = 0; i < 4; i++, rama >>= 2 )
    dst[ 3 - i ] = c[ rama & 3 ];
}
END_UPDATE

/* 320x200, 2-colors, two pages (untested) */
UPDATE_HI( page1 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 1 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ rama & 1 ];
  (void)ramb;
}
END_UPDATE
UPDATE_LOW( page1 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 1 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1 )
    dst[ 7 - i ] = c[ rama & 1 ];
  (void)ramb;
}
END_UPDATE

UPDATE_HI( page2 ) 
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 2 ] ];
  for ( i = 0; i < 16; i += 2, ramb >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = c[ ramb & 1 ];
  (void)rama;
}
END_UPDATE
UPDATE_LOW( page2 )
{
  int i;
  pen_t c[2];
  c[0] = Machine->pens[ pal[ 0 ] ];
  c[1] = Machine->pens[ pal[ 2 ] ];
  for ( i = 0; i < 8; i++, ramb >>= 1 )
    dst[ 7 - i ] = c[ ramb & 1 ];
  (void)rama;
}
END_UPDATE

/* 320x200, 2-colors, two overlaid pages (untested) */
UPDATE_HI( overlay ) 
{
  int i;
  pen_t c[2][2];
  c[0][0] = Machine->pens[ pal[ 0 ] ];
  c[0][1] = c[1][1] = Machine->pens[ pal[ 1 ] ];
  c[1][0] = Machine->pens[ pal[ 2 ] ];
  for ( i = 0; i < 16; i += 2, rama >>= 1, ramb >>= 1 )
    dst[ 15 - i ] =  dst[ 14 - i ] = c[ ramb & 1 ] [ rama & 1 ];
}
END_UPDATE
UPDATE_LOW( overlay ) 
{
  int i;
  pen_t c[2][2];
  c[0][0] = Machine->pens[ pal[ 0 ] ];
  c[0][1] = c[1][1] = Machine->pens[ pal[ 1 ] ];
  c[1][0] = Machine->pens[ pal[ 2 ] ];
  for ( i = 0; i < 8; i++, rama >>= 1, ramb >>= 1 )
    dst[ 7 - i ] = c[ ramb & 1 ] [ rama & 1 ];
}
END_UPDATE

/* 160x200, 4-colors, four overlaid pages (untested) */
UPDATE_HI( overlay3 ) 
{
  static const int p[2][2][2][2] = 
    { { { { 0, 1 }, { 2, 1 }, }, { { 4, 1 }, { 2, 1 } } }, 
      { { { 8, 1 }, { 2, 1 }, }, { { 4, 1 }, { 2, 1 } } } };
  int i;
  for ( i = 0; i < 16; i += 4, rama >>= 1, ramb >>= 1 )
    dst[ 15 - i ] = dst[ 14 - i ] = dst[ 13 - i ] = dst[ 12 - i ] = 
      Machine->pens[ pal[ p[ ramb & 1 ] [ (ramb >> 4) & 1 ] 
 			      [ rama & 1 ] [ (rama >> 4) & 1 ] ] ];
}
END_UPDATE
UPDATE_LOW( overlay3 ) 
{
  static const int p[2][2][2][2] = 
    { { { { 0, 1 }, { 2, 1 }, }, { { 4, 1 }, { 2, 1 } } }, 
      { { { 8, 1 }, { 2, 1 }, }, { { 4, 1 }, { 2, 1 } } } };
  int i;
  for ( i = 0; i < 8; i += 2, rama >>= 1, ramb >>= 1 )
    dst[ 7 - i ] = dst[ 6 - i ] =
      Machine->pens[ pal[ p[ ramb & 1 ] [ (ramb >> 4) & 1 ] 
 			      [ rama & 1 ] [ (rama >> 4) & 1 ] ] ];
}
END_UPDATE

#define FUN(x) { x##_scandraw_8, x##_scandraw_16 }

const thom_scandraw thom_scandraw_funcs[THOM_VMODE_NB][2] = {
  FUN(to770),    FUN(mo5),    FUN(bitmap4), FUN(bitmap4alt),  FUN(mode80),  
  FUN(bitmap16), FUN(page1),  FUN(page2),   FUN(overlay),     FUN(overlay3),
  FUN(to9), FUN(mode80_to9), 
};

/* called at the start of each scanline in the active area, just after
   left border (-1<=y<199), and also after the last scanline (y=199)
*/
void thom_scanline_start( int y )
{
  /* update active-area */
  if ( y >= 0 && 
       (thom_vstate_dirty || thom_vstate_last_dirty || thom_vmem_dirty[y]) ) {
    int x = 0;  
    while ( x < 40 ) {    
      int xx = x;
      unsigned mode = thom_vmodepage[x] >> 8;
      unsigned page = thom_vmodepage[x] & 0xff;
      assert( mode < THOM_VMODE_NB );
      assert( page < 4 );
      do { xx++; } while ( xx < 40 && thom_vmodepage[xx] == -1 );
      thom_scandraw_funcs[ mode ][ thom_hires ]
	( thom_vram + y * 40 + page * 0x4000,
	  thom_vbody + y * 320 * (thom_hires+1), thom_last_pal, x, xx-x );
      x = xx;
    }
    thom_vmem_dirty[y] = 0;
  }

  /* prepare for next scanline */
  if ( y == 199 ) timer_adjust( thom_scanline_timer, TIME_NEVER, 0, 0);
  else { 

    if ( thom_vmodepage_changed ) {
      int x, m = 0;
      for ( x = 0; x <= 40; x++ )
	if ( thom_vmodepage[x] !=-1 ) {
	  m = thom_vmodepage[x];
	  thom_vmodepage[x] = -1;
	}
      thom_vmodepage[0] = m;
      thom_vmodepage_changed = 0;
    }
    
    if ( thom_pal_changed ) {
      memcpy( thom_last_pal, thom_pal, 32 );
      thom_pal_changed = 0;
    }

    timer_adjust( thom_scanline_timer, TIME_IN_USEC(64), y + 1, 0);
  }
}


/* -------------- misc --------------- */

static UINT32 thom_mode_point;

static UINT32 thom_floppy_wcount;
static UINT32 thom_floppy_rcount;

#define FLOP_STATE (thom_floppy_wcount ? 2 : thom_floppy_rcount ? 1 : 0)

void thom_set_mode_point ( int point )
{
  assert( point >= 0 && point <= 1 );
  thom_mode_point = ( ! point ) * 0x2000;
  memory_set_bank( THOM_VRAM_BANK, ! point );
}

void thom_floppy_active ( int write )
{
  int fold = FLOP_STATE, fnew;
  /* stays up for a few frames */
  if ( write ) thom_floppy_wcount = 25; 
  else         thom_floppy_rcount = 25;
  /* update icon */
  fnew = FLOP_STATE;
  if ( fold != fnew ) output_set_value( "floppy", fnew );
}


/* -------------- main update function --------------- */

VIDEO_UPDATE ( thom )
{
  int y, ypos;
  const int scale = thom_hires ? 2 : 1;
  const int xbleft = thom_bwidth * scale;
  const int xbright = ( thom_bwidth + THOM_ACTIVE_WIDTH ) * scale;
  const int xright = ( thom_bwidth * 2 + THOM_ACTIVE_WIDTH ) * scale;
  const int xwidth = THOM_ACTIVE_WIDTH * scale;
  const int yup = THOM_BORDER_HEIGHT + THOM_ACTIVE_HEIGHT;
  const int ybot = THOM_BORDER_HEIGHT + thom_bheight + 200;
  UINT16* v = thom_vbody;
  pen_t border = Machine->pens[ 0 ];
  rectangle wrect = { 0, xright - 1, 0, 0 };
  rectangle lrect = { 0, xbleft - 1, 0, 0 };
  rectangle rrect = { xbright, xright - 1, 0, 0 };

  LOG (( "%f thom: video update called\n", timer_get_time() ));

  /* upper border */
  for ( y = 0; y < THOM_BORDER_HEIGHT - thom_bheight; y++ )
    if ( thom_border[ y ] != -1 ) border = Machine->pens[ thom_border[ y ] ];
  ypos = 0;
  while ( y < THOM_BORDER_HEIGHT ) {
    if ( thom_border[ y ] != -1 ) border = Machine->pens[ thom_border[ y ] ];
    wrect.min_y = ypos;
    do { y++; ypos += scale; }
    while ( y < THOM_BORDER_HEIGHT && thom_border[ y ] == -1 );
    wrect.max_y = ypos - 1;
    fillbitmap( bitmap,  border, &wrect );
  }

  /* left and right borders */
  while ( y < yup ) {
    if ( thom_border[ y ] != -1 ) border = Machine->pens[ thom_border[ y ] ];
    lrect.min_y = rrect.min_y = ypos;
    do { y++; ypos += scale; }
    while ( y < yup &&  thom_border[ y ] == -1 );
    lrect.max_y = rrect.max_y = ypos - 1;
    fillbitmap( bitmap, border, &lrect );
    fillbitmap( bitmap, border, &rrect );
      }

  /* lower border */
  while (y < ybot ) {
    if ( thom_border[ y ] != -1 ) border = Machine->pens[ thom_border[ y ] ];
    wrect.min_y = ypos;
    do { y++; ypos += scale; }
    while ( y < ybot && thom_border[ y ] == -1 );
    wrect.max_y = ypos - 1;
    fillbitmap( bitmap, border, &wrect );
  }

  /* body */
  ypos = thom_bheight * scale;
  for ( y = 0; y < 200; v += xwidth, y++ , ypos += scale ) {
    draw_scanline16( bitmap, xbleft, ypos, xwidth, v, NULL, -1 );
    if ( thom_hires )
      draw_scanline16( bitmap, xbleft, ypos+1, xwidth, v, NULL, -1 );
  }

	/* config */
	if ( readinputport( THOM_INPUT_CONFIG ) & 1 ) 
	{
	}
	else
	{
		draw_crosshair( bitmap, 
			readinputport ( THOM_INPUT_LIGHTPEN     ), 
			readinputport ( THOM_INPUT_LIGHTPEN + 1 ), 
			cliprect, 0 ); 
	}
 
	return 0;
}



/* -------------- frame start ------------------ */

static mame_timer *thom_init_timer;

static void (*thom_init_cb) ( int );

void thom_set_init_callback ( void (*cb) ( int init ) )
{
  LOG (( "thom_set_init_callback called\n" ));
  thom_init_cb = cb;
}

static void thom_set_init ( int init )
{
  LOG (( "%f thom_set_init: %i\n", timer_get_time(), init ));
  if ( thom_init_cb ) thom_init_cb( init );
  if ( ! init )
    timer_adjust( thom_init_timer, TIME_IN_USEC( 64 * THOM_ACTIVE_HEIGHT - 24 ),
		  1-init, 0 );
}

/* call this at the very begining of each new frame */
VIDEO_EOF ( thom )
{
  int fnew, fold = FLOP_STATE;
  int i;
  UINT16 b = 12;
  struct thom_vsignal l = thom_get_lightpen_vsignal( 0, -1, 0 );
  
  LOG (( "%f thom: video eof called\n", timer_get_time() ));

  /* floppy indicator count */
  if ( thom_floppy_wcount ) thom_floppy_wcount--;
  if ( thom_floppy_rcount ) thom_floppy_rcount--;
  fnew = FLOP_STATE;
  if ( fnew != fold ) output_set_value( "floppy", fnew );

  /* prepare state for next frame */
  for ( i = 0; i <= THOM_TOTAL_HEIGHT; i++ )
    if ( thom_border[ i ] != -1 ) {
      b = thom_border[ i ];
      thom_border[ i ] = -1;
  }
  thom_border[ 0 ] = b;
  thom_vstate_last_dirty = thom_vstate_dirty;
  thom_vstate_dirty = 0;
  

  /* schedule first init signal */
  timer_adjust( thom_init_timer, TIME_IN_USEC( 64 * THOM_BORDER_HEIGHT + 7 ), 
		0, 0 );

  /* schedule first lightpen signal */
  l.line &= ~1; /* hack (avoid lock in MO6 palette selection) */
  timer_adjust( thom_lightpen_timer,
		TIME_IN_USEC( 64 * ( THOM_BORDER_HEIGHT + l.line - 2 ) + 16 ),
		0, 0 );

  /* schedule first active-area scanline call-back */
  timer_adjust( thom_scanline_timer, TIME_IN_USEC( 64 * THOM_BORDER_HEIGHT + 7),
		-1, 0 );

  /* reset video frame time */
  mame_timer_adjust( thom_video_timer, time_zero, 0, time_never );

  /* update screen size according to user options */
  if ( thom_update_screen_size() )
    thom_vstate_dirty = 1;
}


/* -------------- initialization --------------- */

static const UINT16 thom_pal_init[16] = { 
  0x1000, /* 0: black */        0x000f, /* 1: red */
  0x00f0, /* 2: geen */         0x00ff, /* 3: yellow */
  0x0f00, /* 4: blue */         0x0f0f, /* 5: purple */
  0x0ff0, /* 6: cyan */         0x0fff, /* 7: white */
  0x0777, /* 8: gray */         0x033a, /* 9: pink */
  0x03a3, /* a: light green */  0x03aa, /* b: light yellow */
  0x0a33, /* c: light blue */   0x0a3a, /* d: redish pink */
  0x0ee7, /* e: light cyan */   0x007b, /* f: orange */
};

VIDEO_START ( thom )
{
  LOG (( "thom: video start called\n" ));

  /* scan-line state */
  memcpy( thom_last_pal, thom_pal_init, 32 );
  memcpy( thom_pal, thom_pal_init, 32 );
  memset( thom_border, 0, sizeof( thom_border ) );
  memset( thom_vbody, 0, sizeof( thom_vbody ) );
  memset( thom_vmodepage, 0, sizeof( thom_vmodepage ) );
  memset( thom_vmem_dirty, 0, sizeof( thom_vmem_dirty ) );
  thom_vmode = 0;
  thom_vpage = 0;
  thom_border_index = 0;
  thom_vstate_dirty = 1;
  thom_vstate_last_dirty = 1;
  state_save_register_global_array( thom_last_pal );
  state_save_register_global_array( thom_pal );
  state_save_register_global_array( thom_border );
  state_save_register_global_array( thom_vbody );
  state_save_register_global_array( thom_vmodepage );
  state_save_register_global_array( thom_vmem_dirty );
  state_save_register_global( thom_pal_changed );
  state_save_register_global( thom_vmodepage_changed );
  state_save_register_global( thom_vmode );
  state_save_register_global( thom_vpage );
  state_save_register_global( thom_border_index );
  state_save_register_global( thom_vstate_dirty );
  state_save_register_global( thom_vstate_last_dirty );

  thom_mode_point = 0;
  state_save_register_global( thom_mode_point );
  memory_set_bank( THOM_VRAM_BANK, 0 );

  thom_floppy_rcount = 0;
  thom_floppy_wcount = 0;
  state_save_register_global( thom_floppy_wcount );
  state_save_register_global( thom_floppy_rcount );

  thom_video_timer = mame_timer_alloc( NULL );

  thom_scanline_timer = mame_timer_alloc( thom_scanline_start );

  thom_lightpen_nb = 0;
  thom_lightpen_cb = NULL;
  thom_lightpen_timer = mame_timer_alloc( thom_lightpen_step );

  thom_init_cb = NULL;
  thom_init_timer = mame_timer_alloc( thom_set_init );
  
  video_eof_thom(machine);

  state_save_register_global( thom_bwidth );
  state_save_register_global( thom_bheight );
  state_save_register_global( thom_hires );

  return 0;
}

PALETTE_INIT ( thom )
{
  float gamma = 0.6;
  unsigned i;

  LOG (( "thom: palette init called\n" ));

  for ( i = 0; i < 4097; i++ )  {
    UINT8 r = 255. * pow( (i & 15) / 15., gamma );
    UINT8 g = 255. * pow( ((i>> 4) & 15) / 15., gamma );
    UINT8 b = 255. * pow( ((i >> 8) & 15) / 15., gamma );
    /* UINT8 alpha = i & 0x1000 ? 0 : 255;  TODO: transparency */
    palette_set_color(machine,  i, r, g, b );
  }
}


/***************************** TO7 / T9000 *************************/

/* write to video memory through addresses 0x4000-0x5fff */
WRITE8_HANDLER ( to7_vram_w )
{
  assert( offset >= 0 && offset < 0x2000 );
  /* force two topmost color bits to 1 */
  if ( thom_mode_point ) data |= 0xc0;
  if ( thom_vram[ offset + thom_mode_point ] == data ) return;
  thom_vram[ offset + thom_mode_point ] = data;
  /* dirty whole scanline */
  thom_vmem_dirty[ offset / 40 ] = 1;
}

/* bits 0-13 : latched gpl of lightpen position */
/* bit    14:  latched INIT */
/* bit    15:  latched INIL */
unsigned to7_lightpen_gpl ( int decx, int decy )
{
  int x = readinputport( THOM_INPUT_LIGHTPEN     );
  int y = readinputport( THOM_INPUT_LIGHTPEN + 1 );
  if ( thom_hires ) { x /= 2; y /= 2; }
  x -= thom_bwidth;
  y -= thom_bheight;
  if ( x < 0 || y < 0 || x >= 320 || y >= 200 ) return 0;
  x += decx;
  y += decy;
  return y*40 + x/8 + (x < 320 ? 0x4000 : 0) + 0x8000;
}



/***************************** TO7/70 ******************************/

/* write to video memory through addresses 0x4000-0x5fff (TO) 
   or 0x0000-0x1fff (MO) */
WRITE8_HANDLER ( to770_vram_w )
{
  assert( offset >= 0 && offset < 0x2000 );
  if ( thom_vram[ offset + thom_mode_point ] == data ) return;
  thom_vram[ offset + thom_mode_point ] = data;
  /* dirty whole scanline */
  thom_vmem_dirty[ offset / 40 ] = 1;
}


/***************************** TO8 ******************************/

/* write to video memory through system space (always page 1) */
WRITE8_HANDLER ( to8_sys_lo_w )
{
  UINT8* dst = thom_vram + offset + 0x6000;
  assert( offset >= 0 && offset < 0x2000 );
  if ( *dst == data ) return;
  *dst = data;
  /* dirty whole scanline */
  thom_vmem_dirty[ offset / 40 ] = 1;
}

WRITE8_HANDLER ( to8_sys_hi_w )
{
  UINT8* dst = thom_vram + offset + 0x4000;
  assert( offset >= 0 && offset < 0x2000 );
  if ( *dst == data ) return;
  *dst = data;
  /* dirty whole scanline */
  thom_vmem_dirty[ offset / 40 ] = 1;
}

/* write to video memory through data space */
WRITE8_HANDLER ( to8_data_lo_w )
{
  UINT8* dst = thom_vram + offset + 0x4000 * to8_data_vpage + 0x2000;
  assert( offset >= 0 && offset < 0x2000 );
  if ( *dst == data ) return;
  *dst = data;
  /* dirty whole scanline */
  if ( to8_data_vpage >= 4 ) return;
  thom_vmem_dirty[ offset / 40 ] = 1;
}

WRITE8_HANDLER ( to8_data_hi_w )
{
  UINT8* dst = thom_vram + offset + 0x4000 * to8_data_vpage;
  assert( offset >= 0 && offset < 0x2000 );
  if ( *dst == data ) return;
  *dst = data;
  /* dirty whole scanline */
  if ( to8_data_vpage >= 4 ) return;
  thom_vmem_dirty[ offset / 40 ] = 1;
}

/* write to video memory page through cartridge addresses space */
WRITE8_HANDLER ( to8_vcart_w )
{
  UINT8* dst = thom_vram + offset + 0x4000 * to8_cart_vpage;
  assert( offset>=0 && offset < 0x4000 );
  if ( *dst == data ) return;
  *dst = data;
  /* dirty whole scanline */
  if ( to8_cart_vpage >= 4  ) return;
  thom_vmem_dirty[ (offset & 0x1fff) / 40 ] = 1;
}
