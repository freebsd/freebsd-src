/*
 *	from ns.h	4.33 (Berkeley) 8/23/90
 *	$Id: ns_glob.h,v 8.56 2000/12/02 18:39:25 vixie Exp $
 */

/*
 * Copyright (c) 1986
 *    The Regents of the University of California.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

/*
 * Portions Copyright (c) 1996-2000 by Internet Software Consortium.
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
 * Global variables for the name server.
 */

	/* original argv[] from main() */
DECL	char			**saved_argv;

#ifdef DEBUG
DECL	int			debug		INIT(0);
DECL	int			desired_debug	INIT(0);
#endif

	/* global event context */
DECL	evContext		ev;

	/* global resolver context. */
DECL	struct __res_state	res;

	/* list of open streams */
DECL	struct qstream		*streamq;

	/* often set to the current time */
DECL	struct timeval		tt;

	/* head of allocated queries */
DECL	struct qinfo		*nsqhead;

	/* datagram socket for sysquery() and ns_forw(). */
DECL	int			ds		INIT(-1);

	/* event ID for reads of "ds". */
DECL	evFileID		ds_evID;

#ifdef QRYLOG
	/* is query logging turned on? */
DECL	int			qrylog;
#endif /*QRYLOG*/

	/* port to which we send queries */
DECL	u_int16_t		ns_port;

	/* Source addr of our internal resolver. */
DECL	struct sockaddr_in	source_addr;	/* INITs to <INADDR_ANY, 0>. */

	/* Used by ns_stats */
DECL	time_t			boottime;

DECL	time_t			resettime;

	/* next query to retry */
DECL	struct qinfo		*retryqp;

	/* configuration file name */
DECL	char			*conffile;

	/* configuration file mtime */
DECL	time_t			confmtime;

	/* default debug output file */
DECL	char			*debugfile;

	/* zone information */
DECL	struct zoneinfo		*zones;

	/* number of zones allocated */
DECL	int			nzones;

	/* free list of unused zones[] elements. */
DECL	LIST(struct zoneinfo)	freezones;

	/* list of zones that have a reload pending. */
DECL	LIST(struct zoneinfo)	reloadingzones;

	/* set if we need a priming */
DECL	int			needs_prime_cache;

	/* is cache being primed */
DECL	int			priming;

	/* ptrs to dnames in msg for dn_comp */
DECL	u_char			*dnptrs[40];

	/* end pointer for dnptrs */
DECL	u_char			**dnptrs_end
				INIT(dnptrs + sizeof dnptrs / sizeof(u_char*));

	/* data about all forwarders */
DECL    struct fwddata          **fwddata;
	/* how many forwarders are there in fwddata? */
DECL	int			fwddata_count;

	/* number of names in addinfo */
DECL	int			addcount;

	/* name of cache file */
DECL	const char		*cache_file;

#ifdef BIND_UPDATE
DECL	const char *		LogSignature	INIT(";BIND LOG V8\n");
DECL	const char *		DumpSignature	INIT(";BIND DUMP V8\n");
DECL	const char *		DumpSuffix	INIT(".dumptmp");
#endif

DECL	const char		sendtoStr[]	INIT("sendto");
DECL	const char		tcpsendStr[]	INIT("tcp_send");

	/* defined in version.c, can't use DECL/INIT */
extern	char			Version[];
extern	char			ShortVersion[];

	/* If getnum() has an error, here will be the result. */
DECL	int			getnum_error		INIT(0);

enum context { domain_ctx, owner_ctx, mailname_ctx, hostname_ctx };
DECL	const char		*context_strings[]
#ifdef MAIN_PROGRAM
	= { "domain", "owner", "mail", "host", NULL }
#endif
;

DECL	const char		*transport_strings[]
#ifdef MAIN_PROGRAM
	= { "primary", "secondary", "response", NULL }
#endif
;

DECL	const char		*severity_strings[]
#ifdef MAIN_PROGRAM
	= { "ignore", "warn", "fail", "not_set", NULL }
#endif
;

DECL	struct in_addr		inaddr_any;		/* Inits to 0.0.0.0 */

DECL	options			server_options		INIT(NULL);

DECL	server_info		nameserver_info		INIT(NULL);
DECL	key_info_list		secretkey_info		INIT(NULL);

DECL	ip_match_list		bogus_nameservers	INIT(NULL);

DECL	log_context 		log_ctx;
DECL	int			log_ctx_valid		INIT(0);

DECL	log_channel		syslog_channel		INIT(NULL);
DECL	log_channel		debug_channel		INIT(NULL);
DECL	log_channel		stderr_channel		INIT(NULL);
DECL	log_channel		eventlib_channel	INIT(NULL);
DECL	log_channel		packet_channel		INIT(NULL);
DECL	log_channel		null_channel		INIT(NULL);

DECL	ip_match_list		local_addresses		INIT(NULL);
DECL	ip_match_list		local_networks		INIT(NULL);

	/* are we running in no-fork mode? */
DECL	int			foreground		INIT(0);

DECL	const struct ns_sym	logging_constants[]
#ifdef MAIN_PROGRAM
= {
	{ log_info,	"info" },
	{ log_notice,	"notice" },
	{ log_warning,	"warning" },
	{ log_error,	"error" },
	{ log_critical,	"critical" },
	{ 0,		NULL }
}
#endif
;

DECL	const struct ns_sym	syslog_constants[]
#ifdef MAIN_PROGRAM
= {
	{ LOG_KERN,	"kern" },
	{ LOG_USER,	"user" },
	{ LOG_MAIL,	"mail" },
	{ LOG_DAEMON,	"daemon" },
	{ LOG_AUTH,	"auth" },
	{ LOG_SYSLOG,	"syslog" },
	{ LOG_LPR,	"lpr" },
#ifdef LOG_NEWS
	{ LOG_NEWS,	"news" },
#endif
#ifdef LOG_UUCP
	{ LOG_UUCP,	"uucp" },
#endif
#ifdef LOG_CRON
	{ LOG_CRON,	"cron" },
#endif
#ifdef LOG_AUTHPRIV
	{ LOG_AUTHPRIV,	"authpriv" },
#endif
#ifdef LOG_FTP
	{ LOG_FTP,	"ftp" },
#endif
	{ LOG_LOCAL0,	"local0"}, 
	{ LOG_LOCAL1,	"local1"}, 
	{ LOG_LOCAL2,	"local2"}, 
	{ LOG_LOCAL3,	"local3"}, 
	{ LOG_LOCAL4,	"local4"}, 
	{ LOG_LOCAL5,	"local5"}, 
	{ LOG_LOCAL6,	"local6"}, 
	{ LOG_LOCAL7,	"local7"}, 
	{ 0,		NULL }
}
#endif
;

DECL	const struct ns_sym	category_constants[]
#ifdef MAIN_PROGRAM
= {
	{ ns_log_default,	"default" },
	{ ns_log_config,	"config" },
	{ ns_log_parser,	"parser" },
	{ ns_log_queries,	"queries" },
	{ ns_log_lame_servers,	"lame-servers" },
	{ ns_log_statistics,	"statistics" },
	{ ns_log_panic,		"panic" },
	{ ns_log_update,	"update" },
	{ ns_log_ncache,	"ncache" },
	{ ns_log_xfer_in,	"xfer-in" },
	{ ns_log_xfer_out,	"xfer-out" },
	{ ns_log_db,		"db" },
	{ ns_log_eventlib,	"eventlib" },
	{ ns_log_packet,	"packet" },
#ifdef BIND_NOTIFY
	{ ns_log_notify,	"notify" },
#endif
	{ ns_log_cname,		"cname" },
	{ ns_log_security,	"security" },
	{ ns_log_os,		"os" },
	{ ns_log_insist,	"insist" },
	{ ns_log_maint,		"maintenance" },
	{ ns_log_load,		"load" },
	{ ns_log_resp_checks,	"response-checks" },
	{ ns_log_control,	"control" },
	{ 0,			NULL }
}
#endif
;

DECL	const char panic_msg_no_options[]
	INIT("no server_options in NS_OPTION_P");

DECL	const char panic_msg_insist_failed[]
	INIT("%s:%d: insist '%s' failed: %s");

DECL	const char panic_msg_bad_which[]
	INIT("%s:%d: INCRSTATS(%s): bad \"which\"");

DECL	u_long			globalStats[nssLast];

DECL	evTimerID		clean_timer;
DECL	evTimerID		interface_timer;
DECL	evTimerID		stats_timer;
DECL	evTimerID		heartbeat_timer;
DECL	int			active_timers		INIT(0);

DECL	uid_t			user_id;
DECL	char *			user_name		INIT(NULL);
DECL	gid_t			group_id;
DECL	char *			group_name		INIT(NULL);
DECL	char *			chroot_dir		INIT(NULL);

DECL	int			loading			INIT(0);

DECL	int			xfers_running		INIT(0);
DECL	int			xfers_deferred		INIT(0);
DECL	int			qserials_running	INIT(0);
