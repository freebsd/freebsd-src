/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1996 Christos Zoulas. 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Christos Zoulas.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*
 * In SVR4 unix domain sockets are referenced sometimes
 * (in putmsg(2) for example) as a [device, inode] pair instead of a pathname.
 * Since there is no iname() routine in the kernel, and we need access to
 * a mapping from inode to pathname, we keep our own table. This is a simple
 * linked list that contains the pathname, the [device, inode] pair, the
 * file corresponding to that socket and the process. When the
 * socket gets closed we remove the item from the list. The list gets loaded
 * every time a stat(2) call finds a socket.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sysproto.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/proc.h>
#include <sys/malloc.h>

#include <compat/svr4/svr4.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_util.h>
#include <compat/svr4/svr4_socket.h>
#include <compat/svr4/svr4_signal.h>
#include <compat/svr4/svr4_sockmod.h>
#include <compat/svr4/svr4_proto.h>

struct sockaddr_un *
svr4_find_socket(td, fp, dev, ino)
	struct thread *td;
	struct file *fp;
	dev_t dev;
	ino_t ino;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *)fp->f_data)->so_emuldata;

	if (svr4_str_initialized != 2) {
		if (atomic_cmpset_acq_int(&svr4_str_initialized, 0, 1)) {
			DPRINTF(("svr4_find_socket: uninitialized [%p,%d,%d]\n",
			    td, dev, ino));
			TAILQ_INIT(&svr4_head);
			atomic_store_rel_int(&svr4_str_initialized, 2);
		}
		return NULL;
	}


	DPRINTF(("svr4_find_socket: [%p,%d,%d]: ", td, dev, ino));
	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == td->td_proc && e->dev == dev && e->ino == ino) {
#ifdef DIAGNOSTIC
			if (e->cookie != NULL && e->cookie != cookie)
				panic("svr4 socket cookie mismatch");
#endif
			e->cookie = cookie;
			DPRINTF(("%s\n", e->sock.sun_path));
			return &e->sock;
		}

	DPRINTF(("not found\n"));
	return NULL;
}


/*
 * svr4_delete_socket() is in sys/dev/streams.c (because it's called by
 * the streams "soo_close()" routine).
 */
int
svr4_add_socket(td, path, st)
	struct thread *td;
	const char *path;
	struct stat *st;
{
	struct svr4_sockcache_entry *e;
	int len, error;

	/*
	 * Wait for the TAILQ to be initialized.  Only the very first CPU
	 * will succeed on the atomic_cmpset().  The other CPU's will spin
	 * until the first one finishes the initialization.  Once the
	 * initialization is complete, the condition will always fail
	 * avoiding expensive atomic operations in the common case.
	 */
	while (svr4_str_initialized != 2)
		if (atomic_cmpset_acq_int(&svr4_str_initialized, 0, 1)) {
			TAILQ_INIT(&svr4_head);
			atomic_store_rel_int(&svr4_str_initialized, 2);
		}

	e = malloc(sizeof(*e), M_TEMP, M_WAITOK);
	e->cookie = NULL;
	e->dev = st->st_dev;
	e->ino = st->st_ino;
	e->p = td->td_proc;

	if ((error = copyinstr(path, e->sock.sun_path,
	    sizeof(e->sock.sun_path), &len)) != 0) {
		DPRINTF(("svr4_add_socket: copyinstr failed %d\n", error));
		free(e, M_TEMP);
		return error;
	}

	e->sock.sun_family = AF_LOCAL;
	e->sock.sun_len = len;

	TAILQ_INSERT_HEAD(&svr4_head, e, entries);
	DPRINTF(("svr4_add_socket: %s [%p,%d,%d]\n", e->sock.sun_path,
		 td->td_proc, e->dev, e->ino));
	return 0;
}


int
svr4_sys_socket(td, uap)
	struct thread *td;
	struct svr4_sys_socket_args *uap;
{
	switch (uap->type) {
	case SVR4_SOCK_DGRAM:
		uap->type = SOCK_DGRAM;
		break;

	case SVR4_SOCK_STREAM:
		uap->type = SOCK_STREAM;
		break;

	case SVR4_SOCK_RAW:
		uap->type = SOCK_RAW;
		break;

	case SVR4_SOCK_RDM:
		uap->type = SOCK_RDM;
		break;

	case SVR4_SOCK_SEQPACKET:
		uap->type = SOCK_SEQPACKET;
		break;
	default:
		return EINVAL;
	}
	return socket(td, (struct socket_args *)uap);
}
