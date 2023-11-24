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

/*
 * Based on libixp code: ©2007-2010 Kris Maglione <maglione.k at Gmail>
 */

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
#include <sys/param.h>
#ifdef __APPLE__
# include "apple_endian.h"
#else
# include <sys/endian.h>
#endif
#include <sys/uio.h>
#include "lib9p.h"
#include "lib9p_impl.h"
#include "log.h"

#define N(ary)          (sizeof(ary) / sizeof(*ary))
#define STRING_SIZE(s)  (L9P_WORD + (s != NULL ? (uint16_t)strlen(s) : 0))
#define QID_SIZE        (L9P_BYTE + L9P_DWORD + L9P_QWORD)

static ssize_t l9p_iov_io(struct l9p_message *, void *, size_t);
static inline ssize_t l9p_pu8(struct l9p_message *, uint8_t *);
static inline ssize_t l9p_pu16(struct l9p_message *, uint16_t *);
static inline ssize_t l9p_pu32(struct l9p_message *, uint32_t *);
static inline ssize_t l9p_pu64(struct l9p_message *, uint64_t *);
static ssize_t l9p_pustring(struct l9p_message *, char **s);
static ssize_t l9p_pustrings(struct l9p_message *, uint16_t *, char **, size_t);
static ssize_t l9p_puqid(struct l9p_message *, struct l9p_qid *);
static ssize_t l9p_puqids(struct l9p_message *, uint16_t *, struct l9p_qid *q);

/*
 * Transfer data from incoming request, or to outgoing response,
 * using msg to track position and direction within request/response.
 *
 * Returns the number of bytes actually transferred (which is always
 * just len itself, converted to signed), or -1 if we ran out of space.
 *
 * Note that if we return -1, subsequent l9p_iov_io() calls with
 * the same (and not-reset) msg and len > 0 will also return -1.
 * This means most users can just check the *last* call for failure.
 */
static ssize_t
l9p_iov_io(struct l9p_message *msg, void *buffer, size_t len)
{
	size_t done = 0;
	size_t left = len;

	assert(msg != NULL);

	if (len == 0)
		return (0);

	if (msg->lm_cursor_iov >= msg->lm_niov)
		return (-1);

	assert(buffer != NULL);

	while (left > 0) {
		size_t idx = msg->lm_cursor_iov;
		size_t space = msg->lm_iov[idx].iov_len - msg->lm_cursor_offset;
		size_t towrite = MIN(space, left);

		if (msg->lm_mode == L9P_PACK) {
			memcpy((char *)msg->lm_iov[idx].iov_base +
			    msg->lm_cursor_offset, (char *)buffer + done,
			    towrite);
		}

		if (msg->lm_mode == L9P_UNPACK) {
			memcpy((char *)buffer + done,
			    (char *)msg->lm_iov[idx].iov_base +
			    msg->lm_cursor_offset, towrite);
		}

		msg->lm_cursor_offset += towrite;

		done += towrite;
		left -= towrite;

		if (space - towrite == 0) {
			/* Advance to next iov */
			msg->lm_cursor_iov++;
			msg->lm_cursor_offset = 0;

			if (msg->lm_cursor_iov >= msg->lm_niov && left > 0)
				return (-1);
		}
	}

	msg->lm_size += done;
	return ((ssize_t)done);
}

/*
 * Pack or unpack a byte (8 bits).
 *
 * Returns 1 (success, 1 byte) or -1 (error).
 */
static inline ssize_t
l9p_pu8(struct l9p_message *msg, uint8_t *val)
{

	return (l9p_iov_io(msg, val, sizeof (uint8_t)));
}

/*
 * Pack or unpack 16-bit value.
 *
 * Returns 2 or -1.
 */
static inline ssize_t
l9p_pu16(struct l9p_message *msg, uint16_t *val)
{
#if _BYTE_ORDER != _LITTLE_ENDIAN
	/*
	 * The ifdefs are annoying, but there is no need
	 * for all of this foolery on little-endian hosts,
	 * and I don't expect the compiler to optimize it
	 * all away.
	 */
	uint16_t copy;
	ssize_t ret;

	if (msg->lm_mode == L9P_PACK) {
		copy = htole16(*val);
		return (l9p_iov_io(msg, &copy, sizeof (uint16_t)));
	}
	ret = l9p_iov_io(msg, val, sizeof (uint16_t));
	*val = le16toh(*val);
	return (ret);
#else
	return (l9p_iov_io(msg, val, sizeof (uint16_t)));
#endif
}

/*
 * Pack or unpack 32-bit value.
 *
 * Returns 4 or -1.
 */
static inline ssize_t
l9p_pu32(struct l9p_message *msg, uint32_t *val)
{
#if _BYTE_ORDER != _LITTLE_ENDIAN
	uint32_t copy;
	ssize_t ret;

	if (msg->lm_mode == L9P_PACK) {
		copy = htole32(*val);
		return (l9p_iov_io(msg, &copy, sizeof (uint32_t)));
	}
	ret = l9p_iov_io(msg, val, sizeof (uint32_t));
	*val = le32toh(*val);
	return (ret);
#else
	return (l9p_iov_io(msg, val, sizeof (uint32_t)));
#endif
}

/*
 * Pack or unpack 64-bit value.
 *
 * Returns 8 or -1.
 */
static inline ssize_t
l9p_pu64(struct l9p_message *msg, uint64_t *val)
{
#if _BYTE_ORDER != _LITTLE_ENDIAN
	uint64_t copy;
	ssize_t ret;

	if (msg->lm_mode == L9P_PACK) {
		copy = htole64(*val);
		return (l9p_iov_io(msg, &copy, sizeof (uint64_t)));
	}
	ret = l9p_iov_io(msg, val, sizeof (uint32_t));
	*val = le64toh(*val);
	return (ret);
#else
	return (l9p_iov_io(msg, val, sizeof (uint64_t)));
#endif
}

/*
 * Pack or unpack a string, encoded as 2-byte length followed by
 * string bytes.  The returned length is 2 greater than the
 * length of the string itself.
 *
 * When unpacking, this allocates a new string (NUL-terminated).
 *
 * Return -1 on error (not space, or failed to allocate string,
 * or illegal string).
 *
 * Note that pustring (and hence pustrings) can return an error
 * even when l9p_iov_io succeeds.
 */
static ssize_t
l9p_pustring(struct l9p_message *msg, char **s)
{
	uint16_t len;

	if (msg->lm_mode == L9P_PACK)
		len = *s != NULL ? (uint16_t)strlen(*s) : 0;

	if (l9p_pu16(msg, &len) < 0)
		return (-1);

	if (msg->lm_mode == L9P_UNPACK) {
		*s = l9p_calloc(1, len + 1);
		if (*s == NULL)
			return (-1);
	}

	if (l9p_iov_io(msg, *s, len) < 0)
		return (-1);

	if (msg->lm_mode == L9P_UNPACK) {
		/*
		 * An embedded NUL byte in a string is illegal.
		 * We don't necessarily have to check (we'll just
		 * treat it as a shorter string), but checking
		 * seems like a good idea.
		 */
		if (memchr(*s, '\0', len) != NULL)
			return (-1);
	}

	return ((ssize_t)len + 2);
}

/*
 * Pack or unpack a number (*num) of strings (but at most max of
 * them).
 *
 * Returns the number of bytes transferred, including the packed
 * number of strings.  If packing and the packed number of strings
 * was reduced, the original *num value is unchanged; only the
 * wire-format number is reduced.  If unpacking and the input
 * number of strings exceeds the max, the incoming *num is reduced
 * to lim, if needed.  (NOTE ASYMMETRY HERE!)
 *
 * Returns -1 on error.
 */
static ssize_t
l9p_pustrings(struct l9p_message *msg, uint16_t *num, char **strings,
    size_t max)
{
	size_t i, lim;
	ssize_t r, ret;
	uint16_t adjusted;

	if (msg->lm_mode == L9P_PACK) {
		lim = *num;
		if (lim > max)
			lim = max;
		adjusted = (uint16_t)lim;
		r = l9p_pu16(msg, &adjusted);
	} else {
		r = l9p_pu16(msg, num);
		lim = *num;
		if (lim > max)
			*num = (uint16_t)(lim = max);
	}
	if (r < 0)
		return (-1);

	for (i = 0; i < lim; i++) {
		ret = l9p_pustring(msg, &strings[i]);
		if (ret < 1)
			return (-1);

		r += ret;
	}

	return (r);
}

/*
 * Pack or unpack a qid.
 *
 * Returns 13 (success) or -1 (error).
 */
static ssize_t
l9p_puqid(struct l9p_message *msg, struct l9p_qid *qid)
{
	ssize_t r;
	uint8_t type;

	if (msg->lm_mode == L9P_PACK) {
		type = qid->type;
		r = l9p_pu8(msg, &type);
	} else {
		r = l9p_pu8(msg, &type);
		qid->type = type;
	}
	if (r > 0)
		r = l9p_pu32(msg, &qid->version);
	if (r > 0)
		r = l9p_pu64(msg, &qid->path);

	return (r > 0 ? QID_SIZE : r);
}

/*
 * Pack or unpack *num qids.
 *
 * Returns 2 + 13 * *num (after possibly setting *num), or -1 on error.
 */
static ssize_t
l9p_puqids(struct l9p_message *msg, uint16_t *num, struct l9p_qid *qids)
{
	size_t i, lim;
	ssize_t ret, r;

	r = l9p_pu16(msg, num);
	if (r <= 0)
		return (r);

	if (*num > L9P_MAX_WELEM)
		return (-1);

	for (i = 0, lim = *num; i < lim; i++) {
		ret = l9p_puqid(msg, &qids[i]);
		if (ret < 0)
			return (-1);
		r += ret;
	}
	return (r);
}

/*
 * Pack or unpack a l9p_stat.
 *
 * These have variable size, and the size further depends on
 * the protocol version.
 *
 * Returns the number of bytes packed/unpacked, or -1 on error.
 */
ssize_t
l9p_pustat(struct l9p_message *msg, struct l9p_stat *stat,
    enum l9p_version version)
{
	ssize_t r = 0;
	uint16_t size;

	/* The on-wire size field excludes the size of the size field. */
	if (msg->lm_mode == L9P_PACK)
		size = l9p_sizeof_stat(stat, version) - 2;

	r += l9p_pu16(msg, &size);
	r += l9p_pu16(msg, &stat->type);
	r += l9p_pu32(msg, &stat->dev);
	r += l9p_puqid(msg, &stat->qid);
	r += l9p_pu32(msg, &stat->mode);
	r += l9p_pu32(msg, &stat->atime);
	r += l9p_pu32(msg, &stat->mtime);
	r += l9p_pu64(msg, &stat->length);
	r += l9p_pustring(msg, &stat->name);
	r += l9p_pustring(msg, &stat->uid);
	r += l9p_pustring(msg, &stat->gid);
	r += l9p_pustring(msg, &stat->muid);

	if (version >= L9P_2000U) {
		r += l9p_pustring(msg, &stat->extension);
		r += l9p_pu32(msg, &stat->n_uid);
		r += l9p_pu32(msg, &stat->n_gid);
		r += l9p_pu32(msg, &stat->n_muid);
	}

	if (r < size + 2)
		return (-1);

	return (r);
}

/*
 * Pack or unpack a variable-length dirent.
 *
 * If unpacking, the name field is malloc()ed and the caller must
 * free it.
 *
 * Returns the wire-format length, or -1 if we ran out of room.
 */
ssize_t
l9p_pudirent(struct l9p_message *msg, struct l9p_dirent *de)
{
	ssize_t r, s;

	r = l9p_puqid(msg, &de->qid);
	r += l9p_pu64(msg, &de->offset);
	r += l9p_pu8(msg, &de->type);
	s = l9p_pustring(msg, &de->name);
	if (r < QID_SIZE + 8 + 1 || s < 0)
		return (-1);
	return (r + s);
}

/*
 * Pack or unpack a request or response (fcall).
 *
 * Returns 0 on success, -1 on error.  (It's up to the caller
 * to call l9p_freefcall on our failure.)
 */
int
l9p_pufcall(struct l9p_message *msg, union l9p_fcall *fcall,
    enum l9p_version version)
{
	uint32_t length = 0;
	ssize_t r;

	/*
	 * Get overall length, type, and tag, which should appear
	 * in all messages.  If not even that works, abort immediately.
	 */
	l9p_pu32(msg, &length);
	l9p_pu8(msg, &fcall->hdr.type);
	r = l9p_pu16(msg, &fcall->hdr.tag);
	if (r < 0)
		return (-1);

	/*
	 * Decode remainder of message.	 When unpacking, this may
	 * allocate memory, even if we fail during the decode.
	 * Note that the initial fcall is zeroed out, though, so
	 * we can just freefcall() to release whatever might have
	 * gotten allocated, if the unpack fails due to a short
	 * packet.
	 */
	switch (fcall->hdr.type) {
	case L9P_TVERSION:
	case L9P_RVERSION:
		l9p_pu32(msg, &fcall->version.msize);
		r = l9p_pustring(msg, &fcall->version.version);
		break;

	case L9P_TAUTH:
		l9p_pu32(msg, &fcall->tauth.afid);
		r = l9p_pustring(msg, &fcall->tauth.uname);
		if (r < 0)
			break;
		r = l9p_pustring(msg, &fcall->tauth.aname);
		if (r < 0)
			break;
		if (version >= L9P_2000U)
			r = l9p_pu32(msg, &fcall->tauth.n_uname);
		break;

	case L9P_RAUTH:
		r = l9p_puqid(msg, &fcall->rauth.aqid);
		break;

	case L9P_TATTACH:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu32(msg, &fcall->tattach.afid);
		r = l9p_pustring(msg, &fcall->tattach.uname);
		if (r < 0)
			break;
		r = l9p_pustring(msg, &fcall->tattach.aname);
		if (r < 0)
			break;
		if (version >= L9P_2000U)
			r = l9p_pu32(msg, &fcall->tattach.n_uname);
		break;

	case L9P_RATTACH:
		r = l9p_puqid(msg, &fcall->rattach.qid);
		break;

	case L9P_RERROR:
		r = l9p_pustring(msg, &fcall->error.ename);
		if (r < 0)
			break;
		if (version >= L9P_2000U)
			r = l9p_pu32(msg, &fcall->error.errnum);
		break;

	case L9P_RLERROR:
		r = l9p_pu32(msg, &fcall->error.errnum);
		break;

	case L9P_TFLUSH:
		r = l9p_pu16(msg, &fcall->tflush.oldtag);
		break;

	case L9P_RFLUSH:
		break;

	case L9P_TWALK:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu32(msg, &fcall->twalk.newfid);
		r = l9p_pustrings(msg, &fcall->twalk.nwname,
		    fcall->twalk.wname, N(fcall->twalk.wname));
		break;

	case L9P_RWALK:
		r = l9p_puqids(msg, &fcall->rwalk.nwqid, fcall->rwalk.wqid);
		break;

	case L9P_TOPEN:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pu8(msg, &fcall->topen.mode);
		break;

	case L9P_ROPEN:
		l9p_puqid(msg, &fcall->ropen.qid);
		r = l9p_pu32(msg, &fcall->ropen.iounit);
		break;

	case L9P_TCREATE:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tcreate.name);
		if (r < 0)
			break;
		l9p_pu32(msg, &fcall->tcreate.perm);
		r = l9p_pu8(msg, &fcall->tcreate.mode);
		if (version >= L9P_2000U)
			r = l9p_pustring(msg, &fcall->tcreate.extension);
		break;

	case L9P_RCREATE:
		l9p_puqid(msg, &fcall->rcreate.qid);
		r = l9p_pu32(msg, &fcall->rcreate.iounit);
		break;

	case L9P_TREAD:
	case L9P_TREADDIR:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu64(msg, &fcall->io.offset);
		r = l9p_pu32(msg, &fcall->io.count);
		break;

	case L9P_RREAD:
	case L9P_RREADDIR:
		r = l9p_pu32(msg, &fcall->io.count);
		break;

	case L9P_TWRITE:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu64(msg, &fcall->io.offset);
		r = l9p_pu32(msg, &fcall->io.count);
		break;

	case L9P_RWRITE:
		r = l9p_pu32(msg, &fcall->io.count);
		break;

	case L9P_TCLUNK:
	case L9P_TSTAT:
	case L9P_TREMOVE:
	case L9P_TSTATFS:
		r = l9p_pu32(msg, &fcall->hdr.fid);
		break;

	case L9P_RCLUNK:
	case L9P_RREMOVE:
		break;

	case L9P_RSTAT:
	{
		uint16_t size = l9p_sizeof_stat(&fcall->rstat.stat,
		    version);
		l9p_pu16(msg, &size);
		r = l9p_pustat(msg, &fcall->rstat.stat, version);
	}
		break;

	case L9P_TWSTAT:
	{
		uint16_t size;
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu16(msg, &size);
		r = l9p_pustat(msg, &fcall->twstat.stat, version);
	}
		break;

	case L9P_RWSTAT:
		break;

	case L9P_RSTATFS:
		l9p_pu32(msg, &fcall->rstatfs.statfs.type);
		l9p_pu32(msg, &fcall->rstatfs.statfs.bsize);
		l9p_pu64(msg, &fcall->rstatfs.statfs.blocks);
		l9p_pu64(msg, &fcall->rstatfs.statfs.bfree);
		l9p_pu64(msg, &fcall->rstatfs.statfs.bavail);
		l9p_pu64(msg, &fcall->rstatfs.statfs.files);
		l9p_pu64(msg, &fcall->rstatfs.statfs.ffree);
		l9p_pu64(msg, &fcall->rstatfs.statfs.fsid);
		r = l9p_pu32(msg, &fcall->rstatfs.statfs.namelen);
		break;

	case L9P_TLOPEN:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pu32(msg, &fcall->tlopen.flags);
		break;

	case L9P_RLOPEN:
		l9p_puqid(msg, &fcall->rlopen.qid);
		r = l9p_pu32(msg, &fcall->rlopen.iounit);
		break;

	case L9P_TLCREATE:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tlcreate.name);
		if (r < 0)
			break;
		l9p_pu32(msg, &fcall->tlcreate.flags);
		l9p_pu32(msg, &fcall->tlcreate.mode);
		r = l9p_pu32(msg, &fcall->tlcreate.gid);
		break;

	case L9P_RLCREATE:
		l9p_puqid(msg, &fcall->rlcreate.qid);
		r = l9p_pu32(msg, &fcall->rlcreate.iounit);
		break;

	case L9P_TSYMLINK:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tsymlink.name);
		if (r < 0)
			break;
		r = l9p_pustring(msg, &fcall->tsymlink.symtgt);
		if (r < 0)
			break;
		r = l9p_pu32(msg, &fcall->tlcreate.gid);
		break;

	case L9P_RSYMLINK:
		r = l9p_puqid(msg, &fcall->rsymlink.qid);
		break;

	case L9P_TMKNOD:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tmknod.name);
		if (r < 0)
			break;
		l9p_pu32(msg, &fcall->tmknod.mode);
		l9p_pu32(msg, &fcall->tmknod.major);
		l9p_pu32(msg, &fcall->tmknod.minor);
		r = l9p_pu32(msg, &fcall->tmknod.gid);
		break;

	case L9P_RMKNOD:
		r = l9p_puqid(msg, &fcall->rmknod.qid);
		break;

	case L9P_TRENAME:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu32(msg, &fcall->trename.dfid);
		r = l9p_pustring(msg, &fcall->trename.name);
		break;

	case L9P_RRENAME:
		break;

	case L9P_TREADLINK:
		r = l9p_pu32(msg, &fcall->hdr.fid);
		break;

	case L9P_RREADLINK:
		r = l9p_pustring(msg, &fcall->rreadlink.target);
		break;

	case L9P_TGETATTR:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pu64(msg, &fcall->tgetattr.request_mask);
		break;

	case L9P_RGETATTR:
		l9p_pu64(msg, &fcall->rgetattr.valid);
		l9p_puqid(msg, &fcall->rgetattr.qid);
		l9p_pu32(msg, &fcall->rgetattr.mode);
		l9p_pu32(msg, &fcall->rgetattr.uid);
		l9p_pu32(msg, &fcall->rgetattr.gid);
		l9p_pu64(msg, &fcall->rgetattr.nlink);
		l9p_pu64(msg, &fcall->rgetattr.rdev);
		l9p_pu64(msg, &fcall->rgetattr.size);
		l9p_pu64(msg, &fcall->rgetattr.blksize);
		l9p_pu64(msg, &fcall->rgetattr.blocks);
		l9p_pu64(msg, &fcall->rgetattr.atime_sec);
		l9p_pu64(msg, &fcall->rgetattr.atime_nsec);
		l9p_pu64(msg, &fcall->rgetattr.mtime_sec);
		l9p_pu64(msg, &fcall->rgetattr.mtime_nsec);
		l9p_pu64(msg, &fcall->rgetattr.ctime_sec);
		l9p_pu64(msg, &fcall->rgetattr.ctime_nsec);
		l9p_pu64(msg, &fcall->rgetattr.btime_sec);
		l9p_pu64(msg, &fcall->rgetattr.btime_nsec);
		l9p_pu64(msg, &fcall->rgetattr.gen);
		r = l9p_pu64(msg, &fcall->rgetattr.data_version);
		break;

	case L9P_TSETATTR:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu32(msg, &fcall->tsetattr.valid);
		l9p_pu32(msg, &fcall->tsetattr.mode);
		l9p_pu32(msg, &fcall->tsetattr.uid);
		l9p_pu32(msg, &fcall->tsetattr.gid);
		l9p_pu64(msg, &fcall->tsetattr.size);
		l9p_pu64(msg, &fcall->tsetattr.atime_sec);
		l9p_pu64(msg, &fcall->tsetattr.atime_nsec);
		l9p_pu64(msg, &fcall->tsetattr.mtime_sec);
		r = l9p_pu64(msg, &fcall->tsetattr.mtime_nsec);
		break;

	case L9P_RSETATTR:
		break;

	case L9P_TXATTRWALK:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu32(msg, &fcall->txattrwalk.newfid);
		r = l9p_pustring(msg, &fcall->txattrwalk.name);
		break;

	case L9P_RXATTRWALK:
		r = l9p_pu64(msg, &fcall->rxattrwalk.size);
		break;

	case L9P_TXATTRCREATE:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->txattrcreate.name);
		if (r < 0)
			break;
		l9p_pu64(msg, &fcall->txattrcreate.attr_size);
		r = l9p_pu32(msg, &fcall->txattrcreate.flags);
		break;

	case L9P_RXATTRCREATE:
		break;

	case L9P_TFSYNC:
		r = l9p_pu32(msg, &fcall->hdr.fid);
		break;

	case L9P_RFSYNC:
		break;

	case L9P_TLOCK:
		l9p_pu32(msg, &fcall->hdr.fid);
		l9p_pu8(msg, &fcall->tlock.type);
		l9p_pu32(msg, &fcall->tlock.flags);
		l9p_pu64(msg, &fcall->tlock.start);
		l9p_pu64(msg, &fcall->tlock.length);
		l9p_pu32(msg, &fcall->tlock.proc_id);
		r = l9p_pustring(msg, &fcall->tlock.client_id);
		break;

	case L9P_RLOCK:
		r = l9p_pu8(msg, &fcall->rlock.status);
		break;

	case L9P_TGETLOCK:
		l9p_pu32(msg, &fcall->hdr.fid);
		/* FALLTHROUGH */

	case L9P_RGETLOCK:
		l9p_pu8(msg, &fcall->getlock.type);
		l9p_pu64(msg, &fcall->getlock.start);
		l9p_pu64(msg, &fcall->getlock.length);
		l9p_pu32(msg, &fcall->getlock.proc_id);
		r = l9p_pustring(msg, &fcall->getlock.client_id);
		break;

	case L9P_TLINK:
		l9p_pu32(msg, &fcall->tlink.dfid);
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tlink.name);
		break;

	case L9P_RLINK:
		break;

	case L9P_TMKDIR:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tmkdir.name);
		if (r < 0)
			break;
		l9p_pu32(msg, &fcall->tmkdir.mode);
		r = l9p_pu32(msg, &fcall->tmkdir.gid);
		break;

	case L9P_RMKDIR:
		r = l9p_puqid(msg, &fcall->rmkdir.qid);
		break;

	case L9P_TRENAMEAT:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->trenameat.oldname);
		if (r < 0)
			break;
		l9p_pu32(msg, &fcall->trenameat.newdirfid);
		r = l9p_pustring(msg, &fcall->trenameat.newname);
		break;

	case L9P_RRENAMEAT:
		break;

	case L9P_TUNLINKAT:
		l9p_pu32(msg, &fcall->hdr.fid);
		r = l9p_pustring(msg, &fcall->tunlinkat.name);
		if (r < 0)
			break;
		r = l9p_pu32(msg, &fcall->tunlinkat.flags);
		break;

	case L9P_RUNLINKAT:
		break;

	default:
		L9P_LOG(L9P_ERROR, "%s(): missing case for type %d",
		    __func__, fcall->hdr.type);
		break;
	}

	/* Check for over- or under-run, or pustring error. */
	if (r < 0)
		return (-1);

	if (msg->lm_mode == L9P_PACK) {
		/* Rewind to the beginning and install size at front. */
		uint32_t len = (uint32_t)msg->lm_size;
		msg->lm_cursor_offset = 0;
		msg->lm_cursor_iov = 0;

		/*
		 * Subtract 4 bytes from current size, becase we're
		 * overwriting size (rewinding message to the beginning)
		 * and writing again, which will increase it 4 more.
		 */
		msg->lm_size -= sizeof(uint32_t);

		if (fcall->hdr.type == L9P_RREAD ||
		    fcall->hdr.type == L9P_RREADDIR)
			len += fcall->io.count;

		l9p_pu32(msg, &len);
	}

	return (0);
}

/*
 * Free any strings or other data malloc'ed in the process of
 * packing or unpacking an fcall.
 */
void
l9p_freefcall(union l9p_fcall *fcall)
{
	uint16_t i;

	switch (fcall->hdr.type) {

	case L9P_TVERSION:
	case L9P_RVERSION:
		free(fcall->version.version);
		return;

	case L9P_TATTACH:
		free(fcall->tattach.aname);
		free(fcall->tattach.uname);
		return;

	case L9P_TWALK:
		for (i = 0; i < fcall->twalk.nwname; i++)
			free(fcall->twalk.wname[i]);
		return;

	case L9P_TCREATE:
	case L9P_TOPEN:
		free(fcall->tcreate.name);
		free(fcall->tcreate.extension);
		return;

	case L9P_RSTAT:
		l9p_freestat(&fcall->rstat.stat);
		return;

	case L9P_TWSTAT:
		l9p_freestat(&fcall->twstat.stat);
		return;

	case L9P_TLCREATE:
		free(fcall->tlcreate.name);
		return;

	case L9P_TSYMLINK:
		free(fcall->tsymlink.name);
		free(fcall->tsymlink.symtgt);
		return;

	case L9P_TMKNOD:
		free(fcall->tmknod.name);
		return;

	case L9P_TRENAME:
		free(fcall->trename.name);
		return;

	case L9P_RREADLINK:
		free(fcall->rreadlink.target);
		return;

	case L9P_TXATTRWALK:
		free(fcall->txattrwalk.name);
		return;

	case L9P_TXATTRCREATE:
		free(fcall->txattrcreate.name);
		return;

	case L9P_TLOCK:
		free(fcall->tlock.client_id);
		return;

	case L9P_TGETLOCK:
	case L9P_RGETLOCK:
		free(fcall->getlock.client_id);
		return;

	case L9P_TLINK:
		free(fcall->tlink.name);
		return;

	case L9P_TMKDIR:
		free(fcall->tmkdir.name);
		return;

	case L9P_TRENAMEAT:
		free(fcall->trenameat.oldname);
		free(fcall->trenameat.newname);
		return;

	case L9P_TUNLINKAT:
		free(fcall->tunlinkat.name);
		return;
	}
}

void
l9p_freestat(struct l9p_stat *stat)
{
	free(stat->name);
	free(stat->extension);
	free(stat->uid);
	free(stat->gid);
	free(stat->muid);
}

uint16_t
l9p_sizeof_stat(struct l9p_stat *stat, enum l9p_version version)
{
	uint16_t size = L9P_WORD /* size */
	    + L9P_WORD /* type */
	    + L9P_DWORD /* dev */
	    + QID_SIZE /* qid */
	    + 3 * L9P_DWORD /* mode, atime, mtime */
	    + L9P_QWORD /* length */
	    + STRING_SIZE(stat->name)
	    + STRING_SIZE(stat->uid)
	    + STRING_SIZE(stat->gid)
	    + STRING_SIZE(stat->muid);

	if (version >= L9P_2000U) {
		size += STRING_SIZE(stat->extension)
		    + 3 * L9P_DWORD;
	}

	return (size);
}
