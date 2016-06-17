#ifndef _IP_CONNTRACK_FTP_H
#define _IP_CONNTRACK_FTP_H
/* FTP tracking. */

#ifdef __KERNEL__

#include <linux/netfilter_ipv4/lockhelp.h>

/* Protects ftp part of conntracks */
DECLARE_LOCK_EXTERN(ip_ftp_lock);

#define FTP_PORT	21

#endif /* __KERNEL__ */

enum ip_ct_ftp_type
{
	/* PORT command from client */
	IP_CT_FTP_PORT,
	/* PASV response from server */
	IP_CT_FTP_PASV,
	/* EPRT command from client */
	IP_CT_FTP_EPRT,
	/* EPSV response from server */
	IP_CT_FTP_EPSV,
};

/* This structure is per expected connection */
struct ip_ct_ftp_expect
{
	/* We record seq number and length of ftp ip/port text here: all in
	 * host order. */

 	/* sequence number of IP address in packet is in ip_conntrack_expect */
	u_int32_t len;			/* length of IP address */
	enum ip_ct_ftp_type ftptype;	/* PORT or PASV ? */
	u_int16_t port; 		/* TCP port that was to be used */
};

/* This structure exists only once per master */
struct ip_ct_ftp_master {
	/* Next valid seq position for cmd matching after newline */
	u_int32_t seq_aft_nl[IP_CT_DIR_MAX];
	/* 0 means seq_match_aft_nl not set */
	int seq_aft_nl_set[IP_CT_DIR_MAX];
};

#endif /* _IP_CONNTRACK_FTP_H */
