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

#ifndef	_SVR4_STATVFS_H_
#define	_SVR4_STATVFS_H_

typedef struct svr4_statvfs {
	u_long			f_bsize;
	u_long			f_frsize;
	svr4_fsblkcnt_t		f_blocks;
	svr4_fsblkcnt_t		f_bfree;
	svr4_fsblkcnt_t		f_bavail;
	svr4_fsblkcnt_t		f_files;
	svr4_fsblkcnt_t		f_ffree;
	svr4_fsblkcnt_t		f_favail;
	u_long			f_fsid;
	char			f_basetype[16];
	u_long			f_flag;
	u_long			f_namemax;
	char			f_fstr[32];
	u_long			f_filler[16];
} svr4_statvfs_t;

typedef struct svr4_statvfs64 {
	u_long			f_bsize;
	u_long			f_frsize;
	svr4_fsblkcnt64_t	f_blocks;
	svr4_fsblkcnt64_t	f_bfree;
	svr4_fsblkcnt64_t	f_bavail;
	svr4_fsblkcnt64_t	f_files;
	svr4_fsblkcnt64_t	f_ffree;
	svr4_fsblkcnt64_t	f_favail;
	u_long			f_fsid;
	char			f_basetype[16];
	u_long			f_flag;
	u_long			f_namemax;
	char			f_fstr[32];
	u_long			f_filler[16];
} svr4_statvfs64_t;

#define	SVR4_ST_RDONLY	0x01
#define	SVR4_ST_NOSUID	0x02
#define	SVR4_ST_NOTRUNC	0x04

#endif /* !_SVR4_STATVFS_H_ */
