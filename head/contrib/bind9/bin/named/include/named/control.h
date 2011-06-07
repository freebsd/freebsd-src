/*
 * Copyright (C) 2004-2007  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
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

/* $Id: control.h,v 1.25 2007-06-19 23:46:59 tbox Exp $ */

#ifndef NAMED_CONTROL_H
#define NAMED_CONTROL_H 1

/*! \file
 * \brief
 * The name server command channel.
 */

#include <isccc/types.h>

#include <isccfg/aclconf.h>

#include <named/types.h>

#define NS_CONTROL_PORT			953

#define NS_COMMAND_STOP		"stop"
#define NS_COMMAND_HALT		"halt"
#define NS_COMMAND_RELOAD	"reload"
#define NS_COMMAND_RECONFIG	"reconfig"
#define NS_COMMAND_REFRESH	"refresh"
#define NS_COMMAND_RETRANSFER	"retransfer"
#define NS_COMMAND_DUMPSTATS	"stats"
#define NS_COMMAND_QUERYLOG	"querylog"
#define NS_COMMAND_DUMPDB	"dumpdb"
#define NS_COMMAND_TRACE	"trace"
#define NS_COMMAND_NOTRACE	"notrace"
#define NS_COMMAND_FLUSH	"flush"
#define NS_COMMAND_FLUSHNAME	"flushname"
#define NS_COMMAND_STATUS	"status"
#define NS_COMMAND_TSIGLIST	"tsig-list"
#define NS_COMMAND_TSIGDELETE	"tsig-delete"
#define NS_COMMAND_FREEZE	"freeze"
#define NS_COMMAND_UNFREEZE	"unfreeze"
#define NS_COMMAND_THAW		"thaw"
#define NS_COMMAND_TIMERPOKE	"timerpoke"
#define NS_COMMAND_RECURSING	"recursing"
#define NS_COMMAND_NULL		"null"
#define NS_COMMAND_NOTIFY	"notify"
#define NS_COMMAND_VALIDATION	"validation"

isc_result_t
ns_controls_create(ns_server_t *server, ns_controls_t **ctrlsp);
/*%<
 * Create an initial, empty set of command channels for 'server'.
 */

void
ns_controls_destroy(ns_controls_t **ctrlsp);
/*%<
 * Destroy a set of command channels.
 *
 * Requires:
 *	Shutdown of the channels has completed.
 */

isc_result_t
ns_controls_configure(ns_controls_t *controls, const cfg_obj_t *config,
		      cfg_aclconfctx_t *aclconfctx);
/*%<
 * Configure zero or more command channels into 'controls'
 * as defined in the configuration parse tree 'config'.
 * The channels will evaluate ACLs in the context of
 * 'aclconfctx'.
 */

void
ns_controls_shutdown(ns_controls_t *controls);
/*%<
 * Initiate shutdown of all the command channels in 'controls'.
 */

isc_result_t
ns_control_docommand(isccc_sexpr_t *message, isc_buffer_t *text);

#endif /* NAMED_CONTROL_H */
