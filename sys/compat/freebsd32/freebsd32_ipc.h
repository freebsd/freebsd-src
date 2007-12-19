/*-
 * Copyright (c) 2002 Doug Rabson
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
 */

#ifndef _COMPAT_FREEBSD32_FREEBSD32_IPC_H_
#define _COMPAT_FREEBSD32_FREEBSD32_IPC_H_

struct ipc_perm32 {
	uint16_t	cuid;
	uint16_t	cgid;
	uint16_t	uid;
	uint16_t	gid;
	uint16_t	mode;
	uint16_t	seq;
	uint32_t	key;
};

struct shmid_ds32 {
	struct ipc_perm32 shm_perm;
	int32_t		shm_segsz;
	int32_t		shm_lpid;
	int32_t		shm_cpid;
	int16_t		shm_nattch;
	int32_t		shm_atime;
	int32_t		shm_dtime;
	int32_t		shm_ctime;
	uint32_t	shm_internal;
};

struct shm_info32 {
	int32_t		used_ids;
	uint32_t	shm_tot;
	uint32_t	shm_rss;
	uint32_t	shm_swp;
	uint32_t	swap_attempts;
	uint32_t	swap_successes;
};

struct shminfo32 {
	uint32_t	shmmax;
	uint32_t	shmmin;
	uint32_t	shmmni;
	uint32_t	shmseg;
	uint32_t	shmall;
};

#endif /* !_COMPAT_FREEBSD32_FREEBSD32_IPC_H_ */
