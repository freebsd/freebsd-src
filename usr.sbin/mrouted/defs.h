/*
 * The mrouted program is covered by the license in the accompanying file
 * named "LICENSE".  Use of the mrouted program represents acceptance of
 * the terms and conditions listed in that file.
 *
 * The mrouted program is COPYRIGHT 1989 by The Board of Trustees of
 * Leland Stanford Junior University.
 *
 *
 * $Id: defs.h,v 1.8 1994/08/24 23:53:23 thyagara Exp $
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

#include "dvmrp.h"
#include "vif.h"
#include "route.h"
#include "prune.h"

/*
 * Miscellaneous constants and macros.
 */
#define FALSE		0
#define TRUE		1

#define EQUAL(s1, s2)	(strcmp((s1), (s2)) == 0)

#define TIMER_INTERVAL	ROUTE_MAX_REPORT_DELAY

#define PROTOCOL_VERSION 3  /* increment when packet format/content changes */

#define MROUTED_VERSION  3  /* increment on local changes or bug fixes, */
			    /* reset to 0 whever PROTOCOL_VERSION increments */

#define MROUTED_LEVEL ( (MROUTED_VERSION << 8) | PROTOCOL_VERSION )
			    /* for IGMP 'group' field of DVMRP messages */

#define	DEL_RTE_GROUP		0
#define	DEL_ALL_ROUTES		1
			    /* for Deleting kernel table entries */

/*
 * External declarations for global variables and functions.
 */
extern char		recv_buf[MAX_IP_PACKET_LEN];
extern char		send_buf[MAX_IP_PACKET_LEN];
extern int		igmp_socket;
extern u_long		allhosts_group;
extern u_long		dvmrp_group;
extern u_long		dvmrp_genid;

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

extern char		s1[];
extern char		s2[];
extern char		s3[];

extern int		errno;
extern int		sys_nerr;
#ifndef __FreeBSD__
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

extern void		accept_group_report();
extern void		query_groups();
extern void		probe_for_neighbors();
extern int		update_neighbor();
extern void		accept_neighbor_request();

extern void		config_vifs_from_kernel();
extern void		config_vifs_from_file();

extern int		inet_valid_host();
extern int		inet_valid_subnet();
extern char *		inet_fmt();
extern char *		inet_fmts();
extern u_long		inet_parse();
extern int		inet_cksum();

extern struct rtentry *	determine_route();

extern void		init_ktable();
extern int 		grplst_mem();
extern void		add_table_entry();
extern void 		del_table_entry();
extern void		update_table_entry();
extern void 		update_lclgrp();
extern void		delete_lclgrp();

extern unsigned		kroutes;
extern void 		send_prune();
extern void 		accept_prune();
extern int		no_entry_exists();
extern struct ktable *	find_src_grp();
extern int 		rtr_cnt();
extern void		free_all_prunes();
extern void 		age_table_entry();
extern void		dump_cache();

extern void		chkgrp_graft();
extern void		accept_graft();
extern void		send_graft_ack();
extern void		send_graft();
extern void 		accept_g_ack();
extern void		mtrace();

extern char *		malloc();
extern char *		fgets();
extern FILE *		fopen();

#ifndef htonl
extern u_long		htonl();
extern u_long		ntohl();
#endif
