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
 * Copyright (c) 1986-1991 by Sun Microsystems Inc. 
 */

#ident	"@(#)key_call.c	1.25	94/04/24 SMI"

#ifndef lint
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * key_call.c, Interface to keyserver
 *
 * setsecretkey(key) - set your secret key
 * encryptsessionkey(agent, deskey) - encrypt a session key to talk to agent
 * decryptsessionkey(agent, deskey) - decrypt ditto
 * gendeskey(deskey) - generate a secure des key
 */

#include "reentrant.h"
#include "namespace.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <rpc/rpc.h>
#include <rpc/auth.h>
#include <rpc/auth_unix.h>
#include <rpc/key_prot.h>
#include <string.h>
#include <netconfig.h>
#include <sys/utsname.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/fcntl.h>
#include "un-namespace.h"


#define	KEY_TIMEOUT	5	/* per-try timeout in seconds */
#define	KEY_NRETRY	12	/* number of retries */

#ifdef DEBUG
#define	debug(msg)	(void) fprintf(stderr, "%s\n", msg);
#else
#define	debug(msg)
#endif /* DEBUG */

/*
 * Hack to allow the keyserver to use AUTH_DES (for authenticated
 * NIS+ calls, for example).  The only functions that get called
 * are key_encryptsession_pk, key_decryptsession_pk, and key_gendes.
 *
 * The approach is to have the keyserver fill in pointers to local
 * implementations of these functions, and to call those in key_call().
 */

cryptkeyres *(*__key_encryptsession_pk_LOCAL)() = 0;
cryptkeyres *(*__key_decryptsession_pk_LOCAL)() = 0;
des_block *(*__key_gendes_LOCAL)() = 0;

static int key_call __P(( u_long, xdrproc_t, char *, xdrproc_t, char * ));

int
key_setsecret(secretkey)
	const char *secretkey;
{
	keystatus status;

	if (!key_call((u_long) KEY_SET, xdr_keybuf, (char *) secretkey,
			xdr_keystatus, (char *)&status)) {
		return (-1);
	}
	if (status != KEY_SUCCESS) {
		debug("set status is nonzero");
		return (-1);
	}
	return (0);
}


/* key_secretkey_is_set() returns 1 if the keyserver has a secret key
 * stored for the caller's effective uid; it returns 0 otherwise
 *
 * N.B.:  The KEY_NET_GET key call is undocumented.  Applications shouldn't
 * be using it, because it allows them to get the user's secret key.
 */

int
key_secretkey_is_set(void)
{
	struct key_netstres 	kres;

	memset((void*)&kres, 0, sizeof (kres));
	if (key_call((u_long) KEY_NET_GET, xdr_void, (char *)NULL,
			xdr_key_netstres, (char *) &kres) &&
	    (kres.status == KEY_SUCCESS) &&
	    (kres.key_netstres_u.knet.st_priv_key[0] != 0)) {
		/* avoid leaving secret key in memory */
		memset(kres.key_netstres_u.knet.st_priv_key, 0, HEXKEYBYTES);
		return (1);
	}
	return (0);
}

int
key_encryptsession_pk(remotename, remotekey, deskey)
	char *remotename;
	netobj *remotekey;
	des_block *deskey;
{
	cryptkeyarg2 arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.remotekey = *remotekey;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT_PK, xdr_cryptkeyarg2, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

int
key_decryptsession_pk(remotename, remotekey, deskey)
	char *remotename;
	netobj *remotekey;
	des_block *deskey;
{
	cryptkeyarg2 arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.remotekey = *remotekey;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT_PK, xdr_cryptkeyarg2, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

int
key_encryptsession(remotename, deskey)
	const char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = (char *) remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

int
key_decryptsession(remotename, deskey)
	const char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = (char *) remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT, xdr_cryptkeyarg, (char *)&arg,
			xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

int
key_gendes(key)
	des_block *key;
{
	if (!key_call((u_long)KEY_GEN, xdr_void, (char *)NULL,
			xdr_des_block, (char *)key)) {
		return (-1);
	}
	return (0);
}

int
key_setnet(arg)
struct key_netstarg *arg;
{
	keystatus status;


	if (!key_call((u_long) KEY_NET_PUT, xdr_key_netstarg, (char *) arg,
		xdr_keystatus, (char *) &status)){
		return (-1);
	}

	if (status != KEY_SUCCESS) {
		debug("key_setnet status is nonzero");
		return (-1);
	}
	return (1);
}


int
key_get_conv(pkey, deskey)
	char *pkey;
	des_block *deskey;
{
	cryptkeyres res;

	if (!key_call((u_long) KEY_GET_CONV, xdr_keybuf, pkey,
		xdr_cryptkeyres, (char *)&res)) {
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("get_conv status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

struct  key_call_private {
	CLIENT	*client;	/* Client handle */
	pid_t	pid;		/* process-id at moment of creation */
	uid_t	uid;		/* user-id at last authorization */
};
static struct key_call_private *key_call_private_main = NULL;

static void
key_call_destroy(void *vp)
{
	register struct key_call_private *kcp = (struct key_call_private *)vp;

	if (kcp) {
		if (kcp->client)
			clnt_destroy(kcp->client);
		free(kcp);
	}
}

/*
 * Keep the handle cached.  This call may be made quite often.
 */
static CLIENT *
getkeyserv_handle(vers)
int	vers;
{
	void *localhandle;
	struct netconfig *nconf;
	struct netconfig *tpconf;
	struct key_call_private *kcp = key_call_private_main;
	struct timeval wait_time;
	struct utsname u;
	int main_thread;
	int fd;
	static thread_key_t key_call_key;
	extern mutex_t tsd_lock;

#define	TOTAL_TIMEOUT	30	/* total timeout talking to keyserver */
#define	TOTAL_TRIES	5	/* Number of tries */

	if ((main_thread = thr_main())) {
		kcp = key_call_private_main;
	} else {
		if (key_call_key == 0) {
			mutex_lock(&tsd_lock);
			if (key_call_key == 0)
				thr_keycreate(&key_call_key, key_call_destroy);
			mutex_unlock(&tsd_lock);
		}
		kcp = (struct key_call_private *)thr_getspecific(key_call_key);
	}	
	if (kcp == (struct key_call_private *)NULL) {
		kcp = (struct key_call_private *)malloc(sizeof (*kcp));
		if (kcp == (struct key_call_private *)NULL) {
			return ((CLIENT *) NULL);
		}
                if (main_thread)
                        key_call_private_main = kcp;
                else
                        thr_setspecific(key_call_key, (void *) kcp);
		kcp->client = NULL;
	}

	/* if pid has changed, destroy client and rebuild */
	if (kcp->client != NULL && kcp->pid != getpid()) {
		clnt_destroy(kcp->client);
		kcp->client = NULL;
	}

	if (kcp->client != NULL) {
		/* if uid has changed, build client handle again */
		if (kcp->uid != geteuid()) {
			kcp->uid = geteuid();
			auth_destroy(kcp->client->cl_auth);
			kcp->client->cl_auth =
				authsys_create("", kcp->uid, 0, 0, NULL);
			if (kcp->client->cl_auth == NULL) {
				clnt_destroy(kcp->client);
				kcp->client = NULL;
				return ((CLIENT *) NULL);
			}
		}
		/* Change the version number to the new one */
		clnt_control(kcp->client, CLSET_VERS, (void *)&vers);
		return (kcp->client);
	}
	if (!(localhandle = setnetconfig())) {
		return ((CLIENT *) NULL);
	}
        tpconf = NULL;
#if defined(__FreeBSD__)
	if (uname(&u) == -1)
#else
#if defined(i386)
	if (_nuname(&u) == -1)
#elif defined(sparc)
	if (_uname(&u) == -1)
#else
#error Unknown architecture!
#endif
#endif
	{
		endnetconfig(localhandle);
		return ((CLIENT *) NULL);
        }

	while (nconf = getnetconfig(localhandle)) {
		if (strcmp(nconf->nc_protofmly, NC_LOOPBACK) == 0) {
			/*
			 * We use COTS_ORD here so that the caller can
			 * find out immediately if the server is dead.
			 */
			if (nconf->nc_semantics == NC_TPI_COTS_ORD) {
				kcp->client = clnt_tp_create(u.nodename,
					KEY_PROG, vers, nconf);
				if (kcp->client)
					break;
			} else {
				tpconf = nconf;
			}
		}
	}
	if ((kcp->client == (CLIENT *) NULL) && (tpconf))
		/* Now, try the CLTS or COTS loopback transport */
		kcp->client = clnt_tp_create(u.nodename,
			KEY_PROG, vers, tpconf);
	endnetconfig(localhandle);

	if (kcp->client == (CLIENT *) NULL) {
		return ((CLIENT *) NULL);
        }
	kcp->uid = geteuid();
	kcp->pid = getpid();
	kcp->client->cl_auth = authsys_create("", kcp->uid, 0, 0, NULL);
	if (kcp->client->cl_auth == NULL) {
		clnt_destroy(kcp->client);
		kcp->client = NULL;
		return ((CLIENT *) NULL);
	}

	wait_time.tv_sec = TOTAL_TIMEOUT/TOTAL_TRIES;
	wait_time.tv_usec = 0;
	(void) clnt_control(kcp->client, CLSET_RETRY_TIMEOUT,
		(char *)&wait_time);
	if (clnt_control(kcp->client, CLGET_FD, (char *)&fd))
		_fcntl(fd, F_SETFD, 1);	/* make it "close on exec" */

	return (kcp->client);
}

/* returns  0 on failure, 1 on success */

static int
key_call(proc, xdr_arg, arg, xdr_rslt, rslt)
	u_long proc;
	xdrproc_t xdr_arg;
	char *arg;
	xdrproc_t xdr_rslt;
	char *rslt;
{
	CLIENT *clnt;
	struct timeval wait_time;

	if (proc == KEY_ENCRYPT_PK && __key_encryptsession_pk_LOCAL) {
		cryptkeyres *res;
		res = (*__key_encryptsession_pk_LOCAL)(geteuid(), arg);
		*(cryptkeyres*)rslt = *res;
		return (1);
	} else if (proc == KEY_DECRYPT_PK && __key_decryptsession_pk_LOCAL) {
		cryptkeyres *res;
		res = (*__key_decryptsession_pk_LOCAL)(geteuid(), arg);
		*(cryptkeyres*)rslt = *res;
		return (1);
	} else if (proc == KEY_GEN && __key_gendes_LOCAL) {
		des_block *res;
		res = (*__key_gendes_LOCAL)(geteuid(), 0);
		*(des_block*)rslt = *res;
		return (1);
	}

	if ((proc == KEY_ENCRYPT_PK) || (proc == KEY_DECRYPT_PK) ||
	    (proc == KEY_NET_GET) || (proc == KEY_NET_PUT) ||
	    (proc == KEY_GET_CONV))
		clnt = getkeyserv_handle(2); /* talk to version 2 */
	else
		clnt = getkeyserv_handle(1); /* talk to version 1 */

	if (clnt == NULL) {
		return (0);
	}

	wait_time.tv_sec = TOTAL_TIMEOUT;
	wait_time.tv_usec = 0;

	if (clnt_call(clnt, proc, xdr_arg, arg, xdr_rslt, rslt,
		wait_time) == RPC_SUCCESS) {
		return (1);
	} else {
		return (0);
	}
}
