#ifndef NETWORKING_H
#define NETWORKING_H

#include <arpa/inet.h>
#include <netinet/in.h>

#include <strings.h>	
#include <errno.h>
#include <config.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <ntp_rfc2553.h>
#include <ntp_stdlib.h>
#include <ntp_machine.h>
#include <ntp_unixtime.h>
#include <ntp_fp.h>
#include <ntp.h>

#include "crypto.h"
#include "log.h"
#include "sntp-opts.h"	
#include "utilities.h"

/* 
 * for 4.2.6 only define AUTOKEY if OPENSSL, so that backported 4.2.7
 * references to AUTOKEY work -- in 4.2.7 AUTOKEY is independent of OPENSSL
 */
#ifdef OPENSSL
#define AUTOKEY
#endif

/* FIXME To be replaced by the constants in ntp.h */
#define SERVER_UNUSEABLE -1 /* Skip server */
#define PACKET_UNUSEABLE -2 /* Discard packet and try to get a useable packet again if not tried too often */
#define SERVER_AUTH_FAIL -3 /* Authentication failed, act upon settings */
#define KOD_DEMOBILIZE -4   /* KOD packet with code DENY or RSTR, stop all communication and save KOD information */
#define KOD_RATE -5	    /* KOD packet with code RATE, reduce poll intervall */
#define BROADCAST_FAILED -6


/* From ntpdate.c */
int is_reachable (struct addrinfo *dst);

int resolve_hosts (const char **hosts, int hostc, struct addrinfo ***res, int pref_family);

void create_socket (SOCKET *rsock, sockaddr_u *dest);

void sendpkt (SOCKET rsock, sockaddr_u *dest, struct pkt *pkt, int len);

int recvdata (SOCKET rsock, sockaddr_u *sender, char *rdata, int rdata_len);

int recvpkt (SOCKET rsock, struct pkt *rpkt, unsigned int rsize, struct pkt *spkt);

int recv_bcst_data (SOCKET rsock, char *rdata, int rdata_len, sockaddr_u *sas, sockaddr_u *ras);

int recv_bcst_pkt (SOCKET rsock, struct pkt *rpkt, unsigned int rsize, sockaddr_u *sas);

int process_pkt (struct pkt *rpkt, sockaddr_u *sas,	int pkt_len, int mode, struct pkt *spkt, char * func_name);

/* Shortened peer structure. Not absolutely necessary yet */
struct speer {
	struct speer *next;
	sockaddr_u srcadr;
	u_char version;
	u_char hmode;
	u_char hpoll;
	u_char minpoll;
	u_char maxpoll;
	u_int flags;
	u_char num_events;
	u_char ttl;
	u_char leap;
	u_char pmode;
	u_char stratum;
	u_char ppoll;
	u_char precision;	/* should be s_char */
	u_int32 refid;
	l_fp reftime;
	keyid_t keyid;

#ifdef AUTOKEY
#define clear_to_zero opcode
	u_int32	opcode;		/* last request opcode */
	associd_t assoc;	/* peer association ID */
	u_int32	crypto;		/* peer status word */
	EVP_PKEY *pkey;		/* public key */
	const EVP_MD *digest;	/* message digest algorithm */
	char	*subject;	/* certificate subject name */
	char	*issuer;	/* certificate issuer name */
	struct cert_info *xinfo; /* issuer certificate */
	keyid_t	pkeyid;		/* previous key ID */
	keyid_t	hcookie;	/* host cookie */
	keyid_t	pcookie;	/* peer cookie */
	const struct pkey_info *ident_pkey; /* identity key */
	BIGNUM	*iffval;	/* identity challenge (IFF, GQ, MV) */
	const BIGNUM *grpkey;	/* identity challenge key (GQ) */
	struct value cookval;	/* receive cookie values */
	struct value recval;	/* receive autokey values */
	struct exten *cmmd;	/* extension pointer */
	u_long	refresh;	/* next refresh epoch */

	/*
	 * Variables used by authenticated server
	 */
	keyid_t	*keylist;	/* session key ID list */
	int	keynumber;	/* current key number */
	struct value encrypt;	/* send encrypt values */
	struct value sndval;	/* send autokey values */
#else	/* !AUTOKEY follows */
#define clear_to_zero status
#endif	/* !AUTOKEY */
	
	l_fp	rec;		/* receive time stamp */
	l_fp	xmt;		/* transmit time stamp */
	l_fp	dst;		/* destination timestamp */
	l_fp	aorg;		/* origin timestamp */
	l_fp	borg;		/* alternate origin timestamp */
	double	offset;		/* peer clock offset */
	double	delay;		/* peer roundtrip delay */
};





#endif
