/* 
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */
/*
 * HISTORY
 * $Log: db_access.c,v $
 * Revision 1.1  1992/03/25  21:44:50  pace
 * Initial revision
 *
 * Revision 2.3  91/02/05  17:05:44  mrt
 * 	Changed to new Mach copyright
 * 	[91/01/31  16:16:22  mrt]
 * 
 * Revision 2.2  90/08/27  21:48:20  dbg
 * 	Fix type declarations.
 * 	[90/08/07            dbg]
 * 	Created.
 * 	[90/07/25            dbg]
 * 
 */
/*
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */
#include "param.h"
#include "proc.h"
#include <machine/db_machdep.h>		/* type definitions */

/*
 * Access unaligned data items on aligned (longword)
 * boundaries.
 */

extern void	db_read_bytes();	/* machine-dependent */
extern void	db_write_bytes();	/* machine-dependent */

int db_extend[] = {	/* table for sign-extending */
	0,
	0xFFFFFF80,
	0xFFFF8000,
	0xFF800000
};

db_expr_t
db_get_value(addr, size, is_signed)
	db_addr_t	addr;
	register int	size;
	boolean_t	is_signed;
{
	char		data[sizeof(int)];
	register db_expr_t value;
	register int	i;

	db_read_bytes(addr, size, data);

	value = 0;
#if	BYTE_MSF
	for (i = 0; i < size; i++)
#else	/* BYTE_LSF */
	for (i = size - 1; i >= 0; i--)
#endif
	{
	    value = (value << 8) + (data[i] & 0xFF);
	}
	    
	if (size < 4) {
	    if (is_signed && (value & db_extend[size]) != 0)
		value |= db_extend[size];
	}
	return (value);
}

void
db_put_value(addr, size, value)
	db_addr_t	addr;
	register int	size;
	register db_expr_t value;
{
	char		data[sizeof(int)];
	register int	i;

#if	BYTE_MSF
	for (i = size - 1; i >= 0; i--)
#else	/* BYTE_LSF */
	for (i = 0; i < size; i++)
#endif
	{
	    data[i] = value & 0xFF;
	    value >>= 8;
	}

	db_write_bytes(addr, size, data);
}

