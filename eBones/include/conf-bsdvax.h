/*
 * Copyright 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 * Machine-type definitions: VAX
 *
 *	from: conf-bsdvax.h,v 4.0 89/01/23 09:58:12 jtkohl Exp $
 *	$Id: conf-bsdvax.h,v 1.2 1994/07/19 19:22:57 g89r4222 Exp $
 */

#define VAX
#define BITS32
#define BIG
#define LSBFIRST
#define BSDUNIX

#ifndef __STDC__
#ifndef NOASM
#define VAXASM
#endif /* no assembly */
#endif /* standard C */
