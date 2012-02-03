/*
 * Copyright (C) 2008  Internet Systems Consortium, Inc. ("ISC")
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: statschannel.h,v 1.3 2008-04-03 05:55:51 marka Exp $ */

#ifndef NAMED_STATSCHANNEL_H
#define NAMED_STATSCHANNEL_H 1

/*! \file
 * \brief
 * The statistics channels built-in the name server.
 */

#include <isccc/types.h>

#include <isccfg/aclconf.h>

#include <named/types.h>

#define NS_STATSCHANNEL_HTTPPORT		80

isc_result_t
ns_statschannels_configure(ns_server_t *server, const cfg_obj_t *config,
			   cfg_aclconfctx_t *aclconfctx);
/*%<
 * [Re]configure the statistics channels.
 *
 * If it is no longer there but was previously configured, destroy
 * it here.
 *
 * If the IP address or port has changed, destroy the old server
 * and create a new one.
 */


void
ns_statschannels_shutdown(ns_server_t *server);
/*%<
 * Initiate shutdown of all the statistics channel listeners.
 */

isc_result_t
ns_stats_dump(ns_server_t *server, FILE *fp);
/*%<
 * Dump statistics counters managed by the server to the file fp.
 */

#endif	/* NAMED_STATSCHANNEL_H */
