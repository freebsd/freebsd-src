/*
 * Copyright (c) 1996, 1997 by Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
 * ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
 * CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * $Id: named.h,v 8.12 1997/12/04 06:52:27 halley Exp $
 */

/* Options. Leave these on. */
#define DEBUG
#define ADDAUTH
#define STUBS
#define RETURNSOA
#define BOGUSNS
#define TRACEROOT
#define XFRNETS
#define QRYLOG
#define YPKLUDGE
#define	RENICE
#define FORCED_RELOAD
#define SLAVE_FORWARD
#define BIND_UPDATE
#define BIND_NOTIFY
#define WANT_PIDFILE
#define FWD_LOOP
#define DOTTED_SERIAL
#define SENSIBLE_DOTS
#define ROUND_ROBIN
#define SORT_RESPONSE
#define DNS_SECURITY
#undef RSAREF
#undef BSAFE
#define ALLOW_LONG_TXT_RDATA

#if 0
#define	strdup	PLEASE_USE_SAVESTR
#define	malloc	PLEASE_USE_DB_MEMGET
#define	calloc	PLEASE_USE_DB_MEMGET
#define	realloc PLEASE_USE_DB_MEMGET
#define free	PLEASE_USE_DB_MEMPUT
#endif

#include <isc/assertions.h>
#include <isc/list.h>

#include "pathnames.h"

#include "ns_defs.h"
#include "db_defs.h"

#include "ns_glob.h"
#include "db_glob.h"

#include "ns_func.h"
#include "db_func.h"
