#ifndef __PPC_DIV64
#define __PPC_DIV64

/* Copyright 2001 PPC64 Team, IBM Corp
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#define do_div(n,base) ({ \
	int __res; \
	__res = ((unsigned long) (n)) % (unsigned) (base); \
	(n) = ((unsigned long) (n)) / (unsigned) (base); \
	__res; })

#endif
