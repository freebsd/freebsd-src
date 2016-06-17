/*
 * linux/net/sunrpc/svcauth_des.c
 *
 * Server-side AUTH_DES handling.
 * 
 * Copyright (C) 1996, 1997 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sunrpc/types.h>
#include <linux/sunrpc/xdr.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/sunrpc/svcsock.h>

#define RPCDBG_FACILITY	RPCDBG_AUTH

/*
 * DES cedential cache.
 * The cache is indexed by fullname/key to allow for multiple sessions
 * by the same user from different hosts.
 * It would be tempting to use the client's IP address rather than the
 * conversation key as an index, but that could become problematic for
 * multi-homed hosts that distribute traffic across their interfaces.
 */
struct des_cred {
	struct des_cred *	dc_next;
	char *			dc_fullname;
	u32			dc_nickname;
	des_cblock		dc_key;		/* conversation key */
	des_cblock		dc_xkey;	/* encrypted conv. key */
	des_key_schedule	dc_keysched;
};

#define ADN_FULLNAME		0
#define ADN_NICKNAME		1

/*
 * The default slack allowed when checking for replayed credentials
 * (in milliseconds).
 */
#define DES_REPLAY_SLACK	2000

/*
 * Make sure we don't place more than one call to the key server at
 * a time.
 */
static int			in_keycall = 0;

#define FAIL(err) \
	{ if (data) put_cred(data);			\
	  *authp = rpc_autherr_##err;			\
	  return;					\
	}

void
svcauth_des(struct svc_rqst *rqstp, u32 *statp, u32 *authp)
{
	struct svc_buf	*argp = &rqstp->rq_argbuf;
	struct svc_buf	*resp = &rqstp->rq_resbuf;
	struct svc_cred	*cred = &rqstp->rq_cred;
	struct des_cred	*data = NULL;
	u32		cryptkey[2];
	u32		cryptbuf[4];
	u32		*p = argp->buf;
	int		len   = argp->len, slen, i;

	*authp = rpc_auth_ok;

	if ((argp->len -= 3) < 0) {
		*statp = rpc_garbage_args;
		return;
	}

	p++;					/* skip length field */
	namekind = ntohl(*p++);			/* fullname/nickname */

	/* Get the credentials */
	if (namekind == ADN_NICKNAME) {
		/* If we can't find the cached session key, initiate a
		 * new session. */
		if (!(data = get_cred_bynick(*p++)))
			FAIL(rejectedcred);
	} else if (namekind == ADN_FULLNAME) {
		p = xdr_decode_string(p, &fullname, &len, RPC_MAXNETNAMELEN);
		if (p == NULL)
			FAIL(badcred);
		cryptkey[0] = *p++;		/* get the encrypted key */
		cryptkey[1] = *p++;
		cryptbuf[2] = *p++;		/* get the encrypted window */
	} else {
		FAIL(badcred);
	}

	/* If we're just updating the key, silently discard the request. */
	if (data && data->dc_locked) {
		*authp = rpc_autherr_dropit;
		_put_cred(data);	/* release but don't unlock */
		return;
	}

	/* Get the verifier flavor and length */
	if (ntohl(*p++) != RPC_AUTH_DES && ntohl(*p++) != 12)
		FAIL(badverf);

	cryptbuf[0] = *p++;			/* encrypted time stamp */
	cryptbuf[1] = *p++;
	cryptbuf[3] = *p++;			/* 0 or window - 1 */

	if (namekind == ADN_NICKNAME) {
		status = des_ecb_encrypt((des_block *) cryptbuf,
					 (des_block *) cryptbuf,
					 data->dc_keysched, DES_DECRYPT);
	} else {
		/* We first have to decrypt the new session key and
		 * fill in the UNIX creds. */
		if (!(data = get_cred_byname(rqstp, authp, fullname, cryptkey)))
			return;
		status = des_cbc_encrypt((des_cblock *) cryptbuf,
					 (des_cblock *) cryptbuf, 16,
					 data->dc_keysched,
					 (des_cblock *) &ivec,
					 DES_DECRYPT);
	}
	if (status) {
		printk("svcauth_des: DES decryption failed (status %d)\n",
				status);
		FAIL(badverf);
	}

	/* Now check the whole lot */
	if (namekind == ADN_FULLNAME) {
		unsigned long	winverf;

		data->dc_window = ntohl(cryptbuf[2]);
		winverf = ntohl(cryptbuf[2]);
		if (window != winverf - 1) {
			printk("svcauth_des: bad window verifier!\n");
			FAIL(badverf);
		}
	}

	/* XDR the decrypted timestamp */
	cryptbuf[0] = ntohl(cryptbuf[0]);
	cryptbuf[1] = ntohl(cryptbuf[1]);
	if (cryptbuf[1] > 1000000) {
		dprintk("svcauth_des: bad usec value %u\n", cryptbuf[1]);
		if (namekind == ADN_NICKNAME)
			FAIL(rejectedverf);
		FAIL(badverf);
	}
	
	/*
	 * Check for replayed credentials. We must allow for reordering
	 * of requests by the network, and the OS scheduler, hence we
	 * cannot expect timestamps to be increasing monotonically.
	 * This opens a small security hole, therefore the replay_slack
	 * value shouldn't be too large.
	 */
	if ((delta = cryptbuf[0] - data->dc_timestamp[0]) <= 0) {
		switch (delta) {
		case -1:	
			delta = -1000000;
		case 0:
			delta += cryptbuf[1] - data->dc_timestamp[1];
			break;
		default:
			delta = -1000000;
		}
		if (delta < DES_REPLAY_SLACK)
			FAIL(rejectedverf);
#ifdef STRICT_REPLAY_CHECKS
		/* TODO: compare time stamp to last five timestamps cached
		 * and reject (drop?) request if a match is found. */
#endif
	}

	now = xtime;
	now.tv_secs -= data->dc_window;
	if (now.tv_secs < cryptbuf[0] ||
	    (now.tv_secs == cryptbuf[0] && now.tv_usec < cryptbuf[1]))
		FAIL(rejectedverf);

	/* Okay, we're done. Update the lot */
	if (namekind == ADN_FULLNAME)
		data->dc_valid = 1;
	data->dc_timestamp[0] = cryptbuf[0];
	data->dc_timestamp[1] = cryptbuf[1];

	put_cred(data);
	return;
garbage:
	*statp = rpc_garbage_args;
	return;
}

/*
 * Call the keyserver to obtain the decrypted conversation key and
 * UNIX creds. We use a Linux-specific keycall extension that does
 * both things in one go.
 */
static struct des_cred *
get_cred_byname(struct svc_rqst *rqstp, u32 *authp, char *fullname, u32 *cryptkey)
{
	static int	in_keycall = 0;
	struct des_cred	*cred;

	if (in_keycall) {
		*authp = rpc_autherr_dropit;
		return NULL;
	}
	in_keycall = 1;
	in_keycall = 0;
	return cred;
}
