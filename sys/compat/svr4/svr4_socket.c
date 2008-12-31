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
__FBSDID("$FreeBSD: src/sys/compat/svr4/svr4_socket.c,v 1.27.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/eventhandler.h>
#include <sys/file.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
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

struct svr4_sockcache_entry {
	struct proc *p;		/* Process for the socket		*/
	void *cookie;		/* Internal cookie used for matching	*/
	struct sockaddr_un sock;/* Pathname for the socket		*/
	dev_t dev;		/* Device where the socket lives on	*/
	ino_t ino;		/* Inode where the socket lives on	*/
	TAILQ_ENTRY(svr4_sockcache_entry) entries;
};

static TAILQ_HEAD(, svr4_sockcache_entry) svr4_head;
static struct mtx svr4_sockcache_lock;
static eventhandler_tag svr4_sockcache_exit_tag, svr4_sockcache_exec_tag;

static void	svr4_purge_sockcache(void *arg, struct proc *p);

int
svr4_find_socket(td, fp, dev, ino, saun)
	struct thread *td;
	struct file *fp;
	dev_t dev;
	ino_t ino;
	struct sockaddr_un *saun;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *)fp->f_data)->so_emuldata;

	DPRINTF(("svr4_find_socket: [%p,%d,%d]: ", td, dev, ino));
	mtx_lock(&svr4_sockcache_lock);
	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == td->td_proc && e->dev == dev && e->ino == ino) {
#ifdef DIAGNOSTIC
			if (e->cookie != NULL && e->cookie != cookie)
				panic("svr4 socket cookie mismatch");
#endif
			e->cookie = cookie;
			DPRINTF(("%s\n", e->sock.sun_path));
			*saun = e->sock;
			mtx_unlock(&svr4_sockcache_lock);
			return (0);
		}

	mtx_unlock(&svr4_sockcache_lock);
	DPRINTF(("not found\n"));
	return (ENOENT);
}

int
svr4_add_socket(td, path, st)
	struct thread *td;
	const char *path;
	struct stat *st;
{
	struct svr4_sockcache_entry *e;
	int len, error;

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

	mtx_lock(&svr4_sockcache_lock);
	TAILQ_INSERT_HEAD(&svr4_head, e, entries);
	mtx_unlock(&svr4_sockcache_lock);
	DPRINTF(("svr4_add_socket: %s [%p,%d,%d]\n", e->sock.sun_path,
		 td->td_proc, e->dev, e->ino));
	return 0;
}

void
svr4_delete_socket(p, fp)
	struct proc *p;
	struct file *fp;
{
	struct svr4_sockcache_entry *e;
	void *cookie = ((struct socket *)fp->f_data)->so_emuldata;

	mtx_lock(&svr4_sockcache_lock);
	TAILQ_FOREACH(e, &svr4_head, entries)
		if (e->p == p && e->cookie == cookie) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			mtx_unlock(&svr4_sockcache_lock);
			DPRINTF(("svr4_delete_socket: %s [%p,%d,%d]\n",
				 e->sock.sun_path, p, (int)e->dev, e->ino));
			free(e, M_TEMP);
			return;
		}
	mtx_unlock(&svr4_sockcache_lock);
}

void
svr4_purge_sockcache(arg, p)
	void *arg;
	struct proc *p;
{
	struct svr4_sockcache_entry *e, *ne;

	mtx_lock(&svr4_sockcache_lock);
	TAILQ_FOREACH_SAFE(e, &svr4_head, entries, ne) {
		if (e->p == p) {
			TAILQ_REMOVE(&svr4_head, e, entries);
			DPRINTF(("svr4_purge_sockcache: %s [%p,%d,%d]\n",
				 e->sock.sun_path, p, (int)e->dev, e->ino));
			free(e, M_TEMP);
		}
	}
	mtx_unlock(&svr4_sockcache_lock);
}

void
svr4_sockcache_init(void)
{

	TAILQ_INIT(&svr4_head);
	mtx_init(&svr4_sockcache_lock, "svr4 socket cache", NULL, MTX_DEF);
	svr4_sockcache_exit_tag = EVENTHANDLER_REGISTER(process_exit,
	    svr4_purge_sockcache, NULL, EVENTHANDLER_PRI_ANY);
	svr4_sockcache_exec_tag = EVENTHANDLER_REGISTER(process_exec,
	    svr4_purge_sockcache, NULL, EVENTHANDLER_PRI_ANY);
}

void
svr4_sockcache_destroy(void)
{

	KASSERT(TAILQ_EMPTY(&svr4_head),
	    ("%s: sockcache entries still around", __func__));
	EVENTHANDLER_DEREGISTER(process_exec, svr4_sockcache_exec_tag);
	EVENTHANDLER_DEREGISTER(process_exit, svr4_sockcache_exit_tag);
	mtx_destroy(&svr4_sockcache_lock);
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
