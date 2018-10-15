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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/uio.h>
#if defined(__FreeBSD__)
#include <sys/sbuf.h>
#else
#include "sbuf/sbuf.h"
#endif
#include "lib9p.h"
#include "fcall.h"
#include "linux_errno.h"

#ifdef __APPLE__
  #define GETGROUPS_GROUP_TYPE_IS_INT
#endif

#define N(ary)          (sizeof(ary) / sizeof(*ary))

/* See l9p_describe_bits() below. */
struct descbits {
	uint64_t	db_mask;	/* mask value */
	uint64_t	db_match;	/* match value */
	const char	*db_name;	/* name for matched value */
};


static bool l9p_describe_bits(const char *, uint64_t, const char *,
    const struct descbits *, struct sbuf *);
static void l9p_describe_fid(const char *, uint32_t, struct sbuf *);
static void l9p_describe_mode(const char *, uint32_t, struct sbuf *);
static void l9p_describe_name(const char *, char *, struct sbuf *);
static void l9p_describe_perm(const char *, uint32_t, struct sbuf *);
static void l9p_describe_lperm(const char *, uint32_t, struct sbuf *);
static void l9p_describe_qid(const char *, struct l9p_qid *, struct sbuf *);
static void l9p_describe_l9stat(const char *, struct l9p_stat *,
    enum l9p_version, struct sbuf *);
static void l9p_describe_statfs(const char *, struct l9p_statfs *,
    struct sbuf *);
static void l9p_describe_time(struct sbuf *, const char *, uint64_t, uint64_t);
static void l9p_describe_readdir(struct sbuf *, struct l9p_f_io *);
static void l9p_describe_size(const char *, uint64_t, struct sbuf *);
static void l9p_describe_ugid(const char *, uint32_t, struct sbuf *);
static void l9p_describe_getattr_mask(uint64_t, struct sbuf *);
static void l9p_describe_unlinkat_flags(const char *, uint32_t, struct sbuf *);
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
	X(SETATTR,	"setattr"),
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
 * This wrapper for getgrouplist() that malloc'ed memory, and
 * papers over FreeBSD vs Mac differences in the getgrouplist()
 * argument types.
 *
 * Note that this function guarantees that *either*:
 *     return value != NULL and *angroups has been set
 * or: return value == NULL and *angroups is 0
 */
gid_t *
l9p_getgrlist(const char *name, gid_t basegid, int *angroups)
{
#ifdef GETGROUPS_GROUP_TYPE_IS_INT
	int i, *int_groups;
#endif
	gid_t *groups;
	int ngroups;

	/*
	 * Todo, perhaps: while getgrouplist() returns -1, expand.
	 * For now just use NGROUPS_MAX.
	 */
	ngroups = NGROUPS_MAX;
	groups = malloc((size_t)ngroups * sizeof(*groups));
#ifdef GETGROUPS_GROUP_TYPE_IS_INT
	int_groups = groups ? malloc((size_t)ngroups * sizeof(*int_groups)) :
	    NULL;
	if (int_groups == NULL) {
		free(groups);
		groups = NULL;
	}
#endif
	if (groups == NULL) {
		*angroups = 0;
		return (NULL);
	}
#ifdef GETGROUPS_GROUP_TYPE_IS_INT
	(void) getgrouplist(name, (int)basegid, int_groups, &ngroups);
	for (i = 0; i < ngroups; i++)
		groups[i] = (gid_t)int_groups[i];
#else
	(void) getgrouplist(name, basegid, groups, &ngroups);
#endif
	*angroups = ngroups;
	return (groups);
}

/*
 * For the various debug describe ops: decode bits in a bit-field-y
 * value.  For example, we might produce:
 *     value=0x3c[FOO,BAR,QUUX,?0x20]
 * when FOO is bit 0x10, BAR is 0x08, and QUUX is 0x04 (as defined
 * by the table).  This leaves 0x20 (bit 5) as a mystery, while bits
 * 4, 3, and 2 were decoded.  (Bits 0 and 1 were 0 on input hence
 * were not attempted here.)
 *
 * For general use we take a uint64_t <value>.  The bit description
 * table <db> is an array of {mask, match, str} values ending with
 * {0, 0, NULL}.
 *
 * If <str> is non-NULL we'll print it and the mask as well (if
 * str is NULL we'll print neither).  The mask is always printed in
 * hex at the moment.  See undec description too.
 *
 * For convenience, you can use a mask-and-match value, e.g., to
 * decode a 2-bit field in bits 0 and 1 you can mask against 3 and
 * match the values 0, 1, 2, and 3.  To handle this, make sure that
 * all masks-with-same-match are sequential.
 *
 * If there are any nonzero undecoded bits, print them after
 * all the decode-able bits have been handled.
 *
 * The <oc> argument defines the open and close bracket characters,
 * typically "[]", that surround the entire string.  If NULL, no
 * brackets are added, else oc[0] goes in the front and oc[1] at
 * the end, after printing any <str><value> part.
 *
 * Returns true if it printed anything (other than the implied
 * str-and-value, that is).
 */
static bool
l9p_describe_bits(const char *str, uint64_t value, const char *oc,
    const struct descbits *db, struct sbuf *sb)
{
	char *sep;
	char bracketbuf[2] = "";
	bool printed = false;

	if (str != NULL)
		sbuf_printf(sb, "%s0x%" PRIx64, str, value);

	if (oc != NULL)
		bracketbuf[0] = oc[0];
	sep = bracketbuf;
	for (; db->db_name != NULL; db++) {
		if ((value & db->db_mask) == db->db_match) {
			sbuf_printf(sb, "%s%s", sep, db->db_name);
			sep = (char *)",";
			printed = true;

			/*
			 * Clear the field, and make sure we
			 * won't match a zero-valued field with
			 * this same mask.
			 */
			value &= ~db->db_mask;
			while (db[1].db_mask == db->db_mask &&
			    db[1].db_name != NULL)
				db++;
		}
	}
	if (value != 0) {
		sbuf_printf(sb, "%s?0x%" PRIx64, sep, value);
		printed = true;
	}
	if (printed && oc != NULL) {
		bracketbuf[0] = oc[1];
		sbuf_cat(sb, bracketbuf);
	}
	return (printed);
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
 * Show file mode (O_RDWR, O_RDONLY, etc).  The argument is
 * an l9p_omode, not a Linux flags mode.  Linux flags are
 * decoded with l9p_describe_lflags.
 */
static void
l9p_describe_mode(const char *str, uint32_t mode, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		{ L9P_OACCMODE,	L9P_OREAD,	"OREAD" },
		{ L9P_OACCMODE,	L9P_OWRITE,	"OWRITE" },
		{ L9P_OACCMODE,	L9P_ORDWR,	"ORDWR" },
		{ L9P_OACCMODE,	L9P_OEXEC,	"OEXEC" },

		{ L9P_OCEXEC,	L9P_OCEXEC,	"OCEXEC" },
		{ L9P_ODIRECT,	L9P_ODIRECT,	"ODIRECT" },
		{ L9P_ORCLOSE,	L9P_ORCLOSE,	"ORCLOSE" },
		{ L9P_OTRUNC,	L9P_OTRUNC,	"OTRUNC" },
		{ 0, 0, NULL }
	};

	(void) l9p_describe_bits(str, mode, "[]", bits, sb);
}

/*
 * Show Linux mode/flags.
 */
static void
l9p_describe_lflags(const char *str, uint32_t flags, struct sbuf *sb)
{
	static const struct descbits bits[] = {
	    { L9P_OACCMODE,	L9P_OREAD,		"O_READ" },
	    { L9P_OACCMODE,	L9P_OWRITE,		"O_WRITE" },
	    { L9P_OACCMODE,	L9P_ORDWR,		"O_RDWR" },
	    { L9P_OACCMODE,	L9P_OEXEC,		"O_EXEC" },

	    { L9P_L_O_APPEND,	L9P_L_O_APPEND,		"O_APPEND" },
	    { L9P_L_O_CLOEXEC,	L9P_L_O_CLOEXEC,	"O_CLOEXEC" },
	    { L9P_L_O_CREAT,	L9P_L_O_CREAT,		"O_CREAT" },
	    { L9P_L_O_DIRECT,	L9P_L_O_DIRECT,		"O_DIRECT" },
	    { L9P_L_O_DIRECTORY, L9P_L_O_DIRECTORY,	"O_DIRECTORY" },
	    { L9P_L_O_DSYNC,	L9P_L_O_DSYNC,		"O_DSYNC" },
	    { L9P_L_O_EXCL,	L9P_L_O_EXCL,		"O_EXCL" },
	    { L9P_L_O_FASYNC,	L9P_L_O_FASYNC,		"O_FASYNC" },
	    { L9P_L_O_LARGEFILE, L9P_L_O_LARGEFILE,	"O_LARGEFILE" },
	    { L9P_L_O_NOATIME,	L9P_L_O_NOATIME,	"O_NOATIME" },
	    { L9P_L_O_NOCTTY,	L9P_L_O_NOCTTY,		"O_NOCTTY" },
	    { L9P_L_O_NOFOLLOW,	L9P_L_O_NOFOLLOW,	"O_NOFOLLOW" },
	    { L9P_L_O_NONBLOCK,	L9P_L_O_NONBLOCK,	"O_NONBLOCK" },
	    { L9P_L_O_PATH,	L9P_L_O_PATH,		"O_PATH" },
	    { L9P_L_O_SYNC,	L9P_L_O_SYNC,		"O_SYNC" },
	    { L9P_L_O_TMPFILE,	L9P_L_O_TMPFILE,	"O_TMPFILE" },
	    { L9P_L_O_TMPFILE,	L9P_L_O_TMPFILE,	"O_TMPFILE" },
	    { L9P_L_O_TRUNC,	L9P_L_O_TRUNC,		"O_TRUNC" },
	    { 0, 0, NULL }
	};

	(void) l9p_describe_bits(str, flags, "[]", bits, sb);
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
 * Show permissions (rwx etc).  Prints the value in hex only if
 * the rwx bits do not cover the entire value.
 */
static void
l9p_describe_perm(const char *str, uint32_t mode, struct sbuf *sb)
{
	char pbuf[12];

	strmode(mode & 0777, pbuf);
	if ((mode & ~(uint32_t)0777) != 0)
		sbuf_printf(sb, "%s0x%" PRIx32 "<%.9s>", str, mode, pbuf + 1);
	else
		sbuf_printf(sb, "%s<%.9s>", str, pbuf + 1);
}

/*
 * Show "extended" permissions: regular permissions, but also the
 * various DM* extension bits from 9P2000.u.
 */
static void
l9p_describe_ext_perm(const char *str, uint32_t mode, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		{ L9P_DMDIR,	L9P_DMDIR,	"DMDIR" },
		{ L9P_DMAPPEND,	L9P_DMAPPEND,	"DMAPPEND" },
		{ L9P_DMEXCL,	L9P_DMEXCL,	"DMEXCL" },
		{ L9P_DMMOUNT,	L9P_DMMOUNT,	"DMMOUNT" },
		{ L9P_DMAUTH,	L9P_DMAUTH,	"DMAUTH" },
		{ L9P_DMTMP,	L9P_DMTMP,	"DMTMP" },
		{ L9P_DMSYMLINK, L9P_DMSYMLINK,	"DMSYMLINK" },
		{ L9P_DMDEVICE,	L9P_DMDEVICE,	"DMDEVICE" },
		{ L9P_DMNAMEDPIPE, L9P_DMNAMEDPIPE, "DMNAMEDPIPE" },
		{ L9P_DMSOCKET,	L9P_DMSOCKET,	"DMSOCKET" },
		{ L9P_DMSETUID,	L9P_DMSETUID,	"DMSETUID" },
		{ L9P_DMSETGID,	L9P_DMSETGID,	"DMSETGID" },
		{ 0, 0, NULL }
	};
	bool need_sep;

	sbuf_printf(sb, "%s[", str);
	need_sep = l9p_describe_bits(NULL, mode & ~(uint32_t)0777, NULL,
	    bits, sb);
	l9p_describe_perm(need_sep ? "," : "", mode & 0777, sb);
	sbuf_cat(sb, "]");
}

/*
 * Show Linux-specific permissions: regular permissions, but also
 * the S_IFMT field.
 */
static void
l9p_describe_lperm(const char *str, uint32_t mode, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		{ S_IFMT,	S_IFIFO,	"S_IFIFO" },
		{ S_IFMT,	S_IFCHR,	"S_IFCHR" },
		{ S_IFMT,	S_IFDIR,	"S_IFDIR" },
		{ S_IFMT,	S_IFBLK,	"S_IFBLK" },
		{ S_IFMT,	S_IFREG,	"S_IFREG" },
		{ S_IFMT,	S_IFLNK,	"S_IFLNK" },
		{ S_IFMT,	S_IFSOCK,	"S_IFSOCK" },
		{ 0, 0, NULL }
	};
	bool need_sep;

	sbuf_printf(sb, "%s[", str);
	need_sep = l9p_describe_bits(NULL, mode & ~(uint32_t)0777, NULL,
	    bits, sb);
	l9p_describe_perm(need_sep ? "," : "", mode & 0777, sb);
	sbuf_cat(sb, "]");
}

/*
 * Show qid (<type, version, path> tuple).
 */
static void
l9p_describe_qid(const char *str, struct l9p_qid *qid, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		/*
		 * NB: L9P_QTFILE is 0, i.e., is implied by no
		 * other bits being set.  We get this produced
		 * when we mask against 0xff and compare for
		 * L9P_QTFILE, but we must do it first so that
		 * we mask against the original (not-adjusted)
		 * value.
		 */
		{ 0xff,		L9P_QTFILE,	"FILE" },
		{ L9P_QTDIR,	L9P_QTDIR,	"DIR" },
		{ L9P_QTAPPEND,	L9P_QTAPPEND,	"APPEND" },
		{ L9P_QTEXCL,	L9P_QTEXCL,	"EXCL" },
		{ L9P_QTMOUNT,	L9P_QTMOUNT,	"MOUNT" },
		{ L9P_QTAUTH,	L9P_QTAUTH,	"AUTH" },
		{ L9P_QTTMP,	L9P_QTTMP,	"TMP" },
		{ L9P_QTSYMLINK, L9P_QTSYMLINK,	"SYMLINK" },
		{ 0, 0, NULL }
	};

	assert(qid != NULL);

	sbuf_cat(sb, str);
	(void) l9p_describe_bits("<", qid->type, "[]", bits, sb);
	sbuf_printf(sb, ",%" PRIu32 ",0x%016" PRIx64 ">",
	    qid->version, qid->path);
}

/*
 * Show size.
 */
static void
l9p_describe_size(const char *str, uint64_t size, struct sbuf *sb)
{

	sbuf_printf(sb, "%s%" PRIu64, str, size);
}

/*
 * Show l9stat (including 9P2000.u extensions if appropriate).
 */
static void
l9p_describe_l9stat(const char *str, struct l9p_stat *st,
    enum l9p_version version, struct sbuf *sb)
{
	bool dotu = version >= L9P_2000U;

	assert(st != NULL);

	sbuf_printf(sb, "%stype=0x%04" PRIx32 " dev=0x%08" PRIx32, str,
	    st->type, st->dev);
	l9p_describe_qid(" qid=", &st->qid, sb);
	l9p_describe_ext_perm(" mode=", st->mode, sb);
	if (st->atime != (uint32_t)-1)
		sbuf_printf(sb, " atime=%" PRIu32, st->atime);
	if (st->mtime != (uint32_t)-1)
		sbuf_printf(sb, " mtime=%" PRIu32, st->mtime);
	if (st->length != (uint64_t)-1)
		sbuf_printf(sb, " length=%" PRIu64, st->length);
	l9p_describe_name(" name=", st->name, sb);
	/*
	 * It's pretty common to have NULL name+gid+muid.  They're
	 * just noise if NULL *and* dot-u; decode only if non-null
	 * or not-dot-u.
	 */
	if (st->uid != NULL || !dotu)
		l9p_describe_name(" uid=", st->uid, sb);
	if (st->gid != NULL || !dotu)
		l9p_describe_name(" gid=", st->gid, sb);
	if (st->muid != NULL || !dotu)
		l9p_describe_name(" muid=", st->muid, sb);
	if (dotu) {
		if (st->extension != NULL)
			l9p_describe_name(" extension=", st->extension, sb);
		sbuf_printf(sb,
		    " n_uid=%" PRIu32 " n_gid=%" PRIu32 " n_muid=%" PRIu32,
		    st->n_uid, st->n_gid, st->n_muid);
	}
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
		sbuf_printf(sb, "%" PRIu64 ".<invalid nsec %" PRIu64 ">)",
		    sec, nsec);
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

/*
 * Decode Tgetattr request_mask field.
 */
static void
l9p_describe_getattr_mask(uint64_t request_mask, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		/*
		 * Note: ALL and BASIC must occur first and second.
		 * This is a little dirty: it depends on the way the
		 * describe_bits code clears the values.  If we
		 * match ALL, we clear all those bits and do not
		 * match BASIC; if we match BASIC, we clear all
		 * those bits and do not match individual bits.  Thus
		 * if we have BASIC but not all the additional bits,
		 * we'll see, e.g., [BASIC,BTIME,GEN]; if we have
		 * all the additional bits too, we'll see [ALL].
		 *
		 * Since <undec> is true below, we'll also spot any
		 * bits added to the protocol since we made this table.
		 */
		{ L9PL_GETATTR_ALL,	L9PL_GETATTR_ALL,	"ALL" },
		{ L9PL_GETATTR_BASIC,	L9PL_GETATTR_BASIC,	"BASIC" },

		/* individual bits in BASIC */
		{ L9PL_GETATTR_MODE,	L9PL_GETATTR_MODE,	"MODE" },
		{ L9PL_GETATTR_NLINK,	L9PL_GETATTR_NLINK,	"NLINK" },
		{ L9PL_GETATTR_UID,	L9PL_GETATTR_UID,	"UID" },
		{ L9PL_GETATTR_GID,	L9PL_GETATTR_GID,	"GID" },
		{ L9PL_GETATTR_RDEV,	L9PL_GETATTR_RDEV,	"RDEV" },
		{ L9PL_GETATTR_ATIME,	L9PL_GETATTR_ATIME,	"ATIME" },
		{ L9PL_GETATTR_MTIME,	L9PL_GETATTR_MTIME,	"MTIME" },
		{ L9PL_GETATTR_CTIME,	L9PL_GETATTR_CTIME,	"CTIME" },
		{ L9PL_GETATTR_INO,	L9PL_GETATTR_INO,	"INO" },
		{ L9PL_GETATTR_SIZE,	L9PL_GETATTR_SIZE,	"SIZE" },
		{ L9PL_GETATTR_BLOCKS,	L9PL_GETATTR_BLOCKS,	"BLOCKS" },

		/* additional bits in ALL */
		{ L9PL_GETATTR_BTIME,	L9PL_GETATTR_BTIME,	"BTIME" },
		{ L9PL_GETATTR_GEN,	L9PL_GETATTR_GEN,	"GEN" },
		{ L9PL_GETATTR_DATA_VERSION, L9PL_GETATTR_DATA_VERSION,
							"DATA_VERSION" },
		{ 0, 0, NULL }
	};

	(void) l9p_describe_bits(" request_mask=", request_mask, "[]", bits,
	    sb);
}

/*
 * Decode Tunlinkat flags.
 */
static void
l9p_describe_unlinkat_flags(const char *str, uint32_t flags, struct sbuf *sb)
{
	static const struct descbits bits[] = {
		{ L9PL_AT_REMOVEDIR, L9PL_AT_REMOVEDIR, "AT_REMOVEDIR" },
		{ 0, 0, NULL }
	};

	(void) l9p_describe_bits(str, flags, "[]", bits, sb);
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
		const char *rr;

		/*
		 * Can't say for sure that this distinction --
		 * an even number is a request, an odd one is
		 * a response -- will be maintained forever,
		 * but it's good enough for now.
		 */
		rr = (type & 1) != 0 ? "response" : "request";
		sbuf_printf(sb, "<unknown %s %d> tag=%d", rr, type,
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
		if (fcall->twalk.nwname) {
			sbuf_cat(sb, " wname=\"");
			for (i = 0; i < fcall->twalk.nwname; i++)
				sbuf_printf(sb, "%s%s", i == 0 ? "" : "/",
				    fcall->twalk.wname[i]);
			sbuf_cat(sb, "\"");
		}
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
		l9p_describe_ext_perm(" perm=", fcall->tcreate.perm, sb);
		l9p_describe_mode(" mode=", fcall->tcreate.mode, sb);
		if (version >= L9P_2000U && fcall->tcreate.extension != NULL)
			l9p_describe_name(" extension=",
			    fcall->tcreate.extension, sb);
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
		l9p_describe_l9stat(" ", &fcall->rstat.stat, version, sb);
		return;

	case L9P_TWSTAT:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_l9stat(" ", &fcall->twstat.stat, version, sb);
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
		l9p_describe_lflags(" flags=", fcall->tlcreate.flags, sb);
		return;

	case L9P_RLOPEN:
		l9p_describe_qid(" qid=", &fcall->rlopen.qid, sb);
		sbuf_printf(sb, " iounit=%d", fcall->rlopen.iounit);
		return;

	case L9P_TLCREATE:
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		l9p_describe_name(" name=", fcall->tlcreate.name, sb);
		/* confusing: "flags" is open-mode, "mode" is permissions */
		l9p_describe_lflags(" flags=", fcall->tlcreate.flags, sb);
		/* TLCREATE mode/permissions have S_IFREG (0x8000) set */
		l9p_describe_lperm(" mode=", fcall->tlcreate.mode, sb);
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
		/*
		 * TMKNOD mode/permissions have S_IFBLK/S_IFCHR/S_IFIFO
		 * bits.  The major and minor values are only meaningful
		 * for S_IFBLK and S_IFCHR, but just decode always here.
		 */
		l9p_describe_lperm(" mode=", fcall->tmknod.mode, sb);
		sbuf_printf(sb, " major=%u minor=%u",
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
		l9p_describe_getattr_mask(fcall->tgetattr.request_mask, sb);
		return;

	case L9P_RGETATTR:
		/* Don't need to decode bits: they're implied by the output */
		mask = fcall->rgetattr.valid;
		sbuf_printf(sb, " valid=0x%016" PRIx64, mask);
		l9p_describe_qid(" qid=", &fcall->rgetattr.qid, sb);
		if (mask & L9PL_GETATTR_MODE)
			l9p_describe_lperm(" mode=", fcall->rgetattr.mode, sb);
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
		/* As with RGETATTR, we'll imply decode via output. */
		l9p_describe_fid(" fid=", fcall->hdr.fid, sb);
		mask = fcall->tsetattr.valid;
		/* NB: tsetattr valid mask is only 32 bits, hence %08x */
		sbuf_printf(sb, " valid=0x%08" PRIx64, mask);
		if (mask & L9PL_SETATTR_MODE)
			l9p_describe_lperm(" mode=", fcall->tsetattr.mode, sb);
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
				sbuf_cat(sb, " atime=now");
		}
		if (mask & L9PL_SETATTR_MTIME) {
			if (mask & L9PL_SETATTR_MTIME_SET)
				l9p_describe_time(sb, " mtime=",
				    fcall->tsetattr.mtime_sec,
				    fcall->tsetattr.mtime_nsec);
			else
				sbuf_cat(sb, " mtime=now");
		}
		if (mask & L9PL_SETATTR_CTIME)
			sbuf_cat(sb, " ctime=now");
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
		/* TMKDIR mode/permissions have S_IFDIR set */
		l9p_describe_lperm(" mode=", fcall->tmkdir.mode, sb);
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
		l9p_describe_unlinkat_flags(" flags=",
		    fcall->tunlinkat.flags, sb);
		return;

	case L9P_RUNLINKAT:
		return;

	default:
		sbuf_printf(sb, " <missing case in %s()>", __func__);
	}
}
