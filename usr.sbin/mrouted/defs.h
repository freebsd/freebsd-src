/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: defs.h,v 3.8 1995/11/29 22:36:34 fenner Rel $
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
#include <sys/time.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/igmp.h>
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

#define VENDOR_CODE	1   /* Get a new vendor code if you make significant
			     * changes to mrouted. */

#define PROTOCOL_VERSION 3  /* increment when packet format/content changes */

#define MROUTED_VERSION  8  /* increment on local changes or bug fixes, */
			    /* reset to 0 whever PROTOCOL_VERSION increments */

#define MROUTED_LEVEL  ((MROUTED_VERSION << 8) | PROTOCOL_VERSION | \
			((NF_PRUNE | NF_GENID | NF_MTRACE) << 16) | \
			(VENDOR_CODE << 24))
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
#define signal(s,f)	sigset(s,f)
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

#define DEFAULT_DEBUG  2	/* default if "-d" given without value */

extern int		debug;
extern u_char		pruning;

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

#define	IGMP_PIM	0x14
#endif

/* main.c */
extern void		log __P((int, int, char *, ...));
extern int		register_input_handler __P((int fd, ihfunc_t func));

/* igmp.c */
extern void		init_igmp __P((void));
extern void		accept_igmp __P((int recvlen));
extern void		send_igmp __P((u_int32 src, u_int32 dst, int type,
						int code, u_int32 group,
						int datalen));

/* callout.c */
extern void		callout_init __P((void));
extern void		age_callout_queue __P((void));
extern int		timer_setTimer __P((int delay, cfunc_t action,
						char *data));
extern void		timer_clearTimer __P((int timer_id));

/* route.c */
extern void		init_routes __P((void));
extern void		start_route_updates __P((void));
extern void		update_route __P((u_int32 origin, u_int32 mask,
						u_int metric, u_int32 src,
						vifi_t vifi));
extern void		age_routes __P((void));
extern void		expire_all_routes __P((void));
extern void		free_all_routes __P((void));
extern void		accept_probe __P((u_int32 src, u_int32 dst,
						char *p, int datalen,
						u_int32 level));
extern void		accept_report __P((u_int32 src, u_int32 dst,
						char *p, int datalen,
						u_int32 level));
extern struct rtentry *	determine_route __P((u_int32 src));
extern void		report __P((int which_routes, vifi_t vifi,
						u_int32 dst));
extern void		report_to_all_neighbors __P((int which_routes));
extern int		report_next_chunk __P((void));
extern void		add_vif_to_routes __P((vifi_t vifi));
extern void		delete_vif_from_routes __P((vifi_t vifi));
extern void		delete_neighbor_from_routes __P((u_int32 addr,
							vifi_t vifi));
extern void		dump_routes __P((FILE *fp));
extern void		start_route_updates __P((void));

/* vif.c */
extern void		init_vifs __P((void));
extern void		check_vif_state __P((void));
extern vifi_t		find_vif __P((u_int32 src, u_int32 dst));
extern void		age_vifs __P((void));
extern void		dump_vifs __P((FILE *fp));
extern void		stop_all_vifs __P((void));
extern struct listaddr *neighbor_info __P((vifi_t vifi, u_int32 addr));
extern void		accept_group_report __P((u_int32 src, u_int32 dst,
					u_int32 group, int r_type));
extern void		query_groups __P((void));
extern void		probe_for_neighbors __P((void));
extern int		update_neighbor __P((vifi_t vifi, u_int32 addr,
					int msgtype, char *p, int datalen,
					u_int32 level));
extern void		accept_neighbor_request __P((u_int32 src, u_int32 dst));
extern void		accept_neighbor_request2 __P((u_int32 src,
					u_int32 dst));
extern void		accept_neighbors __P((u_int32 src, u_int32 dst,
					u_char *p, int datalen, u_int32 level));
extern void		accept_neighbors2 __P((u_int32 src, u_int32 dst,
					u_char *p, int datalen, u_int32 level));
extern void		accept_leave_message __P((u_int32 src, u_int32 dst,
					u_int32 group));
extern void		accept_membership_query __P((u_int32 src, u_int32 dst,
					u_int32 group, int tmo));

/* config.c */
extern void		config_vifs_from_kernel __P((void));

/* cfparse.y */
extern void		config_vifs_from_file __P((void));

/* inet.c */
extern int		inet_valid_host __P((u_int32 naddr));
extern int		inet_valid_subnet __P((u_int32 nsubnet, u_int32 nmask));
extern char *		inet_fmt __P((u_int32 addr, char *s));
extern char *		inet_fmts __P((u_int32 addr, u_int32 mask, char *s));
extern u_int32		inet_parse __P((char *s));
extern int		inet_cksum __P((u_short *addr, u_int len));

/* prune.c */
extern unsigned		kroutes;
extern void		add_table_entry __P((u_int32 origin, u_int32 mcastgrp));
extern void 		del_table_entry __P((struct rtentry *r,
					u_int32 mcastgrp, u_int del_flag));
extern void		update_table_entry __P((struct rtentry *r));
extern void		init_ktable __P((void));
extern void 		accept_prune __P((u_int32 src, u_int32 dst, char *p,
					int datalen));
extern void		steal_sources __P((struct rtentry *rt));
extern void		reset_neighbor_state __P((vifi_t vifi, u_int32 addr));
extern int		grplst_mem __P((vifi_t vifi, u_int32 mcastgrp));
extern int		scoped_addr __P((vifi_t vifi, u_int32 addr));
extern void		free_all_prunes __P((void));
extern void 		age_table_entry __P((void));
extern void		dump_cache __P((FILE *fp2));
extern void 		update_lclgrp __P((vifi_t vifi, u_int32 mcastgrp));
extern void		delete_lclgrp __P((vifi_t vifi, u_int32 mcastgrp));
extern void		chkgrp_graft __P((vifi_t vifi, u_int32 mcastgrp));
extern void		accept_graft __P((u_int32 src, u_int32 dst, char *p,
					int datalen));
extern void 		accept_g_ack __P((u_int32 src, u_int32 dst, char *p,
					int datalen));
/* u_int is promoted u_char */
extern void		accept_mtrace __P((u_int32 src, u_int32 dst,
					u_int32 group, char *data, u_int no,
					int datalen));

/* kern.c */
extern void		k_set_rcvbuf __P((int bufsize));
extern void		k_hdr_include __P((int bool));
extern void		k_set_ttl __P((int t));
extern void		k_set_loop __P((int l));
extern void		k_set_if __P((u_int32 ifa));
extern void		k_join __P((u_int32 grp, u_int32 ifa));
extern void		k_leave __P((u_int32 grp, u_int32 ifa));
extern void		k_init_dvmrp __P((void));
extern void		k_stop_dvmrp __P((void));
extern void		k_add_vif __P((vifi_t vifi, struct uvif *v));
extern void		k_del_vif __P((vifi_t vifi));
extern void		k_add_rg __P((u_int32 origin, struct gtable *g));
extern int		k_del_rg __P((u_int32 origin, struct gtable *g));
extern int		k_get_version __P((void));

#ifdef SNMP
/* prune.c */
extern struct rtentry * snmp_find_route __P(());
extern struct gtable *	find_grp __P(());
extern struct stable *	find_grp_src __P(());
#endif

#ifdef RSRR
/* prune.c */
extern struct gtable	*kernel_table;
extern struct gtable	*gtp;
extern int		find_src_grp __P((u_int32 src, u_int32 mask,
					u_int32 grp));

/* rsrr.c */
extern void		rsrr_init __P((void));
extern void		rsrr_read __P((int f, fd_set *rfd));
extern void		rsrr_clean __P((void));
extern void		rsrr_cache_send __P((struct gtable *gt, int notify));
extern void		rsrr_cache_clean __P((struct gtable *gt));
#endif /* RSRR */
