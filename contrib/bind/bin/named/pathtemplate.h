/*
 *	$Id: pathtemplate.h,v 8.6 2000/04/21 06:54:15 vixie Exp $
 */

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

#include <paths.h>

#ifndef _PATH_CONF
#define _PATH_CONF	"%DESTETC%/named.conf"
#endif

#ifndef _PATH_DEBUG
#define _PATH_DEBUG	"named.run"
#endif

#ifndef _PATH_DUMPFILE
#define _PATH_DUMPFILE	"named_dump.db"
#endif

#ifndef _PATH_NAMED
#define _PATH_NAMED	"%DESTSBIN%/named"
#endif

#ifndef _PATH_PIDFILE
#define _PATH_PIDFILE	"%DESTRUN%/named.pid"
#endif

#ifndef _PATH_NDCSOCK
#ifdef NEED_SECURE_DIRECTORY
#define _PATH_NDCSOCK	"%DESTRUN%/ndc.d/ndc"
#else
#define _PATH_NDCSOCK	"%DESTRUN%/ndc"
#endif
#endif

#ifndef _PATH_STATS
#define _PATH_STATS	"named.stats"
#endif

#ifndef _PATH_MEMSTATS
#define _PATH_MEMSTATS	"named.memstats"
#endif

#ifndef _PATH_TMPXFER
#define _PATH_TMPXFER	"xfer.ddt.XXXXXX"
#endif

#ifndef _PATH_XFER
#define _PATH_XFER	"%DESTEXEC%/named-xfer"
#endif

#ifndef _PATH_XFERTRACE
#define _PATH_XFERTRACE	"xfer.trace"
#endif

#ifndef _PATH_XFERDDT
#define _PATH_XFERDDT	"xfer.ddt"
#endif

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL	"/dev/null"
#endif
