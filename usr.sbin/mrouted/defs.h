/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $FreeBSD: src/usr.sbin/mrouted/defs.h,v 1.12 1999/08/28 01:17:03 peter Exp $
 * defs.h,v 3.8.4.15 1998/03/01 02:51:42 fenner Exp
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#ifdef SYSV
#include <sys/sockio.h>
#endif
#ifdef _AIX
#include <time.h>
#endif
#include <sys/time.h>
#include <sys/uio.h>
#include <net/if.h>
#define rtentry kern_rtentry	/* XXX !!! UGH */
#include <net/route.h>
#undef rtentry
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/igmp.h>
#ifdef __FreeBSD__	/* sigh */
#include <osreldate.h>
#if __FreeBSD_version >= 220000
#define rtentry kernel_rtentry
#include <net/route.h>
#undef rtentry
#endif
#endif
#include <netinet/ip_mroute.h>
#ifdef RSRR
#include <sys/un.h>
#endif /* RSRR */

/*XXX*/
typedef u_int u_int32;

#ifndef __P
#ifdef __STDC__
#define __P(x)	x
#else
#define __P(x)	()
#endif
#endif

typedef void (*cfunc_t) __P((void *));
typedef void (*ihfunc_t) __P((int, fd_set *));

#include "dvmrp.h"
#include "igmpv2.h"
#include "vif.h"
#include "route.h"
#include "prune.h"
#include "pathnames.h"
#ifdef RSRR
#include "rsrr.h"
#include "rsrr_var.h"
#endif /* RSRR */

/*
 * Miscellaneous constants and macros.
 */
#define FALSE		0
#define TRUE		1

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

#define TIMER_INTERVAL	ROUTE_MAX_REPORT_DELAY

#define PROTOCOL_VERSION 3  /* increment when packet format/content changes */
#define MROUTED_VERSION	 9  /* not in DVMRP packets at all */

#define	DVMRP_CONSTANT	0x000eff00	/* constant portion of 'group' field */

#define MROUTED_LEVEL  (DVMRP_CONSTANT | PROTOCOL_VERSION)
			    /* for IGMP 'group' field of DVMRP messages */

#define LEAF_FLAGS	(( vifs_with_neighbors == 1 ) ? 0x010000 : 0)
			    /* more for IGMP 'group' field of DVMRP messages */

#define	DEL_RTE_GROUP		0
#define	DEL_ALL_ROUTES		1
			    /* for Deleting kernel table entries */

/* obnoxious gcc gives an extraneous warning about this constant... */
#if defined(__STDC__) || defined(__GNUC__)
#define JAN_1970	2208988800UL	/* 1970 - 1900 in seconds */
#else
#define JAN_1970	2208988800L	/* 1970 - 1900 in seconds */
#define const		/**/
#endif

#ifdef RSRR
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))
#endif /* RSRR */

#ifdef SYSV
#define bcopy(a, b, c)	memcpy(b, a, c)
#define bzero(s, n) 	memset((s), 0, (n))
#define setlinebuf(s)	setvbuf(s, NULL, _IOLBF, 0)
#endif

#if defined(_AIX) || (defined(BSD) && (BSD >= 199103))
#define	HAVE_SA_LEN
#endif

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE 8192
extern char		*recv_buf;
extern char		*send_buf;
extern int		igmp_socket;
#ifdef RSRR
extern int              rsrr_socket;
#endif /* RSRR */
extern u_int32		allhosts_group;
extern u_int32		allrtrs_group;
extern u_int32		dvmrp_group;
extern u_int32		dvmrp_genid;

#define	IF_DEBUG(l)	if (debug && debug & (l))

#define	DEBUG_PKT	0x0001
#define	DEBUG_PRUNE	0x0002
#define	DEBUG_ROUTE	0x0004
#define	DEBUG_PEER	0x0008
#define	DEBUG_CACHE	0x0010
#define	DEBUG_TIMEOUT	0x0020
#define	DEBUG_IF	0x0040
#define	DEBUG_MEMBER	0x0080
#define	DEBUG_TRACE	0x0100
#define	DEBUG_IGMP	0x0200
#define	DEBUG_RTDETAIL	0x0400
#define	DEBUG_KERN	0x0800
#define	DEBUG_RSRR	0x1000
#define	DEBUG_ICMP	0x2000

#define	DEFAULT_DEBUG	0x02de	/* default if "-d" given without value */

extern int		debug;
extern int		did_final_init;

extern int		routes_changed;
extern int		delay_change_reports;
extern unsigned		nroutes;

extern struct uvif	uvifs[MAXVIFS];
extern vifi_t		numvifs;
extern int		vifs_down;
extern int		udp_socket;
extern int		vifs_with_neighbors;

extern char		s1[];
extern char		s2[];
extern char		s3[];
extern char		s4[];

#if !(defined(BSD) && (BSD >= 199103))
extern int		errno;
extern int		sys_nerr;
extern char *		sys_errlist[];
#endif

#ifdef OLD_KERNEL
#define	MRT_INIT	DVMRP_INIT
#define	MRT_DONE	DVMRP_DONE
#define	MRT_ADD_VIF	DVMRP_ADD_VIF
#define	MRT_DEL_VIF	DVMRP_DEL_VIF
#define	MRT_ADD_MFC	DVMRP_ADD_MFC
#define	MRT_DEL_MFC	DVMRP_DEL_MFC
#endif

#ifndef IGMP_PIM
#define	IGMP_PIM	0x14
#endif
#ifndef IPPROTO_IPIP
#define	IPPROTO_IPIP	4
#endif

/*
 * The original multicast releases defined
 * IGMP_HOST_{MEMBERSHIP_QUERY,MEMBERSHIP_REPORT,NEW_MEMBERSHIP_REPORT
 *   ,LEAVE_MESSAGE}.  Later releases removed the HOST and inserted
 * the IGMP version number.  NetBSD inserted the version number in
 * a different way.  mrouted uses the new names, so we #define them
 * to the old ones if needed.
 */
#if !defined(IGMP_MEMBERSHIP_QUERY) && defined(IGMP_HOST_MEMBERSHIP_QUERY)
#define	IGMP_MEMBERSHIP_QUERY		IGMP_HOST_MEMBERSHIP_QUERY
#define	IGMP_V2_LEAVE_GROUP		IGMP_HOST_LEAVE_MESSAGE
#endif
#ifndef	IGMP_V1_MEMBERSHIP_REPORT
#ifdef	IGMP_HOST_MEMBERSHIP_REPORT
#define	IGMP_V1_MEMBERSHIP_REPORT	IGMP_HOST_MEMBERSHIP_REPORT
#define	IGMP_V2_MEMBERSHIP_REPORT	IGMP_HOST_NEW_MEMBERSHIP_REPORT
#endif
#ifdef	IGMP_v1_HOST_MEMBERSHIP_REPORT
#define	IGMP_V1_MEMBERSHIP_REPORT	IGMP_v1_HOST_MEMBERSHIP_REPORT
#define	IGMP_V2_MEMBERSHIP_REPORT	IGMP_v2_HOST_MEMBERSHIP_REPORT
#endif
#endif

/*
 * NetBSD also renamed the mtrace types.
 */
#if !defined(IGMP_MTRACE_RESP) && defined(IGMP_MTRACE_REPLY)
#define	IGMP_MTRACE_RESP		IGMP_MTRACE_REPLY
#define	IGMP_MTRACE			IGMP_MTRACE_QUERY
#endif

/* main.c */
extern char *		scaletime __P((u_long));
extern void		log __P((int, int, char *, ...));
extern int		register_input_handler __P((int, ihfunc_t));

/* igmp.c */
extern void		init_igmp __P((void));
extern void		accept_igmp __P((int));
extern void		build_igmp __P((u_int32, u_int32, int, int, u_int32,
						int));
extern void		send_igmp __P((u_int32, u_int32, int, int, u_int32,
						int));
extern char *		igmp_packet_kind __P((u_int, u_int));
extern int		igmp_debug_kind __P((u_int, u_int));

/* icmp.c */
extern void		init_icmp __P((void));

/* ipip.c */
extern void		init_ipip __P((void));
extern void		init_ipip_on_vif __P((struct uvif *));
extern void		send_ipip __P((u_int32, u_int32, int, int, u_int32,
						int, struct uvif *));

/* callout.c */
extern void		callout_init __P((void));
extern void		free_all_callouts __P((void));
extern void		age_callout_queue __P((int));
extern int		timer_nextTimer __P((void));
extern int		timer_setTimer __P((int, cfunc_t, void *));
extern int		timer_clearTimer __P((int));
extern int		timer_leftTimer __P((int));

/* route.c */
extern void		init_routes __P((void));
extern void		start_route_updates __P((void));
extern void		update_route __P((u_int32, u_int32, u_int, u_int32,
						vifi_t, struct listaddr *));
extern void		age_routes __P((void));
extern void		expire_all_routes __P((void));
extern void		free_all_routes __P((void));
extern void		accept_probe __P((u_int32, u_int32, char *, int,
						u_int32));
extern void		accept_report __P((u_int32, u_int32, char *, int,
						u_int32));
extern struct rtentry *	determine_route __P((u_int32 src));
extern void		report __P((int, vifi_t, u_int32));
extern void		report_to_all_neighbors __P((int));
extern int		report_next_chunk __P((void));
extern void		blaster_alloc __P((vifi_t));
extern void		add_vif_to_routes __P((vifi_t));
extern void		delete_vif_from_routes __P((vifi_t));
extern void		add_neighbor_to_routes __P((vifi_t, int));
extern void		delete_neighbor_from_routes __P((u_int32,
						vifi_t, int));
extern void		dump_routes __P((FILE *fp));

/* vif.c */
extern void		init_vifs __P((void));
extern void		zero_vif __P((struct uvif *, int));
extern void		init_installvifs __P((void));
extern void		check_vif_state __P((void));
extern void		send_on_vif __P((struct uvif *, u_int32, int, int));
extern vifi_t		find_vif __P((u_int32, u_int32));
extern void		age_vifs __P((void));
extern void		dump_vifs __P((FILE *));
extern void		stop_all_vifs __P((void));
extern struct listaddr *neighbor_info __P((vifi_t, u_int32));
extern void		accept_group_report __P((u_int32, u_int32,
					u_int32, int));
extern void		query_groups __P((void));
extern void		probe_for_neighbors __P((void));
extern struct listaddr *update_neighbor __P((vifi_t, u_int32, int, char *, int,
					u_int32));
extern void		accept_neighbor_request __P((u_int32, u_int32));
extern void		accept_neighbor_request2 __P((u_int32, u_int32));
extern void		accept_info_request __P((u_int32, u_int32,
					u_char *, int));
extern void		accept_info_reply __P((u_int32, u_int32,
					u_char *, int));
extern void		accept_neighbors __P((u_int32, u_int32,
					u_char *, int, u_int32));
extern void		accept_neighbors2 __P((u_int32, u_int32,
					u_char *, int, u_int32));
extern void		accept_leave_message __P((u_int32, u_int32,
					u_int32));
extern void		accept_membership_query __P((u_int32, u_int32,
					u_int32, int));

/* config.c */
extern void		config_vifs_from_kernel __P((void));

/* cfparse.y */
extern void		config_vifs_from_file __P((void));

/* inet.c */
extern int		inet_valid_host __P((u_int32));
extern int		inet_valid_mask __P((u_int32));
extern int		inet_valid_subnet __P((u_int32, u_int32));
extern char *		inet_fmt __P((u_int32, char *));
extern char *		inet_fmts __P((u_int32, u_int32, char *));
extern u_int32		inet_parse __P((char *, int));
extern int		inet_cksum __P((u_short *, u_int));

/* prune.c */
extern unsigned		kroutes;
extern void		determine_forwvifs __P((struct gtable *));
extern void		send_prune_or_graft __P((struct gtable *));
extern void		add_table_entry __P((u_int32, u_int32));
extern void 		del_table_entry __P((struct rtentry *,
					u_int32, u_int));
extern void		update_table_entry __P((struct rtentry *, u_int32));
extern int		find_src_grp __P((u_int32, u_int32, u_int32));
extern void		init_ktable __P((void));
extern void		steal_sources __P((struct rtentry *));
extern void		reset_neighbor_state __P((vifi_t, u_int32));
extern int		grplst_mem __P((vifi_t, u_int32));
extern void		free_all_prunes __P((void));
extern void 		age_table_entry __P((void));
extern void		dump_cache __P((FILE *));
extern void 		update_lclgrp __P((vifi_t, u_int32));
extern void		delete_lclgrp __P((vifi_t, u_int32));
extern void		chkgrp_graft __P((vifi_t, u_int32));
extern void 		accept_prune __P((u_int32, u_int32, char *, int));
extern void		accept_graft __P((u_int32, u_int32, char *, int));
extern void 		accept_g_ack __P((u_int32, u_int32, char *, int));
/* u_int is promoted u_char */
extern void		accept_mtrace __P((u_int32, u_int32,
					u_int32, char *, u_int, int));

/* kern.c */
extern void		k_set_rcvbuf __P((int, int));
extern void		k_hdr_include __P((int));
extern void		k_set_ttl __P((int));
extern void		k_set_loop __P((int));
extern void		k_set_if __P((u_int32));
extern void		k_join __P((u_int32, u_int32));
extern void		k_leave __P((u_int32, u_int32));
extern void		k_init_dvmrp __P((void));
extern void		k_stop_dvmrp __P((void));
extern void		k_add_vif __P((vifi_t, struct uvif *));
extern void		k_del_vif __P((vifi_t));
extern void		k_add_rg __P((u_int32, struct gtable *));
extern int		k_del_rg __P((u_int32, struct gtable *));
extern int		k_get_version __P((void));

#ifdef SNMP
/* prune.c */
extern struct gtable *	find_grp __P((u_int32));
extern struct stable *	find_grp_src __P((struct gtable *, u_int32));
extern int		next_grp_src_mask __P((struct gtable **,
					struct stable **, u_int32,
					u_int32, u_int32));
extern void		refresh_sg __P((struct sioc_sg_req *, struct gtable *,
					struct stable *));
extern int		next_child __P((struct gtable **, struct stable **,
					u_int32, u_int32, u_int32,
					vifi_t *));

/* route.c */
extern struct rtentry * snmp_find_route __P((u_int32, u_int32));
extern int		next_route __P((struct rtentry **, u_int32, u_int32));
extern int		next_route_child __P((struct rtentry **,
				u_int32, u_int32, vifi_t *));
#endif

#ifdef RSRR
/* prune.c */
extern struct gtable	*kernel_table;
extern struct gtable	*gtp;

/* rsrr.c */
extern void		rsrr_init __P((void));
extern void		rsrr_clean __P((void));
extern void		rsrr_cache_send __P((struct gtable *, int));
extern void		rsrr_cache_clean __P((struct gtable *));
#endif /* RSRR */
