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
 *	@(#) $FreeBSD: src/usr.sbin/atm/scspd/scsp_var.h,v 1.3 1999/08/28 01:15:34 peter Exp $
 *
 */

/*
 * Server Cache Synchronization Protocol (SCSP) Support
 * ----------------------------------------------------
 *
 * SCSP message formats
 *
 */

#ifndef _SCSP_SCSP_VAR_H
#define _SCSP_SCSP_VAR_H


/*
 * Protocol constants
 */
#define	SCSP_Open_Interval	30
#define	SCSP_HELLO_Interval	3
#define	SCSP_HELLO_DF		3
#define	SCSP_CAReXmitInterval	3
#define	SCSP_CSUSReXmitInterval	3
#define	SCSP_CSA_HOP_CNT	3
#define	SCSP_CSUReXmitInterval	2
#define	SCSP_CSUReXmitMax	5


/*
 * Operational constants
 */
#define SCSPD_CONFIG	"/etc/scspd.conf"
#define	SCSPD_DIR	"/tmp"
#define SCSPD_DUMP	"/tmp/scspd.dump"
#define SCSP_HASHSZ	19
#define	SCSPD_SOCK_NAME	"SCSPD"


/*
 * HELLO finite state machine states
 */
#define	SCSP_HFSM_DOWN		0
#define	SCSP_HFSM_WAITING	1
#define	SCSP_HFSM_UNI_DIR	2
#define	SCSP_HFSM_BI_DIR	3
#define	SCSP_HFSM_STATE_CNT	SCSP_HFSM_BI_DIR + 1


/*
 * HELLO finite state machine events
 */
#define	SCSP_HFSM_VC_ESTAB	0
#define	SCSP_HFSM_VC_CLOSED	1
#define	SCSP_HFSM_HELLO_T	2
#define	SCSP_HFSM_RCV_T		3
#define	SCSP_HFSM_RCVD		4
#define	SCSP_HFSM_EVENT_CNT	SCSP_HFSM_RCVD + 1


/*
 * Cache Alignment finite state machine states
 */
#define	SCSP_CAFSM_DOWN		0
#define	SCSP_CAFSM_NEG		1
#define	SCSP_CAFSM_MASTER	2
#define	SCSP_CAFSM_SLAVE	3
#define	SCSP_CAFSM_UPDATE	4
#define	SCSP_CAFSM_ALIGNED	5
#define	SCSP_CAFSM_STATE_CNT	SCSP_CAFSM_ALIGNED + 1


/*
 * Cache Alignment finite state machine events
 */
#define	SCSP_CAFSM_HELLO_UP	0
#define	SCSP_CAFSM_HELLO_DOWN	1
#define	SCSP_CAFSM_CA_MSG	2
#define	SCSP_CAFSM_CSUS_MSG	3
#define	SCSP_CAFSM_CSU_REQ	4
#define	SCSP_CAFSM_CSU_REPLY	5
#define	SCSP_CAFSM_CA_T		6
#define	SCSP_CAFSM_CSUS_T	7
#define	SCSP_CAFSM_CSU_T	8
#define	SCSP_CAFSM_CACHE_UPD	9
#define	SCSP_CAFSM_CACHE_RSP	10
#define	SCSP_CAFSM_EVENT_CNT	SCSP_CAFSM_CACHE_RSP + 1


/*
 * Client Interface finite state machine states
 */
#define	SCSP_CIFSM_NULL		0
#define	SCSP_CIFSM_SUM		1
#define	SCSP_CIFSM_UPD		2
#define	SCSP_CIFSM_ALIGN	3
#define	SCSP_CIFSM_STATE_CNT		SCSP_CIFSM_ALIGN + 1


/*
 * Client Interface finite state machine events
 */
#define	SCSP_CIFSM_CA_DOWN	0
#define	SCSP_CIFSM_CA_SUMM	1
#define	SCSP_CIFSM_CA_UPD	2
#define	SCSP_CIFSM_CA_ALIGN	3
#define	SCSP_CIFSM_SOL_RSP	4
#define	SCSP_CIFSM_UPD_REQ	5
#define	SCSP_CIFSM_UPD_RSP	6
#define	SCSP_CIFSM_CSU_REQ	7
#define	SCSP_CIFSM_CSU_REPLY	8
#define	SCSP_CIFSM_CSU_SOL	9
#define	SCSP_CIFSM_EVENT_CNT	SCSP_CIFSM_CSU_SOL + 1


/*
 * Server connection states (not part of any FSM)
 */
#define	SCSP_SS_NULL	0
#define	SCSP_SS_CFG	1
#define	SCSP_SS_ACTIVE	2


/*
 * Hash a cache key
 *
 *	key	pointer to an Scsp_ckey structure
 */
#define SCSP_HASH(key)	scsp_hash((key))


/*
 * Add a cache summary entry to a client's cache summary
 *
 *	cpp	pointer to a server control block
 *	key	pointer to an Scsp_cse structure
 */
#define SCSP_ADD(cpp, key)					\
{								\
	Scsp_cse	**c;					\
	c = &(cpp)->ss_cache[SCSP_HASH(&(key)->sc_key)];	\
	LINK2TAIL((key), Scsp_cse, *c, sc_next);		\
}


/*
 * Delete a cache summary entry from a client's cache summary
 *
 *	cpp	pointer to a server control block
 *	s	pointer to an Scsp_cse structure
 */
#define SCSP_DELETE(cpp, s)				\
{							\
	Scsp_cse	**c;				\
	c = &(cpp)->ss_cache[SCSP_HASH(&(s)->sc_key)];	\
	UNLINK((s), Scsp_cse, *c, sc_next);		\
}


/*
 * Search a client's cache summary for a given key
 *
 *	cpp	pointer to a server control block
 *	key	pointer to an Scsp_ckey structure to find
 *	s	Scsp_cse structure pointer to be set
 */
#define SCSP_LOOKUP(cpp, key, s)				\
{								\
	for ((s) = (cpp)->ss_cache[SCSP_HASH(key)];		\
			(s);					\
			(s) = (s)->sc_next) {			\
		if (scsp_cmp_key((key), &(s)->sc_key) == 0)	\
			break;					\
	}							\
}


/*
 * SCSP pending connection control block
 *
 * The pending connection block is used to keep track of server
 * connections which are open but haven't been identified yet.
 */
struct scsp_pending {
	struct scsp_pending	*sp_next;
	int			sp_sock;
};
typedef	struct scsp_pending	Scsp_pending;


/*
 * SCSP Server instance control block
 */
struct scsp_server {
	struct scsp_server	*ss_next;	/* Server chain */
	char		*ss_name;		/* Server name */
	char		ss_intf[IFNAMSIZ];	/* Interface */
	Atm_media	ss_media;	/* Physical comm medium */
	char		ss_state;	/* Server connection state */
	u_long		ss_pid;		/* Protocol ID */
	int		ss_id_len;	/* ID length */
	int		ss_ckey_len;	/* Cache key length */
	u_long		ss_sgid;	/* Server group ID */
	u_long		ss_fid;		/* Family ID */
	int		ss_sock;	/* Socket to client */
	int		ss_dcs_lsock;	/* DCS listen socket */
	Scsp_id		ss_lsid;	/* Local Server ID */
	Atm_addr	ss_addr;	/* Local ATM addr */
	Atm_addr	ss_subaddr;	/* Local ATM subaddr */
	int		ss_mtu;		/* Interface MTU */
	int		ss_mark;
	struct scsp_dcs	*ss_dcs;	/* Ptr to list of DCSs */
	struct scsp_cse	*ss_cache[SCSP_HASHSZ];	/* Client's cache */
};
typedef	struct scsp_server	Scsp_server;


/*
 * SCSP client cache summary entry control block
 */
struct scsp_cse {
	struct scsp_cse	*sc_next;	/* Next on chain */
	long		sc_seq;		/* CSA sequence no */
	Scsp_ckey	sc_key;		/* Cache key */
	Scsp_id		sc_oid;		/* Origin ID */
};
typedef	struct scsp_cse	Scsp_cse;


/*
 * CSU Request retransmission control block
 */
struct scsp_csu_rexmt {
	struct scsp_csu_rexmt	*sr_next;	/* Next rexmit block */
	struct scsp_dcs		*sr_dcs;	/* DCS block */
	Scsp_csa		*sr_csa;	/* CSAs for rexmit */
	Harp_timer		sr_t;		/* Rexmit timer */
};
typedef	struct scsp_csu_rexmt	Scsp_csu_rexmt;


/*
 * SCSP DCS control block
 */
struct scsp_dcs {
	struct scsp_dcs	*sd_next;	/* DCS chain */
	Scsp_server	*sd_server;	/* Local server */
	Scsp_id		sd_dcsid;	/* DCS ID */
	Atm_addr	sd_addr;	/* DCS ATM address */
	Atm_addr	sd_subaddr;	/* DCS ATM subaddress */
	int		sd_sock;	/* Socket to DCS */
	Harp_timer	sd_open_t;	/* Open VCC retry timer */
	int		sd_hello_state;	/* Hello FSM state */
	int		sd_hello_int;	/* Hello interval */
	int		sd_hello_df;	/* Hello dead factor */
	int		sd_hello_rcvd;	/* Hello msg received */
	Harp_timer	sd_hello_h_t;	/* Hello timer */
	Harp_timer	sd_hello_rcv_t;	/* Hello receive timer */
	int		sd_ca_state;	/* CA FSM state */
	long		sd_ca_seq;	/* CA sequence number */
	int		sd_ca_rexmt_int;	/* CA rexmit interval */
	Scsp_msg	*sd_ca_rexmt_msg;	/* Saved CA msg */
	Scsp_cse	*sd_ca_csas;		/* CSAS still to send */
	Harp_timer	sd_ca_rexmt_t;		/* CA rexmit timer */
	int		sd_csus_rexmt_int;	/* CSUS rexmit int */
	Scsp_csa	*sd_crl;		/* Cache req list */
	Scsp_msg	*sd_csus_rexmt_msg;	/* Saved CSUS msg */
	Harp_timer	sd_csus_rexmt_t;	/* CSUS rexmit timer */
	int		sd_hops;		/* CSA hop count */
	Scsp_csa	*sd_csu_ack_pend;	/* CSUs to be ACKed */
	Scsp_csa	*sd_csu_ack;		/* CSUs ACKed */
	int		sd_csu_rexmt_int;	/* CSU Req rxmt time */
	int		sd_csu_rexmt_max;	/* CSU Req rxmt limit */
	Scsp_csu_rexmt	*sd_csu_rexmt;		/* CSU Req rxmt queue */
	int		sd_client_state;	/* Client I/F state */
};
typedef	struct scsp_dcs	Scsp_dcs;

/*
 * Trace options
 */
#define	SCSP_TRACE_HFSM		1	/* Trace the Hello FSM */
#define	SCSP_TRACE_CAFSM	2	/* Trace the CA FSM */
#define	SCSP_TRACE_CFSM		4	/* Trace the server I/F FSM */
#define	SCSP_TRACE_HELLO_MSG	8	/* Trace Hello protocol msgs */
#define	SCSP_TRACE_CA_MSG	16	/* Trace CA protocol msgs */
#define	SCSP_TRACE_IF_MSG	32	/* Trace server I/F msgs */


/*
 * Global variables
 */
extern char		*prog;
extern FILE		*cfg_file;
extern int		parse_line;
extern char		*scsp_config_file;
extern FILE		*scsp_log_file;
extern int		scsp_log_syslog;
extern Scsp_server	*scsp_server_head;
extern Scsp_pending	*scsp_pending_head;
extern int		scsp_max_socket;
extern int		scsp_debug_mode;
extern int		scsp_trace_mode;
extern FILE		*scsp_trace_file;


/*
 * Executable functions
 */
/* scsp_cafsm.c */
extern int	scsp_cafsm __P((Scsp_dcs *, int, void *));

/* scsp_config.c */
extern int	scsp_config __P((char *));
extern int	start_dcs __P((void));
extern int	finish_dcs __P((void));
extern int	set_dcs_addr __P((char *, char *));
extern int	set_dcs_ca_rexmit __P((int));
extern int	set_dcs_csus_rexmit __P((int));
extern int	set_dcs_csu_rexmit __P((int));
extern int	set_dcs_csu_rexmit_max __P((int));
extern int	set_dcs_hello_df __P((int));
extern int	set_dcs_hello_int __P((int));
extern int	set_dcs_hops __P((int));
extern int	set_dcs_id __P((char *));
extern int	set_intf __P((char *));
extern int	set_protocol __P((int));
extern int	set_server_group __P((int));
extern int	start_server __P((char *));
extern int	finish_server __P((void));
extern int	set_log_file __P((char *));

/* scsp_config_lex.c */
extern int	yylex __P((void));

/* scsp_config_parse.y */
#if __STDC__
extern void	parse_error __P((const char *, ...));
#else
extern void	parse_error __P((char *, va_alist));
#endif

/* scsp_hfsm.c */
extern int	scsp_hfsm __P((Scsp_dcs *, int, Scsp_msg *));

/* scsp_if.c */
extern int	scsp_cfsm __P((Scsp_dcs *, int, Scsp_msg *,
			Scsp_if_msg *));

/* scsp_input.c */
extern void	scsp_free_msg __P((Scsp_msg *));
extern Scsp_msg	*scsp_parse_msg __P((char *, int));

/* scsp_log.c */
#if __STDC__
extern void	scsp_log __P((const int, const char *, ...));
extern void	scsp_trace __P((const char *, ...));
#else
extern void	scsp_log __P((int, char *, va_alist));
extern void	scsp_trace __P((const char *, va_alist));
#endif
extern void	scsp_open_trace __P(());
extern void	scsp_trace_msg __P((Scsp_dcs *, Scsp_msg *, int));
extern void	scsp_mem_err __P((char *));

/* scsp_msg.c */
extern void	scsp_csus_ack __P((Scsp_dcs *, Scsp_msg *));
extern int	scsp_send_ca __P((Scsp_dcs *));
extern int	scsp_send_csus __P((Scsp_dcs *));
extern int	scsp_send_csu_req __P((Scsp_dcs *, Scsp_csa *));
extern int	scsp_send_csu_reply __P((Scsp_dcs *, Scsp_csa *));
extern int	scsp_send_hello __P((Scsp_dcs *));

/* scsp_output.c */
extern int	scsp_format_msg __P((Scsp_dcs *, Scsp_msg *, char **));
extern int	scsp_send_msg __P((Scsp_dcs *, Scsp_msg *));

/* scsp_print.c */
extern char	*format_hfsm_state __P((int));
extern char	*format_hfsm_event __P((int));
extern char	*format_cafsm_state __P((int));
extern char	*format_cafsm_event __P((int));
extern char	*format_cifsm_state __P((int));
extern char	*format_cifsm_event __P((int));
extern void	print_scsp_cse __P((FILE *, Scsp_cse *));
extern void	print_scsp_msg __P((FILE *, Scsp_msg *));
extern void	print_scsp_if_msg __P((FILE *, Scsp_if_msg *));
extern void	print_scsp_pending __P((FILE *, Scsp_pending *));
extern void	print_scsp_server __P((FILE *, Scsp_server *));
extern void	print_scsp_dcs __P((FILE *, Scsp_dcs *));
extern void	print_scsp_dump __P(());

/* scsp_socket.c */
extern Scsp_dcs *	scsp_find_dcs __P((int));
extern Scsp_server *	scsp_find_server __P((int));
extern int		scsp_dcs_connect __P((Scsp_dcs *));
extern int		scsp_dcs_listen __P((Scsp_server *));
extern Scsp_dcs *	scsp_dcs_accept __P((Scsp_server *));
extern int		scsp_dcs_read __P((Scsp_dcs *));
extern int		scsp_server_listen __P(());
extern int		scsp_server_accept __P((int));
extern Scsp_if_msg *	scsp_if_sock_read __P((int));
extern int		scsp_if_sock_write __P((int, Scsp_if_msg *));
extern int		scsp_server_read __P((Scsp_server *));
extern int		scsp_send_cache_ind __P((Scsp_server *));
extern int		scsp_pending_read __P((Scsp_pending *));

/* scsp_subr.c */
extern int		scsp_hash __P((Scsp_ckey *));
extern int		scsp_cmp_id __P((Scsp_id *, Scsp_id *));
extern int		scsp_cmp_key __P((Scsp_ckey *, Scsp_ckey *));
extern int		scsp_is_atmarp_server __P((char *));
extern Scsp_cse *	scsp_dup_cse __P((Scsp_cse *));
extern Scsp_csa *	scsp_dup_csa __P((Scsp_csa *));
extern Scsp_csa *	scsp_cse2csas __P((Scsp_cse *));
extern void		scsp_dcs_cleanup __P((Scsp_dcs *));
extern void		scsp_dcs_delete __P((Scsp_dcs *));
extern void		scsp_server_shutdown __P((Scsp_server *));
extern void		scsp_server_delete __P((Scsp_server *));
extern int		scsp_get_server_info __P((Scsp_server *));
extern void		scsp_process_ca __P((Scsp_dcs *, Scsp_ca *));
extern void		scsp_process_cache_rsp __P((Scsp_server *,
				Scsp_if_msg *));
extern int		scsp_propagate_csa __P(( Scsp_dcs *,
				Scsp_csa *));
extern void		scsp_update_cache __P(( Scsp_dcs *,
				Scsp_csa *));
extern void		scsp_reconfigure __P(());

/* scsp_timer.c */
extern void	scsp_open_timeout __P((Harp_timer *));
extern void	scsp_hello_timeout __P((Harp_timer *));
extern void	scsp_hello_rcv_timeout __P((Harp_timer *));
extern void	scsp_ca_retran_timeout __P((Harp_timer *));
extern void	scsp_csus_retran_timeout __P((Harp_timer *));
extern void	scsp_csu_req_retran_timeout __P((Harp_timer *));



#endif	/* _SCSP_SCSP_VAR_H */
