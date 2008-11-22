/*-
 * Copyright (c) 1998 Mark Newton
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
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
 * $FreeBSD$
 */

#ifndef	_SVR4_STAT_H_
#define	_SVR4_STAT_H_

#include <compat/svr4/svr4_types.h>
#include <sys/stat.h>

struct svr4_stat {
	svr4_o_dev_t	st_dev;
	svr4_o_ino_t	st_ino;
	svr4_o_mode_t	st_mode;
	svr4_o_nlink_t	st_nlink;
	svr4_o_uid_t	st_uid;
	svr4_o_gid_t	st_gid;
	svr4_o_dev_t	st_rdev;
	svr4_off_t	st_size;
	svr4_time_t	st_atim;
	svr4_time_t	st_mtim;
	svr4_time_t	st_ctim;
};

struct svr4_xstat {
	svr4_dev_t		st_dev;
	long			st_pad1[3];
	svr4_ino_t		st_ino;
	svr4_mode_t		st_mode;
	svr4_nlink_t		st_nlink;
	svr4_uid_t		st_uid;
	svr4_gid_t		st_gid;
	svr4_dev_t		st_rdev;
	long			st_pad2[2];
	svr4_off_t		st_size;
	long			st_pad3;
	svr4_timestruc_t	st_atim;
	svr4_timestruc_t	st_mtim;
	svr4_timestruc_t	st_ctim;
	long			st_blksize;
	svr4_blkcnt_t		st_blocks;
	char			st_fstype[16];
	long			st_pad4[8];
};

struct svr4_stat64 {
	svr4_dev_t		st_dev;
	long			st_pad1[3];
	svr4_ino64_t		st_ino;
	svr4_mode_t		st_mode;
	svr4_nlink_t		st_nlink;
	svr4_uid_t		st_uid;
	svr4_gid_t		st_gid;
	svr4_dev_t		st_rdev;
	long			st_pad2[2];
	svr4_off64_t		st_size;
	svr4_timestruc_t	st_atim;
	svr4_timestruc_t	st_mtim;
	svr4_timestruc_t	st_ctim;
	long			st_blksize;
	svr4_blkcnt64_t		st_blocks;
	char			st_fstype[16];
	long			st_pad4[8];
};

#define	SVR4_PC_LINK_MAX		1
#define	SVR4_PC_MAX_CANON		2
#define	SVR4_PC_MAX_INPUT		3
#define	SVR4_PC_NAME_MAX		4
#define	SVR4_PC_PATH_MAX		5
#define	SVR4_PC_PIPE_BUF		6
#define	SVR4_PC_NO_TRUNC		7
#define	SVR4_PC_VDISABLE		8
#define	SVR4_PC_CHOWN_RESTRICTED	9
#define	SVR4_PC_ASYNC_IO		10
#define	SVR4_PC_PRIO_IO			11
#define	SVR4_PC_SYNC_IO			12

#endif /* !_SVR4_STAT_H_ */
