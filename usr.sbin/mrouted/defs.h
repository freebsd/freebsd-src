/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: defs.h,v 3.5 1995/05/09 01:00:39 fenner Exp $
 */


#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
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

#define PROTOCOL_VERSION 3  /* increment when packet format/content changes */

#define MROUTED_VERSION  5  /* increment on local changes or bug fixes, */
			    /* reset to 0 whever PROTOCOL_VERSION increments */

#define MROUTED_LEVEL ( (MROUTED_VERSION << 8) | PROTOCOL_VERSION | \
			((NF_PRUNE | NF_GENID | NF_MTRACE) << 16))
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
#endif

#ifdef RSRR
#define BIT_ZERO(X)      ((X) = 0)
#define BIT_SET(X,n)     ((X) |= 1 << (n))
#define BIT_CLR(X,n)     ((X) &= ~(1 << (n)))
#define BIT_TST(X,n)     ((X) & 1 << (n))
#endif /* RSRR */

/*
 * External declarations for global variables and functions.
 */
#define RECV_BUF_SIZE MAX_IP_PACKET_LEN
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

extern void		log();

extern void		init_igmp();
extern void		accept_igmp();
extern void		send_igmp();

extern void		init_routes();
extern void		start_route_updates();
extern void		update_route();
extern void		age_routes();
extern void		expire_all_routes();
extern void		free_all_routes();

extern void		accept_probe();
extern void		accept_report();
extern void		report();
extern void		report_to_all_neighbors();
extern int		report_next_chunk();
extern void		add_vif_to_routes();
extern void		delete_vif_from_routes();
extern void		delete_neighbor_from_routes();
extern void		dump_routes();

extern void		init_vifs();
extern void		check_vif_state();
extern vifi_t		find_vif();
extern void		age_vifs();
extern void		dump_vifs();
extern void		stop_all_vifs();
extern struct listaddr *neighbor_info();

extern void		accept_group_report();
extern void		query_groups();
extern void		probe_for_neighbors();
extern int		update_neighbor();
extern void		accept_neighbor_request();
extern void		accept_neighbor_request2();
extern void		accept_neighbors();
extern void		accept_neighbors2();

extern void		config_vifs_from_kernel();
extern void		config_vifs_from_file();

extern int		inet_valid_host();
extern int		inet_valid_subnet();
extern char *		inet_fmt();
extern char *		inet_fmts();
extern u_int32		inet_parse();
extern int		inet_cksum();

extern struct rtentry *	determine_route();

extern void		init_ktable();
extern void		add_table_entry();
extern void 		del_table_entry();
extern void		update_table_entry();
extern void 		update_lclgrp();
extern void		delete_lclgrp();

extern unsigned		kroutes;
extern void 		accept_prune();
extern int		no_entry_exists();
extern int 		rtr_cnt();
extern void		free_all_prunes();
extern void 		age_table_entry();
extern void		dump_cache();

#ifdef SNMP
extern struct rtentry * snmp_find_route();
extern struct gtable *	find_grp();
extern struct stable *	find_grp_src();
#endif

extern void		chkgrp_graft();
extern void		accept_graft();
extern void		send_graft_ack();
extern void 		accept_g_ack();
extern void		accept_mtrace();
extern void		accept_leave_message();
extern void		accept_membership_query();
#ifdef RSRR
extern struct gtable	*kernel_table;
extern struct gtable	*gtp;
extern int		find_src_grp();
extern int		grplst_mem();
extern int		scoped_addr();
#endif /* RSRR */

extern void		k_set_rcvbuf();
extern void		k_hdr_include();
extern void		k_set_ttl();
extern void		k_set_loop();
extern void		k_set_if();
extern void		k_join();
extern void		k_leave();
extern void		k_init_dvmrp();
extern void		k_stop_dvmrp();
extern void		k_add_vif();
extern void		k_del_vif();
extern void		k_add_rg();
extern int		k_del_rg();
extern int		k_get_version();

extern char *		malloc();
extern char *		fgets();
extern FILE *		fopen();

#if !defined(htonl) && !defined(__osf__)
extern u_long		htonl();
extern u_long		ntohl();
#endif

#ifdef RSRR
extern void		rsrr_init();
extern void		rsrr_read();
#endif /* RSRR */
