/* ns_func.h - declarations for ns_*.c's externally visible functions
 *
 * $Id: ns_func.h,v 8.6 1995/12/22 10:20:30 vixie Exp $
 */

/* ++from ns_resp.c++ */
extern void		ns_resp __P((u_char *, int)),
			prime_cache __P((void)),
			delete_all __P((struct namebuf *, int, int));
extern struct qinfo	*sysquery __P((const char *, int, int,
				       struct in_addr *, int, int));
extern struct notify	*findNotifyPeer __P((const struct zoneinfo *,
					   struct in_addr));
extern void		sysnotify __P((const char *, int, int));
extern int		doupdate __P((u_char *, int, u_char *, int,
				      struct databuf **, int, u_int)),
			send_msg __P((u_char *, int, struct qinfo *)),
			findns __P((struct namebuf **, int,
				    struct databuf **, int *, int)),
			finddata __P((struct namebuf *, int, int, HEADER *,
				      char **, int *, int *)),
			wanted __P((struct databuf *, int, int)),
			add_data __P((struct namebuf *,
				      struct databuf **,
				      u_char *, int, int *));
/* --from ns_resp.c-- */

/* ++from ns_req.c++ */
extern void		ns_req __P((u_char *, int, int,
				    struct qstream *,
				    struct sockaddr_in *,
				    int)),
			free_addinfo __P((void)),
			free_nsp __P((struct databuf **));
extern int		stale __P((struct databuf *)),
			make_rr __P((const char *, struct databuf *,
				     u_char *, int, int)),
			doaddinfo __P((HEADER *, u_char *, int)),
			doaddauth __P((HEADER *, u_char *, int,
				       struct namebuf *,
				       struct databuf *));
#ifdef BIND_NOTIFY
extern int		findZonePri __P((const struct zoneinfo *,
					 const struct sockaddr_in *));
#endif
/* --from ns_req.c-- */

/* ++from ns_forw.c++ */
extern time_t		retrytime __P((struct qinfo *));
extern int		ns_forw __P((struct databuf *nsp[],
				     u_char *msg,
				     int msglen,
				     struct sockaddr_in *fp,
				     struct qstream *qsp,
				     int dfd,
				     struct qinfo **qpp,
				     char *dname,
				     struct namebuf *np)),
			haveComplained __P((const char *, const char *)),
			nslookup __P((struct databuf *nsp[],
				      struct qinfo *qp,
				      const char *syslogdname,
				      const char *sysloginfo)),
			qcomp __P((struct qserv *, struct qserv *));
extern struct qdatagram	*aIsUs __P((struct in_addr));
extern void		schedretry __P((struct qinfo *, time_t)),
			unsched __P((struct qinfo *)),
			retry __P((struct qinfo *)),
			qflush __P((void)),
			qremove __P((struct qinfo *)),
			qfree __P((struct qinfo *));
extern struct qinfo	*qfindid __P((u_int16_t)),
#ifdef DMALLOC
			*qnew_tagged __P((void));
#		define	qnew() qnew_tagged(__FILE__, __LINE__)
#else
			*qnew();
#endif
/* --from ns_forw.c-- */

/* ++from ns_main.c++ */
extern u_int32_t	net_mask __P((struct in_addr));
extern void		sqrm __P((struct qstream *)),
			sqflush __P((struct qstream *allbut)),
			dqflush __P((time_t gen)),
			sq_done __P((struct qstream *)),
			ns_setproctitle __P((char *, int)),
			getnetconf __P((void)),
			nsid_init __P((void));
extern u_int16_t	nsid_next __P((void));
extern struct netinfo	*findnetinfo __P((struct in_addr));
/* --from ns_main.c-- */

/* ++from ns_maint.c++ */
extern void		ns_maint __P((void)),
			sched_maint __P((void)),
#ifdef CLEANCACHE
			remove_zone __P((struct hashbuf *, int, int)),
#else
			remove_zone __P((struct hashbuf *, int)),
#endif
#ifdef PURGE_ZONE
			purge_zone __P((const char *, struct hashbuf *, int)),
#endif
			loadxfer __P((void)),
			qserial_query __P((struct zoneinfo *)),
			qserial_answer __P((struct qinfo *, u_int32_t));
extern void		holdsigchld __P((void));
extern void		releasesigchld __P((void));
extern SIG_FN		reapchild __P(());
extern void		endxfer __P((void));
extern const char *	zoneTypeString __P((const struct zoneinfo *));
#ifdef DEBUG
extern void		printzoneinfo __P((int));
#endif
/* --from ns_maint.c-- */

/* ++from ns_sort.c++ */
extern struct netinfo	*local __P((struct sockaddr_in *));
extern void		sort_response __P((u_char *, int,
					   struct netinfo *,
					   u_char *));
/* --from ns_sort.c-- */

/* ++from ns_init.c++ */
extern void		ns_refreshtime __P((struct zoneinfo *, time_t)),
			ns_retrytime __P((struct zoneinfo *, time_t)),
			ns_init __P((char *));
/* --from ns_init.c-- */

/* ++from ns_ncache.c++ */
extern void		cache_n_resp __P((u_char *, int));
/* --from ns_ncache.c-- */

/* ++from ns_stats.c++ */
extern void		ns_stats __P((void));
#ifdef XSTATS
extern void		ns_logstats __P((void));
#endif
extern void		qtypeIncr __P((int qtype));
extern struct nameser	*nameserFind __P((struct in_addr addr, int flags));
#define NS_F_INSERT	0x0001
extern void		nameserIncr __P((struct in_addr addr,
					 enum nameserStats which));
/* --from ns_stats.c-- */

/* ++from ns_validate.c++ */
extern int
#ifdef NCACHE
			validate __P((char *, char *, struct sockaddr_in *,
				      int, int, char *, int, int)),
#else
			validate __P((char *, char *, struct sockaddr_in *,
				      int, int, char *, int)),
#endif
			dovalidate __P((u_char *, int, u_char *, int, int,
					char *, struct sockaddr_in *, int *)),
			update_msg __P((u_char *, int *, int Vlist[], int));
extern void		store_name_addr __P((const char *, struct in_addr,
					     const char *, const char *));
/* --from ns_validate.c-- */
