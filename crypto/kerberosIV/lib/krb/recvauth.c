/* 
  Copyright (C) 1989 by the Massachusetts Institute of Technology

   Export of this software from the United States of America is assumed
   to require a specific license from the United States Government.
   It is the responsibility of any person or organization contemplating
   export to obtain such a license before exporting.

WITHIN THAT CONSTRAINT, permission to use, copy, modify, and
distribute this software and its documentation for any purpose and
without fee is hereby granted, provided that the above copyright
notice appear in all copies and that both that copyright notice and
this permission notice appear in supporting documentation, and that
the name of M.I.T. not be used in advertising or publicity pertaining
to distribution of the software without specific, written prior
permission.  M.I.T. makes no representations about the suitability of
this software for any purpose.  It is provided "as is" without express
or implied warranty.

  */

#include "krb_locl.h"

RCSID("$Id: recvauth.c,v 1.17 1997/03/23 03:53:16 joda Exp $");

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

static int
send_error_reply(int fd)
{
    unsigned char tmp[4] = { 255, 255, 255, 255 };
    if(krb_net_write(fd, tmp, sizeof(tmp)) != sizeof(tmp))
	return -1;
    return 0;
}

int
krb_recvauth(int32_t options,	/* bit-pattern of options */
	     int fd,		/* file descr. to read from */
	     KTEXT ticket,	/* storage for client's ticket */
	     char *service,	/* service expected */
	     char *instance,	/* inst expected (may be filled in) */
	     struct sockaddr_in *faddr,	/* address of foreign host on fd */
	     struct sockaddr_in *laddr,	/* local address */
	     AUTH_DAT *kdata,	/* kerberos data (returned) */
	     char *filename,	/* name of file with service keys */
	     struct des_ks_struct *schedule, /* key schedule (return) */
	     char *version)	/* version string (filled in) */
{
    int cc;
    char krb_vers[KRB_SENDAUTH_VLEN + 1]; /* + 1 for the null terminator */
    int rem;
    int32_t priv_len;
    u_char tmp_buf[MAX_KTXT_LEN+max(KRB_SENDAUTH_VLEN+1,21)];

    /* read the protocol version number */
    if (krb_net_read(fd, krb_vers, KRB_SENDAUTH_VLEN) != KRB_SENDAUTH_VLEN)
	return(errno);
    krb_vers[KRB_SENDAUTH_VLEN] = '\0';

    /* read the application version string */
    if (krb_net_read(fd, version, KRB_SENDAUTH_VLEN) != KRB_SENDAUTH_VLEN)
	return(errno);
    version[KRB_SENDAUTH_VLEN] = '\0';

    /* get the length of the ticket */
    {
	char tmp[4];
	if (krb_net_read(fd, tmp, 4) != 4)
	    return -1;
	krb_get_int(tmp, &ticket->length, 4, 0);
    }
    
    /* sanity check */
    if (ticket->length <= 0 || ticket->length > MAX_KTXT_LEN) {
	if (options & KOPT_DO_MUTUAL) {
	    if(send_error_reply(fd))
		return -1;
	    return KFAILURE;
	} else
	    return KFAILURE; /* XXX there may still be junk on the fd? */
    }

    /* read the ticket */
    if (krb_net_read(fd, ticket->dat, ticket->length) != ticket->length)
	return -1;
    /*
     * now have the ticket.  decrypt it to get the authenticated
     * data.
     */
    rem = krb_rd_req(ticket, service, instance, faddr->sin_addr.s_addr,
		     kdata, filename);

    /* if we are doing mutual auth, compose a response */
    if (options & KOPT_DO_MUTUAL) {
	if (rem != KSUCCESS){
	    /* the krb_rd_req failed */
	    if(send_error_reply(fd))
		return -1;
	    return rem;
	}
	
	/* add one to the (formerly) sealed checksum, and re-seal it
	   for return to the client */
	{ 
	    unsigned char cs[4];
	    krb_put_int(kdata->checksum + 1, cs, 4);
#ifndef NOENCRYPTION
	    des_key_sched(&kdata->session,schedule);
#endif
	    priv_len = krb_mk_priv(cs, 
				   tmp_buf+4, 
				   4,
				   schedule,
				   &kdata->session,
				   laddr,
				   faddr);
	}
	/* mk_priv will never fail */
	priv_len += krb_put_int(priv_len, tmp_buf, 4);
	
	if((cc = krb_net_write(fd, tmp_buf, priv_len)) != priv_len)
	    return -1;
    }
    return rem;
}
