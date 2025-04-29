/*	$OpenBSD: arc4random_linux.h,v 1.12 2019/07/11 10:37:28 inoguchi Exp $	*/

/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 * Copyright (c) 2013, Markus Friedl <markus@openbsd.org>
 * Copyright (c) 2014, Theo de Raadt <deraadt@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Stub functions for portability.  From LibreSSL with some adaptations.
 */

#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif

#include <signal.h>

/* OpenSSH isn't multithreaded */
#define _ARC4_LOCK()
#define _ARC4_UNLOCK()
#define _ARC4_ATFORK(f)

static inline void
_getentropy_fail(void)
{
	fatal("getentropy failed");
}

static volatile sig_atomic_t _rs_forked;

static inline void
_rs_forkhandler(void)
{
	_rs_forked = 1;
}

static inline void
_rs_forkdetect(void)
{
	static pid_t _rs_pid = 0;
	pid_t pid = getpid();

	if (_rs_pid == 0 || _rs_pid == 1 || _rs_pid != pid || _rs_forked) {
		_rs_pid = pid;
		_rs_forked = 0;
		if (rs)
			memset(rs, 0, sizeof(*rs));
	}
}

static inline int
_rs_allocate(struct _rs **rsp, struct _rsx **rsxp)
{
#if defined(MAP_ANON) && defined(MAP_PRIVATE)
	if ((*rsp = mmap(NULL, sizeof(**rsp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED)
		return (-1);

	if ((*rsxp = mmap(NULL, sizeof(**rsxp), PROT_READ|PROT_WRITE,
	    MAP_ANON|MAP_PRIVATE, -1, 0)) == MAP_FAILED) {
		munmap(*rsp, sizeof(**rsp));
		*rsp = NULL;
		return (-1);
	}
#else
	if ((*rsp = calloc(1, sizeof(**rsp))) == NULL)
		return (-1);
	if ((*rsxp = calloc(1, sizeof(**rsxp))) == NULL) {
		free(*rsp);
		*rsp = NULL;
		return (-1);
	}
#endif

	_ARC4_ATFORK(_rs_forkhandler);
	return (0);
}
