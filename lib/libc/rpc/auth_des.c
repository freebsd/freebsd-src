/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 * 
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 * 
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 * 
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 * 
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 * 
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */
/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
/*
 * auth_des.c, client-side implementation of DES authentication
 */
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/cdefs.h>
#include <rpc/des_crypt.h>
#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/auth_des.h>
#include <netinet/in.h>	/* XXX: just to get htonl() and ntohl() */
#include <sys/socket.h>
#undef NIS
#include <rpcsvc/nis.h>

#if defined(LIBC_SCCS) && !defined(lint)
/* from: static char sccsid[] = 	"@(#)auth_des.c	2.2 88/07/29 4.0 RPCSRC; from 1.9 88/02/08 SMI"; */
static const char rcsid[] = "$Id$";
#endif

extern bool_t __rpc_get_time_offset __P(( struct timeval *, nis_server *,
						char *, char **, struct sockaddr_in * ));
extern int rtime __P(( struct sockaddr_in *, struct timeval *, struct timeval *));
extern bool_t xdr_authdes_cred __P(( XDR *, struct authdes_cred * ));
extern bool_t xdr_authdes_verf __P(( XDR *, struct authdes_verf * ));

#define MILLION		1000000L
#define RTIME_TIMEOUT 5		/* seconds to wait for sync */

#define AUTH_PRIVATE(auth)	(struct ad_private *) auth->ah_private
#define ALLOC(object_type)	(object_type *) mem_alloc(sizeof(object_type))
#define FREE(ptr, size)		mem_free((char *)(ptr), (int) size)
#define ATTEMPT(xdr_op)		if (!(xdr_op)) return (FALSE)

#define debug(msg)		 /*printf("%s\n", msg) */

/* 
 * DES authenticator operations vector
 */
static void	authdes_nextverf();
static bool_t	authdes_marshal();
static bool_t	authdes_validate();
static bool_t	authdes_refresh();
static void	authdes_destroy();
static struct auth_ops authdes_ops = {
	authdes_nextverf,
	authdes_marshal,
	authdes_validate,
	authdes_refresh,
	authdes_destroy
};
#ifdef foo
static bool_t	synchronize __P(( struct sockaddr *, struct timeval *));
#endif
/*
 * This struct is pointed to by the ah_private field of an "AUTH *"
 */
struct ad_private {
	char *ad_fullname; 		/* client's full name */
	u_int ad_fullnamelen;		/* length of name, rounded up */
	char *ad_servername; 		/* server's full name */
	u_int ad_servernamelen;		/* length of name, rounded up */
	u_int ad_window;	  	/* client specified window */
	bool_t ad_dosync;		/* synchronize? */		
	struct sockaddr ad_syncaddr;	/* remote host to synch with */
	char *ad_timehost;		/* remote host to synch with */
	struct timeval ad_timediff;	/* server's time - client's time */
	u_long ad_nickname;		/* server's nickname for client */
	struct authdes_cred ad_cred;	/* storage for credential */
	struct authdes_verf ad_verf;	/* storage for verifier */
	struct timeval ad_timestamp;	/* timestamp sent */
	des_block ad_xkey;		/* encrypted conversation key */
	u_char ad_pkey[1024];		/* Server's actual public key */
	char *ad_netid;			/* Timehost netid */
	char *ad_uaddr;			/* Timehost uaddr */
	nis_server *ad_nis_srvr;	/* NIS+ server struct */
};
	

/*
 * Create the client des authentication object
 */	
AUTH *
authdes_create(servername, window, syncaddr, ckey)
	char *servername;		/* network name of server */
	u_int window;			/* time to live */
	struct sockaddr *syncaddr;	/* optional addr of host to sync with */
	des_block *ckey;		/* optional conversation key to use*/
{

	AUTH *auth;
	struct ad_private *ad;
	char namebuf[MAXNETNAMELEN+1];
	u_char	pkey_data[1024];

	if (!getpublickey(servername, pkey_data))
		return(NULL);

	/*
 	 * Allocate everything now
	 */
	auth = ALLOC(AUTH);
	ad = ALLOC(struct ad_private);
	(void) getnetname(namebuf);

	ad->ad_fullnamelen = RNDUP(strlen(namebuf));
	ad->ad_fullname = (char *)mem_alloc(ad->ad_fullnamelen + 1);

	ad->ad_servernamelen = strlen(servername);
	ad->ad_servername = (char *)mem_alloc(ad->ad_servernamelen + 1);

	if (auth == NULL || ad == NULL || ad->ad_fullname == NULL ||
	    ad->ad_servername == NULL) {
		debug("authdes_create: out of memory");
		goto failed;
	}

	/* 
	 * Set up private data
	 */
	bcopy(namebuf, ad->ad_fullname, ad->ad_fullnamelen + 1);
	bcopy(servername, ad->ad_servername, ad->ad_servernamelen + 1);
	bcopy(pkey_data, ad->ad_pkey, strlen(pkey_data) + 1);
	if (syncaddr != NULL) {
		ad->ad_syncaddr = *syncaddr;
		ad->ad_dosync = TRUE;
	} else {
		ad->ad_dosync = FALSE;
	}
	ad->ad_window = window;
	if (ckey == NULL) {
		if (key_gendes(&auth->ah_key) < 0) {
			debug("authdes_create: unable to gen conversation key");
			return (NULL);
		}
	} else {
		auth->ah_key = *ckey;
	}

	/*
	 * Set up auth handle
	 */ 
	auth->ah_cred.oa_flavor = AUTH_DES;
	auth->ah_verf.oa_flavor = AUTH_DES;
	auth->ah_ops = &authdes_ops;
	auth->ah_private = (caddr_t)ad;

	if (!authdes_refresh(auth)) {
		goto failed;
	}	
	return (auth);

failed:
	if (auth != NULL) 
		FREE(auth, sizeof(AUTH)); 
	if (ad != NULL) 
		FREE(ad, sizeof(struct ad_private));
	if (ad->ad_fullname != NULL) 
		FREE(ad->ad_fullname, ad->ad_fullnamelen + 1);
	if (ad->ad_servername != NULL) 
		FREE(ad->ad_servername, ad->ad_servernamelen + 1);
	return (NULL);
}

/*
 * Slightly modified version of authdes_create which takes the public key
 * of the server principal as an argument. This spares us a call to
 * getpublickey() which in the nameserver context can cause a deadlock.
 */
AUTH *
authdes_pk_create(servername, pkey, window, timehost, ckey, srvr)
	char *servername;		/* network name of server */
	netobj *pkey;			/* public key of server */
	u_int window;			/* time to live */
	char *timehost;			/* optional hostname to sync with */
	des_block *ckey;		/* optional conversation key to use */
	nis_server *srvr;		/* optional NIS+ server struct */
{
	AUTH *auth;
	struct ad_private *ad;
	char namebuf[MAXNETNAMELEN+1];

	/*
	 * Allocate everything now
	 */
	auth = ALLOC(AUTH);
	if (auth == NULL) {
		debug("authdes_pk_create: out of memory");
		return (NULL);
	}
	ad = ALLOC(struct ad_private);
	if (ad == NULL) {
		debug("authdes_pk_create: out of memory");
		goto failed;
	}
	ad->ad_fullname = ad->ad_servername = NULL; /* Sanity reasons */
	ad->ad_timehost = NULL;
	ad->ad_netid = NULL;
	ad->ad_uaddr = NULL;
	ad->ad_nis_srvr = NULL;
	ad->ad_timediff.tv_sec = 0;
	ad->ad_timediff.tv_usec = 0;
	memcpy(ad->ad_pkey, pkey->n_bytes, pkey->n_len);
	if (!getnetname(namebuf))
		goto failed;
	ad->ad_fullnamelen = RNDUP((u_int) strlen(namebuf));
	ad->ad_fullname = (char *)mem_alloc(ad->ad_fullnamelen + 1);
	ad->ad_servernamelen = strlen(servername);
	ad->ad_servername = (char *)mem_alloc(ad->ad_servernamelen + 1);

	if (ad->ad_fullname == NULL || ad->ad_servername == NULL) {
		debug("authdes_pk_create: out of memory");
		goto failed;
	}
	if (timehost != NULL) {
		ad->ad_timehost = (char *)mem_alloc(strlen(timehost) + 1);
		if (ad->ad_timehost == NULL) {
			debug("authdes_pk_create: out of memory");
			goto failed;
		}
		memcpy(ad->ad_timehost, timehost, strlen(timehost) + 1);
		ad->ad_dosync = TRUE;
	} else if (srvr != NULL) {
		ad->ad_nis_srvr = srvr;	/* transient */
		ad->ad_dosync = TRUE;
	} else {
		ad->ad_dosync = FALSE;
	}
	memcpy(ad->ad_fullname, namebuf, ad->ad_fullnamelen + 1);
	memcpy(ad->ad_servername, servername, ad->ad_servernamelen + 1);
	ad->ad_window = window;
	if (ckey == NULL) {
		if (key_gendes(&auth->ah_key) < 0) {
			debug("authdes_pk_create: unable to gen conversation key");
			goto failed;
		}
	} else {
		auth->ah_key = *ckey;
	}

	/*
	 * Set up auth handle
	 */
	auth->ah_cred.oa_flavor = AUTH_DES;
	auth->ah_verf.oa_flavor = AUTH_DES;
	auth->ah_ops = &authdes_ops;
	auth->ah_private = (caddr_t)ad;

	if (!authdes_refresh(auth)) {
		goto failed;
	}
	ad->ad_nis_srvr = NULL; /* not needed any longer */
	return (auth);

failed:
	if (auth)
		FREE(auth, sizeof (AUTH));
	if (ad) {
		if (ad->ad_fullname)
			FREE(ad->ad_fullname, ad->ad_fullnamelen + 1);
		if (ad->ad_servername)
			FREE(ad->ad_servername, ad->ad_servernamelen + 1);
		if (ad->ad_timehost)
			FREE(ad->ad_timehost, strlen(ad->ad_timehost) + 1);
		if (ad->ad_netid)
			free(ad->ad_netid);
		if (ad->ad_uaddr)
			free(ad->ad_uaddr);
		FREE(ad, sizeof (struct ad_private));
	}
	return (NULL);
}
/*
 * Implement the five authentication operations
 */


/*
 * 1. Next Verifier
 */	
/*ARGSUSED*/
static void
authdes_nextverf(auth)
	AUTH *auth;
{
	/* what the heck am I supposed to do??? */
}
		


/*
 * 2. Marshal
 */
static bool_t
authdes_marshal(auth, xdrs)
	AUTH *auth;
	XDR *xdrs;
{
	struct ad_private *ad = AUTH_PRIVATE(auth);
	struct authdes_cred *cred = &ad->ad_cred;
	struct authdes_verf *verf = &ad->ad_verf;
	des_block cryptbuf[2];	
	des_block ivec;
	int status;
	long len;
	int32_t *ixdr;

	/*
	 * Figure out the "time", accounting for any time difference
	 * with the server if necessary.
	 */
	(void) gettimeofday(&ad->ad_timestamp, (struct timezone *)NULL);
	ad->ad_timestamp.tv_sec += ad->ad_timediff.tv_sec;
	ad->ad_timestamp.tv_usec += ad->ad_timediff.tv_usec;
	if (ad->ad_timestamp.tv_usec >= MILLION) {
		ad->ad_timestamp.tv_usec -= MILLION;
		ad->ad_timestamp.tv_sec += 1;
	}

	/*
	 * XDR the timestamp and possibly some other things, then
	 * encrypt them.
	 */
	ixdr = (int32_t *)cryptbuf;
	IXDR_PUT_LONG(ixdr, ad->ad_timestamp.tv_sec);
	IXDR_PUT_LONG(ixdr, ad->ad_timestamp.tv_usec);
	if (ad->ad_cred.adc_namekind == ADN_FULLNAME) {
		IXDR_PUT_U_LONG(ixdr, ad->ad_window);
		IXDR_PUT_U_LONG(ixdr, ad->ad_window - 1);
		ivec.key.high = ivec.key.low = 0;	
		status = cbc_crypt((char *)&auth->ah_key, (char *)cryptbuf, 
			2*sizeof(des_block), DES_ENCRYPT | DES_HW, (char *)&ivec);
	} else {
		status = ecb_crypt((char *)&auth->ah_key, (char *)cryptbuf, 
			sizeof(des_block), DES_ENCRYPT | DES_HW);
	}
	if (DES_FAILED(status)) {
		debug("authdes_marshal: DES encryption failure");
		return (FALSE);
	}
	ad->ad_verf.adv_xtimestamp = cryptbuf[0];
	if (ad->ad_cred.adc_namekind == ADN_FULLNAME) {
		ad->ad_cred.adc_fullname.window = cryptbuf[1].key.high;
		ad->ad_verf.adv_winverf = cryptbuf[1].key.low;
	} else {
		ad->ad_cred.adc_nickname = ad->ad_nickname;
		ad->ad_verf.adv_winverf = 0;
	}

	/*
	 * Serialize the credential and verifier into opaque
	 * authentication data.
	 */
	if (ad->ad_cred.adc_namekind == ADN_FULLNAME) {
		len = ((1 + 1 + 2 + 1)*BYTES_PER_XDR_UNIT + ad->ad_fullnamelen);
	} else {
		len = (1 + 1)*BYTES_PER_XDR_UNIT;
	}

	if ((ixdr = xdr_inline(xdrs, 2*BYTES_PER_XDR_UNIT))) {
		IXDR_PUT_LONG(ixdr, AUTH_DES);
		IXDR_PUT_LONG(ixdr, len);
	} else {
		ATTEMPT(xdr_putlong(xdrs, (long *)&auth->ah_cred.oa_flavor)); 
		ATTEMPT(xdr_putlong(xdrs, &len)); 
	}
	ATTEMPT(xdr_authdes_cred(xdrs, cred));

	len = (2 + 1)*BYTES_PER_XDR_UNIT; 
	if ((ixdr = xdr_inline(xdrs, 2*BYTES_PER_XDR_UNIT))) {
		IXDR_PUT_LONG(ixdr, AUTH_DES);
		IXDR_PUT_LONG(ixdr, len);
	} else {
		ATTEMPT(xdr_putlong(xdrs, (long *)&auth->ah_verf.oa_flavor)); 
		ATTEMPT(xdr_putlong(xdrs, &len)); 
	}
	ATTEMPT(xdr_authdes_verf(xdrs, verf));
	return (TRUE);
}


/*
 * 3. Validate
 */
static bool_t
authdes_validate(auth, rverf)
	AUTH *auth;
	struct opaque_auth *rverf;
{
	struct ad_private *ad = AUTH_PRIVATE(auth);
	struct authdes_verf verf;
	int status;
	register u_long *ixdr;

	if (rverf->oa_length != (2 + 1) * BYTES_PER_XDR_UNIT) {
		return (FALSE);
	}
	ixdr = (u_long *)rverf->oa_base;
	verf.adv_xtimestamp.key.high = (u_long)*ixdr++;
	verf.adv_xtimestamp.key.low = (u_long)*ixdr++;
	verf.adv_int_u = (u_long)*ixdr++;	/* nickname not XDR'd ! */

	/*
	 * Decrypt the timestamp
	 */
	status = ecb_crypt((char *)&auth->ah_key, (char *)&verf.adv_xtimestamp,
		sizeof(des_block), DES_DECRYPT | DES_HW);

	if (DES_FAILED(status)) {
		debug("authdes_validate: DES decryption failure");
		return (FALSE);
	}

	/*
	 * xdr the decrypted timestamp 
	 */
	ixdr = (u_long *)verf.adv_xtimestamp.c;
	verf.adv_timestamp.tv_sec = IXDR_GET_LONG(ixdr) + 1;
	verf.adv_timestamp.tv_usec = IXDR_GET_LONG(ixdr);

	/*
	 * validate
	 */
	if (bcmp((char *)&ad->ad_timestamp, (char *)&verf.adv_timestamp,
		 sizeof(struct timeval)) != 0) {
		debug("authdes_validate: verifier mismatch\n");
		return (FALSE);
	}

	/*
	 * We have a nickname now, let's use it
	 */
	ad->ad_nickname = verf.adv_nickname;	
	ad->ad_cred.adc_namekind = ADN_NICKNAME; 
	return (TRUE);	
}

/*
 * 4. Refresh
 */
static bool_t
authdes_refresh(auth)
	AUTH *auth;
{
	struct ad_private *ad = AUTH_PRIVATE(auth);
	struct authdes_cred *cred = &ad->ad_cred;
	netobj		pkey;

	if (ad->ad_dosync &&
#ifdef old
			!synchronize(&ad->ad_syncaddr, &ad->ad_timediff)) {
#else
			!__rpc_get_time_offset(&ad->ad_timediff,ad->ad_nis_srvr,
					       ad->ad_timehost, &(ad->ad_uaddr),
				(struct sockaddr_in *)&(ad->ad_syncaddr))) {
#endif
		/*
		 * Hope the clocks are synced!
		 */
		ad->ad_timediff.tv_sec = ad->ad_timediff.tv_usec = 0;
		ad->ad_dosync = 0;
		debug("authdes_refresh: unable to synchronize with server");
	}
	ad->ad_xkey = auth->ah_key;
	pkey.n_bytes = (char *)(ad->ad_pkey);
	pkey.n_len = strlen((char *)ad->ad_pkey) + 1;
	if (key_encryptsession_pk(ad->ad_servername, &pkey, &ad->ad_xkey) < 0) {
		debug("authdes_create: unable to encrypt conversation key");
		return (FALSE);
	}
	cred->adc_fullname.key = ad->ad_xkey;
	cred->adc_namekind = ADN_FULLNAME;
	cred->adc_fullname.name = ad->ad_fullname;
	return (TRUE);
}


/*
 * 5. Destroy
 */
static void
authdes_destroy(auth)
	AUTH *auth;
{
	struct ad_private *ad = AUTH_PRIVATE(auth);

	FREE(ad->ad_fullname, ad->ad_fullnamelen + 1);
	FREE(ad->ad_servername, ad->ad_servernamelen + 1);
	FREE(ad, sizeof(struct ad_private));
	FREE(auth, sizeof(AUTH));
}
	

#ifdef old
/*
 * Synchronize with the server at the given address, that is,
 * adjust timep to reflect the delta between our clocks
 */
static bool_t
synchronize(syncaddr, timep)
	struct sockaddr *syncaddr;
	struct timeval *timep;
{
	struct timeval mytime;
	struct timeval timeout;

	timeout.tv_sec = RTIME_TIMEOUT;
	timeout.tv_usec = 0;
	if (rtime((struct sockaddr_in *)syncaddr, timep, NULL /*&timeout*/) < 0) {
		return (FALSE);
	}
	(void) gettimeofday(&mytime, (struct timezone *)NULL);
	timep->tv_sec -= mytime.tv_sec;
	if (mytime.tv_usec > timep->tv_usec) {
		timep->tv_sec -= 1;
		timep->tv_usec += MILLION;
	}
	timep->tv_usec -= mytime.tv_usec;
	return (TRUE);
}
#endif
