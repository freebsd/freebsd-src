/*
 * Copyright (c) 1996 by Internet Software Consortium.
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

#if !defined(LINT) && !defined(CODECENTER)
static char rcsid[] = "$Id: getservent.c,v 1.10 1997/12/04 04:57:54 halley Exp $";
#endif

/* Imports */

#include "port_before.h"

#include <sys/types.h>

#include <errno.h>
#include <stdio.h>

#include <irs.h>

#include "port_after.h"

#include "irs_data.h"

/* Forward */

static struct irs_sv *	init(void);

/* Public */

struct servent *
getservent(void) {
	struct irs_sv *sv = init();
	
	if (!sv)
		return (NULL);
	net_data.sv_last = (*sv->next)(sv);
	return (net_data.sv_last);
}

struct servent *
getservbyname(const char *name, const char *proto) {
	struct irs_sv *sv = init();
	char **sap;
	
	if (!sv)
		return (NULL);
	if (net_data.sv_stayopen && net_data.sv_last)
		if (!proto || !strcmp(net_data.sv_last->s_proto,proto)) {
			if (!strcmp(net_data.sv_last->s_name, name))
				return (net_data.sv_last);
			for (sap = net_data.sv_last->s_aliases;
			     sap && *sap; sap++) 
				if (!strcmp(name, *sap))
					return (net_data.sv_last);
		}
	net_data.sv_last = (*sv->byname)(sv, name, proto);
	if (!net_data.sv_stayopen)
		endservent();
	return (net_data.sv_last);
}

struct servent *
getservbyport(int port, const char *proto) {
	struct irs_sv *sv = init();
	
	if (!sv)
		return (NULL);
	if (net_data.sv_stayopen && net_data.sv_last)
		if (port == net_data.sv_last->s_port && 
		    ( !proto || 
		     !strcmp(net_data.sv_last->s_proto, proto)))
			return (net_data.sv_last);
	net_data.sv_last = (*sv->byport)(sv, port,proto);
	return (net_data.sv_last);
}

void
setservent(int stayopen) {
	struct irs_sv *sv = init();

	if (!sv)
		return;
	(*sv->rewind)(sv);
	net_data.sv_stayopen = (stayopen != 0);
}

void
endservent() {
	struct irs_sv *sv = init();

	if (sv != NULL)
		(*sv->minimize)(sv);
}

/* Private */

static struct irs_sv *
init() {
	if (!net_data_init())
		goto error;
	if (!net_data.sv)
		net_data.sv = (*net_data.irs->sv_map)(net_data.irs);
	if (!net_data.sv) {
 error:		
		errno = EIO;
		return (NULL);
	}
	return (net_data.sv);
}
