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
#include <sys/stat.h>

#include "libsecureboot-priv.h"
#include <verify_file.h>

/*
 * Values to pass to kernel by envs.
 */
static char manifest_path[MAXPATHLEN];
static char manifest_prefix[MAXPATHLEN];
static char manifest_hash[2 * br_sha256_SIZE + 2];
static int manifest_present = 0;

/*
 * Verify and pass manifest path and digest to kernel through envs.
 * The paths in manifest can be either absolute,
 * or "prefix", if exists will be added to the ones that are not.
 */
int
pass_manifest(const char *path, const char *prefix)
{
	char *content;
	struct stat st;
	unsigned char digest[br_sha256_SIZE];
	const br_hash_class *md;
	br_hash_compat_context ctx;
	int rc;

	content = NULL;
	md = &br_sha256_vtable;

	if (strnlen(path, MAXPATHLEN) == MAXPATHLEN ||
	    strnlen(prefix, MAXPATHLEN) == MAXPATHLEN)
		return (EINVAL);

	rc = stat(path, &st);
	if (rc != 0)
		goto out;

	if (!S_ISREG(st.st_mode)) {
		rc = EINVAL;
		goto out;
	}

	rc = is_verified(&st);

	if (rc != VE_NOT_CHECKED && rc != VE_VERIFIED) {
		rc = EPERM;
		goto out;
	}

	if (rc == VE_VERIFIED)
		content = read_file(path, NULL);
	else
		content = (char *)verify_signed(path, VEF_VERBOSE);

	if (content == NULL) {
		add_verify_status(&st, VE_FINGERPRINT_WRONG);
		rc = EIO;
		goto out;
	}

	add_verify_status(&st, VE_VERIFIED);

	md->init(&ctx.vtable);
	md->update(&ctx.vtable, content, st.st_size);
	md->out(&ctx.vtable, digest);

	if (prefix == NULL)
		manifest_prefix[0] = '\0';
	else
		strcpy(manifest_prefix, prefix);

	strcpy(manifest_path, path);

	hexdigest(manifest_hash, 2 * br_sha256_SIZE + 2,
	    digest, br_sha256_SIZE);
	manifest_hash[2*br_sha256_SIZE] = '\0';

	manifest_present = 1;
	rc = 0;

out:
	if (content != NULL)
		free(content);

	return (rc);
}

/*
 * Set appropriate envs to inform kernel about manifest location and digest.
 * This should be called right before boot so that envs can't be replaced.
 */
int
pass_manifest_export_envs()
{
	int rc;

	/* If we have nothing to pass make sure that envs are empty. */
	if (!manifest_present) {
		unsetenv("veriexec.manifest_path");
		unsetenv("veriexec.manifest_hash");
		unsetenv("veriexec.manifest_prefix");
		return (0);
	}

	rc = setenv("veriexec.manifest_path", manifest_path, 1);
	if (rc != 0)
		return (rc);

	rc = setenv("veriexec.manifest_hash", manifest_hash, 1);
	if (rc != 0) {
		unsetenv("veriexec.manifest_path");
		return (rc);
	}

	if (manifest_prefix[0] != '\0')
		rc = setenv("veriexec.manifest_prefix", manifest_prefix, 1);

	return (rc);
}

