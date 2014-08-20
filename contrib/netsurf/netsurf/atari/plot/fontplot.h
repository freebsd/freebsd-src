#ifndef FONT_PLOT_H
#define FONT_PLOT_H

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include <string.h>

#include "desktop/plot_style.h"
#include "image/bitmap.h"
#include "utils/log.h"
#include "atari/bitmap.h"
#include "atari/plot/eddi.h"
#include "atari/gui.h"
#include "atari/misc.h"
#include "atari/osspec.h"

typedef struct s_font_plotter * FONT_PLOTTER;

struct s_font_driver_table_entry
{
	const char * name;
	int (*ctor)( FONT_PLOTTER self );
	int flags;
};

/* declaration of font plotter member functions: (_fpmf_ prefix) */
typedef int (*_fpmf_str_width)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char * str, size_t length, int * width);
typedef int (*_fpmf_str_split)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
typedef int (*_fpmf_pixel_pos)( FONT_PLOTTER self, const plot_font_style_t *fstyle,
						const char *string, size_t length,
						int x, size_t *char_offset, int *actual_x);
typedef int (*_fpmf_text)( FONT_PLOTTER self, int x, int y, const char *text,
													size_t length, const plot_font_style_t *fstyle);

typedef void (*_fpmf_draw_glyph)(FONT_PLOTTER self, GRECT * clip, GRECT * loc,
								uint8_t * pixdata, int pitch, uint32_t colour);
typedef int (*_fpmf_dtor)( FONT_PLOTTER self );


/* prototype of the font plotter "object" */
struct s_font_plotter
{
	char * name;
	int flags;
	int vdi_handle;
	void * priv_data;

	_fpmf_str_width str_width;
	_fpmf_str_split str_split;
	_fpmf_pixel_pos pixel_pos;
	_fpmf_text text;
	_fpmf_draw_glyph draw_glyph;
	_fpmf_dtor dtor;
};


FONT_PLOTTER plot_get_text_plotter(void);
/* Set the font plotting engine. 
*/
void plot_set_text_plotter(FONT_PLOTTER font_plotter);
void dump_font_drivers(void);
FONT_PLOTTER new_font_plotter( int vdihandle, char * name, unsigned long flags,
		int * error);
int delete_font_plotter( FONT_PLOTTER p );

#ifdef WITH_VDI_FONT_DRIVER
 #include "atari/plot/font_vdi.h"
#endif
#ifdef WITH_INTERNAL_FONT_DRIVER
 #include "atari/plot/font_internal.h"
#endif
#ifdef WITH_FREETYPE_FONT_DRIVER
 #include "atari/plot/font_freetype.h"
#endif



#endif
