/*
 * Copyright (C) 2004-2007, 2009-2016  Internet Systems Consortium, Inc. ("ISC")
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

/* $Id$ */

/*! \file */

#include <config.h>


#include <isc/app.h>
#include <isc/event.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/timer.h>
#include <isc/util.h>

#include <dns/result.h>

#include <isccc/alist.h>
#include <isccc/cc.h>
#include <isccc/result.h>

#include <named/control.h>
#include <named/globals.h>
#include <named/log.h>
#include <named/os.h>
#include <named/server.h>
#ifdef HAVE_LIBSCF
#include <named/ns_smf_globals.h>
#endif

static isc_result_t
getcommand(isc_lex_t *lex, char **cmdp) {
	isc_result_t result;
	isc_token_t token;

	REQUIRE(cmdp != NULL && *cmdp == NULL);

	result = isc_lex_gettoken(lex, ISC_LEXOPT_EOF, &token);
	if (result != ISC_R_SUCCESS)
		return (result);

	isc_lex_ungettoken(lex, &token);

	if (token.type != isc_tokentype_string)
		return (ISC_R_FAILURE);

	*cmdp = token.value.as_textregion.base;

	return (ISC_R_SUCCESS);
}

static inline isc_boolean_t
command_compare(const char *str, const char *command) {
	return ISC_TF(strcasecmp(str, command) == 0);
}

/*%
 * This function is called to process the incoming command
 * when a control channel message is received.
 */
isc_result_t
ns_control_docommand(isccc_sexpr_t *message, isc_buffer_t *text) {
	isccc_sexpr_t *data;
	char *cmdline = NULL;
	char *command = NULL;
	isc_result_t result;
	int log_level;
	isc_buffer_t src;
	isc_lex_t *lex = NULL;
#ifdef HAVE_LIBSCF
	ns_smf_want_disable = 0;
#endif

	data = isccc_alist_lookup(message, "_data");
	if (!isccc_alist_alistp(data)) {
		/*
		 * No data section.
		 */
		return (ISC_R_FAILURE);
	}

	result = isccc_cc_lookupstring(data, "type", &cmdline);
	if (result != ISC_R_SUCCESS) {
		/*
		 * We have no idea what this is.
		 */
		return (result);
	}

	result = isc_lex_create(ns_g_mctx, strlen(cmdline), &lex);
	if (result != ISC_R_SUCCESS)
		return (result);

	isc_buffer_init(&src, cmdline, strlen(cmdline));
	isc_buffer_add(&src, strlen(cmdline));
	result = isc_lex_openbuffer(lex, &src);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	result = getcommand(lex, &command);
	if (result != ISC_R_SUCCESS)
		goto cleanup;

	/*
	 * Compare the 'command' parameter against all known control commands.
	 */
	if (command_compare(command, NS_COMMAND_NULL) ||
	    command_compare(command, NS_COMMAND_STATUS)) {
		log_level = ISC_LOG_DEBUG(1);
	} else {
		log_level = ISC_LOG_INFO;
	}

	isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
		      NS_LOGMODULE_CONTROL, log_level,
		      "received control channel command '%s'",
		      command);

	if (command_compare(command, NS_COMMAND_RELOAD)) {
		result = ns_server_reloadcommand(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_RECONFIG)) {
		result = ns_server_reconfigcommand(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_REFRESH)) {
		result = ns_server_refreshcommand(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_RETRANSFER)) {
		result = ns_server_retransfercommand(ns_g_server,
						     lex, text);
	} else if (command_compare(command, NS_COMMAND_HALT)) {
#ifdef HAVE_LIBSCF
		/*
		 * If we are managed by smf(5), AND in chroot, then
		 * we cannot connect to the smf repository, so just
		 * return with an appropriate message back to rndc.
		 */
		if (ns_smf_got_instance == 1 && ns_smf_chroot == 1) {
			result = ns_smf_add_message(text);
			goto cleanup;
		}
		/*
		 * If we are managed by smf(5) but not in chroot,
		 * try to disable ourselves the smf way.
		 */
		if (ns_smf_got_instance == 1 && ns_smf_chroot == 0)
			ns_smf_want_disable = 1;
		/*
		 * If ns_smf_got_instance = 0, ns_smf_chroot
		 * is not relevant and we fall through to
		 * isc_app_shutdown below.
		 */
#endif
		/* Do not flush master files */
		ns_server_flushonshutdown(ns_g_server, ISC_FALSE);
		ns_os_shutdownmsg(cmdline, text);
		isc_app_shutdown();
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_STOP)) {
		/*
		 * "stop" is the same as "halt" except it does
		 * flush master files.
		 */
#ifdef HAVE_LIBSCF
		if (ns_smf_got_instance == 1 && ns_smf_chroot == 1) {
			result = ns_smf_add_message(text);
			goto cleanup;
		}
		if (ns_smf_got_instance == 1 && ns_smf_chroot == 0)
			ns_smf_want_disable = 1;
#endif
		ns_server_flushonshutdown(ns_g_server, ISC_TRUE);
		ns_os_shutdownmsg(cmdline, text);
		isc_app_shutdown();
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_DUMPSTATS)) {
		result = ns_server_dumpstats(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_QUERYLOG)) {
		result = ns_server_togglequerylog(ns_g_server, lex);
	} else if (command_compare(command, NS_COMMAND_DUMPDB)) {
		ns_server_dumpdb(ns_g_server, lex);
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_SECROOTS)) {
		result = ns_server_dumpsecroots(ns_g_server, lex);
	} else if (command_compare(command, NS_COMMAND_TRACE)) {
		result = ns_server_setdebuglevel(ns_g_server, lex);
	} else if (command_compare(command, NS_COMMAND_NOTRACE)) {
		ns_g_debuglevel = 0;
		isc_log_setdebuglevel(ns_g_lctx, ns_g_debuglevel);
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_FLUSH)) {
		result = ns_server_flushcache(ns_g_server, lex);
	} else if (command_compare(command, NS_COMMAND_FLUSHNAME)) {
		result = ns_server_flushnode(ns_g_server, lex, ISC_FALSE);
	} else if (command_compare(command, NS_COMMAND_FLUSHTREE)) {
		result = ns_server_flushnode(ns_g_server, lex, ISC_TRUE);
	} else if (command_compare(command, NS_COMMAND_STATUS)) {
		result = ns_server_status(ns_g_server, text);
	} else if (command_compare(command, NS_COMMAND_TSIGLIST)) {
		result = ns_server_tsiglist(ns_g_server, text);
	} else if (command_compare(command, NS_COMMAND_TSIGDELETE)) {
		result = ns_server_tsigdelete(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_FREEZE)) {
		result = ns_server_freeze(ns_g_server, ISC_TRUE, lex,
					  text);
	} else if (command_compare(command, NS_COMMAND_UNFREEZE) ||
		   command_compare(command, NS_COMMAND_THAW)) {
		result = ns_server_freeze(ns_g_server, ISC_FALSE, lex,
					  text);
	} else if (command_compare(command, NS_COMMAND_SYNC)) {
		result = ns_server_sync(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_RECURSING)) {
		result = ns_server_dumprecursing(ns_g_server);
	} else if (command_compare(command, NS_COMMAND_TIMERPOKE)) {
		result = ISC_R_SUCCESS;
		isc_timermgr_poke(ns_g_timermgr);
	} else if (command_compare(command, NS_COMMAND_NULL)) {
		result = ISC_R_SUCCESS;
	} else if (command_compare(command, NS_COMMAND_NOTIFY)) {
		result = ns_server_notifycommand(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_VALIDATION)) {
		result = ns_server_validation(ns_g_server, lex);
	} else if (command_compare(command, NS_COMMAND_SIGN) ||
		   command_compare(command, NS_COMMAND_LOADKEYS)) {
		result = ns_server_rekey(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_ADDZONE)) {
		result = ns_server_add_zone(ns_g_server, cmdline, text);
	} else if (command_compare(command, NS_COMMAND_DELZONE)) {
		result = ns_server_del_zone(ns_g_server, lex, text);
	} else if (command_compare(command, NS_COMMAND_SIGNING)) {
		result = ns_server_signing(ns_g_server, lex, text);
	} else {
		isc_log_write(ns_g_lctx, NS_LOGCATEGORY_GENERAL,
			      NS_LOGMODULE_CONTROL, ISC_LOG_WARNING,
			      "unknown control channel command '%s'",
			      command);
		result = DNS_R_UNKNOWNCOMMAND;
	}

 cleanup:
	if (lex != NULL)
		isc_lex_destroy(&lex);

	return (result);
}
