/* flonum_const.c - Useful Flonum constants
   Copyright (C) 1987, 1990, 1991, 1992 Free Software Foundation, Inc.

   This file is part of GAS, the GNU Assembler.

   GAS is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   GAS is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with GAS; see the file COPYING.  If not, write to
   the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.  */

#ifndef lint
static char rcsid[] = "$FreeBSD: src/gnu/usr.bin/as/flo-const.c,v 1.5 1999/08/27 23:34:14 peter Exp $";
#endif

#include "flonum.h"
/* JF:  I added the last entry to this table, and I'm not
   sure if its right or not.  Could go either way.  I wish
   I really understood this stuff. */


const int table_size_of_flonum_powers_of_ten = 11;

static const LITTLENUM_TYPE zero[] = {     1 };

/***********************************************************************\
 *									*
 *	Warning: the low order bits may be WRONG here.			*
 *	I took this from a suspect bc(1) script.			*
 *	"minus_X"[] is supposed to be 10^(2^-X) expressed in base 2^16.	*
 *	The radix point is just AFTER the highest element of the []	*
 *									*
 *	Because bc rounds DOWN for printing (I think), the lowest	*
 *	significance littlenums should probably have 1 added to them.	*
 *									*
 \***********************************************************************/

/* JF:  If this equals 6553/(2^16)+39321/(2^32)+...  it approaches .1 */
static const LITTLENUM_TYPE minus_1[] = {
	39321, 39321, 39321, 39321, 39321, 39321, 39321, 39321, 39321, 39321,
	39321, 39321, 39321, 39321, 39321, 39321, 39321, 39321, 39321,  6553 };
static const LITTLENUM_TYPE plus_1[] = {    10                             };

/* JF:  If this equals 655/(2^16) + 23592/(2^32) + ... it approaches .01 */
static const LITTLENUM_TYPE minus_2[] = {
	10485, 36700, 62914, 23592, 49807, 10485, 36700, 62914, 23592, 49807,
	10485, 36700, 62914, 23592, 49807, 10485, 36700, 62914, 23592,   655 };
static const LITTLENUM_TYPE plus_2[] = {   100                             };

/* This approaches .0001 */
static const LITTLENUM_TYPE minus_3[] = {
	52533, 20027, 37329, 65116, 64067, 60397, 14784, 18979, 33659, 19503,
	2726,  9542,   629,  2202, 40475, 10590,  4299, 47815, 36280,     6 };
static const LITTLENUM_TYPE plus_3[] = { 10000                             };

/* JF: this approaches 1e-8 */
static const LITTLENUM_TYPE minus_4[] = {
	22516, 49501, 54293, 19424, 60699,  6716, 24348, 22618, 23904, 21327,
	3919, 44703, 19149, 28803, 48959,  6259, 50273, 62237,    42        };
/* This equals 1525 * 2^16 + 57600 */
static const LITTLENUM_TYPE plus_4[] = { 57600,  1525                      };

/* This approaches 1e-16 */
static const LITTLENUM_TYPE minus_5[] = {
	22199, 45957, 17005, 26266, 10526, 16260, 55017, 35680, 40443, 19789,
	17356, 30195, 55905, 28426, 63010, 44197,  1844                      };
static const LITTLENUM_TYPE plus_5[] = { 28609, 34546,    35               };

static const LITTLENUM_TYPE minus_6[] = {
	30926, 26518, 13110, 43018, 54982, 48258, 24658, 15209, 63366, 11929,
	20069, 43857, 60487,    51                                           };
static const LITTLENUM_TYPE plus_6[] = { 61313, 34220, 16731, 11629,  1262 };

static const LITTLENUM_TYPE minus_7[] = {
	29819, 14733, 21490, 40602, 31315, 65186,  2695                      };
static const LITTLENUM_TYPE plus_7[] = {
	7937, 49002, 60772, 28216, 38893, 55975, 63988, 59711, 20227,    24 };

static const LITTLENUM_TYPE minus_8[] = {
	45849, 19069, 18068, 36324, 37948, 48745, 10873, 64360, 15961, 20566,
	24178, 15922, 59427,   110                                           };
static const LITTLENUM_TYPE plus_8[] = {
	15873, 11925, 39177,   991, 14589, 19735, 25347, 65086, 53853,  938,
	37209, 47086, 33626, 23253, 32586, 42547,  9731, 59679,  590         };

static const LITTLENUM_TYPE minus_9[] = {
	63601, 55221, 43562, 33661, 29067, 28203, 65417, 64352, 22462, 41110,
	12570, 28635, 23199, 50572, 28471, 27074, 46375, 64028, 13106, 63700,
	32698, 17493, 32420, 34382, 22750, 20681, 12300                      };
static const LITTLENUM_TYPE plus_9[] = {
	63564, 61556, 29377, 54467, 18621, 28141, 36415, 61241, 47119, 30026,
	19740, 46002, 13541, 61413, 30480, 38664, 32205, 50593, 51112, 48904,
	48263, 43814,   286, 30826, 52813, 62575, 61390, 24540, 21495,     5 };

static const LITTLENUM_TYPE minus_10[] = {
	50313, 34681,  1464, 25889, 19575, 41125, 17635,  4598, 49708, 13427,
	17287, 56115, 53783, 38255, 32415, 17778, 31596,  7557, 20951, 18477,
	40353,  1178, 44405, 11837, 11571, 50963, 15649, 11698, 40675,  2308,  };
static const LITTLENUM_TYPE plus_10[] = {
	18520, 53764, 54535, 61910, 61962, 59843, 46270, 58053, 12473, 63785,
	2449, 43230, 50044, 47595, 10403, 35766, 32607,  1124, 24966, 35044,
	25524, 23631, 18826, 14518, 58448, 14562, 49618,  5588, 25396,    28 };

static const LITTLENUM_TYPE minus_11[] = {
	6223, 59909, 62437, 59960, 14652, 45336, 48800,  7647, 51962, 37982,
	60436, 58176, 26767,  8440,  9831, 48556, 20994, 14148,  6757, 17221,
	60624, 46129, 53210, 44085, 54016, 24259, 11232, 21229, 21313,    81,  };
static const LITTLENUM_TYPE plus_11[] = {
	36159,  2055, 33615, 61362, 23581, 62454,  9748, 15275, 39284, 58636,
	16269, 42793, 47240, 45774, 50861, 48400,  9413, 40281,  4030,  9572,
	7984, 33038, 59522, 19450, 40593, 24486, 54320,  6661, 55766,   805,  };

/* Shut up complaints about differing pointer types.  They only differ
   in the const attribute, but there isn't any easy way to do this
   */
#define X (LITTLENUM_TYPE *)

const FLONUM_TYPE flonum_negative_powers_of_ten[] = {
	{X zero,	X zero,		X zero,		  0, '+'},
	{X minus_1,	X minus_1 +19,	X minus_1  + 19, -20, '+'},
	{X minus_2,	X minus_2 +19,	X minus_2  + 19, -20, '+'},
	{X minus_3,	X minus_3 +19,	X minus_3  + 19, -20, '+'},
	{X minus_4,	X minus_4 +18,	X minus_4  + 18, -20, '+'},
	{X minus_5,	X minus_5 +16,	X minus_5  + 16, -20, '+'},
	{X minus_6,	X minus_6 +13,	X minus_6  + 13, -20, '+'},
	{X minus_7,	X minus_7 + 6,	X minus_7  +  6, -20, '+'},
	{X minus_8,	X minus_8 +13,	X minus_8  + 13, -40, '+'},
	{X minus_9,	X minus_9 +26,	X minus_9  + 26, -80, '+'},
	{X minus_10,	X minus_10+29,	X minus_10 + 29,-136, '+'},
	{X minus_11,	X minus_11+29,	X minus_11 + 29,-242, '+'},
};

const FLONUM_TYPE flonum_positive_powers_of_ten[] = {
	{X zero,	X zero,		X zero,		  0, '+'},
	{X plus_1,	X plus_1  +  0,	X plus_1  +  0,	  0, '+'},
	{X plus_2,	X plus_2  +  0,	X plus_2  +  0,	  0, '+'},
	{X plus_3,	X plus_3  +  0,	X plus_3  +  0,	  0, '+'},
	{X plus_4,	X plus_4  +  1,	X plus_4  +  1,	  0, '+'},
	{X plus_5,	X plus_5  +  2,	X plus_5  +  2,	  1, '+'},
	{X plus_6,	X plus_6  +  4,	X plus_6  +  4,	  2, '+'},
	{X plus_7,	X plus_7  +  9,	X plus_7  +  9,	  4, '+'},
	{X plus_8,	X plus_8  + 18,	X plus_8  + 18,	  8, '+'},
	{X plus_9,	X plus_9  + 29,	X plus_9  + 29,	 24, '+'},
	{X plus_10,	X plus_10 + 29,	X plus_10 + 29,	 77, '+'},
	{X plus_11,	X plus_11 + 29,	X plus_11 + 29,	183, '+'},
};

#ifdef HO_VMS
dummy1()
{
}
#endif
/* end of flonum_const.c */
