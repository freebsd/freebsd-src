/******************************************************************************
 *
 *	(C)Copyright 1998,1999 SysKonnect,
 *	a business unit of Schneider & Koch & Co. Datensysteme GmbH.
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	The information in this file is provided "AS IS" without warranty.
 *
 ******************************************************************************/

#include	<linux/types.h>
/*
	----------------------
	Basic SMT system types
	----------------------
*/
#ifndef _TYPES_
#define	_TYPES_

#define _packed
#ifndef far
#define far
#endif
#ifndef _far
#define _far
#endif

#ifndef MEM_MAPPED_IO // "normal" IO
#define inp(p)  inb(p)
#define inpw(p)	inw(p)
#define inpd(p) inl(p)
#define outp(p,c)  outb(c,p)
#define outpw(p,s) outw(s,p)
#define outpd(p,l) outl(l,p)
#else // memory mapped io
#define inp(a)		readb(a)
#define inpw(a)		readw(a)
#define inpd(a)		readl(a)
#define outp(a,v)	writeb(v, a)	
#define outpw(a,v)	writew(v, a)	
#define outpd(a,v)	writel(v, a)	
#endif

#endif	/* _TYPES_ */
