/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Comparison macros to emulate LSBFIRST comparison results of network
 * byte-order quantities
 *
 *	from: lsb_addr_comp.h,v 4.0 89/01/23 15:44:46 jtkohl Exp $
 *	$FreeBSD$
 */

#ifndef LSB_ADDR_COMP_DEFS
#define LSB_ADDR_COMP_DEFS

#include "osconf.h"

#ifdef LSBFIRST
#define lsb_net_ulong_less(x,y) ((x < y) ? -1 : ((x > y) ? 1 : 0))
#define lsb_net_ushort_less(x,y) ((x < y) ? -1 : ((x > y) ? 1 : 0))
#else
/* MSBFIRST */
#define u_char_comp(x,y) \
        (((x)>(y))?(1):(((x)==(y))?(0):(-1)))
/* This is gross, but... */
#define lsb_net_ulong_less(x, y) long_less_than((u_char *)&x, (u_char *)&y)
#define lsb_net_ushort_less(x, y) short_less_than((u_char *)&x, (u_char *)&y)

#define long_less_than(x,y) \
        (u_char_comp((x)[3],(y)[3])?u_char_comp((x)[3],(y)[3]): \
	 (u_char_comp((x)[2],(y)[2])?u_char_comp((x)[2],(y)[2]): \
	  (u_char_comp((x)[1],(y)[1])?u_char_comp((x)[1],(y)[1]): \
	   (u_char_comp((x)[0],(y)[0])))))
#define short_less_than(x,y) \
	  (u_char_comp((x)[1],(y)[1])?u_char_comp((x)[1],(y)[1]): \
	   (u_char_comp((x)[0],(y)[0])))

#endif /* LSBFIRST */

#endif /*  LSB_ADDR_COMP_DEFS */
