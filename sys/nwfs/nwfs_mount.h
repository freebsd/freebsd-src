/*
 * Copyright (c) 1999, Boris Popov
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
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
#ifndef _NWFS_MOUNT_H_
#define	_NWFS_MOUNT_H_

#ifndef _NCP_NCP_NLS_H_
#include <netncp/ncp_nls.h>
#endif

#define NWFS_VERMAJ	1
#define NWFS_VERMIN	3400
#define NWFS_VERSION	(NWFS_VERMAJ*100000 + NWFS_VERMIN)

/* Values for flags */
#define NWFS_MOUNT_SOFT		0x0001
#define WNFS_MOUNT_INTR		0x0002
#define NWFS_MOUNT_STRONG	0x0004
#define NWFS_MOUNT_NO_OS2	0x0008
#define NWFS_MOUNT_NO_NFS	0x0010
#define NWFS_MOUNT_NO_LONG	0x0020
#define	NWFS_MOUNT_GET_SYSENT	0x0040	/* special case, look to vfsops :) */
#define	NWFS_MOUNT_HAVE_NLS	0x0080

#define	NWFS_VOLNAME_LEN	48


/* Layout of the mount control block for a netware file system. */
struct nwfs_args {
	int		connRef;		/* connection reference */
	char		mount_point[MAXPATHLEN];
	u_int		flags;
	u_char		mounted_vol[NWFS_VOLNAME_LEN + 1];
	u_char		root_path[512+1];
	int		version;
	uid_t		uid;
	gid_t 		gid;
	mode_t 		file_mode;
	mode_t 		dir_mode;
	struct ncp_nlstables nls;
	int		tz;
};

#ifdef _KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_NWFSMNT);
#endif

#endif
#endif /* !_NWFS_MOUNT_H_ */
