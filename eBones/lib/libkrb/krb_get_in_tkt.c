/*
 * Copyright 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: der: krb_get_in_tkt.c,v 4.19 89/07/18 16:31:31 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <krb.h>
#include <des.h>
#include <prot.h>

#include <stdio.h>
#include <strings.h>
#include <errno.h>

/* use the bsd time.h struct defs for PC too! */
#include <sys/time.h>
#include <sys/types.h>

int     swap_bytes;

/*
 * decrypt_tkt(): Given user, instance, realm, passwd, key_proc
 * and the cipher text sent from the KDC, decrypt the cipher text
 * using the key returned by key_proc.
 */

static int
decrypt_tkt(user, instance, realm, arg, key_proc, cipp)
  char *user;
  char *instance;
  char *realm;
  char *arg;
  int (*key_proc)();
  KTEXT *cipp;
{
    KTEXT cip = *cipp;
    C_Block key;		/* Key for decrypting cipher */
    Key_schedule key_s;

#ifndef NOENCRYPTION
    /* Attempt to decrypt it */
#endif

    /* generate a key */

    {
	register int rc;
	rc = (*key_proc)(user,instance,realm,arg,key);
	if (rc)
	    return(rc);
    }

#ifndef NOENCRYPTION
    key_sched(&key,key_s);
    pcbc_encrypt((C_Block *)cip->dat,(C_Block *)cip->dat,
		 (long) cip->length,key_s,(C_Block *)key,DES_DECRYPT);
#endif /* !NOENCRYPTION */
    /* Get rid of all traces of key */
    bzero((char *)key,sizeof(key));
    bzero((char *)key_s,sizeof(key_s));

    return(0);
}

/*
 * krb_get_in_tkt() gets a ticket for a given principal to use a given
 * service and stores the returned ticket and session key for future
 * use.
 *
 * The "user", "instance", and "realm" arguments give the identity of
 * the client who will use the ticket.  The "service" and "sinstance"
 * arguments give the identity of the server that the client wishes
 * to use.  (The realm of the server is the same as the Kerberos server
 * to whom the request is sent.)  The "life" argument indicates the
 * desired lifetime of the ticket; the "key_proc" argument is a pointer
 * to the routine used for getting the client's private key to decrypt
 * the reply from Kerberos.  The "decrypt_proc" argument is a pointer
 * to the routine used to decrypt the reply from Kerberos; and "arg"
 * is an argument to be passed on to the "key_proc" routine.
 *
 * If all goes well, krb_get_in_tkt() returns INTK_OK, otherwise it
 * returns an error code:  If an AUTH_MSG_ERR_REPLY packet is returned
 * by Kerberos, then the error code it contains is returned.  Other
 * error codes returned by this routine include INTK_PROT to indicate
 * wrong protocol version, INTK_BADPW to indicate bad password (if
 * decrypted ticket didn't make sense), INTK_ERR if the ticket was for
 * the wrong server or the ticket store couldn't be initialized.
 *
 * The format of the message sent to Kerberos is as follows:
 *
 * Size			Variable		Field
 * ----			--------		-----
 *
 * 1 byte		KRB_PROT_VERSION	protocol version number
 * 1 byte		AUTH_MSG_KDC_REQUEST |	message type
 *			HOST_BYTE_ORDER		local byte order in lsb
 * string		user			client's name
 * string		instance		client's instance
 * string		realm			client's realm
 * 4 bytes		tlocal.tv_sec		timestamp in seconds
 * 1 byte		life			desired lifetime
 * string		service			service's name
 * string		sinstance		service's instance
 */

int
krb_get_in_tkt(user, instance, realm, service, sinstance, life,
	       key_proc, decrypt_proc, arg)
    char *user;
    char *instance;
    char *realm;
    char *service;
    char *sinstance;
    int life;
    int (*key_proc)();
    int (*decrypt_proc)();
    char *arg;
{
    KTEXT_ST pkt_st;
    KTEXT pkt = &pkt_st;	/* Packet to KDC */
    KTEXT_ST rpkt_st;
    KTEXT rpkt = &rpkt_st;	/* Returned packet */
    KTEXT_ST cip_st;
    KTEXT cip = &cip_st;	/* Returned Ciphertext */
    KTEXT_ST tkt_st;
    KTEXT tkt = &tkt_st;	/* Current ticket */
    C_Block ses;                /* Session key for tkt */
    int kvno;			/* Kvno for session key */
    unsigned char *v = pkt->dat; /* Prot vers no */
    unsigned char *t = (pkt->dat+1); /* Prot msg type */

    char s_name[SNAME_SZ];
    char s_instance[INST_SZ];
    char rlm[REALM_SZ];
    int lifetime;
    int msg_byte_order;
    int kerror;
    unsigned long exp_date;
    char *ptr;

    struct timeval t_local;

    unsigned long rep_err_code;

    unsigned long kdc_time;   /* KDC time */

    /* BUILD REQUEST PACKET */

    /* Set up the fixed part of the packet */
    *v = (unsigned char) KRB_PROT_VERSION;
    *t = (unsigned char) AUTH_MSG_KDC_REQUEST;
    *t |= HOST_BYTE_ORDER;

    /* Now for the variable info */
    (void) strcpy((char *)(pkt->dat+2),user); /* aname */
    pkt->length = 3 + strlen(user);
    (void) strcpy((char *)(pkt->dat+pkt->length),
		  instance);	/* instance */
    pkt->length += 1 + strlen(instance);
    (void) strcpy((char *)(pkt->dat+pkt->length),realm); /* realm */
    pkt->length += 1 + strlen(realm);

    (void) gettimeofday(&t_local,(struct timezone *) 0);
    /* timestamp */
    bcopy((char *)&(t_local.tv_sec),(char *)(pkt->dat+pkt->length), 4);
    pkt->length += 4;

    *(pkt->dat+(pkt->length)++) = (char) life;
    (void) strcpy((char *)(pkt->dat+pkt->length),service);
    pkt->length += 1 + strlen(service);
    (void) strcpy((char *)(pkt->dat+pkt->length),sinstance);
    pkt->length += 1 + strlen(sinstance);

    rpkt->length = 0;

    /* SEND THE REQUEST AND RECEIVE THE RETURN PACKET */

    if ((kerror = send_to_kdc(pkt, rpkt, realm))) return(kerror);

    /* check packet version of the returned packet */
    if (pkt_version(rpkt) != KRB_PROT_VERSION)
        return(INTK_PROT);

    /* Check byte order */
    msg_byte_order = pkt_msg_type(rpkt) & 1;
    swap_bytes = 0;
    if (msg_byte_order != HOST_BYTE_ORDER) {
        swap_bytes++;
    }

    switch (pkt_msg_type(rpkt) & ~1) {
    case AUTH_MSG_KDC_REPLY:
        break;
    case AUTH_MSG_ERR_REPLY:
        bcopy(pkt_err_code(rpkt),(char *) &rep_err_code,4);
        if (swap_bytes) swap_u_long(rep_err_code);
        return((int)rep_err_code);
    default:
        return(INTK_PROT);
    }

    /* EXTRACT INFORMATION FROM RETURN PACKET */

    /* get the principal's expiration date */
    bcopy(pkt_x_date(rpkt),(char *) &exp_date,sizeof(exp_date));
    if (swap_bytes) swap_u_long(exp_date);

    /* Extract the ciphertext */
    cip->length = pkt_clen(rpkt);       /* let clen do the swap */

    if ((cip->length < 0) || (cip->length > sizeof(cip->dat)))
	return(INTK_ERR);		/* no appropriate error code
					 currently defined for INTK_ */
    /* copy information from return packet into "cip" */
    bcopy((char *) pkt_cipher(rpkt),(char *)(cip->dat),cip->length);

    /* Attempt to decrypt the reply. */
    if (decrypt_proc == NULL)
	decrypt_proc = decrypt_tkt;
    (*decrypt_proc)(user, instance, realm, arg, key_proc, &cip);

    ptr = (char *) cip->dat;

    /* extract session key */
    bcopy(ptr,(char *)ses,8);
    ptr += 8;

    if ((strlen(ptr) + (ptr - (char *) cip->dat)) > cip->length)
	return(INTK_BADPW);

    /* extract server's name */
    (void) strcpy(s_name,ptr);
    ptr += strlen(s_name) + 1;

    if ((strlen(ptr) + (ptr - (char *) cip->dat)) > cip->length)
	return(INTK_BADPW);

    /* extract server's instance */
    (void) strcpy(s_instance,ptr);
    ptr += strlen(s_instance) + 1;

    if ((strlen(ptr) + (ptr - (char *) cip->dat)) > cip->length)
	return(INTK_BADPW);

    /* extract server's realm */
    (void) strcpy(rlm,ptr);
    ptr += strlen(rlm) + 1;

    /* extract ticket lifetime, server key version, ticket length */
    /* be sure to avoid sign extension on lifetime! */
    lifetime = (unsigned char) ptr[0];
    kvno = (unsigned char) ptr[1];
    tkt->length = (unsigned char) ptr[2];
    ptr += 3;

    if ((tkt->length < 0) ||
	((tkt->length + (ptr - (char *) cip->dat)) > cip->length))
	return(INTK_BADPW);

    /* extract ticket itself */
    bcopy(ptr,(char *)(tkt->dat),tkt->length);
    ptr += tkt->length;

    if (strcmp(s_name, service) || strcmp(s_instance, sinstance) ||
        strcmp(rlm, realm))	/* not what we asked for */
	return(INTK_ERR);	/* we need a better code here XXX */

    /* check KDC time stamp */
    bcopy(ptr,(char *)&kdc_time,4); /* Time (coarse) */
    if (swap_bytes) swap_u_long(kdc_time);

    ptr += 4;

    (void) gettimeofday(&t_local,(struct timezone *) 0);
    if (abs((int)(t_local.tv_sec - kdc_time)) > CLOCK_SKEW) {
        return(RD_AP_TIME);		/* XXX should probably be better
					   code */
    }

    /* initialize ticket cache */
    if (in_tkt(user,instance) != KSUCCESS)
	return(INTK_ERR);

    /* stash ticket, session key, etc. for future use */
    if ((kerror = save_credentials(s_name, s_instance, rlm, ses,
				  lifetime, kvno, tkt, t_local.tv_sec)))
	return(kerror);

    return(INTK_OK);
}
