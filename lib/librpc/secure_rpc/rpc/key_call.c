#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = 	"@(#)key_call.c	2.2 88/08/15 4.0 RPCSRC; from 1.11 88/02/08 SMI";
#endif
/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */
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
 * key_call.c, Interface to keyserver
 *
 * setsecretkey(key) - set your secret key
 * encryptsessionkey(agent, deskey) - encrypt a session key to talk to agent
 * decryptsessionkey(agent, deskey) - decrypt ditto
 * gendeskey(deskey) - generate a secure des key
 * netname2user(...) - get unix credential for given name (kernel only)
 */
#include <sys/param.h>
#include <sys/socket.h>
#include <rpc/rpc.h>
#include <rpc/key_prot.h>

#define KEY_TIMEOUT	5	/* per-try timeout in seconds */
#define KEY_NRETRY	12	/* number of retries */

#define debug(msg)		/* turn off debugging */

static struct timeval trytimeout = { KEY_TIMEOUT, 0 };
static struct timeval tottimeout = { KEY_TIMEOUT * KEY_NRETRY, 0 };

key_setsecret(secretkey)
	char *secretkey;
{
	keystatus status;

	if (!key_call((u_long)KEY_SET, xdr_keybuf, secretkey, xdr_keystatus, 
		(char*)&status)) 
	{
		return (-1);
	}
	if (status != KEY_SUCCESS) {
		debug("set status is nonzero");
		return (-1);
	}
	return (0);
}


key_encryptsession(remotename, deskey)
	char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_ENCRYPT, 
		xdr_cryptkeyarg, (char *)&arg, xdr_cryptkeyres, (char *)&res)) 
	{
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("encrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}


key_decryptsession(remotename, deskey)
	char *remotename;
	des_block *deskey;
{
	cryptkeyarg arg;
	cryptkeyres res;

	arg.remotename = remotename;
	arg.deskey = *deskey;
	if (!key_call((u_long)KEY_DECRYPT, 
		xdr_cryptkeyarg, (char *)&arg, xdr_cryptkeyres, (char *)&res))  
	{
		return (-1);
	}
	if (res.status != KEY_SUCCESS) {
		debug("decrypt status is nonzero");
		return (-1);
	}
	*deskey = res.cryptkeyres_u.deskey;
	return (0);
}

key_gendes(key)
	des_block *key;
{
	struct sockaddr_in sin;
	CLIENT *client;
	int socket;
	enum clnt_stat stat;

 
	sin.sin_family = AF_INET;
	sin.sin_port = 0;
	sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	bzero(sin.sin_zero, sizeof(sin.sin_zero));
	socket = RPC_ANYSOCK;
	client = clntudp_bufcreate(&sin, (u_long)KEY_PROG, (u_long)KEY_VERS,
		trytimeout, &socket, RPCSMALLMSGSIZE, RPCSMALLMSGSIZE);
	if (client == NULL) {
		return (-1);
	}
	stat = clnt_call(client, KEY_GEN, xdr_void, NULL,
		xdr_des_block, key, tottimeout);
	clnt_destroy(client);
	(void) close(socket);
	if (stat != RPC_SUCCESS) {
		return (-1);
	}
	return (0);
}


#include <stdio.h>
#include <sys/wait.h>

 
static
key_call(proc, xdr_arg, arg, xdr_rslt, rslt)
	u_long proc;
	bool_t (*xdr_arg)();
	char *arg;
	bool_t (*xdr_rslt)();
	char *rslt;
{
	XDR xdrargs;
	XDR xdrrslt;
	FILE *fargs;
	FILE *frslt;
	int (*osigchild)();
	union wait status;
	int pid;
	int success;
	int ruid;
	int euid;
	static char MESSENGER[] = "/usr/etc/keyenvoy";

	success = 1;
	osigchild = signal(SIGCHLD, SIG_IGN);

	/*
	 * We are going to exec a set-uid program which makes our effective uid
	 * zero, and authenticates us with our real uid. We need to make the 
	 * effective uid be the real uid for the setuid program, and 
	 * the real uid be the effective uid so that we can change things back.
	 */
	euid = geteuid();
	ruid = getuid();
	(void) setreuid(euid, ruid);
	pid = _openchild(MESSENGER, &fargs, &frslt);
	(void) setreuid(ruid, euid);
	if (pid < 0) {
		debug("open_streams");
		return (0);
	}
	xdrstdio_create(&xdrargs, fargs, XDR_ENCODE);
	xdrstdio_create(&xdrrslt, frslt, XDR_DECODE);

	if (!xdr_u_long(&xdrargs, &proc) || !(*xdr_arg)(&xdrargs, arg)) {
		debug("xdr args");
		success = 0; 
	}
	(void) fclose(fargs);

	if (success && !(*xdr_rslt)(&xdrrslt, rslt)) {
		debug("xdr rslt");
		success = 0;
	}

#ifdef NOTDEF
    /*
     * WARNING! XXX
     * The original code appears first.  wait4 returns only after the process
     * with the requested pid terminates.  The effect of using wait() instead
     * has not been determined.
     */
	(void) fclose(frslt);
	if (wait4(pid, &status, 0, NULL) < 0 || status.w_retcode != 0) {
		debug("wait4");
		success = 0;
	}
#endif /* def NOTDEF */
	if (wait(&status) < 0 || status.w_retcode != 0) {
		debug("wait");
		success = 0;
	}
	(void)signal(SIGCHLD, osigchild);

	return (success);
}

