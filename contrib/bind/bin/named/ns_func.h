/*
 * Copyright (c) 1985, 1990
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
 * Portions Copyright (c) 1999 by Check Point Software Technologies, Inc.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Check Point Software Technologies Incorporated not be used 
 * in advertising or publicity pertaining to distribution of the document 
 * or software without specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND CHECK POINT SOFTWARE TECHNOLOGIES 
 * INCORPORATED DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.   
 * IN NO EVENT SHALL CHECK POINT SOFTWARE TECHNOLOGIES INCORPRATED
 * BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR 
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT 
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* ns_func.h - declarations for ns_*.c's externally visible functions
 *
 * $Id: ns_func.h,v 8.115 2002/01/29 03:59:38 marka Exp $
 */

/* ++from ns_glue.c++ */
struct in_addr		ina_get(const u_char *data);
const char *		sin_ntoa(struct sockaddr_in);
int			ns_wouldlog(int category, int level);
void			ns_debug(int, int, const char *, ...) ISC_FORMAT_PRINTF(3, 4);
void			ns_info(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void			ns_notice(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void			ns_warning(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void			ns_error(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void			ns_critical(int, const char *, ...) ISC_FORMAT_PRINTF(2, 3);
void			ns_panic(int, int, const char *, ...) ISC_FORMAT_PRINTF(3, 4);
void			ns_assertion_failed(const char *file, int line,
					    assertion_type type,
					    const char *cond, int print_errno);
void			panic(const char *, const void *);
void			gettime(struct timeval *);
int			nlabels(const char *);
int			my_close(int);
int			my_fclose(FILE *);
void *			__freestr(char *);
char *			__newstr(size_t, int);
char *			__savestr(const char *, int);
const char *		checked_ctime(const time_t *t);
const char *		ctimel(long);
void *			__freestr_record(char *, const char *, int);
char *			__newstr_record(size_t, int, const char *, int);
char *			__savestr_record(const char *, int, const char *, int);
u_char *		ina_put(struct in_addr ina, u_char *data);
u_char *		savebuf(const u_char *, size_t, int);
void			dprintf(int level, const char *format, ...) ISC_FORMAT_PRINTF(2, 3);
#ifdef DEBUG_STRINGS
char *			debug_newstr(size_t, int, const char *, int);
char *			debug_savestr(const char *, int, const char *, int);
void *			debug_freestr(char *, const char *, int);
#define newstr(l, n) debug_newstr((l), (n), __FILE__, __LINE__)
#define savestr(s, n) debug_savestr((s), (n), __FILE__, __LINE__)
#define freestr(s) debug_freestr((s), __FILE__, __LINE__)
#else
#ifdef RECORD_STRINGS
#define newstr(l, n) __newstr_record((l), (n), __FILE__, __LINE__)
#define savestr(s, n) __savestr_record((s), (n), __FILE__, __LINE__)
#define freestr(s) __freestr_record((s), __FILE__, __LINE__)
#else
#define newstr(l, n) __newstr((l), (n))
#define savestr(s, n) __savestr((s), (n))
#define freestr(s) __freestr((s))
#endif
#endif /* DEBUG_STRINGS */
/* --from ns_glue.c-- */

/* ++from ns_notify.c++ */
#ifdef BIND_NOTIFY
void			ns_notify(const char *, ns_class, ns_type);
void			notify_afterload(void);
void			ns_unnotify(void);
void			ns_stopnotify(const char *, ns_class);
#endif
/* --from ns_notify.c-- */

/* ++from ns_resp.c++ */
void			ns_resp(u_char *, int, struct sockaddr_in,
				struct qstream *);
void			prime_cache(void);
void			delete_all(struct namebuf *, int, int);
int			delete_stale(struct namebuf *);
struct qinfo *		sysquery(const char *, int, int,
				 struct in_addr *, struct dst_key **keys,
				 int, u_int16_t, int, int);
int			doupdate(u_char *, u_char *, struct databuf **,
				 int, int, int, u_int, struct sockaddr_in);
int			send_msg(u_char *, int, struct qinfo *);
int			findns(struct namebuf **, int,
			       struct databuf **, int *, int);
int			finddata(struct namebuf *, int, int, HEADER *,
				 char **, int *, int *);
int			add_data(struct namebuf *,
				 struct databuf **,
				 u_char *, int, int *);
int			trunc_adjust(u_char *, int, int);
/* --from ns_resp.c-- */

/* ++from ns_req.c++ */
int			ns_get_opt(u_char *msg, u_char *eom,
				   u_int8_t *versionp, u_int16_t *rcodep,
				   u_int16_t *flagp, u_int16_t *bufsizep,
				   u_char **optionsp, size_t *optsizep);
int			ns_add_opt(u_char *msg, u_char *cp, size_t buflen,
				   u_int8_t version, u_int16_t rcode,
				   u_int16_t size, u_int16_t flags,
				   u_char *options, size_t optlen);
void			ns_req(u_char *, int, int,
			       struct qstream *,
			       struct sockaddr_in,
			       int);
void			free_addinfo(void);
void			free_nsp(struct databuf **);
int			stale(struct databuf *);
int			make_rr(const char *, struct databuf *,
				u_char *, int, int,
				u_char **, u_char **, int);
int			doaddinfo(HEADER *, u_char *, int);
int			doaddauth(HEADER *, u_char *, int,
				  struct namebuf *,
				  struct databuf *);
#ifdef BIND_NOTIFY
int			findZonePri(const struct zoneinfo *,
				    const struct sockaddr_in);
#endif
int			drop_port(u_int16_t);
/* --from ns_req.c-- */

/* ++from ns_xfr.c++ */
void			ns_xfr(struct qstream *qsp, struct namebuf *znp,
			       int zone, int class, int type,
			       int id, int opcode, u_int32_t serial_ixfr, 
			       struct tsig_record *in_tsig);
void			ns_stopxfrs(struct zoneinfo *);
void			ns_freexfr(struct qstream *);
void			sx_newmsg(struct qstream *qsp);
void			sx_sendlev(struct qstream *qsp);
void			sx_sendsoa(struct qstream *qsp);
/* --from ns_xfr.c-- */

/* ++from ns_ctl.c++ */
void			ns_ctl_initialize(void);
void			ns_ctl_shutdown(void);
void			ns_ctl_defaults(controls *);
void			ns_ctl_add(controls *, control);
control			ns_ctl_new_inet(struct in_addr, u_int, ip_match_list);
#ifndef NO_SOCKADDR_UN
control			ns_ctl_new_unix(const char *, mode_t, uid_t, gid_t);
#endif
void			ns_ctl_install(controls *);
/* --from ns_ctl.c-- */

/* ++from ns_ixfr.c++ */
void			sx_send_ixfr(struct qstream *);
int			ixfr_log_maint(struct zoneinfo *);
/* --from ns_ixfr.c-- */

/* ++from ns_forw.c++ */
time_t			retrytime(struct qinfo *);
int			ns_forw(struct databuf *nsp[],
				u_char *msg,
				int msglen,
				struct sockaddr_in from,
				struct qstream *qsp,
				int dfd,
				struct qinfo **qpp,
				const char *dname,
				int class,
				int type,
				struct namebuf *np,
				int use_tcp,
				struct tsig_record *in_tsig);
int			haveComplained(u_long, u_long);
int			nslookup(struct databuf *nsp[],
				 struct qinfo *qp,
				 const char *syslogdname,
				 const char *sysloginfo);
int			qcomp(struct qserv *, struct qserv *);
void			schedretry(struct qinfo *, time_t);
void			unsched(struct qinfo *);
void			reset_retrytimer(void);
void			retrytimer(evContext ctx, void *uap,
				   struct timespec due, struct timespec ival);
void			retry(struct qinfo *, int);
void			qflush(void);
void			qremove(struct qinfo *);
void			ns_freeqns(struct qinfo *);
void			ns_freeqry(struct qinfo *);
void			freeComplaints(void);
void			nsfwdadd(struct qinfo *, struct fwdinfo *);
struct qinfo *		qfindid(u_int16_t);
struct qinfo *		qnew(const char *, int, int, int);
/* --from ns_forw.c-- */

/* ++from ns_main.c++ */
void			toggle_qrylog(void);
struct in_addr		net_mask(struct in_addr);
void			sq_remove(struct qstream *);
void			sq_flushw(struct qstream *);
void			sq_flush(struct qstream *allbut);
void			dq_remove_gen(time_t gen);
void			dq_remove_all(void);
void			sq_done(struct qstream *);
void			ns_setproctitle(char *, int);
void			getnetconf(int);
void			nsid_init(void);
void			ns_setoption(int option);
void			writestream(struct qstream *, const u_char *, int);
void			ns_need_unsafe(enum need);
void			ns_need(enum need);
void			opensocket_f(void);
void			nsid_hash(u_char *, size_t);
u_int16_t		nsid_next(void);
int			sq_openw(struct qstream *, int);
int			sq_writeh(struct qstream *, sq_closure);
int			sq_write(struct qstream *, const u_char *, int);
int			tcp_send(struct qinfo *);
int			aIsUs(struct in_addr);
/* --from ns_main.c-- */

/* ++from ns_maint.c++ */
void			zone_maint(struct zoneinfo *);
void			sched_zone_maint(struct zoneinfo *);
void			ns_cleancache(evContext ctx, void *uap,
				      struct timespec due,
				      struct timespec inter);
void			clean_cache_from(char *dname, struct hashbuf *htp);
void			remove_zone(struct zoneinfo *, const char *);
void			purge_zone(const char *, struct hashbuf *, int);
void			loadxfer(void);
void			qserial_retrytime(struct zoneinfo *, time_t);
void			qserial_query(struct zoneinfo *);
void			qserial_answer(struct qinfo *);
#ifdef DEBUG
void			printzoneinfo(int, int, int);
#endif
void			endxfer(void);
void			addxfer(struct zoneinfo *);
void			ns_zreload(void);
void			ns_reload(void);
void			ns_reconfig(void);
void			ns_noexpired(void);
#if 0
int			reload_all_unsafe(void);
#endif
int			zonefile_changed_p(struct zoneinfo *);
int			reload_master(struct zoneinfo *);
const char *		deferred_reload_unsafe(struct zoneinfo *);
struct namebuf *	purge_node(struct hashbuf *htp, struct namebuf *np);
int			clean_cache(struct hashbuf *, int);
void			reapchild(void);
const char *		zoneTypeString(unsigned int);
void			ns_heartbeat(evContext ctx, void *uap,
				     struct timespec, struct timespec);
void			make_new_zones(void);
void			free_zone(struct zoneinfo *);
struct zoneinfo *	find_auth_zone(const char *, ns_class);
int			purge_nonglue(const char *dname, struct hashbuf *htp,
				      int class, int log);
/* --from ns_maint.c-- */

/* ++from ns_sort.c++ */
void			sort_response(u_char *, u_char *, int,
				      struct sockaddr_in *);
/* --from ns_sort.c-- */

/* ++from ns_init.c++ */
void			ns_refreshtime(struct zoneinfo *, time_t);
void			ns_retrytime(struct zoneinfo *, time_t);
time_t			ns_init(const char *);
void			purgeandload(struct zoneinfo *zp);
enum context		ns_ptrcontext(const char *owner);
enum context		ns_ownercontext(int type, enum transport);
int			ns_nameok(const struct qinfo *qry, const char *name,
				  int class, struct zoneinfo *zp,
				  enum transport, enum context,
				  const char *owner,
				  struct in_addr source);
int			ns_wildcard(const char *name);
void			zoneinit(struct zoneinfo *);
void			do_reload(const char *, int, int, int);
void			ns_shutdown(void);
/* --from ns_init.c-- */

/* ++from ns_ncache.c++ */
void			cache_n_resp(u_char *, int, struct sockaddr_in,
				     const char *, int, int);
/* --from ns_ncache.c-- */

/* ++from ns_udp.c++ */
void			ns_udp(void);
/* --from ns_udp.c-- */

/* ++from ns_stats.c++ */
void			ns_stats(void);
void			ns_stats_dumpandclear(void);
void			ns_freestats(void);
void			ns_logstats(evContext ctx, void *uap,
				    struct timespec, struct timespec);
void			qtypeIncr(int qtype);
struct nameser *	nameserFind(struct in_addr addr, int flags);
#define NS_F_INSERT	0x0001
#define nameserIncr(a,w) NS_INCRSTAT(a,w)	/* XXX should change name. */
/* --from ns_stats.c-- */

/* ++from ns_update.c++ */
struct databuf *	findzonesoa(struct zoneinfo *);
void			free_rrecp(ns_updque *, int rcode, struct sockaddr_in);
int			findzone(const char *, int, int, int *, int);
u_char *		findsoaserial(u_char *data);
u_int32_t		get_serial_unchecked(struct zoneinfo *zp);
u_int32_t		get_serial(struct zoneinfo *zp);
void			set_serial(struct zoneinfo *zp, u_int32_t serial);
int			schedule_soa_update(struct zoneinfo *, int);
int			schedule_dump(struct zoneinfo *);
int			incr_serial(struct zoneinfo *zp);
int			merge_logs(struct zoneinfo *zp, char *logname);
int			zonedump(struct zoneinfo *zp, int isixfr);
void			dynamic_about_to_exit(void);
enum req_action		req_update(HEADER *hp, u_char *cp, u_char *eom,
				   u_char *msg, struct sockaddr_in from,
				   struct tsig_record *in_tsig);
void			rdata_dump(struct databuf *dp, FILE *fp);
/* --from ns_update.c-- */

/* ++from ns_config.c++ */
void			add_to_rrset_order_list(rrset_order_list,
						rrset_order_element);
const char *		p_order(int);
int			set_zone_ixfr_file(zone_config, char *);
int			set_zone_master_port(zone_config, u_short);
int			set_zone_max_log_size_ixfr(zone_config, int);
int			set_zone_dialup(zone_config, int);
int			set_trusted_key(const char *, const int,
					const int, const int, const char *);
int			set_zone_ixfr_tmp(zone_config, char *);
void			free_zone_timerinfo(struct zoneinfo *);
void			free_zone_contents(struct zoneinfo *, int);
struct zoneinfo *	find_zone(const char *, int);
zone_config		begin_zone(char *, int);
void			end_zone(zone_config, int);
int			set_zone_type(zone_config, int);
int			set_zone_filename(zone_config, char *);
int 			set_zone_checknames(zone_config, enum severity);
#ifdef BIND_NOTIFY
int			set_zone_notify(zone_config, int value);
#endif
int			set_zone_maintain_ixfr_base(zone_config, int value);
int			set_zone_update_acl(zone_config, ip_match_list);
int			set_zone_query_acl(zone_config, ip_match_list);
int			set_zone_transfer_acl(zone_config, ip_match_list);
int			set_zone_transfer_source(zone_config, struct in_addr);
int			set_zone_pubkey(zone_config, const int, const int,
					const int, const char *);
int 			set_zone_transfer_time_in(zone_config, long);
int			add_zone_master(zone_config, struct in_addr,
					struct dst_key *);
#ifdef BIND_NOTIFY
int			add_zone_notify(zone_config, struct in_addr);
#endif
void			set_zone_forward(zone_config);
void			add_zone_forwarder(zone_config, struct in_addr);
void			set_zone_boolean_option(zone_config, int, int);
options			new_options(void);
void			free_options(options);
void			free_rrset_order_list(rrset_order_list);
void			set_global_boolean_option(options, int, int);
listen_info_list	new_listen_info_list(void);
void			free_listen_info_list(listen_info_list);
void			add_listen_on(options, u_short, ip_match_list);
FILE *			write_open(char *filename);
void			update_pid_file(void);
void			set_options(options, int);
void			use_default_options(void);
enum ordering		lookup_ordering(const char *);
rrset_order_list	new_rrset_order_list(void);
rrset_order_element	new_rrset_order_element(int, int, char *, enum ordering);
ip_match_list		new_ip_match_list(void);
void			free_ip_match_list(ip_match_list);
ip_match_element	new_ip_match_pattern(struct in_addr, u_int);
ip_match_element	new_ip_match_mask(struct in_addr, struct in_addr);
ip_match_element	new_ip_match_indirect(ip_match_list);
ip_match_element	new_ip_match_key(struct dst_key *dst_key);
ip_match_element	new_ip_match_localhost(void);
ip_match_element	new_ip_match_localnets(void);
void			ip_match_negate(ip_match_element);
void			add_to_ip_match_list(ip_match_list, ip_match_element);
void			dprint_ip_match_list(int, ip_match_list, int,
					     const char *, const char *);
int			ip_match_address(ip_match_list, struct in_addr);
int			ip_match_addr_or_key(ip_match_list, struct in_addr,
					     struct dst_key *key);
int			ip_address_allowed(ip_match_list, struct in_addr);
int			ip_addr_or_key_allowed(ip_match_list iml,
					       struct in_addr,
					       struct dst_key *key);
int			ip_match_network(ip_match_list, struct in_addr,
					 struct in_addr);
int			ip_match_key_name(ip_match_list iml, char *name);
int			distance_of_address(ip_match_list, struct in_addr);
int			ip_match_is_none(ip_match_list);
#ifdef BIND_NOTIFY
void			free_also_notify(options);
int			add_global_also_notify(options, struct in_addr);
#endif
void			add_global_forwarder(options, struct in_addr);
void			free_forwarders(struct fwdinfo *);
server_info		find_server(struct in_addr);
server_config		begin_server(struct in_addr);
void			end_server(server_config, int);
void			set_server_option(server_config, int, int);
void			set_server_transfers(server_config, int);
void			set_server_transfer_format(server_config,
						   enum axfr_format);
void			add_server_key_info(server_config, struct dst_key *);
struct dst_key		*new_key_info(char *, char *, char *);
void			free_key_info(struct dst_key *);
struct dst_key		*find_key(char *name, char *algorithm);
void			dprint_key_info(struct dst_key *);
key_info_list		new_key_info_list(void);
void			free_key_info_list(key_info_list);
void			add_to_key_info_list(key_info_list, struct dst_key *);
void			dprint_key_info_list(key_info_list);
log_config		begin_logging(void);
void			add_log_channel(log_config, int, log_channel);
void			open_special_channels(void);
void			set_logging(log_config, int);
void			end_logging(log_config, int);
void			use_default_logging(void);
void			init_logging(void);
void			shutdown_logging(void);
void			init_configuration(void);
void			shutdown_configuration(void);
time_t			load_configuration(const char *);
/* --from ns_config.c-- */

/* ++from parser.y++ */
ip_match_list		lookup_acl(const char *);
void			define_acl(const char *, ip_match_list);
struct dst_key		*lookup_key(char *);
void			define_key(const char *, struct dst_key *);
time_t			parse_configuration(const char *);
void			parser_initialize(void);
void			parser_shutdown(void);
/* --from parser.y-- */

/* ++from ns_signal.c++ */
void                    init_signals(void);
void			block_signals(void);
void			unblock_signals(void);
/* --from ns_signal.c-- */
