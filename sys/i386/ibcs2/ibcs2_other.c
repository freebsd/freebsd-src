/*
 * Copyright (c) 1995 Steven Wallace
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: ibcs2_other.c,v 1.7 1997/08/25 21:57:55 bde Exp $
 */

/*
 * IBCS2 compatibility module.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysproto.h>

#include <i386/ibcs2/ibcs2_types.h>
#include <i386/ibcs2/ibcs2_signal.h>
#include <i386/ibcs2/ibcs2_util.h>
#include <i386/ibcs2/ibcs2_proto.h>

#define IBCS2_SECURE_GETLUID 1
#define IBCS2_SECURE_SETLUID 2

int
ibcs2_secure(struct proc *p, struct ibcs2_secure_args *uap)
{
	switch (uap->cmd) {

	case IBCS2_SECURE_GETLUID:		/* get login uid */
		p->p_retval[0] = p->p_ucred->cr_uid;
		return 0;

	case IBCS2_SECURE_SETLUID:		/* set login uid */
		return EPERM;

	default:
		printf("IBCS2: 'secure' cmd=%d not implemented\n", uap->cmd);
	}

	return EINVAL;
}

int
ibcs2_lseek(struct proc *p, register struct ibcs2_lseek_args *uap)
{
	struct lseek_args largs;
	int error;

	largs.fd = uap->fd;
	largs.offset = uap->offset;
	largs.whence = uap->whence;
	error = lseek(p, &largs);
	return (error);
}

#ifdef SPX_HACK
#include <sys/socket.h>
#include <sys/un.h>     

int
spx_open(struct proc *p, void *uap)
{
	struct socket_args sock;
	struct connect_args conn;
	struct sockaddr_un *Xaddr;
	int fd, error;
	caddr_t sg = stackgap_init();

	/* obtain a socket. */
	DPRINTF(("SPX: open socket\n"));
	sock.domain = AF_UNIX;
	sock.type = SOCK_STREAM;
	sock.protocol = 0;
	error = socket(p, &sock);
	if (error)
		return error;

	/* connect the socket to standard X socket */
	DPRINTF(("SPX: connect to /tmp/X11-unix/X0\n"));
	Xaddr = stackgap_alloc(&sg, sizeof(struct sockaddr_un));
	Xaddr->sun_family = AF_UNIX;
	Xaddr->sun_len = sizeof(struct sockaddr_un) - sizeof(Xaddr->sun_path) +
	  strlen(Xaddr->sun_path) + 1;
	copyout("/tmp/.X11-unix/X0", Xaddr->sun_path, 18);

	conn.s = fd = p->p_retval[0];
	conn.name = (caddr_t)Xaddr;
	conn.namelen = sizeof(struct sockaddr_un);
	error = connect(p, &conn);
	if (error) {
		struct close_args cl;
		cl.fd = fd;
		close(p, &cl);
		return error;
	}
	p->p_retval[0] = fd;
	return 0;
}
#endif /* SPX_HACK */
