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

#ifndef LIB9P_FID_H
#define LIB9P_FID_H

#include <stdbool.h>

/*
 * Data structure for a fid.  All active fids in one session
 * are stored in a hash table; the hash table provides the
 * iterator to process them.  (See also l9p_connection in lib9p.h.)
 *
 * The back-end code has additional data per fid, found via
 * lo_aux.  Currently this is allocated with a separate calloc().
 *
 * Most fids represent a file or directory, but a few are special
 * purpose, including the auth fid from Tauth+Tattach, and the
 * fids used for extended attributes.  We have our own set of
 * flags here in lo_flags.
 *
 * Note that all new fids start as potentially-valid (reserving
 * their 32-bit fid value), but not actually-valid.  If another
 * (threaded) op is invoked on a not-yet-valid fid, the fid cannot
 * be used.  A fid can also be locked against other threads, in
 * which case they must wait for it: this happens during create
 * and open, which on success result in the fid changing from a
 * directory to a file.  (At least, all this applies in principle
 * -- we're currently single-threaded per connection so the locks
 * are nop-ed out and the valid bit is mainly just for debug.)
 *
 * Fids that are "open" (the underlying file or directory is open)
 * are marked as well.
 *
 * Locking is managed by the front end (request.c); validation
 * and type-marking can be done by either side as needed.
 *
 * Fid types and validity are manipulated by set* and unset*
 * functions, and tested by is* ops.  Note that we only
 * distinguish between "directory" and "not directory" at this
 * level, i.e., symlinks and devices are just "not a directory
 * fid".  Also, fids cannot be unset as auth or xattr fids,
 * nor can an open fid become closed, except by being clunked.
 * While files should not normally become directories, it IS normal
 * for directory fids to become file fids due to Twalk operations.
 *
 * (These accessor functions are just to leave wiggle room for
 * different future implementations.)
 */
struct l9p_fid {
	void	*lo_aux;
	uint32_t lo_fid;
	uint32_t lo_flags;	/* volatile atomic_t when threaded? */
};

enum l9p_lo_flags {
	L9P_LO_ISAUTH = 0x01,
	L9P_LO_ISDIR = 0x02,
	L9P_LO_ISOPEN = 0x04,
	L9P_LO_ISVALID = 0x08,
	L9P_LO_ISXATTR = 0x10,
};

static inline bool
l9p_fid_isauth(struct l9p_fid *fid)
{
	return ((fid->lo_flags & L9P_LO_ISAUTH) != 0);
}

static inline void
l9p_fid_setauth(struct l9p_fid *fid)
{
	fid->lo_flags |= L9P_LO_ISAUTH;
}

static inline bool
l9p_fid_isdir(struct l9p_fid *fid)
{
	return ((fid->lo_flags & L9P_LO_ISDIR) != 0);
}

static inline void
l9p_fid_setdir(struct l9p_fid *fid)
{
	fid->lo_flags |= L9P_LO_ISDIR;
}

static inline void
l9p_fid_unsetdir(struct l9p_fid *fid)
{
	fid->lo_flags &= ~(uint32_t)L9P_LO_ISDIR;
}

static inline bool
l9p_fid_isopen(struct l9p_fid *fid)
{
	return ((fid->lo_flags & L9P_LO_ISOPEN) != 0);
}

static inline void
l9p_fid_setopen(struct l9p_fid *fid)
{
	fid->lo_flags |= L9P_LO_ISOPEN;
}

static inline bool
l9p_fid_isvalid(struct l9p_fid *fid)
{
	return ((fid->lo_flags & L9P_LO_ISVALID) != 0);
}

static inline void
l9p_fid_setvalid(struct l9p_fid *fid)
{
	fid->lo_flags |= L9P_LO_ISVALID;
}

static inline void
l9p_fid_unsetvalid(struct l9p_fid *fid)
{
	fid->lo_flags &= ~(uint32_t)L9P_LO_ISVALID;
}

static inline bool
l9p_fid_isxattr(struct l9p_fid *fid)
{
	return ((fid->lo_flags & L9P_LO_ISXATTR) != 0);
}

static inline void
l9p_fid_setxattr(struct l9p_fid *fid)
{
	fid->lo_flags |= L9P_LO_ISXATTR;
}

#endif  /* LIB9P_FID_H */
