/*
 * Copyright (c) 1991, 1993
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 * from: @(#)nfsswapvmunix.c	8.1 (Berkeley) 6/10/93
 * $FreeBSD$
 */

/*
 * Sample NFS swapkernel configuration file.
 * This should be filled in by the bootstrap program.
 * See /sys/nfs/nfsdiskless.h for details of the fields.
 */

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/socket.h>
#include <sys/mount.h>

#include <net/if.h>
#include <nfs/nfsv2.h>
#include <nfs/nfsdiskless.h>

extern int nfs_mountroot();
int (*mountroot)() = nfs_mountroot;

dev_t	rootdev = NODEV;
dev_t	argdev  = NODEV;
dev_t	dumpdev = NODEV;

struct	swdevt swdevt[] = {
	{ NODEV,	0,	5000 },	/* happy:/u/swap.dopey  */
	{ 0, 0, 0 }
};
struct nfs_diskless nfs_diskless = {
	{ { 'q', 'e', '0', '\0' },
	  { 0x10, 0x2, { 0x0, 0x0, 0x83, 0x68, 0x30, 0x2, } },
	  { 0x10, 0x2, { 0x0, 0x0, 0x83, 0x68, 0x30, 0xff, } },
	  { 0x10, 0x0, { 0x0, 0x0, 0xff, 0xff, 0xff, 0x0, } },
 	},
	{ 0x10, 0x2, { 0x0, 0x0, 0x83, 0x68, 0x30, 0x12, } },
	{
	  (struct sockaddr *)0, SOCK_DGRAM, 0, (nfsv2fh_t *)0,
	  0, 8192, 8192, 10, 100, (char *)0,
	},
	{
		0xf,
		0x9,
		0x0,
		0x0,
		0x1,
		0x0,
		0x0,
		0x0,
		0xc,
		0x0,
		0x0,
		0x0,
		0x6,
		0x0,
		0x0,
		0x0,
		0x27,
		0x18,
		0x79,
		0x27,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
	},
	{ 0x10, 0x2, { 0x8, 0x1, 0x83, 0x68, 0x30, 0x5, } },
	"happy",
	{
	  (struct sockaddr *)0, SOCK_DGRAM, 0, (nfsv2fh_t *)0,
	  0, 8192, 8192, 10, 100, (char *)0,
	},
	{
		0x0,
		0x9,
		0x0,
		0x0,
		0x1,
		0x0,
		0x0,
		0x0,
		0xc,
		0x0,
		0x0,
		0x0,
		0x2,
		0x0,
		0x0,
		0x0,
		0xd0,
		0x48,
		0x42,
		0x25,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
		0x0,
	},
	{ 0x10, 0x2, { 0x8, 0x1, 0x83, 0x68, 0x30, 0x5, } },
	"happy",
};
