/* ctype.c */

/* This file contains the tables and initialization function for elvis'
 * version of <ctype.h>.  It should be portable.
 */

#include "config.h"
#include "ctype.h"

void _ct_init P_((uchar *));

uchar	_ct_toupper[256];
uchar	_ct_tolower[256];
uchar	_ct_ctypes[256];

/* This function initializes the tables used by the ctype macros.  It should
 * be called at the start of the program.  It can be called again anytime you
 * wish to change the non-standard "flipcase" list.  The "flipcase" list is
 * a string of characters which are taken to be lowercase/uppercase pairs.
 * If you don't want to use any special flipcase characters, then pass an
 * empty string.
 */
void _ct_init(flipcase)
	uchar	*flipcase;	/* list of non-standard lower/upper letter pairs */
{
	int	i;
	uchar	*scan;

	/* reset all of the tables */
	for (i = 0; i < 256; i++)
	{
		_ct_toupper[i] = _ct_tolower[i] = i;
		_ct_ctypes[i] = 0;
	}

	/* add the digits */
	for (scan = (uchar *)"0123456789"; *scan; scan++)
	{
		_ct_ctypes[*scan] |= _CT_DIGIT | _CT_ALNUM;
	}

	/* add the whitespace */
	for (scan = (uchar *)" \t\n\r\f"; *scan; scan++)
	{
		_ct_ctypes[*scan] |= _CT_SPACE;
	}

	/* add the standard ASCII letters */
	for (scan = (uchar *)"aAbBcCdDeEfFgGhHiIjJkKlLmMnNoOpPqQrRsStTuUvVwWxXyYzZ"; *scan; scan += 2)
	{
		_ct_ctypes[scan[0]] |= _CT_LOWER | _CT_ALNUM;
		_ct_ctypes[scan[1]] |= _CT_UPPER | _CT_ALNUM;
		_ct_toupper[scan[0]] = scan[1];
		_ct_tolower[scan[1]] = scan[0];
	}

	/* add the flipcase letters */
	for (scan = flipcase; scan[0] && scan[1]; scan += 2)
	{
		_ct_ctypes[scan[0]] |= _CT_LOWER | _CT_ALNUM;
		_ct_ctypes[scan[1]] |= _CT_UPPER | _CT_ALNUM;
		_ct_toupper[scan[0]] = scan[1];
		_ct_tolower[scan[1]] = scan[0];
	}

	/* include '_' in the isalnum() list */
	_ct_ctypes[UCHAR('_')] |= _CT_ALNUM;

	/* !!! find the control characters in an ASCII-dependent way */
	for (i = 0; i < ' '; i++)
	{
		_ct_ctypes[i] |= _CT_CNTRL;
	}
	_ct_ctypes[127] |= _CT_CNTRL;
	_ct_ctypes[255] |= _CT_CNTRL;
}
