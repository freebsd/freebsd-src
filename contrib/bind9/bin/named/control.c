/*
 * Copyright (C) 2004  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001-2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
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

/* $Id: control.c,v 1.7.2.2.2.11 2004/09/03 03:43:31 marka Exp $ */

#include <config.h>

#include <string.h>

#include <isc/app.h>
#include <isc/event.h>
#include <isc/mem.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/result.h>

#include <isccc/alist.h>
#include <isccc/cc.h>
#include <isccc/result.h>

#include <named/control.h>
#include <named/log.h>
#include <named/os.h>
#include <named/server.h>

static isc_boolean_t
command_compare(const char *text, const char *command) {
	unsigned int commandlen = strlen(command);
	if (strncasecmp(text, command, commandlen) == 0 &&
	    (text[commandlen] == '\0' ||
	     text[commandlen] == ' ' ||
	     text[commandlen] == '\t'))
		return (ISC_TRUE);
	return (ISC_FALSE);
}

/*
 * This function is called to process the incoming command
 * when a control channel message is received.  
 */
isc_result_t
ns_control_docommand(isccc_sexpr_t *message, isc_buffer_t *text) {
	isccc_sexpr_t *data;
	char *command;
	isc_result_t result;

	data = isccc_alist_lookup(message, "_data");
	if (data == NULL) {
		/*
		 * No data section.
		 */
		return (ISC_R_FAILURE);
	}

	result = isccc_cc_lookupstring(data, "type", &command);
	if (result != ISC_R_SUCCESS) {
		/*
		 * We have no idea what this is.
		 */
		return (result);
	}

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_CONTROL, ISC_LOG_DEBUG(1),
		      "received control channel command '%s'",
		      command);

	/*
	 * Compare the 'command' parameter against all known control commands.
	 */
	if (command_compare(command, NS_COMMAND_RELOAD)) {
		result = ns_server_reloadcommand(ns_g_server, command, text);
	} else if (command_compare(command, NS_COMMAND_RECONFIG)) {
		result = ns_server_reconfigcommand(ns_g_server, command);
	} else if (command_compare(command, NS_COMMAND_REFRESH)) {
		result = ns_server_refreshcommand(ns_g_server, command, text);
	} else if (command_compare(command, NS_COMMAND_RETRANSFER)) {
		result = ns_server_retransfercommand(ns_g_server, command);
	} else if (command_compare(command, NS_COMMAND_HALT)) {
		ns_server_flushonshutdown(ns_g_server, ISC_FALSE);
		ns_os_shutdownmsg(command, text);
		isc_app_shutdown();
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_STOP)) {
		ns_server_flushonshutdown(ns_g_server, ISC_TRUE);
		ns_os_shutdownmsg(command, text);
		isc_app_shutdown();
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_DUMPSTATS)) {
		result = ns_server_dumpstats(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_QUERYLOG)) {
		result = ns_server_togglequerylog(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_DUMPDB)) {
		ns_server_dumpdb(ns_g_server, command);
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_TRACE)) {
		result = ns_server_setdebuglevel(ns_g_server, command);
	} else if (command_compare(command, NS_COMMAND_NOTRACE)) {
		ns_g_debuglevel = 0;
		isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_FLUSH)) {
		result = ns_server_flushcache(ns_g_server, command);
	} else if (command_compare(command, NS_COMMAND_FLUSHNAME)) {
		result = ns_server_flushname(ns_g_server, command);
	} else if (command_compare(command, NS_COMMAND_STATUS)) {
		result = ns_server_status(ns_g_server, text);
	} else if (command_compare(command, NS_COMMAND_FREEZE)) {
		result = ns_server_freeze(ns_g_server, ISC_TRUE, command);
	} else if (command_compare(command, NS_COMMAND_UNFREEZE) ||
		   command_compare(command, NS_COMMAND_THAW)) {
		result = ns_server_freeze(ns_g_server, ISC_FALSE, command);
	} else if (command_compare(command, NS_COMMAND_RECURSING)) {
		result = ns_server_dumprecursing(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_NULL)) {
		result = ISC_R_SUCCESS;
	} else {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "unknown control channel command '%s'",
			      command);
		result = DNS_R_UNKNOWNCOMMAND;
	}

	return (result);
}
