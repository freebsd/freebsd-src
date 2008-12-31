/*-
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
 *	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ddb/db_access.c,v 1.20.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/kdb.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>

/*
 * Access unaligned data items on aligned (longword)
 * boundaries.
 */

static unsigned db_extend[] = {	/* table for sign-extending */
	0,
	0xFFFFFF80U,
	0xFFFF8000U,
	0xFF800000U
};

#ifndef BYTE_MSF
#define	BYTE_MSF	0
#endif

db_expr_t
db_get_value(addr, size, is_signed)
	db_addr_t	addr;
	register int	size;
	boolean_t	is_signed;
{
	char		data[sizeof(u_int64_t)];
	register db_expr_t value;
	register int	i;

	if (db_read_bytes(addr, size, data) != 0) {
		db_printf("*** error reading from address %llx ***\n",
		    (long long)addr);
		kdb_reenter();
	}

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

	if (db_write_bytes(addr, size, data) != 0) {
		db_printf("*** error writing to address %llx ***\n",
		    (long long)addr);
		kdb_reenter();
	}
}
