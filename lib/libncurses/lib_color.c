/* This work is copyrighted. See COPYRIGHT.NEW for                   *
*  details. If they are missing then this copy is in violation of    *
*  the copyright conditions.                                        */

/* lib_color.c 
 *  
 * Handles color emulation of SYS V curses
 *
 */

#include <stdlib.h>
#include "curses.priv.h"
#include "terminfo.h"

int COLOR_PAIRS;
int COLORS;
unsigned char *color_pairs;

int start_color()
{
	T(("start_color() called."));

	if (orig_pair != NULL)
		putp(orig_pair);
	else return ERR;
	if (max_pairs != -1)
		COLOR_PAIRS = max_pairs;
	else return ERR;
	color_pairs = malloc(max_pairs);
	if (max_colors != -1)
		COLORS = max_colors;
	else return ERR;
	SP->_coloron = 1;

	T(("started color: COLORS = %d, COLOR_PAIRS = %d", COLORS, COLOR_PAIRS));

	return OK;
}

int init_pair(short pair, short f, short b)
{
	T(("init_pair( %d, %d, %d )", pair, f, b));

	if ((pair < 1) || (pair >= COLOR_PAIRS))
		return ERR;
	if ((f  < 0) || (f >= COLORS) || (b < 0) || (b >= COLORS))
		return ERR;

	/* still to do:
	   if pair was initialized before a screen update is performed
	   replacing original pair colors with the new ones
	*/

	color_pairs[pair] = ( (f & 0x0f) | (b & 0x0f) << 4 );

	return OK;
}

int init_color(short color, short r, short g, short b)
{
	if (initialize_color != NULL) {
		if (color < 0 || color >= COLORS)
			return ERR;
		if (hue_lightness_saturation == TRUE)
			if (r < 0 || r > 360 || g < 0 || g > 100 || b < 0 || b > 100)
				return ERR;	
		if (hue_lightness_saturation == FALSE)
			if (r < 0 || r > 1000 || g < 0 ||  g > 1000 || b < 0 || b > 1000)
				return ERR;
				
		putp(tparm(initialize_color, color, r, g, b));
		return OK;
	}
	
	return ERR;
}

bool can_change_color()
{
	return can_change;
}

int has_colors()
{
	return ((orig_pair != NULL) && (max_colors != -1) && (max_pairs != -1)
		&& 
		(((set_foreground != NULL) && (set_background != NULL)) ||
		((set_a_foreground != NULL) && (set_a_background != NULL)))
		);
}

int color_content(short color, short *r, short *g, short *b)
{
	return ERR;
}

int pair_content(short pair, short *f, short *b)
{

	if ((pair < 1) || (pair > COLOR_PAIRS))
		return ERR;
	*f = color_pairs[pair] & 0x0f;
	*b = color_pairs[pair] & 0xf0;
	*b >>= 4;
	return OK;
}

