/*
 * Copyright 1987, 1988 by the Massachusetts Institute of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: recvauth.c,v 4.4 90/03/10 19:03:08 jon Exp $";
 *	$FreeBSD$
 */

#if 0
#ifndef	lint
static char rcsid[] =
"$FreeBSD$";
#endif	lint
#endif

#include <krb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <syslog.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <strings.h>


#define	KRB_SENDAUTH_VERS	"AUTHV0.1" /* MUST be KRB_SENDAUTH_VLEN
					      chars */

/*
 * If the protocol changes, you will need to change the version string
 * and make appropriate changes in krb_sendauth.c
 * be sure to support old versions of krb_sendauth!
 */

extern int errno;

/*
 * krb_recvauth() reads (and optionally responds to) a message sent
 * using krb_sendauth().  The "options" argument is a bit-field of
 * selected options (see "sendauth.c" for options description).
 * The only option relevant to krb_recvauth() is KOPT_DO_MUTUAL
 * (mutual authentication requested).  The "fd" argument supplies
 * a file descriptor to read from (and write to, if mutual authenti-
 * cation is requested).
 *
 * Part of the received message will be a Kerberos ticket sent by the
 * client; this is read into the "ticket" argument.  The "service" and
 * "instance" arguments supply the server's Kerberos name.  If the
 * "instance" argument is the string "*", it is treated as a wild card
 * and filled in during the krb_rd_req() call (see read_service_key()).
 *
 * The "faddr" and "laddr" give the sending (client) and receiving
 * (local server) network addresses.  ("laddr" may be left NULL unless
 * mutual authentication is requested, in which case it must be set.)
 *
 * The authentication information extracted from the message is returned
 * in "kdata".  The "filename" argument indicates the file where the
 * server's key can be found.  (It is passed on to krb_rd_req().)  If
 * left null, the default "/etc/srvtab" will be used.
 *
 * If mutual authentication is requested, the session key schedule must
 * be computed in order to reply; this schedule is returned in the
 * "schedule" argument.  A string containing the application version
 * number from the received message is returned in "version", which
 * should be large enough to hold a KRB_SENDAUTH_VLEN-character string.
 *
 * See krb_sendauth() for the format of the received client message.
 *
 * This routine supports another client format, for backward
 * compatibility, consisting of:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * string		tmp_buf, tkt_len	length of ticket, in
 * 						ascii
 *
 * char			' ' (space char)	separator
 *
 * tkt_len		ticket->dat		the ticket
 *
 * This old-style version does not support mutual authentication.
 *
 * krb_recvauth() first reads the protocol version string from the
 * given file descriptor.  If it doesn't match the current protocol
 * version (KRB_SENDAUTH_VERS), the old-style format is assumed.  In
 * that case, the string of characters up to the first space is read
 * and interpreted as the ticket length, then the ticket is read.
 *
 * If the first string did match KRB_SENDAUTH_VERS, krb_recvauth()
 * next reads the application protocol version string.  Then the
 * ticket length and ticket itself are read.
 *
 * The ticket is decrypted and checked by the call to krb_rd_req().
 * If no mutual authentication is required, the result of the
 * krb_rd_req() call is retured by this routine.  If mutual authenti-
 * cation is required, a message in the following format is returned
 * on "fd":
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 4 bytes		tkt_len			length of ticket or -1
 *						if error occurred
 *
 * priv_len		tmp_buf			"private" message created
 *						by krb_mk_priv() which
 *						contains the incremented
 *						checksum sent by the client
 *						encrypted in the session
 *						key.  (This field is not
 *						present in case of error.)
 *
 * If all goes well, KSUCCESS is returned; otherwise KFAILURE or some
 * other error code is returned.
 */

#ifndef max
#define	max(a,b) (((a) > (b)) ? (a) : (b))
#endif /* max */

int
krb_recvauth(options, fd, ticket, service, instance, faddr, laddr, kdata,
	     filename, schedule, version)
long options;			 /* bit-pattern of options */
int fd;				 /* file descr. to read from */
KTEXT ticket;			 /* storage for client's ticket */
char *service;			 /* service expected */
char *instance;			 /* inst expected (may be filled in) */
struct sockaddr_in *faddr;	 /* address of foreign host on fd */
struct sockaddr_in *laddr;	 /* local address */
AUTH_DAT *kdata;		 /* kerberos data (returned) */
char *filename;			 /* name of file with service keys */
Key_schedule schedule;		 /* key schedule (return) */
char *version;			 /* version string (filled in) */
{

    int i, cc, old_vers = 0;
    char krb_vers[KRB_SENDAUTH_VLEN + 1]; /* + 1 for the null terminator */
    char *cp;
    int rem;
    long tkt_len, priv_len;
    u_long cksum;
    u_char tmp_buf[MAX_KTXT_LEN+max(KRB_SENDAUTH_VLEN+1,21)];

    /* read the protocol version number */
    if (krb_net_read(fd, krb_vers, KRB_SENDAUTH_VLEN) !=
	KRB_SENDAUTH_VLEN)
	    return(errno);
    krb_vers[KRB_SENDAUTH_VLEN] = '\0';

    /* check version string */
    if (strcmp(krb_vers,KRB_SENDAUTH_VERS)) {
	/* Assume the old version of sendkerberosdata: send ascii
	   length, ' ', and ticket. */
	if (options & KOPT_DO_MUTUAL)
	    return(KFAILURE);	 /* XXX can't do old style with mutual auth */
	old_vers = 1;

	/* copy what we have read into tmp_buf */
	(void) bcopy(krb_vers, (char *) tmp_buf, KRB_SENDAUTH_VLEN);

	/* search for space, and make it a null */
	for (i = 0; i < KRB_SENDAUTH_VLEN; i++)
	    if (tmp_buf[i]== ' ') {
		tmp_buf[i] = '\0';
		/* point cp to the beginning of the real ticket */
		cp = (char *) &tmp_buf[i+1];
		break;
	    }

	if (i == KRB_SENDAUTH_VLEN)
	    /* didn't find the space, keep reading to find it */
	    for (; i<20; i++) {
		if (read(fd, (char *)&tmp_buf[i], 1) != 1) {
		    return(KFAILURE);
		}
		if (tmp_buf[i] == ' ') {
		    tmp_buf[i] = '\0';
		    /* point cp to the beginning of the real ticket */
		    cp = (char *) &tmp_buf[i+1];
		    break;
		}
	    }

	tkt_len = (long) atoi((char *) tmp_buf);

	/* sanity check the length */
	if ((i==20)||(tkt_len<=0)||(tkt_len>MAX_KTXT_LEN))
	    return(KFAILURE);

	if (i < KRB_SENDAUTH_VLEN) {
	    /* since we already got the space, and part of the ticket,
	       we read fewer bytes to get the rest of the ticket */
	    if (krb_net_read(fd, (char *)(tmp_buf+KRB_SENDAUTH_VLEN),
			     (int) (tkt_len - KRB_SENDAUTH_VLEN + 1 + i))
		!= (int)(tkt_len - KRB_SENDAUTH_VLEN + 1 + i))
		return(errno);
	} else {
	    if (krb_net_read(fd, (char *)(tmp_buf+i), (int)tkt_len) !=
		(int) tkt_len)
		return(errno);
	}
	ticket->length = tkt_len;
	/* copy the ticket into the struct */
	(void) bcopy(cp, (char *) ticket->dat, ticket->length);

    } else {
	/* read the application version string */
	if (krb_net_read(fd, version, KRB_SENDAUTH_VLEN) !=
	    KRB_SENDAUTH_VLEN)
	    return(errno);
	version[KRB_SENDAUTH_VLEN] = '\0';

	/* get the length of the ticket */
	if (krb_net_read(fd, (char *)&tkt_len, sizeof(tkt_len)) !=
	    sizeof(tkt_len))
	    return(errno);

	/* sanity check */
	ticket->length = ntohl((unsigned long)tkt_len);
	if ((ticket->length <= 0) || (ticket->length > MAX_KTXT_LEN)) {
	    if (options & KOPT_DO_MUTUAL) {
		rem = KFAILURE;
		goto mutual_fail;
	    } else
		return(KFAILURE); /* XXX there may still be junk on the fd? */
	}

	/* read the ticket */
	if (krb_net_read(fd, (char *) ticket->dat, ticket->length)
	    != ticket->length)
	    return(errno);
    }
    /*
     * now have the ticket.  decrypt it to get the authenticated
     * data.
     */
    rem = krb_rd_req(ticket,service,instance,faddr->sin_addr.s_addr,
		     kdata,filename);

    if (old_vers) return(rem);	 /* XXX can't do mutual with old client */

    /* if we are doing mutual auth, compose a response */
    if (options & KOPT_DO_MUTUAL) {
	if (rem != KSUCCESS)
	    /* the krb_rd_req failed */
	    goto mutual_fail;

	/* add one to the (formerly) sealed checksum, and re-seal it
	   for return to the client */
	cksum = kdata->checksum + 1;
	cksum = htonl(cksum);
#ifndef NOENCRYPTION
	key_sched((C_Block *)kdata->session,schedule);
#endif
	priv_len = krb_mk_priv((unsigned char *)&cksum,
			       tmp_buf,
			       (unsigned long) sizeof(cksum),
			       schedule,
			       kdata->session,
			       laddr,
			       faddr);
	if (priv_len < 0) {
	    /* re-sealing failed; notify the client */
	    rem = KFAILURE;	 /* XXX */
mutual_fail:
	    priv_len = -1;
	    tkt_len = htonl((unsigned long) priv_len);
	    /* a length of -1 is interpreted as an authentication
	       failure by the client */
	    if ((cc = krb_net_write(fd, (char *)&tkt_len, sizeof(tkt_len)))
		!= sizeof(tkt_len))
		return(cc);
	    return(rem);
	} else {
	    /* re-sealing succeeded, send the private message */
	    tkt_len = htonl((unsigned long)priv_len);
	    if ((cc = krb_net_write(fd, (char *)&tkt_len, sizeof(tkt_len)))
		 != sizeof(tkt_len))
		return(cc);
	    if ((cc = krb_net_write(fd, (char *)tmp_buf, (int) priv_len))
		!= (int) priv_len)
		return(cc);
	}
    }
    return(rem);
}
