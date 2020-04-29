/*
 * Copyright 2016 Jakub Klama <jceel@FreeBSD.org>
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */


#ifndef LIB9P_BACKEND_H
#define LIB9P_BACKEND_H

struct l9p_backend {
	void *softc;
	void (*freefid)(void *, struct l9p_fid *);
	int (*attach)(void *, struct l9p_request *);
	int (*clunk)(void *, struct l9p_fid *);
	int (*create)(void *, struct l9p_request *);
	int (*open)(void *, struct l9p_request *);
	int (*read)(void *, struct l9p_request *);
	int (*remove)(void *, struct l9p_fid *);
	int (*stat)(void *, struct l9p_request *);
	int (*walk)(void *, struct l9p_request *);
	int (*write)(void *, struct l9p_request *);
	int (*wstat)(void *, struct l9p_request *);
	int (*statfs)(void *, struct l9p_request *);
	int (*lopen)(void *, struct l9p_request *);
	int (*lcreate)(void *, struct l9p_request *);
	int (*symlink)(void *, struct l9p_request *);
	int (*mknod)(void *, struct l9p_request *);
	int (*rename)(void *, struct l9p_request *);
	int (*readlink)(void *, struct l9p_request *);
	int (*getattr)(void *, struct l9p_request *);
	int (*setattr)(void *, struct l9p_request *);
	int (*xattrwalk)(void *, struct l9p_request *);
	int (*xattrcreate)(void *, struct l9p_request *);
	int (*xattrread)(void *, struct l9p_request *);
	int (*xattrwrite)(void *, struct l9p_request *);
	int (*xattrclunk)(void *, struct l9p_fid *);
	int (*readdir)(void *, struct l9p_request *);
	int (*fsync)(void *, struct l9p_request *);
	int (*lock)(void *, struct l9p_request *);
	int (*getlock)(void *, struct l9p_request *);
	int (*link)(void *, struct l9p_request *);
	int (*mkdir)(void *, struct l9p_request *);
	int (*renameat)(void *, struct l9p_request *);
	int (*unlinkat)(void *, struct l9p_request *);
};

#endif  /* LIB9P_BACKEND_H */
