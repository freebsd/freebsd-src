/*
 *
 * ===================================
 * HARP  |  Host ATM Research Platform
 * ===================================
 *
 *
 * This Host ATM Research Platform ("HARP") file (the "Software") is
 * made available by Network Computing Services, Inc. ("NetworkCS")
 * "AS IS".  NetworkCS does not provide maintenance, improvements or
 * support of any kind.
 *
 * NETWORKCS MAKES NO WARRANTIES OR REPRESENTATIONS, EXPRESS OR IMPLIED,
 * INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE, AS TO ANY ELEMENT OF THE
 * SOFTWARE OR ANY SUPPORT PROVIDED IN CONNECTION WITH THIS SOFTWARE.
 * In no event shall NetworkCS be responsible for any damages, including
 * but not limited to consequential damages, arising from or relating to
 * any use of the Software or related support.
 *
 * Copyright 1994-1998 Network Computing Services, Inc.
 *
 * Copies of this Software may be made, however, the above copyright
 * notice must be reproduced on all copies.
 *
 *	@(#) $FreeBSD$
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP-ATMARP server interface: control blocks
 *
 */

#ifndef _ATMARP_ATMARP_VAR_H
#define _ATMARP_ATMARP_VAR_H

#ifndef TRUE
#define	TRUE	1
#endif

#ifndef FALSE
#define	FALSE	0
#endif

/*
 * Operational constants
 */
#define	ATMARP_DIR			"/tmp"
#define	ATMARP_SOCK_PREFIX		"AA_"
#define	ATMARP_CACHE_INTERVAL		50
#define	ATMARP_PERM_INTERVAL		600
#define	ATMARP_KEEPALIVE_INTERVAL	5


/*
 * Macros for manipulating ATMARP tables and entries
 */
#define	ATMARP_HASHSIZ		19	/* Hash table size */

#define	ATMARP_HASH(ip)	((u_long)(ip) % ATMARP_HASHSIZ)

#define ATMARP_ADD(ai, aa)					\
{								\
	Atmarp	**h;						\
	h = &ai->ai_arptbl[ATMARP_HASH((aa)->aa_dstip.s_addr)];	\
	LINK2TAIL((aa), Atmarp, *h, aa_next);			\
}

#define ATMARP_DELETE(ai, aa)					\
{								\
	Atmarp	**h;						\
	h = &ai->ai_arptbl[ATMARP_HASH((aa)->aa_dstip.s_addr)];	\
	UNLINK((aa), Atmarp, *h, aa_next);			\
}

#define ATMARP_LOOKUP(ai, ip, aa)				\
{								\
	for ((aa) = (ai)->ai_arptbl[ATMARP_HASH(ip)];		\
				(aa); (aa) = (aa)->aa_next) {	\
		if ((aa)->aa_dstip.s_addr == (ip))		\
			break;					\
	}							\
}


/*
 * Macro to compare originator ID structures
 */
#define	OID_EQUAL(id1, id2)					\
	(((id1)->id_len == (id2)->id_len) &&			\
	(bcmp((caddr_t)(id1)->id,				\
	 	(caddr_t)(id2)->id,				\
		(id1)->id_len) == 0))

#define	KEY_EQUAL(key1, key2)					\
	(((key1)->key_len == (key2)->key_len) &&		\
	(bcmp((caddr_t)(key1)->key,				\
	 	(caddr_t)(key2)->key,				\
		(key1)->key_len) == 0))


/*
 * Interface entry for ATMARP SCSP interface daemon
 */
struct atmarp_intf {
	struct atmarp_intf	*ai_next;	/* Next chained I/F */
	char		ai_intf[IFNAMSIZ];	/* Network I/F name */
	struct in_addr	ai_ip_addr;		/* IP address */
	struct in_addr 	ai_subnet_mask;        	/* Subnet mask */
	int		ai_mtu;			/* IP MTU */
	Atm_addr	ai_atm_addr;		/* ATM address */
	Atm_addr	ai_atm_subaddr;		/* ATM subaddress */
	int		ai_scsp_sock;		/* Socket to SCSP */
	Harp_timer	ai_keepalive_t;		/* Keepalive timer */
	char		*ai_scsp_sockname;	/* Socket name */
	u_char		ai_state;		/* Interface state */
	u_char		ai_mark;
	struct atmarp	*ai_arptbl[ATMARP_HASHSIZ];	/* ARP cache */
};
typedef	struct atmarp_intf	Atmarp_intf;

#define	AI_STATE_NULL	0
#define	AI_STATE_UP	1


/*
 * Super-LIS control block for ATMARP server daemon
 */
struct atmarp_slis {
	struct atmarp_slis	*as_next;	/* Next super-LIS */
	char			*as_name;	/* Name of super-LIS */
	int			as_cnt;		/* LIS count */
	Atmarp_intf		*as_intfs;	/* List of intfs */
};
typedef	struct atmarp_slis	Atmarp_slis;


/*
 * ATMARP cache entry format
 */
struct atmarp {
	struct atmarp	*aa_next;	/* Hash chain link */
	struct in_addr	aa_dstip;	/* Destination IP addr */
	Atm_addr	aa_dstatm;	/* Destination ATM addr */
	Atm_addr	aa_dstatmsub;	/* Destination ATM subaddr */
	struct scsp_ckey aa_key;	/* SCSP cache key */
	struct scsp_id	aa_oid;		/* SCSP originator ID */
	long		aa_seq;		/* SCSP sequence no. */
	Atmarp_intf	*aa_intf;	/* Interface for entry */
	u_char		aa_flags;	/* Flags (see below) */
	u_char		aa_origin;	/* Entry origin */
	char		aa_mark;	/* Mark */
};
typedef	struct atmarp	Atmarp;

/*
 * ATMARP Entry Flags
 */
#define	AAF_PERM	0x01	/* Entry is permanent */
#define	AAF_SERVER	0x02	/* Entry is for the server */


/*
 * Global variables
 */
extern char		*prog;
extern int		atmarp_debug_mode;
extern int		atmarp_max_socket;
extern Atmarp_intf	*atmarp_intf_head;
extern Atmarp_slis	*atmarp_slis_head;
extern FILE		*atmarp_log_file;


/*
 * Function definitions
 */

/* atmarp_config.c */
extern int	atmarp_cfg_netif(char *);

/* atmarp_log.c */
#if __STDC__
extern void	atmarp_log(const int, const char *, ...);
#else
extern void	atmarp_log(int, char *, va_alist);
#endif
extern void	atmarp_mem_err(char *);

/* atmarp_scsp.c */
extern int	atmarp_scsp_cache(Atmarp_intf *, Scsp_if_msg *);
extern int	atmarp_scsp_update(Atmarp *, int);
extern int	atmarp_scsp_update_in(Atmarp_intf *, Scsp_if_msg *);
extern int	atmarp_scsp_read(Atmarp_intf *);
extern int	atmarp_scsp_out(Atmarp_intf *, char *, int);
extern int	atmarp_scsp_connect(Atmarp_intf *);
extern void	atmarp_scsp_close(Atmarp_intf *);
extern int	atmarp_scsp_disconnect(Atmarp_intf *);

/* atmarp_subr.c */
extern Atmarp_intf	 *atmarp_find_intf_sock(int);
extern Atmarp_intf	 *atmarp_find_intf_name(char *);
extern void	atmarp_clear_marks();
extern int	atmarp_is_server(Atmarp_intf *);
extern int	atmarp_if_ready(Atmarp_intf *);
extern Atmarp *	atmarp_copy_cache_entry(struct air_arp_rsp *);
extern int	atmarp_update_kernel(Atmarp *);
extern void	atmarp_get_updated_cache();
extern void	atmarp_process_cache_entry(struct air_arp_rsp *);
extern void	print_atmarp_intf(FILE *, Atmarp_intf *);
extern void	print_atmarp_cache(FILE *, Atmarp *);
extern void	dump_atmarp_cache(FILE *, Atmarp_intf *);
extern void	atmarp_sigint(int);

/* atmarp_timer.c */
extern void	atmarp_cache_timeout(Harp_timer *);
extern void	atmarp_perm_timeout(Harp_timer *);
extern void	atmarp_keepalive_timeout(Harp_timer *);


#endif	/* _ATMARP_ATMARP_VAR_H */
