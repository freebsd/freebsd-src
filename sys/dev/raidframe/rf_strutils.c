/*	$NetBSD: rf_strutils.c,v 1.3 1999/02/05 00:06:18 oster Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*
 * rf_strutils.c
 *
 * String-parsing funcs
 */
/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Mark Holland
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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
 * rf_strutils.c -- some simple utilities for munging on strings.
 * I put them in a file by themselves because they're needed in
 * setconfig, in the user-level driver, and in the kernel.
 *
 */

#include <dev/raidframe/rf_utils.h>

/* finds a non-white character in the line */
char   *
rf_find_non_white(char *p)
{
	for (; *p != '\0' && (*p == ' ' || *p == '\t'); p++);
	return (p);
}
/* finds a white character in the line */
char   *
rf_find_white(char *p)
{
	for (; *p != '\0' && (*p != ' ' && *p != '\t'); p++);
	return (p);
}
