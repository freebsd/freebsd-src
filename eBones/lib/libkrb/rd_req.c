/*
 * Copyright 1985, 1986, 1987, 1988 by the Massachusetts Institute
 * of Technology.
 * For copying and distribution information, please see the file
 * <Copyright.MIT>.
 *
 *	from: der: rd_req.c,v 4.16 89/03/22 14:52:06 jtkohl Exp $
 *	$FreeBSD$
 */

#if 0
#ifndef lint
static char *rcsid =
"$FreeBSD$";
#endif /* lint */
#endif

#include <des.h>
#include <krb.h>
#include <prot.h>
#include <sys/time.h>
#include <strings.h>

extern int krb_ap_req_debug;

static struct timeval t_local = { 0, 0 };

/*
 * Keep the following information around for subsequent calls
 * to this routine by the same server using the same key.
 */

static Key_schedule serv_key;	/* Key sched to decrypt ticket */
static C_Block ky;              /* Initialization vector */
static int st_kvno;		/* version number for this key */
static char st_rlm[REALM_SZ];	/* server's realm */
static char st_nam[ANAME_SZ];	/* service name */
static char st_inst[INST_SZ];	/* server's instance */

/*
 * This file contains two functions.  krb_set_key() takes a DES
 * key or password string and returns a DES key (either the original
 * key, or the password converted into a DES key) and a key schedule
 * for it.
 *
 * krb_rd_req() reads an authentication request and returns information
 * about the identity of the requestor, or an indication that the
 * identity information was not authentic.
 */

/*
 * krb_set_key() takes as its first argument either a DES key or a
 * password string.  The "cvt" argument indicates how the first
 * argument "key" is to be interpreted: if "cvt" is null, "key" is
 * taken to be a DES key; if "cvt" is non-null, "key" is taken to
 * be a password string, and is converted into a DES key using
 * string_to_key().  In either case, the resulting key is returned
 * in the external static variable "ky".  A key schedule is
 * generated for "ky" and returned in the external static variable
 * "serv_key".
 *
 * This routine returns the return value of des_key_sched.
 *
 * krb_set_key() needs to be in the same .o file as krb_rd_req() so that
 * the key set by krb_set_key() is available in private storage for
 * krb_rd_req().
 */

int
krb_set_key(key,cvt)
    char *key;
    int cvt;
{
#ifdef NOENCRYPTION
    bzero(ky, sizeof(ky));
    return KSUCCESS;
#else
    if (cvt)
	string_to_key(key,(C_Block *)ky);
    else
	bcopy(key,(char *)ky,8);
    return(des_key_sched((C_Block *)ky,serv_key));
#endif
}


/*
 * krb_rd_req() takes an AUTH_MSG_APPL_REQUEST or
 * AUTH_MSG_APPL_REQUEST_MUTUAL message created by krb_mk_req(),
 * checks its integrity and returns a judgement as to the requestor's
 * identity.
 *
 * The "authent" argument is a pointer to the received message.
 * The "service" and "instance" arguments name the receiving server,
 * and are used to get the service's ticket to decrypt the ticket
 * in the message, and to compare against the server name inside the
 * ticket.  "from_addr" is the network address of the host from which
 * the message was received; this is checked against the network
 * address in the ticket.  If "from_addr" is zero, the check is not
 * performed.  "ad" is an AUTH_DAT structure which is
 * filled in with information about the sender's identity according
 * to the authenticator and ticket sent in the message.  Finally,
 * "fn" contains the name of the file containing the server's key.
 * (If "fn" is NULL, the server's key is assumed to have been set
 * by krb_set_key().  If "fn" is the null string ("") the default
 * file KEYFILE, defined in "krb.h", is used.)
 *
 * krb_rd_req() returns RD_AP_OK if the authentication information
 * was genuine, or one of the following error codes (defined in
 * "krb.h"):
 *
 *	RD_AP_VERSION		- wrong protocol version number
 *	RD_AP_MSG_TYPE		- wrong message type
 *	RD_AP_UNDEC		- couldn't decipher the message
 *	RD_AP_INCON		- inconsistencies found
 *	RD_AP_BADD		- wrong network address
 *	RD_AP_TIME		- client time (in authenticator)
 *				  too far off server time
 *	RD_AP_NYV		- Kerberos time (in ticket) too
 *				  far off server time
 *	RD_AP_EXP		- ticket expired
 *
 * For the message format, see krb_mk_req().
 *
 * Mutual authentication is not implemented.
 */

int
krb_rd_req(authent,service,instance,from_addr,ad,fn)
    register KTEXT authent;	/* The received message */
    char *service;		/* Service name */
    char *instance;		/* Service instance */
    long from_addr;		/* Net address of originating host */
    AUTH_DAT *ad;		/* Structure to be filled in */
    char *fn;			/* Filename to get keys from */
{
    static KTEXT_ST ticket;     /* Temp storage for ticket */
    static KTEXT tkt = &ticket;
    static KTEXT_ST req_id_st;  /* Temp storage for authenticator */
    register KTEXT req_id = &req_id_st;

    char realm[REALM_SZ];	/* Realm of issuing kerberos */
    static Key_schedule seskey_sched; /* Key sched for session key */
    unsigned char skey[KKEY_SZ]; /* Session key from ticket */
    char sname[SNAME_SZ];	/* Service name from ticket */
    char iname[INST_SZ];	/* Instance name from ticket */
    char r_aname[ANAME_SZ];	/* Client name from authenticator */
    char r_inst[INST_SZ];	/* Client instance from authenticator */
    char r_realm[REALM_SZ];	/* Client realm from authenticator */
    unsigned int r_time_ms;     /* Fine time from authenticator */
    unsigned long r_time_sec;   /* Coarse time from authenticator */
    register char *ptr;		/* For stepping through */
    unsigned long delta_t;      /* Time in authenticator - local time */
    long tkt_age;		/* Age of ticket */
    static int swap_bytes;	/* Need to swap bytes? */
    static int mutual;		/* Mutual authentication requested? */
    static unsigned char s_kvno;/* Version number of the server's key
				 * Kerberos used to encrypt ticket */
    int status;

    if (authent->length <= 0)
	return(RD_AP_MODIFIED);

    ptr = (char *) authent->dat;

    /* get msg version, type and byte order, and server key version */

    /* check version */
    if (KRB_PROT_VERSION != (unsigned int) *ptr++)
        return(RD_AP_VERSION);

    /* byte order */
    swap_bytes = 0;
    if ((*ptr & 1) != HOST_BYTE_ORDER)
        swap_bytes++;

    /* check msg type */
    mutual = 0;
    switch (*ptr++ & ~1) {
    case AUTH_MSG_APPL_REQUEST:
        break;
    case AUTH_MSG_APPL_REQUEST_MUTUAL:
        mutual++;
        break;
    default:
        return(RD_AP_MSG_TYPE);
    }

#ifdef lint
    /* XXX mutual is set but not used; why??? */
    /* this is a crock to get lint to shut up */
    if (mutual)
        mutual = 0;
#endif /* lint */
    s_kvno = *ptr++;		/* get server key version */
    (void) strcpy(realm,ptr);   /* And the realm of the issuing KDC */
    ptr += strlen(ptr) + 1;     /* skip the realm "hint" */

    /*
     * If "fn" is NULL, key info should already be set; don't
     * bother with ticket file.  Otherwise, check to see if we
     * already have key info for the given server and key version
     * (saved in the static st_* variables).  If not, go get it
     * from the ticket file.  If "fn" is the null string, use the
     * default ticket file.
     */
    if (fn && (strcmp(st_nam,service) || strcmp(st_inst,instance) ||
               strcmp(st_rlm,realm) || (st_kvno != s_kvno))) {
        if (*fn == 0) fn = KEYFILE;
        st_kvno = s_kvno;
#ifndef NOENCRYPTION
	if (read_service_key(service,instance,realm,s_kvno,fn,(char *)skey))
		return(RD_AP_UNDEC);
	if ((status=krb_set_key((char *)skey,0))) return(status);
#endif
        (void) strcpy(st_rlm,realm);
        (void) strcpy(st_nam,service);
        (void) strcpy(st_inst,instance);
    }

    /* Get ticket from authenticator */
    tkt->length = (int) *ptr++;
    if ((tkt->length + (ptr+1 - (char *) authent->dat)) > authent->length)
	return(RD_AP_MODIFIED);
    bcopy(ptr+1,(char *)(tkt->dat),tkt->length);

    if (krb_ap_req_debug)
        log("ticket->length: %d",tkt->length);

#ifndef NOENCRYPTION
    /* Decrypt and take apart ticket */
#endif

    if (decomp_ticket(tkt,&ad->k_flags,ad->pname,ad->pinst,ad->prealm,
                      &(ad->address),ad->session, &(ad->life),
                      &(ad->time_sec),sname,iname,ky,serv_key))
        return(RD_AP_UNDEC);

    if (krb_ap_req_debug) {
        log("Ticket Contents.");
        log(" Aname:   %s.%s",ad->pname,
            ((int)*(ad->prealm) ? ad->prealm : "Athena"));
        log(" Service: %s%s%s",sname,((int)*iname ? "." : ""),iname);
    }

    /* Extract the authenticator */
    req_id->length = (int) *(ptr++);
    if ((req_id->length + (ptr + tkt->length - (char *) authent->dat)) >
	authent->length)
	return(RD_AP_MODIFIED);
    bcopy(ptr + tkt->length, (char *)(req_id->dat),req_id->length);

#ifndef NOENCRYPTION
    key_sched((C_Block *)ad->session,seskey_sched);
    pcbc_encrypt((C_Block *)req_id->dat,(C_Block *)req_id->dat,
	(long)req_id->length,seskey_sched,(C_Block *)ad->session,DES_DECRYPT);
#endif /* NOENCRYPTION */

#define check_ptr() if ((ptr - (char *) req_id->dat) > req_id->length) return(RD_AP_MODIFIED);

    ptr = (char *) req_id->dat;
    (void) strcpy(r_aname,ptr);	/* Authentication name */
    ptr += strlen(r_aname)+1;
    check_ptr();
    (void) strcpy(r_inst,ptr);	/* Authentication instance */
    ptr += strlen(r_inst)+1;
    check_ptr();
    (void) strcpy(r_realm,ptr);	/* Authentication name */
    ptr += strlen(r_realm)+1;
    check_ptr();
    bcopy(ptr,(char *)&ad->checksum,4);	/* Checksum */
    ptr += 4;
    check_ptr();
    if (swap_bytes) swap_u_long(ad->checksum);
    r_time_ms = *(ptr++);	/* Time (fine) */
#ifdef lint
    /* XXX r_time_ms is set but not used.  why??? */
    /* this is a crock to get lint to shut up */
    if (r_time_ms)
        r_time_ms = 0;
#endif /* lint */
    check_ptr();
    /* assume sizeof(r_time_sec) == 4 ?? */
    bcopy(ptr,(char *)&r_time_sec,4); /* Time (coarse) */
    if (swap_bytes) swap_u_long(r_time_sec);

    /* Check for authenticity of the request */
    if (krb_ap_req_debug)
        log("Pname:   %s %s",ad->pname,r_aname);
    if (strcmp(ad->pname,r_aname) != 0)
        return(RD_AP_INCON);
    if (strcmp(ad->pinst,r_inst) != 0)
        return(RD_AP_INCON);
    if (krb_ap_req_debug)
        log("Realm:   %s %s",ad->prealm,r_realm);
    if ((strcmp(ad->prealm,r_realm) != 0))
        return(RD_AP_INCON);

    if (krb_ap_req_debug)
        log("Address: %d %d",ad->address,from_addr);
    if (from_addr && (ad->address != from_addr))
        return(RD_AP_BADD);

    (void) gettimeofday(&t_local,(struct timezone *) 0);
    delta_t = abs((int)(t_local.tv_sec - r_time_sec));
    if (delta_t > CLOCK_SKEW) {
        if (krb_ap_req_debug)
            log("Time out of range: %d - %d = %d",
                t_local.tv_sec,r_time_sec,delta_t);
        return(RD_AP_TIME);
    }

    /* Now check for expiration of ticket */

    tkt_age = t_local.tv_sec - ad->time_sec;
    if (krb_ap_req_debug)
        log("Time: %d Issue Date: %d Diff: %d Life %x",
            t_local.tv_sec,ad->time_sec,tkt_age,ad->life);

    if (t_local.tv_sec < ad->time_sec) {
        if ((ad->time_sec - t_local.tv_sec) > CLOCK_SKEW)
            return(RD_AP_NYV);
    }
    else if ((t_local.tv_sec - ad->time_sec) > 5 * 60 * ad->life)
        return(RD_AP_EXP);

    /* All seems OK */
    ad->reply.length = 0;

    return(RD_AP_OK);
}
