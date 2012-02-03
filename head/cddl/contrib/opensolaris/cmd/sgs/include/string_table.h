/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2008 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

#ifndef	_STRING_TABLE_DOT_H
#define	_STRING_TABLE_DOT_H

#pragma ident	"%Z%%M%	%I%	%E% SMI"

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Exported, opaque string table handle.
 */
typedef struct str_tbl	Str_tbl;

/*
 * Exported string table functions.
 */
extern int		st_delstring(Str_tbl *, const char *);
extern void		st_destroy(Str_tbl *);
extern size_t		st_getstrtab_sz(Str_tbl *);
extern const char	*st_getstrbuf(Str_tbl *);
extern int		st_insert(Str_tbl *, const char *);
extern Str_tbl		*st_new(uint_t);
extern int		st_setstrbuf(Str_tbl *, char *, size_t);
extern int		st_setstring(Str_tbl *, const char *, size_t *);

/*
 * Exported flags values for st_new().
 */
#define	FLG_STNEW_COMPRESS	0x01	/* compressed string table */

#ifdef __cplusplus
}
#endif

#endif /* _STRING_TABLE_DOT_H */
