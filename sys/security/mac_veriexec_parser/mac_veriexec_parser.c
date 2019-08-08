/*-
 * Copyright (c) 2019 Stormshield.
 * Copyright (c) 2019 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/ctype.h>
#include <sys/eventhandler.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/vnode.h>

#include <crypto/sha2/sha256.h>
#include <crypto/sha2/sha384.h>
#include <crypto/sha2/sha512.h>

#include <security/mac_veriexec/mac_veriexec.h>
#include <security/mac_veriexec/mac_veriexec_internal.h>

/* The following are based on sbin/veriexec */
struct fingerprint_type {
	const char	*fp_type;
	int		fp_size;
};

struct fp_flag {
	const char	*flag_name;
	int		flag;
};

static const struct fingerprint_type fp_table[] = {
	{"sha256=", SHA256_DIGEST_LENGTH},
#if MAXFINGERPRINTLEN >= SHA384_DIGEST_LENGTH
	{"sha384=", SHA384_DIGEST_LENGTH},
#endif
#if MAXFINGERPRINTLEN >= SHA512_DIGEST_LENGTH
	{"sha512=", SHA512_DIGEST_LENGTH},
#endif
	{NULL, 0}
};

static const struct fp_flag flags_table[] = {
	{"indirect",  VERIEXEC_INDIRECT},
	{"no_ptrace", VERIEXEC_NOTRACE},
	{"trusted",   VERIEXEC_TRUSTED},
	{"no_fips",   VERIEXEC_NOFIPS},
	{NULL, 0}
};

extern struct mtx ve_mutex;

static unsigned char	hexchar_to_byte(unsigned char c);
static int		hexstring_to_bin(unsigned char *buf);

static int	get_flags(const char *entry);
static int	get_fp(const char *entry, char **type,
		    unsigned char **digest, int *flags);
static int	verify_digest(const char *data, size_t len,
		    const unsigned char *expected_hash);

static int	open_file(const char *path, struct nameidata *nid);
static char	*read_manifest(char *path, unsigned char *digest);
static int	parse_entry(char *entry, char *prefix);
static int	parse_manifest(char *path, unsigned char *hash, char *prefix);

static unsigned char
hexchar_to_byte(unsigned char c)
{

	if (isdigit(c))
		return (c - '0');

	return (isupper(c) ? c - 'A' + 10 : c - 'a' + 10);
}

static int
hexstring_to_bin(unsigned char *buf)
{
	size_t		i, len;
	unsigned char	byte;

	len = strlen(buf);
	for (i = 0; i < len / 2; i++) {
		if (!isxdigit(buf[2 * i]) || !isxdigit(buf[2 * i + 1]))
			return (EINVAL);

		byte = hexchar_to_byte(buf[2 * i]) << 4;
		byte += hexchar_to_byte(buf[2 * i + 1]);
		buf[i] = byte;
	}
	return (0);
}

static int
get_flags(const char *entry)
{
	int	i;
	int	result = 0;

	for (i = 0; flags_table[i].flag_name != NULL; i++)
		if (strstr(entry, flags_table[i].flag_name) != NULL)
			result |= flags_table[i].flag;

	return (result);
}

/*
 * Parse a single line of manifest looking for a digest and its type.
 * We expect it to be in form of "path shaX=hash".
 * The line will be split into path, hash type and hash value.
 */
static int
get_fp(const char *entry, char **type, unsigned char **digest, int *flags)
{
	char	*delimiter;
	char	*local_digest;
	char	*fp_type;
	char	*prev_fp_type;
	size_t	min_len;
	int	i;

	delimiter = NULL;
	fp_type = NULL;
	prev_fp_type = NULL;

	for (i = 0; fp_table[i].fp_type != NULL; i++) {
		fp_type = strstr(entry, fp_table[i].fp_type);
		/* Look for the last "shaX=hash" in line */
		while (fp_type != NULL) {
			prev_fp_type = fp_type;
			fp_type++;
			fp_type = strstr(fp_type, fp_table[i].fp_type);
		}
		fp_type = prev_fp_type;
		if (fp_type != NULL) {
			if (fp_type == entry || fp_type[-1] != ' ')
				return (EINVAL);

			/*
			 * The entry should contain at least
			 * fp_type and digest in hexadecimal form.
			 */
			min_len = strlen(fp_table[i].fp_type) +
				2 * fp_table[i].fp_size;

			if (strnlen(fp_type, min_len) < min_len)
				return (EINVAL);

			local_digest = &fp_type[strlen(fp_table[i].fp_type)];
			delimiter = &local_digest[2 * fp_table[i].fp_size];

			/*
			 * Make sure that digest is followed by
			 * some kind of delimiter.
			 */
			if (*delimiter != '\n' &&
			    *delimiter != '\0' &&
			    *delimiter != ' ')
				return (EINVAL);

			/*
			 * Does the entry contain flags we need to parse?
			 */
			if (*delimiter == ' ' && flags != NULL)
				*flags = get_flags(delimiter);

			/*
			 * Split entry into three parts:
			 * path, fp_type and digest.
			 */
			local_digest[-1] = '\0';
			*delimiter = '\0';
			fp_type[-1] = '\0';
			break;
		}
	}

	if (fp_type == NULL)
		return (EINVAL);

	if (type != NULL)
		*type = fp_type;

	if (digest != NULL)
		*digest = local_digest;

	return (0);
}

/*
 * Currently we verify manifest using sha256.
 * In future another env with hash type could be introduced.
 */
static int
verify_digest(const char *data, size_t len, const unsigned char *expected_hash)
{
	SHA256_CTX	ctx;
	unsigned char	hash[SHA256_DIGEST_LENGTH];

	SHA256_Init(&ctx);
	SHA256_Update(&ctx, data, len);
	SHA256_Final(hash, &ctx);

	return (memcmp(expected_hash, hash, SHA256_DIGEST_LENGTH));
}


static int
open_file(const char *path, struct nameidata *nid)
{
	int flags, rc;

	flags = FREAD;

	pwd_ensure_dirs();

	NDINIT(nid, LOOKUP, 0, UIO_SYSSPACE, path, curthread);
	rc = vn_open(nid, &flags, 0, NULL);
	NDFREE(nid, NDF_ONLY_PNBUF);
	if (rc != 0)
		return (rc);

	return (0);
}

/*
 * Read the manifest from location specified in path and verify its digest.
 */
static char*
read_manifest(char *path, unsigned char *digest)
{
	struct nameidata	nid;
	struct vattr		va;
	char			*data;
	ssize_t			bytes_read, resid;
	int			rc;

	data = NULL;
	bytes_read = 0;

	rc = open_file(path, &nid);
	if (rc != 0)
		goto fail;

	rc = VOP_GETATTR(nid.ni_vp, &va, curthread->td_ucred);
	if (rc != 0)
		goto fail;

	data = (char *)malloc(va.va_size + 1, M_VERIEXEC, M_WAITOK);

	while (bytes_read < va.va_size) {
		rc = vn_rdwr(
		    UIO_READ, nid.ni_vp, data,
		    va.va_size - bytes_read, bytes_read,
		    UIO_SYSSPACE, IO_NODELOCKED,
		    curthread->td_ucred, NOCRED, &resid, curthread);
		if (rc != 0)
			goto fail;

		bytes_read = va.va_size - resid;
	}

	data[bytes_read] = '\0';

	VOP_UNLOCK(nid.ni_vp, 0);
	(void)vn_close(nid.ni_vp, FREAD, curthread->td_ucred, curthread);

	/*
	 * If digest is wrong someone might be trying to fool us.
	 */
	if (verify_digest(data, va.va_size, digest))
		panic("Manifest hash doesn't match expected value!");

	return (data);

fail:
	if (data != NULL)
		free(data, M_VERIEXEC);

	return (NULL);
}

/*
 * Process single line.
 * First split it into path, digest_type and digest.
 * Then try to open the file and insert its fingerprint into metadata store.
 */
static int
parse_entry(char *entry, char *prefix)
{
	struct nameidata	nid;
	struct vattr		va;
	char			path[MAXPATHLEN];
	char			*fp_type;
	unsigned char		*digest;
	int			rc, is_exec, flags;

	fp_type = NULL;
	digest = NULL;
	flags = 0;

	rc = get_fp(entry, &fp_type, &digest, &flags);
	if (rc != 0)
		return (rc);

	rc = hexstring_to_bin(digest);
	if (rc != 0)
		return (rc);

	if (strnlen(entry, MAXPATHLEN) == MAXPATHLEN)
		return (EINVAL);

	/* If the path is not absolute prepend it with a prefix */
	if (prefix != NULL && entry[0] != '/') {
		rc = snprintf(path, MAXPATHLEN, "%s/%s",
			    prefix, entry);
		if (rc < 0)
			return (-rc);
	} else {
		strcpy(path, entry);
	}

	rc = open_file(path, &nid);
	NDFREE(&nid, NDF_ONLY_PNBUF);
	if (rc != 0)
		return (rc);

	rc = VOP_GETATTR(nid.ni_vp, &va, curthread->td_ucred);
	if (rc != 0)
		goto out;

	is_exec = (va.va_mode & VEXEC);

	mtx_lock(&ve_mutex);
	rc = mac_veriexec_metadata_add_file(
	    is_exec == 0,
	    va.va_fsid, va.va_fileid, va.va_gen,
	    digest,
	    NULL, 0,
	    flags, fp_type, 1);
	mtx_unlock(&ve_mutex);

out:
	VOP_UNLOCK(nid.ni_vp, 0);
	vn_close(nid.ni_vp, FREAD, curthread->td_ucred, curthread);
	return (rc);
}

/*
 * Look for manifest in env that have beed passed by loader.
 * This routine should be called right after the rootfs is mounted.
 */
static int
parse_manifest(char *path, unsigned char *hash, char *prefix)
{
	char	*data;
	char	*entry;
	char	*next_entry;
	int	rc, success_count;

	data = NULL;
	success_count = 0;
	rc = 0;

	data = read_manifest(path, hash);
	if (data == NULL) {
		rc = EIO;
		goto out;
	}

	entry = data;
	while (entry != NULL) {
		next_entry = strchr(entry, '\n');
		if (next_entry != NULL) {
			*next_entry = '\0';
			next_entry++;
		}
		if (entry[0] == '\n' || entry[0] == '\0') {
			entry = next_entry;
			continue;
		}
		if ((rc = parse_entry(entry, prefix)))
			printf("mac_veriexec_parser: Warning: Failed to parse"
			       " entry with rc:%d, entry:\"%s\"\n", rc, entry);
		else
			success_count++;

		entry = next_entry;
	}
	rc = 0;

out:
	if (data != NULL)
		free(data, M_VERIEXEC);

	if (success_count == 0)
		rc = EINVAL;

	return (rc);
}

static void
parse_manifest_event(void *dummy)
{
	char		*manifest_path;
	char		*manifest_prefix;
	unsigned char	*manifest_hash;
	int		rc;

	/* If the envs are not set fail silently */
	manifest_path = kern_getenv("veriexec.manifest_path");
	if (manifest_path == NULL)
		return;

	manifest_hash = kern_getenv("veriexec.manifest_hash");
	if (manifest_hash == NULL) {
		freeenv(manifest_path);
		return;
	}

	manifest_prefix = kern_getenv("veriexec.manifest_prefix");

	if (strlen(manifest_hash) != 2 * SHA256_DIGEST_LENGTH)
		panic("veriexec.manifest_hash has incorrect size");

	rc = hexstring_to_bin(manifest_hash);
	if (rc != 0)
		panic("mac_veriexec: veriexec.loader.manifest_hash"
		    " doesn't contain a hash in hexadecimal form");

	rc = parse_manifest(manifest_path, manifest_hash, manifest_prefix);
	if (rc != 0)
		panic("mac_veriexec: Failed to parse manifest err=%d", rc);

	mtx_lock(&ve_mutex);
	mac_veriexec_set_state(
	    VERIEXEC_STATE_LOADED | VERIEXEC_STATE_ACTIVE |
	    VERIEXEC_STATE_LOCKED | VERIEXEC_STATE_ENFORCE);
	mtx_unlock(&ve_mutex);

	freeenv(manifest_path);
	freeenv(manifest_hash);
	if (manifest_prefix != NULL)
		freeenv(manifest_prefix);
}

EVENTHANDLER_DEFINE(mountroot, parse_manifest_event, NULL, 0);
