/*
 * Copyright (C) 1997-2000 by Darren Reed.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that this notice is preserved and due credit is given
 * to the original author and the contributors.
 *
 * $Id: ip_proxy.h,v 2.1.2.1 1999/09/19 12:18:20 darrenr Exp $
 * $FreeBSD: src/sys/netinet/ip_proxy.h,v 1.7.2.1 2000/07/19 23:27:55 darrenr Exp $
 */

#ifndef	__IP_PROXY_H__
#define	__IP_PROXY_H__

#ifndef SOLARIS
#define SOLARIS (defined(sun) && (defined(__svr4__) || defined(__SVR4)))
#endif

#ifndef	APR_LABELLEN
#define	APR_LABELLEN	16
#endif
#define	AP_SESS_SIZE	53

struct	nat;
struct	ipnat;

typedef	struct	ap_tcp {
	u_short	apt_sport;	/* source port */
	u_short	apt_dport;	/* destination port */
	short	apt_sel[2];	/* {seq,ack}{off,min} set selector */
	short	apt_seqoff[2];	/* sequence # difference */
	tcp_seq	apt_seqmin[2];	/* don't change seq-off until after this */
	short	apt_ackoff[2];	/* sequence # difference */
	tcp_seq	apt_ackmin[2];	/* don't change seq-off until after this */
	u_char	apt_state[2];	/* connection state */
} ap_tcp_t;

typedef	struct	ap_udp {
	u_short	apu_sport;	/* source port */
	u_short	apu_dport;	/* destination port */
} ap_udp_t;

typedef	struct ap_session {
	struct	aproxy	*aps_apr;
	union {
		struct	ap_tcp	apu_tcp;
		struct	ap_udp	apu_udp;
	} aps_un;
	u_int	aps_flags;
	U_QUAD_T aps_bytes;	/* bytes sent */
	U_QUAD_T aps_pkts;	/* packets sent */
	void	*aps_nat;	/* pointer back to nat struct */
	void	*aps_data;	/* private data */
	int	aps_p;		/* protocol */
	int	aps_psiz;	/* size of private data */
	struct	ap_session	*aps_hnext;
	struct	ap_session	*aps_next;
} ap_session_t;

#define	aps_sport	aps_un.apu_tcp.apt_sport
#define	aps_dport	aps_un.apu_tcp.apt_dport
#define	aps_sel		aps_un.apu_tcp.apt_sel
#define	aps_seqoff	aps_un.apu_tcp.apt_seqoff
#define	aps_seqmin	aps_un.apu_tcp.apt_seqmin
#define	aps_state	aps_un.apu_tcp.apt_state
#define	aps_ackoff	aps_un.apu_tcp.apt_ackoff
#define	aps_ackmin	aps_un.apu_tcp.apt_ackmin


typedef	struct	aproxy	{
	struct	aproxy	*apr_next;
	char	apr_label[APR_LABELLEN];	/* Proxy label # */
	u_char	apr_p;		/* protocol */
	int	apr_ref;	/* +1 per rule referencing it */
	int	apr_flags;
	int	(* apr_init) __P((void));
	void	(* apr_fini) __P((void));
	int	(* apr_new) __P((fr_info_t *, ip_t *,
				 ap_session_t *, struct nat *));
	int	(* apr_inpkt) __P((fr_info_t *, ip_t *,
				   ap_session_t *, struct nat *));
	int	(* apr_outpkt) __P((fr_info_t *, ip_t *,
				    ap_session_t *, struct nat *));
} aproxy_t;

#define	APR_DELETE	1

#define	APR_ERR(x)	(((x) & 0xffff) << 16)
#define	APR_EXIT(x)	(((x) >> 16) & 0xffff)
#define	APR_INC(x)	((x) & 0xffff)

#define	FTP_BUFSZ	160
/*
 * For the ftp proxy.
 */
typedef struct  ftpside {
	char	*ftps_rptr;
	char	*ftps_wptr;
	u_32_t	ftps_seq;
	int	ftps_junk;
	char	ftps_buf[FTP_BUFSZ];
} ftpside_t;

typedef struct  ftpinfo {
	u_int   	ftp_passok;
	ftpside_t	ftp_side[2];
} ftpinfo_t;

/*
 * Real audio proxy structure and #defines
 */
typedef	struct	{
	int	rap_seenpna;
	int	rap_seenver;
	int	rap_version;
	int	rap_eos;	/* End Of Startup */
	int	rap_gotid;
	int	rap_gotlen;
	int	rap_mode;
	int	rap_sdone;
	u_short	rap_plport;
	u_short	rap_prport;
	u_short	rap_srport;
	char	rap_svr[19];
	u_32_t	rap_sbf;	/* flag to indicate which of the 19 bytes have
				 * been filled
				 */
	tcp_seq	rap_sseq;
} raudio_t;

#define	RA_ID_END	0
#define	RA_ID_UDP	1
#define	RA_ID_ROBUST	7

#define	RAP_M_UDP	1
#define	RAP_M_ROBUST	2
#define	RAP_M_TCP	4
#define	RAP_M_UDP_ROBUST	(RAP_M_UDP|RAP_M_ROBUST)


extern	ap_session_t	*ap_sess_tab[AP_SESS_SIZE];
extern	ap_session_t	*ap_sess_list;
extern	aproxy_t	ap_proxies[];
extern	int		ippr_ftp_pasvonly;

extern	int	appr_add __P((aproxy_t *));
extern	int	appr_del __P((aproxy_t *));
extern	int	appr_init __P((void));
extern	void	appr_unload __P((void));
extern	int	appr_ok __P((ip_t *, tcphdr_t *, struct ipnat *));
extern	void	appr_free __P((aproxy_t *));
extern	void	aps_free __P((ap_session_t *));
extern	int	appr_check __P((ip_t *, fr_info_t *, struct nat *));
extern	aproxy_t	*appr_match __P((u_int, char *));

#endif /* __IP_PROXY_H__ */
