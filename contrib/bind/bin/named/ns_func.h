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
 * Portions Copyright (c) 1996-1999 by Internet Software Consortium.
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
 * $Id: ns_func.h,v 8.91 1999/11/16 06:01:38 vixie Exp $
 */

/* ++from ns_glue.c++ */
extern struct in_addr	ina_get(const u_char *data);
extern const char	*sin_ntoa(struct sockaddr_in);
extern int		ns_wouldlog(int category, int level);
extern void		ns_debug(int, int, const char *, ...),
			ns_info(int, const char *, ...),
			ns_notice(int, const char *, ...),
			ns_warning(int, const char *, ...),
			ns_error(int, const char *, ...),
			ns_panic(int, int, const char *, ...),
			ns_assertion_failed(char *file, int line,
					    assertion_type type, char *cond,
					    int print_errno);
extern void		panic(const char *, const void *),
			gettime(struct timeval *);
extern int		nlabels(const char *),
			my_close(int),
			my_fclose(FILE *);
extern void		__freestr(char *);
extern char		*__newstr(size_t, int),
			*__savestr(const char *, int),
			*checked_ctime(const time_t *t),
			*ctimel(long);
extern void		__freestr_record(char *, char *, int);
extern char		*__newstr_record(size_t, int, char *, int);
extern char		*__savestr_record(const char *, int, char *, int);
extern u_char		*ina_put(struct in_addr ina, u_char *data),
			*savebuf(const u_char *, size_t, int);
extern void		dprintf(int level, const char *format, ...);
#ifdef DEBUG_STRINGS
extern char		*debug_newstr(size_t, int, const char *, int),
			*debug_savestr(const char *, int, const char *, int);
extern void		debug_freestr(char *, const char *, int);
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
int movefile(const char *, const char *);
/* --from ns_glue.c-- */

/* ++from ns_notify.c++ */
#ifdef BIND_NOTIFY
void			ns_notify(const char *, ns_class, ns_type);
void			notify_afterload(void);
void			ns_unnotify(void);
#endif
/* --from ns_notify.c-- */

/* ++from ns_resp.c++ */
extern void		ns_resp(u_char *, int, struct sockaddr_in,
				struct qstream *),
			prime_cache(void),
			delete_all(struct namebuf *, int, int);
extern int		delete_stale(struct namebuf *);
extern struct qinfo	*sysquery(const char *, int, int,
				  struct in_addr *, int, u_int16_t, int);
extern int		doupdate(u_char *, u_char *, struct databuf **,
				 int, int, int, u_int, struct sockaddr_in),
			send_msg(u_char *, int, struct qinfo *),
			findns(struct namebuf **, int,
			       struct databuf **, int *, int),
			finddata(struct namebuf *, int, int, HEADER *,
				 char **, int *, int *),
			add_data(struct namebuf *,
				 struct databuf **,
				 u_char *, int, int *),
			trunc_adjust(u_char *, int, int);
/* --from ns_resp.c-- */

/* ++from ns_req.c++ */
extern void		ns_req(u_char *, int, int,
			       struct qstream *,
			       struct sockaddr_in,
			       int),
			free_addinfo(void),
			free_nsp(struct databuf **);
extern int		stale(struct databuf *),
			make_rr(const char *, struct databuf *,
				u_char *, int, int,
				u_char **, u_char **, int),
			doaddinfo(HEADER *, u_char *, int),
			doaddauth(HEADER *, u_char *, int,
				  struct namebuf *,
				  struct databuf *);
#ifdef BIND_NOTIFY
extern int		findZonePri(const struct zoneinfo *,
				    const struct sockaddr_in);
#endif
/* --from ns_req.c-- */

/* ++from ns_xfr.c++ */
void			ns_xfr(struct qstream *qsp, struct namebuf *znp,
			       int zone, int class, int type,
			       int id, int opcode, u_int32_t serial_ixfr, 
				   struct tsig_record *in_tsig),
			ns_stopxfrs(struct zoneinfo *),
			ns_freexfr(struct qstream *),
			sx_newmsg(struct qstream *qsp),
			sx_sendlev(struct qstream *qsp),
			sx_sendsoa(struct qstream *qsp);
/* --from ns_xfr.c-- */

/* ++from ns_ctl.c++ */
void			ns_ctl_initialize(void);
void			ns_ctl_shutdown(void);
void			ns_ctl_defaults(controls *);
void			ns_ctl_add(controls *, control);
control			ns_ctl_new_inet(struct in_addr, u_int, ip_match_list);
#ifndef WINNT
control			ns_ctl_new_unix(char *, mode_t, uid_t, gid_t);
#endif
void			ns_ctl_install(controls *);
/* --from ns_ctl.c-- */

/* ++from ns_ixfr.c++ */
void			sx_send_ixfr(struct qstream *qsp);
/* --from ns_ixfr.c-- */

/* ++from ns_forw.c++ */
extern time_t		retrytime(struct qinfo *);
extern int		ns_forw(struct databuf *nsp[],
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
				struct tsig_record *in_tsig),
			haveComplained(u_long, u_long),
			nslookup(struct databuf *nsp[],
				 struct qinfo *qp,
				 const char *syslogdname,
				 const char *sysloginfo),
			qcomp(struct qserv *, struct qserv *);
extern void		schedretry(struct qinfo *, time_t),
			unsched(struct qinfo *),
			reset_retrytimer(void),
			retrytimer(evContext ctx, void *uap,
				   struct timespec due, struct timespec ival),
			retry(struct qinfo *),
			qflush(void),
			qremove(struct qinfo *),
                        ns_freeqns(struct qinfo *, char *),
			ns_freeqry(struct qinfo *),
			freeComplaints(void),
			nsfwdadd(struct qinfo *, struct fwdinfo *);
extern struct qinfo	*qfindid(u_int16_t),
			*qnew(const char *, int, int, int);
/* --from ns_forw.c-- */

/* ++from ns_main.c++ */
extern struct in_addr	net_mask(struct in_addr);
extern void		sq_remove(struct qstream *),
			sq_flushw(struct qstream *),
			sq_flush(struct qstream *allbut),
			dq_remove_gen(time_t gen),
			dq_remove_all(),
			sq_done(struct qstream *),
			ns_setproctitle(char *, int),
			getnetconf(int),
			nsid_init(void),
			ns_setoption(int option),
			writestream(struct qstream *, const u_char *, int),
			ns_need_unsafe(enum need),
			ns_need(enum need),
			opensocket_f(void),
			nsid_hash(u_char *, size_t);
extern u_int16_t	nsid_next(void);
extern int		sq_openw(struct qstream *, int),
			sq_writeh(struct qstream *, sq_closure),
			sq_write(struct qstream *, const u_char *, int),
			tcp_send(struct qinfo *),
			aIsUs(struct in_addr);
/* --from ns_main.c-- */

/* ++from ns_maint.c++ */
extern void		zone_maint(struct zoneinfo *),
			sched_zone_maint(struct zoneinfo *),
			ns_cleancache(evContext ctx, void *uap,
				      struct timespec due,
				      struct timespec inter),
			clean_cache_from(char *dname, struct hashbuf *htp),
			remove_zone(struct zoneinfo *, const char *),
			purge_zone(const char *, struct hashbuf *, int),
			loadxfer(void),
			qserial_retrytime(struct zoneinfo *, time_t),
			qserial_query(struct zoneinfo *),
			qserial_answer(struct qinfo *),
#ifdef DEBUG
			printzoneinfo(int, int, int),
#endif
			endxfer(void),
			addxfer(struct zoneinfo *),
			ns_zreload(void),
			ns_reload(void),
			ns_reconfig(void);
#if 0
extern int		reload_all_unsafe(void);
#endif
extern int		zonefile_changed_p(struct zoneinfo *);
int			reload_master(struct zoneinfo *);
extern const char *	deferred_reload_unsafe(struct zoneinfo *);
extern struct namebuf *	purge_node(struct hashbuf *htp, struct namebuf *np);
extern int		clean_cache(struct hashbuf *, int);
extern void		reapchild(void);
extern const char *	zoneTypeString(unsigned int);
extern void		ns_heartbeat(evContext ctx, void *uap,
				     struct timespec, struct timespec);
extern void		make_new_zones(void);
extern void		free_zone(struct zoneinfo *);
extern struct zoneinfo *find_auth_zone(const char *, ns_class);
/* --from ns_maint.c-- */

/* ++from ns_sort.c++ */
extern void		sort_response(u_char *, u_char *, int,
				      struct sockaddr_in *);
/* --from ns_sort.c-- */

/* ++from ns_init.c++ */
extern void		ns_refreshtime(struct zoneinfo *, time_t);
extern void		ns_retrytime(struct zoneinfo *, time_t);
extern void		ns_init(const char *);
extern void		purgeandload(struct zoneinfo *zp);
extern enum context	ns_ptrcontext(const char *owner);
extern enum context	ns_ownercontext(int type, enum transport);
extern int		ns_nameok(const struct qinfo *qry, const char *name,
				  int class, struct zoneinfo *zp,
				  enum transport, enum context,
				  const char *owner,
				  struct in_addr source);
extern int		ns_wildcard(const char *name);
extern void		zoneinit(struct zoneinfo *);
extern void		do_reload(const char *, int, int, int);
extern void		ns_shutdown(void);
/* --from ns_init.c-- */

/* ++from ns_ncache.c++ */
extern void		cache_n_resp(u_char *, int, struct sockaddr_in,
				     const char *, int, int);
/* --from ns_ncache.c-- */

/* ++from ns_udp.c++ */
extern void		ns_udp(void);
/* --from ns_udp.c-- */

/* ++from ns_stats.c++ */
extern void		ns_stats(void),
			ns_freestats(void);
extern void		ns_logstats(evContext ctx, void *uap,
				    struct timespec, struct timespec);
extern void		qtypeIncr(int qtype);
extern struct nameser	*nameserFind(struct in_addr addr, int flags);
#define NS_F_INSERT	0x0001
#define nameserIncr(a,w) NS_INCRSTAT(a,w)	/* XXX should change name. */
/* --from ns_stats.c-- */

/* ++from ns_update.c++ */
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
				   u_char *msg, struct qstream *qsp,
				   int dfd, struct sockaddr_in from,
				   struct tsig_record *in_tsig);
void			rdata_dump(struct databuf *dp, FILE *fp);
/* --from ns_update.c-- */

/* ++from ns_config.c++ */
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
int			add_zone_master(zone_config, struct in_addr);
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
void			add_listen_on(options, u_int16_t, ip_match_list);
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
void			dprint_ip_match_list(int, ip_match_list, int, char *,
					     char *);
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
void			load_configuration(const char *);
/* --from ns_config.c-- */
/* ++from parser.y++ */
ip_match_list		lookup_acl(char *);
void			define_acl(char *, ip_match_list);
struct dst_key		*lookup_key(char *);
void			define_key(char *, struct dst_key *);
void			parse_configuration(const char *);
void			parser_initialize(void);
void			parser_shutdown(void);
/* --from parser.y-- */
/* ++from ns_signal.c++ */
void                    init_signals(void);
void			block_signals(void);
void			unblock_signals(void);
/* --from ns_signal.c-- */
