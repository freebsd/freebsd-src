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
/* $FreeBSD$ */

#include "krb_locl.h"

RCSID("$Id: sendauth.c,v 1.18 1999/09/16 20:41:55 assar Exp $");

/*
 * krb_sendauth() transmits a ticket over a file descriptor for a
 * desired service, instance, and realm, doing mutual authentication
 * with the server if desired.
 */

/*
 * The first argument to krb_sendauth() contains a bitfield of
 * options (the options are defined in "krb.h"):
 *
 * KOPT_DONT_CANON	Don't canonicalize instance as a hostname.
 *			(If this option is not chosen, krb_get_phost()
 *			is called to canonicalize it.)
 *
 * KOPT_DONT_MK_REQ 	Don't request server ticket from Kerberos.
 *			A ticket must be supplied in the "ticket"
 *			argument.
 *			(If this option is not chosen, and there
 *			is no ticket for the given server in the
 *			ticket cache, one will be fetched using
 *			krb_mk_req() and returned in "ticket".)
 *
 * KOPT_DO_MUTUAL	Do mutual authentication, requiring that the
 * 			receiving server return the checksum+1 encrypted
 *			in the session key.  The mutual authentication
 *			is done using krb_mk_priv() on the other side
 *			(see "recvauth.c") and krb_rd_priv() on this
 *			side.
 *
 * The "fd" argument is a file descriptor to write to the remote
 * server on.  The "ticket" argument is used to store the new ticket
 * from the krb_mk_req() call. If the KOPT_DONT_MK_REQ options is
 * chosen, the ticket must be supplied in the "ticket" argument.
 * The "service", "inst", and "realm" arguments identify the ticket.
 * If "realm" is null, the local realm is used.
 *
 * The following arguments are only needed if the KOPT_DO_MUTUAL option
 * is chosen:
 *
 *   The "checksum" argument is a number that the server will add 1 to
 *   to authenticate itself back to the client; the "msg_data" argument
 *   holds the returned mutual-authentication message from the server
 *   (i.e., the checksum+1); the "cred" structure is used to hold the
 *   session key of the server, extracted from the ticket file, for use
 *   in decrypting the mutual authentication message from the server;
 *   and "schedule" holds the key schedule for that decryption.  The
 *   the local and server addresses are given in "laddr" and "faddr".
 *
 * The application protocol version number (of up to KRB_SENDAUTH_VLEN
 * characters) is passed in "version".
 *
 * If all goes well, KSUCCESS is returned, otherwise some error code.
 *
 * The format of the message sent to the server is:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * KRB_SENDAUTH_VLEN	KRB_SENDAUTH_VER	sendauth protocol
 * bytes					version number
 *
 * KRB_SENDAUTH_VLEN	version			application protocol
 * bytes					version number
 *
 * 4 bytes		ticket->length		length of ticket
 *
 * ticket->length	ticket->dat		ticket itself
 */

int
krb_sendauth(int32_t options,	/* bit-pattern of options */
	     int fd,		/* file descriptor to write onto */
	     KTEXT ticket,	/* where to put ticket (return); or
				 * supplied in case of KOPT_DONT_MK_REQ */
	     char *service,	/* service name, instance, realm */
	     char *instance,
	     char *realm,
	     u_int32_t checksum, /* checksum to include in request */
	     MSG_DAT *msg_data,	/* mutual auth MSG_DAT (return) */
	     CREDENTIALS *cred,	/* credentials (return) */
	     des_key_schedule schedule, /* key schedule (return) */
	     struct sockaddr_in *laddr, /* local address */
	     struct sockaddr_in *faddr,	/* address of foreign host on fd */
	     char *version)	/* version string */
{
    int ret;
    KTEXT_ST buf;
    char realrealm[REALM_SZ];

    if (realm == NULL) {
	ret = krb_get_lrealm (realrealm, 1);
	if (ret != KSUCCESS)
	    return ret;
	realm = realrealm;
    }
    ret = krb_mk_auth (options, ticket, service, instance, realm, checksum,
		       version, &buf);
    if (ret != KSUCCESS)
	return ret;
    ret = krb_net_write(fd, buf.dat, buf.length);
    if(ret < 0)
	return -1;
      
    if (options & KOPT_DO_MUTUAL) {
	char tmp[4];
	u_int32_t len;
	char inst[INST_SZ];
	char *i;

	ret = krb_net_read (fd, tmp, 4);
	if (ret < 0)
	    return -1;

	krb_get_int (tmp, &len, 4, 0);
	if (len == 0xFFFFFFFF || len > sizeof(buf.dat))
	    return KFAILURE;
	buf.length = len;
	ret = krb_net_read (fd, buf.dat, len);
	if (ret < 0)
	    return -1;

	if (options & KOPT_DONT_CANON)
	    i = instance;
	else
	    i = krb_get_phost(instance);
	strlcpy (inst, i, sizeof(inst));

	ret = krb_get_cred (service, inst, realm, cred);
	if (ret != KSUCCESS)
	    return ret;

	des_key_sched(&cred->session, schedule);

	ret = krb_check_auth (&buf, checksum, msg_data, &cred->session, 
			      schedule, laddr, faddr);
	if (ret != KSUCCESS)
	    return ret;
    }
    return KSUCCESS;
}
