/*
 * Copyright (c) 1995, 1996
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
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
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ypxfrd.x,v 1.8 1996/06/03 20:17:04 wpaul Exp $
 */

/*
 * This protocol definition file describes a file transfer
 * system used to very quickly move NIS maps from one host to
 * another. This is similar to what Sun does with their ypxfrd
 * protocol, but it must be stressed that this protocol is _NOT_
 * compatible with Sun's. There are a couple of reasons for this:
 *
 * 1) Sun's protocol is proprietary. The protocol definition is
 *    not freely available in any of the SunRPC source distributions,
 *    even though the NIS v2 protocol is.
 *
 * 2) The idea here is to transfer entire raw files rather than
 *    sending just the records. Sun uses ndbm for its NIS map files,
 *    while FreeBSD uses Berkeley DB. Both are hash databases, but the
 *    formats are incompatible, making it impossible for them to
 *    use each others' files. Even if FreeBSD adopted ndbm for its
 *    database format, FreeBSD/i386 is a little-endian OS and
 *    SunOS/SPARC is big-endian; ndbm is byte-order sensitive and
 *    not very smart about it, which means an attempt to read a
 *    database on a little-endian box that was created on a big-endian
 *    box (or vice-versa) can cause the ndbm code to eat itself.
 *    Luckily, Berkeley DB is able to deal with this situation in
 *    a more graceful manner.
 *
 * While the protocol is incompatible, the idea is the same: we just open
 * up a TCP pipe to the client and transfer the raw map database 
 * from the master server to the slave. This is many times faster than
 * the standard yppush/ypxfr transfer method since it saves us from
 * having to recreate the map databases via the DB library each time.
 * For example: creating a passwd database with 30,000 entries with yp_mkdb
 * can take a couple of minutes, but to just copy the file takes only a few
 * seconds.
 */

#ifndef RPC_HDR
%#ifndef lint
%static const char rcsid[] = "$Id: ypxfrd.x,v 1.8 1996/06/03 20:17:04 wpaul Exp $";
%#endif /* not lint */
#endif

/* XXX cribbed from yp.x */
const _YPMAXRECORD = 1024;
const _YPMAXDOMAIN = 64;
const _YPMAXMAP = 64;
const _YPMAXPEER = 64;

/* Suggested default -- not necesarrily the one used. */
const YPXFRBLOCK = 32767;

enum xfrstat {
	XFR_REQUEST_OK	= 1,	/* Transfer request granted */
	XFR_DENIED	= 2,	/* Transfer request denied */
	XFR_NOFILE	= 3,	/* Requested map file doesn't exist */
	XFR_ACCESS	= 4,	/* File exists, but I couldn't access it */
	XFR_BADDB	= 5,	/* File is not a hash database */
	XFR_READ_OK	= 6,	/* Block read successfully */
	XFR_READ_ERR	= 7,	/* Read error during transfer */
	XFR_DONE	= 8	/* Transfer completed */
};

typedef string xfrdomain<_YPMAXDOMAIN>;
typedef string xfrmap<_YPMAXMAP>;

/* Ask the remote ypxfrd for a map using this structure */
struct ypxfr_mapname {
	xfrmap xfrmap;
	xfrdomain xfrdomain;
};

/* Read response using this structure. */
union xfr switch (bool ok) {
case TRUE:
	opaque xfrblock_buf<>;
case FALSE:
	enum xfrstat xfrstat;
};

program YPXFRD_FREEBSD_PROG {
	version YPXFRD_FREEBSD_VERS {
		union xfr
		YPXFRD_GETMAP(ypxfr_mapname) = 1;
	} = 1;
} = 600100069;	/* 100069 + 60000000 -- 100069 is the Sun ypxfrd prog number */
