/*
 *  Copyright (C) 1994 Geoffrey M. Rehmet
 *
 *  This program is free software; you may redistribute it and/or 
 *  modify it, provided that it retain the above copyright notice 
 *  and the following disclaimer.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
 *
 *	Geoff Rehmet, Rhodes University, South Africa <csgr@cs.ru.ac.za>
 *
 */

#ifndef _LPT_PRINTER_H_
#define _LPT_PRINTER_H_

#include <sys/types.h>
#include <sys/ioctl.h>

#define	LPT_IRQ		_IOW('p', 1, long)	/* set interrupt status */

#endif
