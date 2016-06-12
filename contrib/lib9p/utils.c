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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/uio.h>
#if defined(__FreeBSD__)
#include <sys/sbuf.h>
#else
#include "sbuf/sbuf.h"
#endif
#include "lib9p.h"
#include "fcall.h"
#include "linux_errno.h"

#define N(ary)          (sizeof(ary) / sizeof(*ary))

static void l9p_describe_fid(const char *, uint32_t, struct sbuf *);
static void l9p_describe_mode(const char *, uint32_t, struct sbuf *);
static void l9p_describe_name(const char *, char *, struct sbuf *);
static void l9p_describe_perm(const char *, uint32_t, struct sbuf *);
static void l9p_describe_qid(const char *, struct l9p_qid *, struct sbuf *);
static void l9p_describe_stat(const char *, struct l9p_stat *, struct sbuf *);
static void l9p_describe_statfs(const char *, struct l9p_statfs *,
    struct sbuf *);
static void l9p_describe_time(struct sbuf *, const char *, uint64_t, uint64_t);
static void l9p_describe_readdir(struct sbuf *, struct l9p_f_io *);
static void l9p_describe_size(const char *, uint64_t, struct sbuf *);
static void l9p_describe_ugid(const char *, uint32_t, struct sbuf *);
static const char *lookup_linux_errno(uint32_t);

/*
 * Using indexed initializers, we can have these occur in any order.
 * Using adjacent-string concatenation ("T" #name, "R" #name), we
 * get both Tfoo and Rfoo strings with one copy of the name.
 * Alas, there is no stupid cpp trick to lowercase-ify, so we
 * have to write each name twice.  In which case we might as well
 * make the second one a string in the first place and not bother
 * with the stringizing.
 *
 * This table should have entries for each enum value in fcall.h.
 */
#define X(NAME, name)	[L9P_T##NAME - L9P__FIRST] = "T" name, \
			[L9P_R##NAME - L9P__FIRST] = "R" name
static const char *ftype_names[] = {
	X(VERSION,	"version"),
	X(AUTH,		"auth"),
	X(ATTACH,	"attach"),
	X(ERROR,	"error"),
	X(LERROR,	"lerror"),
	X(FLUSH,	"flush"),
	X(WALK,		"walk"),
	X(OPEN,		"open"),
	X(CREATE,	"create"),
	X(READ,		"read"),
	X(WRITE,	"write"),
	X(CLUNK,	"clunk"),
	X(REMOVE,	"remove"),
	X(STAT,		"stat"),
	X(WSTAT,	"wstat"),
	X(STATFS,	"statfs"),
	X(LOPEN,	"lopen"),
	X(LCREATE,	"lcreate"),
	X(SYMLINK,	"symlink"),
	X(MKNOD,	"mknod"),
	X(RENAME,	"rename"),
	X(READLINK,	"readlink"),
	X(GETATTR,	"getattr"),
	X(XATTRWALK,	"xattrwalk"),
	X(XATTRCREATE,	"xattrcreate"),
	X(READDIR,	"readdir"),
	X(FSYNC,	"fsync"),
	X(LOCK,		"lock"),
	X(GETLOCK,	"getlock"),
	X(LINK,		"link"),
	X(MKDIR,	"mkdir"),
	X(RENAMEAT,	"renameat"),
	X(UNLINKAT,	"unlinkat"),
};
#undef X

void
l9p_seek_iov(struct iovec *iov1, size_t niov1, struct iovec *iov2,
    size_t *niov2, size_t seek)
{
	size_t remainder = 0;
	size_t left = seek;
	size_t i, j;

	for (i = 0; i < niov1; i++) {
		size_t toseek = MIN(left, iov1[i].iov_len);
		left -= toseek;

		if (toseek == iov1[i].iov_len)
			continue;

		if (left == 0) {
			remainder = toseek;
			break;
		}
	}

	for (j = i; j < niov1; j++) {
		iov2[j - i].iov_base = (char *)iov1[j].iov_base + remainder;
		iov2[j - i].iov_len = iov1[j].iov_len - remainder;
		remainder = 0;
	}

	*niov2 = j - i;
}

size_t
l9p_truncate_iov(struct iovec *iov, size_t niov, size_t length)
{
	size_t i, done = 0;

	for (i = 0; i < niov; i++) {
		size_t toseek = MIN(length - done, iov[i].iov_len);
		done += toseek;

		if (toseek < iov[i].iov_len) {
			iov[i].iov_len = toseek;
			return (i + 1);
		}
	}

	return (niov);
}

/*
 * Show file ID.
 */
static void
l9p_describe_fid(const char *str, uint32_t fid, struct sbuf *sb)
{

	sbuf_printf(sb, "%s%" PRIu32, str, fid);
}

/*
 * Show user or group ID.
 */
static void
l9p_describe_ugid(const char *str, uint32_t ugid, struct sbuf *sb)
{

	sbuf_printf(sb, "%s%" PRIu32, str, ugid);
}

/*
 * Show file mode (O_RDWR, O_RDONLY, etc) - note that upper bits
 * may be set for .L open, where this is called "flags".
 *
 * For now we just decode in hex.
 */
static void
l9p_describe_mode(const char *str, uint32_t mode, struct sbuf *sb)
{

	sbuf_printf(sb, "%s%" PRIx32, str, mode);
}

/*
 * Show file name or other similar, potentially-very-long string.
 * Actual strings get quotes, a NULL name (if it occurs) gets
 * <null> (no quotes), so you can tell the difference.
 */
static void
l9p_describe_name(const char *str, char *name, struct sbuf *sb)
{
	size_t len;

	if (name == NULL) {
		sbuf_printf(sb, "%s<null>", str);
		return;
	}

	len = strlen(name);

	if (len > 32)
		sbuf_printf(sb, "%s\"%.*s...\"", str, 32 - 3, name);
	else
		sbuf_printf(sb, "%s\"%.*s\"", str, (int)len, name);
}

/*
 * Show permissions (rwx etc).
 */
static void
l9p_describe_perm(const char *str, uint32_t mode, struct sbuf *sb)
{
	char pbuf[12];

	strmode(mode & 0777, pbuf);
	sbuf_printf(sb, "%s%" PRIx32 "<%.9s>", str, mode, pbuf + 1);
}

/*
 * Show qid (<type, version, path> tuple).
 */
static void
l9p_describe_qid(const char *str, struct l9p_qid *qid, struct sbuf *sb)
{

	assert(qid != NULL);

	sbuf_printf(sb, "%s<0x%02x,%u,0x%016" PRIx64 ">", str,
	    qid->type, qid->version, qid->path);
}

/*
 * Show size.
 */
static void
l9p_describe_size(const char *str, uint64_t size, struct sbuf *sb)
{

	sbuf_printf(sb, "%s%" PRIu64, str, size);
}

static void
l9p_describe_stat(const char *str, struct l9p_stat *st, struct sbuf *sb)
{

	assert(st != NULL);

	sbuf_printf(sb, "%stype=0x%04x dev=0x%08" PRIx32,
	    str, st->type, st->dev);
	l9p_describe_name(" name=", st->name, sb);
	l9p_describe_name(" uid=", st->uid, sb);
}

static void
l9p_describe_statfs(const char *str, struct l9p_statfs *st, struct sbuf *sb)
{

	assert(st != NULL);

	sbuf_printf(sb, "%stype=0x%04lx bsize=%lu blocks=%" PRIu64
	    " bfree=%" PRIu64 " bavail=%" PRIu64 " files=%" PRIu64
	    " ffree=%" PRIu64 " fsid=0x%" PRIx64 " namelen=%" PRIu32 ">",
	    str, (u_long)st->type, (u_long)st->bsize, st->blocks,
	    st->bfree, st->bavail, st->files,
	    st->ffree, st->fsid, st->namelen);
}

/*
 * Decode a <seconds,nsec> timestamp.
 *
 * Perhaps should use asctime_r.  For now, raw values.
 */
static void
l9p_describe_time(struct sbuf *sb, const char *s, uint64_t sec, uint64_t nsec)
{

	sbuf_cat(sb, s);
	if (nsec > 999999999)
		sbuf_printf(sb, "%" PRIu64 ".<invalid nsec %" PRIu64 ">)", sec, nsec);
	else
		sbuf_printf(sb, "%" PRIu64 ".%09" PRIu64, sec, nsec);
}

/*
 * Decode readdir data (.L format, variable length names).
 */
static void
l9p_describe_readdir(struct sbuf *sb, struct l9p_f_io *io)
{
	uint32_t count;
#ifdef notyet
	int i;
	struct l9p_message msg;
	struct l9p_dirent de;
#endif

	if ((count = io->count) == 0) {
		sbuf_printf(sb, " EOF (count=0)");
		return;
	}

	/*
	 * Can't do this yet because we do not have the original
	 * req.
	 */
#ifdef notyet
	sbuf_printf(sb, " count=%" PRIu32 " [", count);

	l9p_init_msg(&msg, req, L9P_UNPACK);
	for (i = 0; msg.lm_size < count; i++) {
		if (l9p_pudirent(&msg, &de) < 0) {
			sbuf_printf(sb, " bad count");
			break;
		}

		sbuf_printf(sb, i ? ", " : " ");
		l9p_describe_qid(" qid=", &de.qid, sb);
		sbuf_printf(sb, " offset=%" PRIu64 " type=%d",
		    de.offset, de.type);
		l9p_describe_name(" name=", de.name);
		free(de.name);
	}
	sbuf_printf(sb, "]=%d dir entries", i);
#else /* notyet */
	sbuf_printf(sb, " count=%" PRIu32, count);
#endif
}

static const char *
lookup_linux_errno(uint32_t linux_errno)
{
	static char unknown[50];

	/*
	 * Error numbers in the "base" range (1..ERANGE) are common
	 * across BSD, MacOS, Linux, and Plan 9.
	 *
	 * Error numbers outside that range require translation.
	 */
	const char *const table[] = {
#define X0(name) [name] = name ## _STR
#define	X(name) [name] = name ## _STR
		X(LINUX_EAGAIN),
		X(LINUX_EDEADLK),
		X(LINUX_ENAMETOOLONG),
		X(LINUX_ENOLCK),
		X(LINUX_ENOSYS),
		X(LINUX_ENOTEMPTY),
		X(LINUX_ELOOP),
		X(LINUX_ENOMSG),
		X(LINUX_EIDRM),
		X(LINUX_ECHRNG),
		X(LINUX_EL2NSYNC),
		X(LINUX_EL3HLT),
		X(LINUX_EL3RST),
		X(LINUX_ELNRNG),
		X(LINUX_EUNATCH),
		X(LINUX_ENOCSI),
		X(LINUX_EL2HLT),
		X(LINUX_EBADE),
		X(LINUX_EBADR),
		X(LINUX_EXFULL),
		X(LINUX_ENOANO),
		X(LINUX_EBADRQC),
		X(LINUX_EBADSLT),
		X(LINUX_EBFONT),
		X(LINUX_ENOSTR),
		X(LINUX_ENODATA),
		X(LINUX_ETIME),
		X(LINUX_ENOSR),
		X(LINUX_ENONET),
		X(LINUX_ENOPKG),
		X(LINUX_EREMOTE),
		X(LINUX_ENOLINK),
		X(LINUX_EADV),
		X(LINUX_ESRMNT),
		X(LINUX_ECOMM),
		X(LINUX_EPROTO),
		X(LINUX_EMULTIHOP),
		X(LINUX_EDOTDOT),
		X(LINUX_EBADMSG),
		X(LINUX_EOVERFLOW),
		X(LINUX_ENOTUNIQ),
		X(LINUX_EBADFD),
		X(LINUX_EREMCHG),
		X(LINUX_ELIBACC),
		X(LINUX_ELIBBAD),
		X(LINUX_ELIBSCN),
		X(LINUX_ELIBMAX),
		X(LINUX_ELIBEXEC),
		X(LINUX_EILSEQ),
		X(LINUX_ERESTART),
		X(LINUX_ESTRPIPE),
		X(LINUX_EUSERS),
		X(LINUX_ENOTSOCK),
		X(LINUX_EDESTADDRREQ),
		X(LINUX_EMSGSIZE),
		X(LINUX_EPROTOTYPE),
		X(LINUX_ENOPROTOOPT),
		X(LINUX_EPROTONOSUPPORT),
		X(LINUX_ESOCKTNOSUPPORT),
		X(LINUX_EOPNOTSUPP),
		X(LINUX_EPFNOSUPPORT),
		X(LINUX_EAFNOSUPPORT),
		X(LINUX_EADDRINUSE),
		X(LINUX_EADDRNOTAVAIL),
		X(LINUX_ENETDOWN),
		X(LINUX_ENETUNREACH),
		X(LINUX_ENETRESET),
		X(LINUX_ECONNABORTED),
		X(LINUX_ECONNRESET),
		X(LINUX_ENOBUFS),
		X(LINUX_EISCONN),
		X(LINUX_ENOTCONN),
		X(LINUX_ESHUTDOWN),
		X(LINUX_ETOOMANYREFS),
		X(LINUX_ETIMEDOUT),
		X(LINUX_ECONNREFUSED),
		X(LINUX_EHOSTDOWN),
		X(LINUX_EHOSTUNREACH),
		X(LINUX_EALREADY),
		X(LINUX_EINPROGRESS),
		X(LINUX_ESTALE),
		X(LINUX_EUCLEAN),
		X(LINUX_ENOTNAM),
		X(LINUX_ENAVAIL),
		X(LINUX_EISNAM),
		X(LINUX_EREMOTEIO),
		X(LINUX_EDQUOT),
		X(LINUX_ENOMEDIUM),
		X(LINUX_EMEDIUMTYPE),
		X(LINUX_ECANCELED),
		X(LINUX_ENOKEY),
		X(LINUX_EKEYEXPIRED),
		X(LINUX_EKEYREVOKED),
		X(LINUX_EKEYREJECTED),
		X(LINUX_EOWNERDEAD),
		X(LINUX_ENOTRECOVERABLE),
		X(LINUX_ERFKILL),
		X(LINUX_EHWPOISON),
#undef X0
#undef X
	};
	if ((size_t)linux_errno < N(table) && table[linux_errno] != NULL)
		return (table[linux_errno]);
	if (linux_errno <= ERANGE)
		return (strerror((int)linux_errno));
	(void) snprintf(unknown, sizeof(unknown),
	    "Unknown error %d", linux_errno);
	return (unknown);
}

void
l9p_describe_fcall(union l9p_fcall *fcall, enum l9p_version version,
    struct sbuf *sb)
{
	uint64_t mask;
	uint8_t type;
	int i;

	assert(fcall != NULL);
	assert(sb != NULL);
	assert(version <= L9P_2000L && version >= L9P_INVALID_VERSION);

	type = fcall->hdr.type;

	if (type < L9P__FIRST || type >= L9P__LAST_PLUS_1 ||
	    ftype_names[type - L9P__FIRST] == NULL) {
		sbuf_printf(sb, "<unknown request %d> tag=%d", type,
		    fcall->hdr.tag);
	} else {
		sbuf_printf(sb, "%s tag=%d", ftype_names[type - L9P__FIRST],
		    fcall->hdr.tag);
	}

	switch (type) {
	case L9P_TVERSION:
	case L9P_RVERSION:
		sbuf_printf(sb, " version=\"%s\" msize=%d", fcall->version.version,
		    fcall->version.msize);
		return;

	case L9P_TAUTH:
		l9p_describe_fid(" afid=", fcall->hdr.fid, sb);
		sbuf_printf(sb, " uname=\"%s\" aname=\"%s\"",
		    fcall->tauth.uname, fcall->tauth.aname);
		return;

	case L9P_TATTACH:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_fid(" afid=", fcall->tattach.afid, sb);
		sbuf_printf(sb, " uname=\"%s\" aname=\"%s\"",
		    fcall->tattach.uname, fcall->tattach.aname);
		if (version >= L9P_2000U)
			sbuf_printf(sb, " n_uname=%d", fcall->tattach.n_uname);
		return;

	case L9P_RATTACH:
		l9p_describe_qid(" ", &fcall->rattach.qid, sb);
		return;

	case L9P_RERROR:
		sbuf_printf(sb, " ename=\"%s\" errnum=%d", fcall->error.ename,
		    fcall->error.errnum);
		return;

	case L9P_RLERROR:
		sbuf_printf(sb, " errnum=%d (%s)", fcall->error.errnum,
		    lookup_linux_errno(fcall->error.errnum));
		return;

	case L9P_TFLUSH:
		sbuf_printf(sb, " oldtag=%d", fcall->tflush.oldtag);
		return;

	case L9P_RFLUSH:
		return;

	case L9P_TWALK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_fid(" newfid=", fcall->twalk.newfid, sb);
		sbuf_cat(sb, " wname=\"");
		for (i = 0; i < fcall->twalk.nwname; i++)
			sbuf_printf(sb, "%s%s", i == 0 ? "" : "/",
			    fcall->twalk.wname[i]);
		sbuf_cat(sb, "\"");

		return;

	case L9P_RWALK:
		sbuf_printf(sb, " wqid=[");
		for (i = 0; i < fcall->rwalk.nwqid; i++)
			l9p_describe_qid(i == 0 ? "" : ",",
			    &fcall->rwalk.wqid[i], sb);
		sbuf_cat(sb, "]");
		return;

	case L9P_TOPEN:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_mode(" mode=", fcall->tcreate.mode, sb);
		return;

	case L9P_ROPEN:
		l9p_describe_qid(" qid=", &fcall->ropen.qid, sb);
		sbuf_printf(sb, " iounit=%d", fcall->ropen.iounit);
		return;

	case L9P_TCREATE:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tcreate.name, sb);
		l9p_describe_perm(" perm=", fcall->tcreate.perm, sb);
		l9p_describe_mode(" mode=", fcall->tcreate.mode, sb);
		return;

	case L9P_RCREATE:
		l9p_describe_qid(" qid=", &fcall->rcreate.qid, sb);
		sbuf_printf(sb, " iounit=%d", fcall->rcreate.iounit);
		return;

	case L9P_TREAD:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		sbuf_printf(sb, " offset=%" PRIu64 " count=%" PRIu32,
		    fcall->io.offset, fcall->io.count);
		return;

	case L9P_RREAD:
	case L9P_RWRITE:
		sbuf_printf(sb, " count=%" PRIu32, fcall->io.count);
		return;

	case L9P_TWRITE:
	case L9P_TREADDIR:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		sbuf_printf(sb, " offset=%" PRIu64 " count=%" PRIu32,
		    fcall->io.offset, fcall->io.count);
		return;

	case L9P_TCLUNK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		sbuf_printf(sb, " fid=%d", fcall->hdr.fid);
		return;

	case L9P_RCLUNK:
		return;

	case L9P_TREMOVE:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		return;

	case L9P_RREMOVE:
		return;

	case L9P_TSTAT:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		return;

	case L9P_RSTAT:
		l9p_describe_stat(" ", &fcall->rstat.stat, sb);
		return;

	case L9P_TWSTAT:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_stat(" ", &fcall->twstat.stat, sb);
		return;

	case L9P_RWSTAT:
		return;

	case L9P_TSTATFS:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		return;

	case L9P_RSTATFS:
		l9p_describe_statfs(" ", &fcall->rstatfs.statfs, sb);
		return;

	case L9P_TLOPEN:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_mode(" flags=", fcall->tlcreate.flags, sb);
		return;

	case L9P_RLOPEN:
		l9p_describe_qid(" qid=", &fcall->rlopen.qid, sb);
		sbuf_printf(sb, " iounit=%d", fcall->rlopen.iounit);
		return;

	case L9P_TLCREATE:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tlcreate.name, sb);
		/* confusing: "flags" is open-mode, "mode" is permissions */
		l9p_describe_mode(" flags=", fcall->tlcreate.flags, sb);
		l9p_describe_perm(" mode=", fcall->tlcreate.mode, sb);
		l9p_describe_ugid(" gid=", fcall->tlcreate.gid, sb);
		return;

	case L9P_RLCREATE:
		l9p_describe_qid(" qid=", &fcall->rlcreate.qid, sb);
		sbuf_printf(sb, " iounit=%d", fcall->rlcreate.iounit);
		return;

	case L9P_TSYMLINK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tsymlink.name, sb);
		l9p_describe_name(" symtgt=", fcall->tsymlink.symtgt, sb);
		l9p_describe_ugid(" gid=", fcall->tsymlink.gid, sb);
		return;

	case L9P_RSYMLINK:
		l9p_describe_qid(" qid=", &fcall->ropen.qid, sb);
		return;

	case L9P_TMKNOD:
		l9p_describe_fid(" dfid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tmknod.name, sb);
		/* can't just use permission decode: mode contains blk/chr */
		sbuf_printf(sb, " mode=0x%08x major=%u minor=%u",
		    fcall->tmknod.mode,
		    fcall->tmknod.major, fcall->tmknod.minor);
		l9p_describe_ugid(" gid=", fcall->tmknod.gid, sb);
		return;

	case L9P_RMKNOD:
		l9p_describe_qid(" qid=", &fcall->rmknod.qid, sb);
		return;

	case L9P_TRENAME:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_fid(" dfid=", fcall->trename.dfid, sb);
		l9p_describe_name(" name=", fcall->trename.name, sb);
		return;

	case L9P_RRENAME:
		return;

	case L9P_TREADLINK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		return;

	case L9P_RREADLINK:
		l9p_describe_name(" target=", fcall->rreadlink.target, sb);
		return;

	case L9P_TGETATTR:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		mask = fcall->tgetattr.request_mask;
		sbuf_printf(sb, " request_mask=0x%016" PRIx64, mask);
		/* XXX decode request_mask later */
		return;

	case L9P_RGETATTR:
		mask = fcall->rgetattr.valid;
		sbuf_printf(sb, " valid=0x%016" PRIx64, mask);
		l9p_describe_qid(" qid=", &fcall->rgetattr.qid, sb);
		if (mask & L9PL_GETATTR_MODE)
			sbuf_printf(sb, " mode=0x%08x", fcall->rgetattr.mode);
		if (mask & L9PL_GETATTR_UID)
			l9p_describe_ugid(" uid=", fcall->rgetattr.uid, sb);
		if (mask & L9PL_GETATTR_GID)
			l9p_describe_ugid(" gid=", fcall->rgetattr.gid, sb);
		if (mask & L9PL_GETATTR_NLINK)
			sbuf_printf(sb, " nlink=%" PRIu64,
			    fcall->rgetattr.nlink);
		if (mask & L9PL_GETATTR_RDEV)
			sbuf_printf(sb, " rdev=0x%" PRIx64,
			    fcall->rgetattr.rdev);
		if (mask & L9PL_GETATTR_SIZE)
			l9p_describe_size(" size=", fcall->rgetattr.size, sb);
		if (mask & L9PL_GETATTR_BLOCKS)
			sbuf_printf(sb, " blksize=%" PRIu64 " blocks=%" PRIu64,
			    fcall->rgetattr.blksize, fcall->rgetattr.blocks);
		if (mask & L9PL_GETATTR_ATIME)
			l9p_describe_time(sb, " atime=",
			    fcall->rgetattr.atime_sec,
			    fcall->rgetattr.atime_nsec);
		if (mask & L9PL_GETATTR_MTIME)
			l9p_describe_time(sb, " mtime=",
			    fcall->rgetattr.mtime_sec,
			    fcall->rgetattr.mtime_nsec);
		if (mask & L9PL_GETATTR_CTIME)
			l9p_describe_time(sb, " ctime=",
			    fcall->rgetattr.ctime_sec,
			    fcall->rgetattr.ctime_nsec);
		if (mask & L9PL_GETATTR_BTIME)
			l9p_describe_time(sb, " btime=",
			    fcall->rgetattr.btime_sec,
			    fcall->rgetattr.btime_nsec);
		if (mask & L9PL_GETATTR_GEN)
			sbuf_printf(sb, " gen=0x%" PRIx64, fcall->rgetattr.gen);
		if (mask & L9PL_GETATTR_DATA_VERSION)
			sbuf_printf(sb, " data_version=0x%" PRIx64,
			    fcall->rgetattr.data_version);
		return;

	case L9P_TSETATTR:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		mask = fcall->tsetattr.valid;
		/* NB: tsetattr valid mask is only 32 bits, hence %08x */
		sbuf_printf(sb, " valid=0x%08" PRIx64, mask);
		if (mask & L9PL_SETATTR_MODE)
			sbuf_printf(sb, " mode=0x%08x", fcall->tsetattr.mode);
		if (mask & L9PL_SETATTR_UID)
			l9p_describe_ugid(" uid=", fcall->tsetattr.uid, sb);
		if (mask & L9PL_SETATTR_GID)
			l9p_describe_ugid(" uid=", fcall->tsetattr.gid, sb);
		if (mask & L9PL_SETATTR_SIZE)
			l9p_describe_size(" size=", fcall->tsetattr.size, sb);
		if (mask & L9PL_SETATTR_ATIME) {
			if (mask & L9PL_SETATTR_ATIME_SET)
				l9p_describe_time(sb, " atime=",
				    fcall->tsetattr.atime_sec,
				    fcall->tsetattr.atime_nsec);
			else
				sbuf_printf(sb, " atime=now");
		}
		if (mask & L9PL_SETATTR_MTIME) {
			if (mask & L9PL_SETATTR_MTIME_SET)
				l9p_describe_time(sb, " mtime=",
				    fcall->tsetattr.mtime_sec,
				    fcall->tsetattr.mtime_nsec);
			else
				sbuf_printf(sb, " mtime=now");
		}
		return;

	case L9P_RSETATTR:
		return;

	case L9P_TXATTRWALK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_fid(" newfid=", fcall->txattrwalk.newfid, sb);
		l9p_describe_name(" name=", fcall->txattrwalk.name, sb);
		return;

	case L9P_RXATTRWALK:
		l9p_describe_size(" size=", fcall->rxattrwalk.size, sb);
		return;

	case L9P_TXATTRCREATE:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->txattrcreate.name, sb);
		l9p_describe_size(" size=", fcall->txattrcreate.attr_size, sb);
		sbuf_printf(sb, " flags=%" PRIu32, fcall->txattrcreate.flags);
		return;

	case L9P_RXATTRCREATE:
		return;

	case L9P_RREADDIR:
		l9p_describe_readdir(sb, &fcall->io);
		return;

	case L9P_TFSYNC:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		return;

	case L9P_RFSYNC:
		return;

	case L9P_TLOCK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		/* decode better later */
		sbuf_printf(sb, " type=%d flags=0x%" PRIx32
		    " start=%" PRIu64 " length=%" PRIu64
		    " proc_id=0x%" PRIx32 " client_id=\"%s\"",
		    fcall->tlock.type, fcall->tlock.flags,
		    fcall->tlock.start, fcall->tlock.length,
		    fcall->tlock.proc_id, fcall->tlock.client_id);
		return;

	case L9P_RLOCK:
		sbuf_printf(sb, " status=%d", fcall->rlock.status);
		return;

	case L9P_TGETLOCK:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		/* FALLTHROUGH */

	case L9P_RGETLOCK:
		/* decode better later */
		sbuf_printf(sb, " type=%d "
		    " start=%" PRIu64 " length=%" PRIu64
		    " proc_id=0x%" PRIx32 " client_id=\"%s\"",
		    fcall->getlock.type,
		    fcall->getlock.start, fcall->getlock.length,
		    fcall->getlock.proc_id, fcall->getlock.client_id);
		return;

	case L9P_TLINK:
		l9p_describe_fid(" dfid=", fcall->tlink.dfid, sb);
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tlink.name, sb);
		return;

	case L9P_RLINK:
		return;

	case L9P_TMKDIR:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tmkdir.name, sb);
		l9p_describe_perm(" mode=", fcall->tmkdir.mode, sb);
		l9p_describe_ugid(" gid=", fcall->tmkdir.gid, sb);
		return;

	case L9P_RMKDIR:
		l9p_describe_qid(" qid=", &fcall->rmkdir.qid, sb);
		return;

	case L9P_TRENAMEAT:
		l9p_describe_fid(" olddirfid=", fcall->hdr.fid, sb);
		l9p_describe_name(" oldname=", fcall->trenameat.oldname,
		    sb);
		l9p_describe_fid(" newdirfid=", fcall->trenameat.newdirfid, sb);
		l9p_describe_name(" newname=", fcall->trenameat.newname,
		    sb);
		return;

	case L9P_RRENAMEAT:
		return;

	case L9P_TUNLINKAT:
		l9p_describe_fid(" dirfd=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tunlinkat.name, sb);
		sbuf_printf(sb, " flags=0x%08" PRIx32, fcall->tunlinkat.flags);
		return;

	case L9P_RUNLINKAT:
		return;

	default:
		sbuf_printf(sb, " <missing case in %s()>", __func__);
	}
}
