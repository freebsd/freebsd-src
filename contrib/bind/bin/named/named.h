/*
 * Copyright (c) 1996-2000 by Internet Software Consortium.
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
 * $Id: named.h,v 8.31 2002/02/01 00:05:38 marka Exp $
 */

/* Options. Change them at your peril. */
#define DEBUG
#define ADDAUTH
#define STUBS
#define RETURNSOA
#define BOGUSNS
#define TRACEROOT
#define XFRNETS
#define QRYLOG
#undef YPKLUDGE
#define	RENICE
#define BIND_IXFR
#define BIND_NOTIFY
#define BIND_UPDATE
#define WANT_PIDFILE
#define FWD_LOOP
#define DOTTED_SERIAL
#define SENSIBLE_DOTS
#define ROUND_ROBIN
#define DNS_SECURITY
#undef RSAREF
#undef BSAFE
#define ALLOW_LONG_TXT_RDATA
#define STRICT_RFC2308
#undef BIND_ZXFR
#undef LOG_TSIG_BUG
#define NOADDITIONAL
#undef FORWARD_ALLOWS	/* enable allow-query for forward zones. */

#include <isc/assertions.h>
#include <isc/list.h>
#include <isc/ctl.h>

#include <res_update.h>

#include "pathnames.h"

#include "ns_defs.h"
#include "db_defs.h"

#include "ns_glob.h"
#include "db_glob.h"

#include "ns_func.h"
#include "db_func.h"
