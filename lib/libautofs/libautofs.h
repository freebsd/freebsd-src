/*
 * Copyright (c) 2004 Alfred Perlstein <alfred@FreeBSD.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 * $Id: libautofs.h,v 1.4 2004/09/08 08:12:21 bright Exp $
 */
#ifndef _LIBAUTOFS_H
#define _LIBAUTOFS_H

struct auto_handle;
typedef struct auto_handle * autoh_t;
struct autofs_userreq;
typedef struct autofs_userreq * autoreq_t;
typedef uint64_t autoino_t;

#define AUTO_INODE_NONE		0

#define AUTO_DIRECT		1
#define AUTO_INDIRECT		2
#define AUTO_MOUNTER		3
#define AUTO_BROWSE		4

enum autoreq_op {
	AUTOREQ_OP_UNKNOWN = 0,
	AUTOREQ_OP_LOOKUP,
	AUTOREQ_OP_STAT,
	AUTOREQ_OP_READDIR
};

/* get a handle based on a path. */
int		autoh_get(const char *, autoh_t *);
/* release. */
void		autoh_free(autoh_t);

/*
 * Get an array of pointers to handles for all autofs mounts, returns count
 * or -1
 */
int		autoh_getall(autoh_t **, int *cnt);
/* free the array of pointers */
void		autoh_freeall(autoh_t *);

/* return fd to select on. */
int		autoh_fd(autoh_t);

/* returns the mount point of the autofs instance. */
const char	*autoh_mp(autoh_t);

/* get an array of pending requests */
int		autoreq_get(autoh_t, autoreq_t **, int *);
/* free an array of requests */
void		autoreq_free(autoh_t, autoreq_t *);
/* serve a request */
int		autoreq_serv(autoh_t, autoreq_t);

/* get the operation requested */
enum autoreq_op	autoreq_getop(autoreq_t);

/* get a request's file name. */
const char	*autoreq_getpath(autoreq_t);
/* get a request's inode.  a indirect mount may return AUTO_INODE_NONE. */
autoino_t	autoreq_getino(autoreq_t);
/*
 * set a request's inode.  an indirect mount may return AUTO_INODE_NONE,
 * this is a fixup for indirect mounts.
 */
void		autoreq_setino(autoreq_t, autoino_t);
/* get a request's directory inode. */
autoino_t	autoreq_getdirino(autoreq_t);
void		autoreq_seterrno(autoreq_t, int);
void		autoreq_setaux(autoreq_t, void *, size_t);
void		autoreq_getaux(autoreq_t, void **, size_t *);
void		autoreq_seteof(autoreq_t, int);
void		autoreq_getoffset(autoreq_t, off_t *);
void		autoreq_getxid(autoreq_t, int *);

/* toggle by path. args = handle, AUTO_?, pid (-1 to disable), path. */
int		autoh_togglepath(autoh_t, int, pid_t,  const char *);
/* toggle by fd. args = handle, AUTO_?, pid (-1 to disable), fd. */
int		autoh_togglefd(autoh_t, int, pid_t, int);

#endif
