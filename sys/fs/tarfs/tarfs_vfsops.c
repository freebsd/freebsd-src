/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 Juniper Networks, Inc.
 * Copyright (c) 2022-2024 Klara, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

#include "opt_tarfs.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/fcntl.h>
#include <sys/libkern.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <vm/vm_param.h>

#include <geom/geom.h>
#include <geom/geom_vfs.h>

#include <fs/tarfs/tarfs.h>
#include <fs/tarfs/tarfs_dbg.h>

CTASSERT(ZERO_REGION_SIZE >= TARFS_BLOCKSIZE);

struct ustar_header {
	char	name[100];		/* File name */
	char	mode[8];		/* Mode flags */
	char	uid[8];			/* User id */
	char	gid[8];			/* Group id */
	char	size[12];		/* Size */
	char	mtime[12];		/* Modified time */
	char	checksum[8];		/* Checksum */
	char	typeflag[1];		/* Type */
	char	linkname[100];		/* "old format" stops here */
	char	magic[6];		/* POSIX UStar "ustar\0" indicator */
	char	version[2];		/* POSIX UStar version "00" */
	char	uname[32];		/* User name */
	char	gname[32];		/* Group name */
	char	major[8];		/* Device major number */
	char	minor[8];		/* Device minor number */
	char	prefix[155];		/* Path prefix */
	char	_pad[12];
};

CTASSERT(sizeof(struct ustar_header) == TARFS_BLOCKSIZE);

#define	TAR_EOF			((off_t)-1)

#define	TAR_TYPE_FILE		'0'
#define	TAR_TYPE_HARDLINK	'1'
#define	TAR_TYPE_SYMLINK	'2'
#define	TAR_TYPE_CHAR		'3'
#define	TAR_TYPE_BLOCK		'4'
#define	TAR_TYPE_DIRECTORY	'5'
#define	TAR_TYPE_FIFO		'6'
#define	TAR_TYPE_CONTIG		'7'
#define	TAR_TYPE_GLOBAL_EXTHDR	'g'
#define	TAR_TYPE_EXTHDR		'x'
#define	TAR_TYPE_GNU_SPARSE	'S'

#define	USTAR_MAGIC		(uint8_t []){ 'u', 's', 't', 'a', 'r', 0 }
#define	USTAR_VERSION		(uint8_t []){ '0', '0' }
#define	GNUTAR_MAGIC		(uint8_t []){ 'u', 's', 't', 'a', 'r', ' ' }
#define	GNUTAR_VERSION		(uint8_t []){ ' ', '\x0' }

#define	DEFDIRMODE	(S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)

MALLOC_DEFINE(M_TARFSMNT, "tarfs mount", "tarfs mount structures");
MALLOC_DEFINE(M_TARFSNODE, "tarfs node", "tarfs node structures");

static vfs_mount_t	tarfs_mount;
static vfs_unmount_t	tarfs_unmount;
static vfs_root_t	tarfs_root;
static vfs_statfs_t	tarfs_statfs;
static vfs_fhtovp_t	tarfs_fhtovp;

static const char *tarfs_opts[] = {
	"as", "from", "gid", "mode", "uid", "verify",
	NULL
};

/*
 * Reads a len-width signed octal number from strp.  Returns 0 on success
 * and non-zero on error.
 */
static int
tarfs_str2octal(const char *strp, size_t len, int64_t *num)
{
	int64_t val;
	size_t idx;
	int sign;

	idx = 0;
	if (strp[idx] == '-') {
		sign = -1;
		idx++;
	} else {
		sign = 1;
	}

	val = 0;
	for (; idx < len && strp[idx] != '\0' && strp[idx] != ' '; idx++) {
		if (strp[idx] < '0' || strp[idx] > '7')
			return (EINVAL);
		val <<= 3;
		val += strp[idx] - '0';
		if (val > INT64_MAX / 8)
			return (ERANGE);
	}

	*num = val * sign;
	return (0);
}

/*
 * Reads a len-byte extended numeric value from strp.  The first byte has
 * bit 7 set to indicate the format; the remaining 7 bits + the (len - 1)
 * bytes that follow form a big-endian signed two's complement binary
 * number.  Returns 0 on success and non-zero on error;
 */
static int
tarfs_str2base256(const char *strp, size_t len, int64_t *num)
{
	int64_t val;
	size_t idx;

	KASSERT(strp[0] & 0x80, ("not an extended numeric value"));

	/* Sign-extend the first byte */
	if ((strp[0] & 0x40) != 0)
		val = (int64_t)-1;
	else
		val = 0;
	val <<= 6;
	val |= (strp[0] & 0x3f);

	/* Read subsequent bytes */
	for (idx = 1; idx < len; idx++) {
		val <<= 8;
		val |= (0xff & (int64_t)strp[idx]);
		if (val > INT64_MAX / 256 || val < INT64_MIN / 256)
			return (ERANGE);
	}

	*num = val;
	return (0);
}

/*
 * Read a len-byte numeric field from strp.  If bit 7 of the first byte it
 * set, assume an extended numeric value (signed two's complement);
 * otherwise, assume a signed octal value.
 */
static int
tarfs_str2int64(const char *strp, size_t len, int64_t *num)
{
	if (len < 1)
		return (EINVAL);
	if ((strp[0] & 0x80) != 0)
		return (tarfs_str2base256(strp, len, num));
	return (tarfs_str2octal(strp, len, num));
}

/*
 * Verifies the checksum of a header.  Returns true if the checksum is
 * valid, false otherwise.
 */
static boolean_t
tarfs_checksum(struct ustar_header *hdrp)
{
	const unsigned char *ptr;
	int64_t checksum, hdrsum;

	if (tarfs_str2int64(hdrp->checksum, sizeof(hdrp->checksum), &hdrsum) != 0) {
		TARFS_DPF(CHECKSUM, "%s: invalid header checksum \"%.*s\"\n",
		    __func__, (int)sizeof(hdrp->checksum), hdrp->checksum);
		return (false);
	}
	TARFS_DPF(CHECKSUM, "%s: header checksum \"%.*s\" = %#lo\n", __func__,
	    (int)sizeof(hdrp->checksum), hdrp->checksum, hdrsum);

	checksum = 0;
	for (ptr = (const unsigned char *)hdrp;
	     ptr < (const unsigned char *)hdrp->checksum; ptr++)
		checksum += *ptr;
	for (;
	     ptr < (const unsigned char *)hdrp->typeflag; ptr++)
		checksum += 0x20;
	for (;
	     ptr < (const unsigned char *)(hdrp + 1); ptr++)
		checksum += *ptr;
	TARFS_DPF(CHECKSUM, "%s: calc unsigned checksum %#lo\n", __func__,
	    checksum);
	if (hdrsum == checksum)
		return (true);

	/*
	 * Repeat test with signed bytes, some older formats use a broken
	 * form of the calculation
	 */
	checksum = 0;
	for (ptr = (const unsigned char *)hdrp;
	     ptr < (const unsigned char *)&hdrp->checksum; ptr++)
		checksum += *((const signed char *)ptr);
	for (;
	     ptr < (const unsigned char *)&hdrp->typeflag; ptr++)
		checksum += 0x20;
	for (;
	     ptr < (const unsigned char *)(hdrp + 1); ptr++)
		checksum += *((const signed char *)ptr);
	TARFS_DPF(CHECKSUM, "%s: calc signed checksum %#lo\n", __func__,
	    checksum);
	if (hdrsum == checksum)
		return (true);

	return (false);
}


/*
 * Looks up a path in the tarfs node tree.
 *
 * - If the path exists, stores a pointer to the corresponding tarfs_node
 *   in retnode and a pointer to its parent in retparent.
 *
 * - If the path does not exist, but create_dirs is true, creates ancestor
 *   directories and returns NULL in retnode and the parent in retparent.
 *
 * - If the path does not exist and create_dirs is false, stops at the
 *   first missing path name component.
 *
 * - In all cases, on return, endp and sepp point to the beginning and
 *   end, respectively, of the last-processed path name component.
 *
 * - Returns 0 if the node was found, ENOENT if it was not, and some other
 *   positive errno value on failure.
 */
static int
tarfs_lookup_path(struct tarfs_mount *tmp, char *name, size_t namelen,
    char **endp, char **sepp, struct tarfs_node **retparent,
    struct tarfs_node **retnode, boolean_t create_dirs)
{
	struct componentname cn = { };
	struct tarfs_node *parent, *tnp;
	char *sep;
	size_t len;
	int error;
	boolean_t do_lookup;

	MPASS(name != NULL && namelen != 0);

	do_lookup = true;
	error = 0;
	parent = tnp = tmp->root;
	if (tnp == NULL)
		panic("%s: root node not yet created", __func__);

	TARFS_DPF(LOOKUP, "%s: full path: %.*s\n", __func__,
	    (int)namelen, name);

	sep = NULL;
	for (;;) {
		/* skip leading slash(es) */
		while (name[0] == '/' && namelen > 0)
			name++, namelen--;

		/* did we reach the end? */
		if (namelen == 0 || name[0] == '\0') {
			name = do_lookup ? NULL : cn.cn_nameptr;
			namelen = do_lookup ? 0 : cn.cn_namelen;
			break;
		}

		/* we're not at the end, so we must be in a directory */
		if (tnp != NULL && tnp->type != VDIR) {
			TARFS_DPF(LOOKUP, "%s: %.*s is not a directory\n", __func__,
			    (int)tnp->namelen, tnp->name);
			error = ENOTDIR;
			break;
		}

		/* locate the next separator */
		for (sep = name, len = 0;
		     *sep != '\0' && *sep != '/' && len < namelen;
		     sep++, len++)
			/* nothing */ ;

		/* check for . and .. */
		if (name[0] == '.' && len == 1) {
			name += len;
			namelen -= len;
			continue;
		}
		if (name[0] == '.' && name[1] == '.' && len == 2) {
			if (tnp == tmp->root) {
				error = EINVAL;
				break;
			}
			tnp = parent;
			parent = tnp->parent;
			cn.cn_nameptr = tnp->name;
			cn.cn_namelen = tnp->namelen;
			do_lookup = true;
			TARFS_DPF(LOOKUP, "%s: back to %.*s/\n", __func__,
			    (int)tnp->namelen, tnp->name);
			name += len;
			namelen -= len;
			continue;
		}

		/* create parent if necessary */
		if (!do_lookup) {
			TARFS_DPF(ALLOC, "%s: creating %.*s\n", __func__,
			    (int)cn.cn_namelen, cn.cn_nameptr);
			error = tarfs_alloc_node(tmp, cn.cn_nameptr,
			    cn.cn_namelen, VDIR, -1, 0, tmp->mtime, 0, 0,
			    DEFDIRMODE, 0, NULL, NODEV, parent, &tnp);
			if (error != 0)
				break;
		}

		parent = tnp;
		tnp = NULL;
		cn.cn_nameptr = name;
		cn.cn_namelen = len;
		TARFS_DPF(LOOKUP, "%s: looking up %.*s in %.*s/\n", __func__,
		    (int)cn.cn_namelen, cn.cn_nameptr,
		    (int)parent->namelen, parent->name);
		if (do_lookup) {
			tnp = tarfs_lookup_node(parent, NULL, &cn);
			if (tnp == NULL) {
				do_lookup = false;
				if (!create_dirs) {
					error = ENOENT;
					break;
				}
			}
		}
		name += cn.cn_namelen;
		namelen -= cn.cn_namelen;
	}

	TARFS_DPF(LOOKUP, "%s: parent %p node %p\n", __func__, parent, tnp);

	if (retparent)
		*retparent = parent;
	if (retnode)
		*retnode = tnp;
	if (endp) {
		if (namelen > 0)
			*endp = name;
		else
			*endp = NULL;
	}
	if (sepp)
		*sepp = sep;
	return (error);
}

/*
 * Frees a tarfs_mount structure and everything it references.
 */
static void
tarfs_free_mount(struct tarfs_mount *tmp)
{
	struct mount *mp;
	struct tarfs_node *tnp, *tnp_next;

	MPASS(tmp != NULL);

	TARFS_DPF(ALLOC, "%s: Freeing mount structure %p\n", __func__, tmp);

	TARFS_DPF(ALLOC, "%s: freeing tarfs_node structures\n", __func__);
	TAILQ_FOREACH_SAFE(tnp, &tmp->allnodes, entries, tnp_next) {
		tarfs_free_node(tnp);
	}

	(void)tarfs_io_fini(tmp);

	TARFS_DPF(ALLOC, "%s: deleting unr header\n", __func__);
	delete_unrhdr(tmp->ino_unr);
	mp = tmp->vfs;
	mp->mnt_data = NULL;

	TARFS_DPF(ALLOC, "%s: freeing structure\n", __func__);
	free(tmp, M_TARFSMNT);
}

/*
 * Processes the tar file header at block offset blknump and allocates and
 * populates a tarfs_node structure for the file it describes.  Updated
 * blknump to point to the next unread tar file block, or TAR_EOF if EOF
 * is reached.  Returns 0 on success or EOF and a positive errno value on
 * failure.
 */
static int
tarfs_alloc_one(struct tarfs_mount *tmp, off_t *blknump)
{
	char block[TARFS_BLOCKSIZE];
	struct ustar_header *hdrp = (struct ustar_header *)block;
	struct sbuf *namebuf = NULL;
	char *exthdr = NULL, *name = NULL, *link = NULL;
	off_t blknum = *blknump;
	int64_t num;
	int endmarker = 0;
	char *namep, *sep;
	struct tarfs_node *parent, *tnp, *other;
	size_t namelen = 0, linklen = 0, realsize = 0, sz;
	ssize_t res;
	dev_t rdev;
	gid_t gid;
	mode_t mode;
	time_t mtime;
	uid_t uid;
	long major = -1, minor = -1;
	unsigned int flags = 0;
	int error;
	boolean_t sparse = false;

again:
	/* read next header */
	res = tarfs_io_read_buf(tmp, false, block,
	    TARFS_BLOCKSIZE * blknum, TARFS_BLOCKSIZE);
	if (res < 0) {
		error = -res;
		goto bad;
	} else if (res < TARFS_BLOCKSIZE) {
		goto eof;
	}
	blknum++;

	/* check for end marker */
	if (memcmp(block, zero_region, TARFS_BLOCKSIZE) == 0) {
		if (endmarker++) {
			if (exthdr != NULL) {
				TARFS_DPF(IO, "%s: orphaned extended header at %zu\n",
				    __func__, TARFS_BLOCKSIZE * (blknum - 1));
				free(exthdr, M_TEMP);
			}
			TARFS_DPF(IO, "%s: end of archive at %zu\n", __func__,
			    TARFS_BLOCKSIZE * blknum);
			tmp->nblocks = blknum;
			*blknump = TAR_EOF;
			return (0);
		}
		goto again;
	}

	/* verify magic */
	if (memcmp(hdrp->magic, USTAR_MAGIC, sizeof(USTAR_MAGIC)) == 0 &&
	    memcmp(hdrp->version, USTAR_VERSION, sizeof(USTAR_VERSION)) == 0) {
		/* POSIX */
	} else if (memcmp(hdrp->magic, GNUTAR_MAGIC, sizeof(GNUTAR_MAGIC)) == 0 &&
	    memcmp(hdrp->magic, GNUTAR_MAGIC, sizeof(GNUTAR_MAGIC)) == 0) {
		TARFS_DPF(ALLOC, "%s: GNU tar format at %zu\n", __func__,
		    TARFS_BLOCKSIZE * (blknum - 1));
		error = EFTYPE;
		goto bad;
	} else {
		TARFS_DPF(ALLOC, "%s: unsupported TAR format at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		error = EINVAL;
		goto bad;
	}

	/* verify checksum */
	if (!tarfs_checksum(hdrp)) {
		TARFS_DPF(ALLOC, "%s: header checksum failed at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		error = EINVAL;
		goto bad;
	}

	/* get standard attributes */
	if (tarfs_str2int64(hdrp->mode, sizeof(hdrp->mode), &num) != 0 ||
	    num < 0 || num > (S_IFMT|ALLPERMS)) {
		TARFS_DPF(ALLOC, "%s: invalid file mode at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		mode = S_IRUSR;
	} else {
		mode = num & ALLPERMS;
	}
	if (tarfs_str2int64(hdrp->uid, sizeof(hdrp->uid), &num) != 0 ||
	    num < 0 || num > UID_MAX) {
		TARFS_DPF(ALLOC, "%s: invalid UID at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		uid = tmp->root->uid;
		mode &= ~S_ISUID;
	} else {
		uid = num;
	}
	if (tarfs_str2int64(hdrp->gid, sizeof(hdrp->gid), &num) != 0 ||
	    num < 0 || num > GID_MAX) {
		TARFS_DPF(ALLOC, "%s: invalid GID at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		gid = tmp->root->gid;
		mode &= ~S_ISGID;
	} else {
		gid = num;
	}
	if (tarfs_str2int64(hdrp->size, sizeof(hdrp->size), &num) != 0 ||
	    num < 0) {
		TARFS_DPF(ALLOC, "%s: invalid size at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		error = EINVAL;
		goto bad;
	}
	sz = num;
	if (tarfs_str2int64(hdrp->mtime, sizeof(hdrp->mtime), &num) != 0) {
		TARFS_DPF(ALLOC, "%s: invalid modification time at %zu\n",
		    __func__, TARFS_BLOCKSIZE * (blknum - 1));
		error = EINVAL;
		goto bad;
	}
	mtime = num;
	rdev = NODEV;
	TARFS_DPF(ALLOC, "%s: [%c] %zu @%jd %o %d:%d\n", __func__,
	    hdrp->typeflag[0], sz, (intmax_t)mtime, mode, uid, gid);

	/* extended header? */
	if (hdrp->typeflag[0] == TAR_TYPE_GLOBAL_EXTHDR) {
		printf("%s: unsupported global extended header at %zu\n",
		    __func__, (size_t)(TARFS_BLOCKSIZE * (blknum - 1)));
		error = EFTYPE;
		goto bad;
	}
	if (hdrp->typeflag[0] == TAR_TYPE_EXTHDR) {
		if (exthdr != NULL) {
			TARFS_DPF(IO, "%s: multiple extended headers at %zu\n",
			    __func__, TARFS_BLOCKSIZE * (blknum - 1));
			error = EFTYPE;
			goto bad;
		}
		/* read the contents of the exthdr */
		TARFS_DPF(ALLOC, "%s: %zu-byte extended header at %zd\n",
		    __func__, sz, TARFS_BLOCKSIZE * (blknum - 1));
		exthdr = malloc(sz, M_TEMP, M_WAITOK);
		res = tarfs_io_read_buf(tmp, false, exthdr,
		    TARFS_BLOCKSIZE * blknum, sz);
		if (res < 0) {
			error = -res;
			goto bad;
		}
		if (res < sz) {
			goto eof;
		}
		blknum += TARFS_SZ2BLKS(res);
		/* XXX TODO: refactor this parser */
		char *line = exthdr;
		while (line < exthdr + sz) {
			char *eol, *key, *value, *sep;
			size_t len = strtoul(line, &sep, 10);
			if (len == 0 || sep == line || *sep != ' ') {
				TARFS_DPF(ALLOC, "%s: exthdr syntax error\n",
				    __func__);
				error = EINVAL;
				goto bad;
			}
			if ((uintptr_t)line + len < (uintptr_t)line ||
			    line + len > exthdr + sz) {
				TARFS_DPF(ALLOC, "%s: exthdr overflow\n",
				    __func__);
				error = EINVAL;
				goto bad;
			}
			eol = line + len - 1;
			*eol = '\0';
			line += len;
			key = sep + 1;
			sep = strchr(key, '=');
			if (sep == NULL) {
				TARFS_DPF(ALLOC, "%s: exthdr syntax error\n",
				    __func__);
				error = EINVAL;
				goto bad;
			}
			*sep = '\0';
			value = sep + 1;
			TARFS_DPF(ALLOC, "%s: exthdr %s=%s\n", __func__,
			    key, value);
			if (strcmp(key, "linkpath") == 0) {
				link = value;
				linklen = eol - value;
			} else if (strcmp(key, "GNU.sparse.major") == 0) {
				sparse = true;
				major = strtol(value, &sep, 10);
				if (sep != eol) {
					printf("exthdr syntax error\n");
					error = EINVAL;
					goto bad;
				}
			} else if (strcmp(key, "GNU.sparse.minor") == 0) {
				sparse = true;
				minor = strtol(value, &sep, 10);
				if (sep != eol) {
					printf("exthdr syntax error\n");
					error = EINVAL;
					goto bad;
				}
			} else if (strcmp(key, "GNU.sparse.name") == 0) {
				sparse = true;
				name = value;
				namelen = eol - value;
				if (namelen == 0) {
					printf("exthdr syntax error\n");
					error = EINVAL;
					goto bad;
				}
			} else if (strcmp(key, "GNU.sparse.realsize") == 0) {
				sparse = true;
				realsize = strtoul(value, &sep, 10);
				if (sep != eol) {
					printf("exthdr syntax error\n");
					error = EINVAL;
					goto bad;
				}
			} else if (strcmp(key, "SCHILY.fflags") == 0) {
				flags |= tarfs_strtofflags(value, &sep);
				if (sep != eol) {
					printf("exthdr syntax error\n");
					error = EINVAL;
					goto bad;
				}
			}
		}
		goto again;
	}

	/* sparse file consistency checks */
	if (sparse) {
		TARFS_DPF(ALLOC, "%s: %s: sparse %ld.%ld (%zu bytes)\n", __func__,
		    name, major, minor, realsize);
		if (major != 1 || minor != 0 || name == NULL || realsize == 0 ||
		    hdrp->typeflag[0] != TAR_TYPE_FILE) {
			TARFS_DPF(ALLOC, "%s: invalid sparse format\n", __func__);
			error = EINVAL;
			goto bad;
		}
	}

	/* file name */
	if (name == NULL) {
		if (hdrp->prefix[0] != '\0') {
			namebuf = sbuf_new_auto();
			sbuf_printf(namebuf, "%.*s/%.*s",
			    (int)sizeof(hdrp->prefix), hdrp->prefix,
			    (int)sizeof(hdrp->name), hdrp->name);
			sbuf_finish(namebuf);
			name = sbuf_data(namebuf);
			namelen = sbuf_len(namebuf);
		} else {
			name = hdrp->name;
			namelen = strnlen(hdrp->name, sizeof(hdrp->name));
		}
	}

	error = tarfs_lookup_path(tmp, name, namelen, &namep,
	    &sep, &parent, &tnp, true);
	if (error != 0) {
		TARFS_DPF(ALLOC, "%s: failed to look up %.*s\n", __func__,
		    (int)namelen, name);
		error = EINVAL;
		goto bad;
	}
	if (tnp != NULL) {
		if (hdrp->typeflag[0] == TAR_TYPE_DIRECTORY) {
			/* XXX set attributes? */
			goto skip;
		}
		TARFS_DPF(ALLOC, "%s: duplicate file %.*s\n", __func__,
		    (int)namelen, name);
		error = EINVAL;
		goto bad;
	}
	switch (hdrp->typeflag[0]) {
	case TAR_TYPE_DIRECTORY:
		error = tarfs_alloc_node(tmp, namep, sep - namep, VDIR,
		    0, 0, mtime, uid, gid, mode, flags, NULL, 0,
		    parent, &tnp);
		break;
	case TAR_TYPE_FILE:
		error = tarfs_alloc_node(tmp, namep, sep - namep, VREG,
		    blknum * TARFS_BLOCKSIZE, sz, mtime, uid, gid, mode,
		    flags, NULL, 0, parent, &tnp);
		if (error == 0 && sparse) {
			error = tarfs_load_blockmap(tnp, realsize);
		}
		break;
	case TAR_TYPE_HARDLINK:
		if (link == NULL) {
			link = hdrp->linkname;
			linklen = strnlen(link, sizeof(hdrp->linkname));
		}
		if (linklen == 0) {
			TARFS_DPF(ALLOC, "%s: %.*s: link without target\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		error = tarfs_lookup_path(tmp, link, linklen, NULL,
		    NULL, NULL, &other, false);
		if (error != 0 || other == NULL ||
		    other->type != VREG || other->other != NULL) {
			TARFS_DPF(ALLOC, "%s: %.*s: invalid link to %.*s\n",
			    __func__, (int)namelen, name, (int)linklen, link);
			error = EINVAL;
			goto bad;
		}
		error = tarfs_alloc_node(tmp, namep, sep - namep, VREG,
		    0, 0, 0, 0, 0, 0, 0, NULL, 0, parent, &tnp);
		if (error == 0) {
			tnp->other = other;
			tnp->other->nlink++;
		}
		break;
	case TAR_TYPE_SYMLINK:
		if (link == NULL) {
			link = hdrp->linkname;
			linklen = strnlen(link, sizeof(hdrp->linkname));
		}
		if (linklen == 0) {
			TARFS_DPF(ALLOC, "%s: %.*s: link without target\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		error = tarfs_alloc_node(tmp, namep, sep - namep, VLNK,
		    0, linklen, mtime, uid, gid, mode, flags, link, 0,
		    parent, &tnp);
		break;
	case TAR_TYPE_BLOCK:
		if (tarfs_str2int64(hdrp->major, sizeof(hdrp->major), &num) != 0 ||
		    num < 0 || num > INT_MAX) {
			TARFS_DPF(ALLOC, "%s: %.*s: invalid device major\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		major = num;
		if (tarfs_str2int64(hdrp->minor, sizeof(hdrp->minor), &num) != 0 ||
		    num < 0 || num > INT_MAX) {
			TARFS_DPF(ALLOC, "%s: %.*s: invalid device minor\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		minor = num;
		rdev = makedev(major, minor);
		error = tarfs_alloc_node(tmp, namep, sep - namep, VBLK,
		    0, 0, mtime, uid, gid, mode, flags, NULL, rdev,
		    parent, &tnp);
		break;
	case TAR_TYPE_CHAR:
		if (tarfs_str2int64(hdrp->major, sizeof(hdrp->major), &num) != 0 ||
		    num < 0 || num > INT_MAX) {
			TARFS_DPF(ALLOC, "%s: %.*s: invalid device major\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		major = num;
		if (tarfs_str2int64(hdrp->minor, sizeof(hdrp->minor), &num) != 0 ||
		    num < 0 || num > INT_MAX) {
			TARFS_DPF(ALLOC, "%s: %.*s: invalid device minor\n",
			    __func__, (int)namelen, name);
			error = EINVAL;
			goto bad;
		}
		minor = num;
		rdev = makedev(major, minor);
		error = tarfs_alloc_node(tmp, namep, sep - namep, VCHR,
		    0, 0, mtime, uid, gid, mode, flags, NULL, rdev,
		    parent, &tnp);
		break;
	default:
		TARFS_DPF(ALLOC, "%s: unsupported type %c for %.*s\n",
		    __func__, hdrp->typeflag[0], (int)namelen, name);
		error = EINVAL;
		break;
	}
	if (error != 0)
		goto bad;

skip:
	blknum += TARFS_SZ2BLKS(sz);
	tmp->nblocks = blknum;
	*blknump = blknum;
	if (exthdr != NULL) {
		free(exthdr, M_TEMP);
	}
	if (namebuf != NULL) {
		sbuf_delete(namebuf);
	}
	return (0);
eof:
	TARFS_DPF(IO, "%s: premature end of file\n", __func__);
	error = EIO;
	goto bad;
bad:
	if (exthdr != NULL) {
		free(exthdr, M_TEMP);
	}
	if (namebuf != NULL) {
		sbuf_delete(namebuf);
	}
	return (error);
}

/*
 * Allocates and populates the metadata structures for the tar file
 * referenced by vp.  On success, a pointer to the tarfs_mount structure
 * is stored in tmpp.  Returns 0 on success or a positive errno value on
 * failure.
 */
static int
tarfs_alloc_mount(struct mount *mp, struct vnode *vp,
    uid_t root_uid, gid_t root_gid, mode_t root_mode,
    struct tarfs_mount **tmpp)
{
	struct vattr va;
	struct thread *td = curthread;
	struct tarfs_mount *tmp;
	struct tarfs_node *root;
	off_t blknum;
	time_t mtime;
	int error;

	KASSERT(tmpp != NULL, ("tarfs mount return is NULL"));
	ASSERT_VOP_LOCKED(vp, __func__);

	tmp = NULL;

	TARFS_DPF(ALLOC, "%s: Allocating tarfs mount structure for vp %p\n",
	    __func__, vp);

	/* Get source metadata */
	error = VOP_GETATTR(vp, &va, td->td_ucred);
	if (error != 0) {
		return (error);
	}
	VOP_UNLOCK(vp);
	mtime = va.va_mtime.tv_sec;

	/* Allocate and initialize tarfs mount structure */
	tmp = malloc(sizeof(*tmp), M_TARFSMNT, M_WAITOK | M_ZERO);
	TARFS_DPF(ALLOC, "%s: Allocated mount structure\n", __func__);
	mp->mnt_data = tmp;

	mtx_init(&tmp->allnode_lock, "tarfs allnode lock", NULL,
	    MTX_DEF);
	TAILQ_INIT(&tmp->allnodes);
	tmp->ino_unr = new_unrhdr(TARFS_MININO, INT_MAX, &tmp->allnode_lock);
	tmp->vp = vp;
	tmp->vfs = mp;
	tmp->mtime = mtime;

	/* Initialize I/O layer */
	tmp->iosize = 1U << tarfs_ioshift;
	error = tarfs_io_init(tmp);
	if (error != 0)
		goto bad;

	error = tarfs_alloc_node(tmp, NULL, 0, VDIR, 0, 0, mtime, root_uid,
	    root_gid, root_mode & ALLPERMS, 0, NULL, NODEV, NULL, &root);
	if (error != 0 || root == NULL)
		goto bad;
	tmp->root = root;

	blknum = 0;
	do {
		if ((error = tarfs_alloc_one(tmp, &blknum)) != 0) {
			goto bad;
		}
	} while (blknum != TAR_EOF);

	*tmpp = tmp;

	TARFS_DPF(ALLOC, "%s: pfsmnt_root %p\n", __func__, tmp->root);
	return (0);

bad:
	tarfs_free_mount(tmp);
	return (error);
}

/*
 * VFS Operations.
 */

static int
tarfs_mount(struct mount *mp)
{
	struct nameidata nd;
	struct vattr va;
	struct tarfs_mount *tmp = NULL;
	struct thread *td = curthread;
	struct vnode *vp;
	char *as, *from;
	uid_t root_uid;
	gid_t root_gid;
	mode_t root_mode;
	int error, flags, aslen, len;

	if (mp->mnt_flag & MNT_UPDATE)
		return (EOPNOTSUPP);

	if (vfs_filteropt(mp->mnt_optnew, tarfs_opts))
		return (EINVAL);

	vn_lock(mp->mnt_vnodecovered, LK_SHARED | LK_RETRY);
	error = VOP_GETATTR(mp->mnt_vnodecovered, &va, mp->mnt_cred);
	VOP_UNLOCK(mp->mnt_vnodecovered);
	if (error)
		return (error);

	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "gid", "%d", &root_gid) != 1)
		root_gid = va.va_gid;
	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "uid", "%d", &root_uid) != 1)
		root_uid = va.va_uid;
	if (mp->mnt_cred->cr_ruid != 0 ||
	    vfs_scanopt(mp->mnt_optnew, "mode", "%ho", &root_mode) != 1)
		root_mode = va.va_mode;

	error = vfs_getopt(mp->mnt_optnew, "from", (void **)&from, &len);
	if (error != 0 || from[len - 1] != '\0')
		return (EINVAL);
	error = vfs_getopt(mp->mnt_optnew, "as", (void **)&as, &aslen);
	if (error != 0 || as[aslen - 1] != '\0')
		as = from;

	/* Find the source tarball */
	TARFS_DPF(FS, "%s(%s%s%s, uid=%u, gid=%u, mode=%o)\n", __func__,
	    from, (as != from) ? " as " : "", (as != from) ? as : "",
	    root_uid, root_gid, root_mode);
	flags = FREAD;
	if (vfs_flagopt(mp->mnt_optnew, "verify", NULL, 0)) {
	    flags |= O_VERIFY;
	}
	NDINIT(&nd, LOOKUP, ISOPEN | FOLLOW | LOCKLEAF, UIO_SYSSPACE, from);
	error = namei(&nd);
	if (error != 0)
		return (error);
	NDFREE_PNBUF(&nd);
	vp = nd.ni_vp;
	TARFS_DPF(FS, "%s: N: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	/* vp is now held and locked */

	/* Open the source tarball */
	error = vn_open_vnode(vp, flags, td->td_ucred, td, NULL);
	if (error != 0) {
		TARFS_DPF(FS, "%s: failed to open %s: %d\n", __func__,
		    from, error);
		vput(vp);
		goto bad;
	}
	TARFS_DPF(FS, "%s: O: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	if (vp->v_type != VREG) {
		TARFS_DPF(FS, "%s: not a regular file\n", __func__);
		error = EOPNOTSUPP;
		goto bad_open_locked;
	}
	error = priv_check(td, PRIV_VFS_MOUNT_PERM);
	if (error != 0) {
		TARFS_DPF(FS, "%s: not permitted to mount\n", __func__);
		goto bad_open_locked;
	}
	if (flags & O_VERIFY) {
		mp->mnt_flag |= MNT_VERIFIED;
	}

	/* Allocate the tarfs mount */
	error = tarfs_alloc_mount(mp, vp, root_uid, root_gid, root_mode, &tmp);
	/* vp is now held but unlocked */
	if (error != 0) {
		TARFS_DPF(FS, "%s: failed to mount %s: %d\n", __func__,
		    from, error);
		goto bad_open_unlocked;
	}
	TARFS_DPF(FS, "%s: M: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));

	/* Unconditionally mount as read-only */
	MNT_ILOCK(mp);
	mp->mnt_flag |= (MNT_LOCAL | MNT_RDONLY);
	MNT_IUNLOCK(mp);

	vfs_getnewfsid(mp);
	vfs_mountedfrom(mp, as);
	TARFS_DPF(FS, "%s: success\n", __func__);

	return (0);

bad_open_locked:
	/* vp must be held and locked */
	TARFS_DPF(FS, "%s: L: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	VOP_UNLOCK(vp);
bad_open_unlocked:
	/* vp must be held and unlocked */
	TARFS_DPF(FS, "%s: E: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	(void)vn_close(vp, flags, td->td_ucred, td);
bad:
	/* vp must be released and unlocked */
	TARFS_DPF(FS, "%s: X: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	return (error);
}

/*
 * Unmounts a tarfs filesystem.
 */
static int
tarfs_unmount(struct mount *mp, int mntflags)
{
	struct thread *td = curthread;
	struct tarfs_mount *tmp;
	struct vnode *vp;
	int error;
	int flags = 0;

	TARFS_DPF(FS, "%s: Unmounting %p\n", __func__, mp);

	/* Handle forced unmounts */
	if (mntflags & MNT_FORCE)
		flags |= FORCECLOSE;

	/* Finalize all pending I/O */
	error = vflush(mp, 0, flags, curthread);
	if (error != 0)
		return (error);
	tmp = MP_TO_TARFS_MOUNT(mp);
	vp = tmp->vp;

	MPASS(vp != NULL);
	TARFS_DPF(FS, "%s: U: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	vn_close(vp, FREAD, td->td_ucred, td);
	TARFS_DPF(FS, "%s: C: hold %u use %u lock 0x%x\n", __func__,
	    vp->v_holdcnt, vp->v_usecount, VOP_ISLOCKED(vp));
	tarfs_free_mount(tmp);

	return (0);
}

/*
 * Gets the root of a tarfs filesystem.  Returns 0 on success or a
 * positive errno value on failure.
 */
static int
tarfs_root(struct mount *mp, int flags, struct vnode **vpp)
{
	struct vnode *nvp;
	int error;

	TARFS_DPF(FS, "%s: Getting root vnode\n", __func__);

	error = VFS_VGET(mp, TARFS_ROOTINO, LK_EXCLUSIVE, &nvp);
	if (error != 0)
		return (error);

	nvp->v_vflag |= VV_ROOT;
	*vpp = nvp;
	return (0);
}

/*
 * Gets statistics for a tarfs filesystem.  Returns 0.
 */
static int
tarfs_statfs(struct mount *mp, struct statfs *sbp)
{
	struct tarfs_mount *tmp;

	tmp = MP_TO_TARFS_MOUNT(mp);

	sbp->f_bsize = TARFS_BLOCKSIZE;
	sbp->f_iosize = tmp->iosize;
	sbp->f_blocks = tmp->nblocks;
	sbp->f_bfree = 0;
	sbp->f_bavail = 0;
	sbp->f_files = tmp->nfiles;
	sbp->f_ffree = 0;

	return (0);
}

/*
 * Gets a vnode for the given inode.  On success, a pointer to the vnode
 * is stored in vpp.  Returns 0 on success or a positive errno value on
 * failure.
 */
static int
tarfs_vget(struct mount *mp, ino_t ino, int lkflags, struct vnode **vpp)
{
	struct tarfs_mount *tmp;
	struct tarfs_node *tnp;
	struct thread *td;
	struct vnode *vp;
	int error;

	TARFS_DPF(FS, "%s: mp %p, ino %lu, lkflags %d\n", __func__, mp, ino,
	    lkflags);

	td = curthread;
	error = vfs_hash_get(mp, ino, lkflags, td, vpp, NULL, NULL);
	if (error != 0)
		return (error);

	if (*vpp != NULL) {
		TARFS_DPF(FS, "%s: found hashed vnode %p\n", __func__, *vpp);
		return (error);
	}

	TARFS_DPF(FS, "%s: no hashed vnode for inode %lu\n", __func__, ino);

	tmp = MP_TO_TARFS_MOUNT(mp);

	if (ino == TARFS_ZIOINO) {
		error = vget(tmp->znode, lkflags);
		if (error != 0)
			return (error);
		*vpp = tmp->znode;
		return (0);
	}

	/* XXX Should use hash instead? */
	TAILQ_FOREACH(tnp, &tmp->allnodes, entries) {
		if (tnp->ino == ino)
			break;
	}
	TARFS_DPF(FS, "%s: search of all nodes found %p\n", __func__, tnp);
	if (tnp == NULL)
		return (ENOENT);

	(void)getnewvnode("tarfs", mp, &tarfs_vnodeops, &vp);
	TARFS_DPF(FS, "%s: allocated vnode\n", __func__);
	vp->v_data = tnp;
	vp->v_type = tnp->type;
	tnp->vnode = vp;

	lockmgr(vp->v_vnlock, lkflags, NULL);
	error = insmntque(vp, mp);
	if (error != 0)
		goto bad;
	TARFS_DPF(FS, "%s: inserting entry into VFS hash\n", __func__);
	error = vfs_hash_insert(vp, ino, lkflags, td, vpp, NULL, NULL);
	if (error != 0 || *vpp != NULL)
		return (error);

	vn_set_state(vp, VSTATE_CONSTRUCTED);
	*vpp = vp;
	return (0);

bad:
	*vpp = NULLVP;
	return (error);
}

static int
tarfs_fhtovp(struct mount *mp, struct fid *fhp, int flags, struct vnode **vpp)
{
	struct tarfs_node *tnp;
	struct tarfs_fid *tfp;
	struct vnode *nvp;
	int error;

	tfp = (struct tarfs_fid *)fhp;
	MP_TO_TARFS_MOUNT(mp);
	if (tfp->ino < TARFS_ROOTINO || tfp->ino > INT_MAX)
		return (ESTALE);

	error = VFS_VGET(mp, tfp->ino, LK_EXCLUSIVE, &nvp);
	if (error != 0) {
		*vpp = NULLVP;
		return (error);
	}
	tnp = VP_TO_TARFS_NODE(nvp);
	if (tnp->mode == 0 ||
	    tnp->gen != tfp->gen ||
	    tnp->nlink <= 0) {
		vput(nvp);
		*vpp = NULLVP;
		return (ESTALE);
	}
	*vpp = nvp;
	return (0);
}

static struct vfsops tarfs_vfsops = {
	.vfs_fhtovp =	tarfs_fhtovp,
	.vfs_mount =	tarfs_mount,
	.vfs_root =	tarfs_root,
	.vfs_statfs =	tarfs_statfs,
	.vfs_unmount =	tarfs_unmount,
	.vfs_vget =	tarfs_vget,
};
VFS_SET(tarfs_vfsops, tarfs, VFCF_READONLY);
MODULE_VERSION(tarfs, 1);
MODULE_DEPEND(tarfs, xz, 1, 1, 1);
