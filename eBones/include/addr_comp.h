/*
 * Copyright 1987, 1988, 1989 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Include file for address comparison macros.
 *
 *	from: addr_comp.h,v 4.0 89/01/23 09:57:44 jtkohl Exp $
 *	$FreeBSD$
 */

#ifndef ADDR_COMP_DEFS
#define ADDR_COMP_DEFS

/*
** Look boys and girls, a big kludge
** We need to compare the two internet addresses in network byte order, not
**   local byte order.  This is a *really really slow way of doing that*
** But.....
**         .....it works
** so we run with it
**
** long_less_than gets fed two (u_char *)'s....
*/

#define u_char_comp(x,y) \
        (((x)>(y))?(1):(((x)==(y))?(0):(-1)))

#define long_less_than(x,y) \
        (u_char_comp((x)[0],(y)[0])?u_char_comp((x)[0],(y)[0]): \
	 (u_char_comp((x)[1],(y)[1])?u_char_comp((x)[1],(y)[1]): \
	  (u_char_comp((x)[2],(y)[2])?u_char_comp((x)[2],(y)[2]): \
	   (u_char_comp((x)[3],(y)[3])))))

#endif /* ADDR_COMP_DEFS */
