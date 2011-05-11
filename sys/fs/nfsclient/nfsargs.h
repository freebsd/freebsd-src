/*-
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
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

#ifndef _NFSCLIENT_NFSARGS_H_
#define	_NFSCLIENT_NFSARGS_H_

/*
 * Arguments to mount NFS
 */
#define	NFS_ARGSVERSION	4		/* change when nfs_args changes */
struct nfs_args {
	int		version;	/* args structure version number */
	struct sockaddr	*addr;		/* file server address */
	int		addrlen;	/* length of address */
	int		sotype;		/* Socket type */
	int		proto;		/* and Protocol */
	u_char		*fh;		/* File handle to be mounted */
	int		fhsize;		/* Size, in bytes, of fh */
	int		flags;		/* flags */
	int		wsize;		/* write size in bytes */
	int		rsize;		/* read size in bytes */
	int		readdirsize;	/* readdir size in bytes */
	int		timeo;		/* initial timeout in .1 secs */
	int		retrans;	/* times to retry send */
	int		readahead;	/* # of blocks to readahead */
	int		iothreadcnt;	/* and count of assoc threads */
	int		wcommitsize;	/* Max. write commit size in bytes */
	char		*hostname;	/* server's name */
	int		acregmin;	/* cache attrs for reg files min time */
	int		acregmax;	/* cache attrs for reg files max time */
	int		acdirmin;	/* cache attrs for dirs min time */
	int		acdirmax;	/* cache attrs for dirs max time */
	int		krbnamelen;	/* KerberosV principal name "-P" */
	char		*krbname;
	int		dirlen;		/* Mount pt path for NFSv4 */
	char		*dirpath;
	int		srvkrbnamelen;	/* Server kerberos target principal */
	char		*srvkrbname;	/* and the name */
};

/*
 * NFS mount option flags
 */
#define	NFSMNT_SOFT		0x00000001  /* soft mount (hard is default) */
#define	NFSMNT_WSIZE		0x00000002  /* set write size */
#define	NFSMNT_RSIZE		0x00000004  /* set read size */
#define	NFSMNT_TIMEO		0x00000008  /* set initial timeout */
#define	NFSMNT_RETRANS		0x00000010  /* set number of request retries */
#define	NFSMNT_DIRECTIO		0x00000020  /* set maximum grouplist size */
#define	NFSMNT_INT		0x00000040  /* allow interrupts on hard mount */
#define	NFSMNT_NOCONN		0x00000080  /* Don't Connect the socket */
#define	NFSMNT_NFSV4		0x00000100  /* Use NFSv4 */
#define	NFSMNT_NFSV3		0x00000200  /* Use NFS Version 3 protocol */
#define	NFSMNT_KERB		0x00000400  /* Use Kerberos authentication */
#define	NFSMNT_STRICT3530	0x00000800  /* Follow RFC3530 strictly */
#define	NFSMNT_WCOMMITSIZE	0x00001000  /* set max write commit size */
#define	NFSMNT_READAHEAD	0x00002000  /* set read ahead */
#define	NFSMNT_INTEGRITY	0x00004000  /* Use Integrity cksum - krb5i */
#define	NFSMNT_PRIVACY		0x00008000  /* Encrypt RPCs - krb5p */
#define	NFSMNT_RDIRPLUS		0x00010000  /* Use Readdirplus for V3 */
#define	NFSMNT_READDIRSIZE	0x00020000  /* Set readdir size */
#define	NFSMNT_ACREGMIN		0x00040000
#define	NFSMNT_ACREGMAX		0x00080000
#define	NFSMNT_ACDIRMIN		0x00100000
#define	NFSMNT_ACDIRMAX		0x00200000
#define	NFSMNT_NOLOCKD		0x00400000 /* Locks are local */
#define	NFSMNT_ALLGSSNAME	0x00800000 /* All RPCs use host principal */
#define	NFSMNT_HASWRITEVERF	0x01000000 /* NFSv4 Write verifier */
#define	NFSMNT_HASSETFSID	0x02000000 /* Has set FSID */
#define	NFSMNT_RESVPORT		0x04000000 /* Use a reserved port (Bunk!!) */
#define	NFSMNT_AUTOM		0x08000000 /* Done by autofs */
#define	NFSMNT_NOCTO		0x20000000 /* Don't flush attrcache on open */

#endif	/* _NFSCLIENT_NFSARGS_H_ */
