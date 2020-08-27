/*-
 * Copyright (c) 2017-2020, Juniper Networks, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Routines to verify files loaded.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <string.h>
#include <sys/queue.h>
#include <sys/kenv.h>

#include "libsecureboot.h"
#include <verify_file.h>
#include <manifests.h>

#ifdef UNIT_TEST
# include <err.h>
# define panic warn
/*
 * define MANIFEST_SKIP to Skip - in tests/tvo.c so that
 * tvo can control the value we use in find_manifest()
 */
extern char *Destdir;
extern size_t DestdirLen;
extern char *Skip;
# undef MANIFEST_SKIP
# define MANIFEST_SKIP Skip
# undef VE_DEBUG_LEVEL
#endif

/*
 * We sometimes need to know if input is verified or not.
 * The extra slot is for tracking most recently opened.
 */
static int ve_status[SOPEN_MAX+1];
static int ve_status_state;
struct verify_status;
struct verify_status *verified_files = NULL;
static int loaded_manifests = 0;	/* have we loaded anything? */

#define VE_STATUS_NONE	1
#define VE_STATUS_VALID	2

/**
 * @brief set ve status for fd
 */
static void
ve_status_set(int fd, int ves)
{
	if (fd >= 0 && fd < SOPEN_MAX) {
		ve_status[fd] = ves;
		ve_status_state = VE_STATUS_VALID;
	}
	ve_status[SOPEN_MAX] = ves;
}

/**
 * @brief get ve status of fd
 *
 * What we return depends on ve_status_state.
 *
 * @return
 *	@li ve_status[fd] if ve_status_state is valid
 *	@li ve_status[SOPEN_MAX] if ve_status_state is none
 *	@li VE_NOT_CHECKED if ve_status_state uninitialized
 */
int
ve_status_get(int fd)
{
	if (!ve_status_state) {
		return (VE_NOT_CHECKED);
	}
	if (ve_status_state == VE_STATUS_VALID &&
		fd >= 0 && fd < SOPEN_MAX)
		return (ve_status[fd]);
	return (ve_status[SOPEN_MAX]);	/* most recent */
}

/**
 * @brief track verify status
 *
 * occasionally loader will make multiple calls
 * for the same file, we need only check it once.
 */
struct verify_status {
	dev_t	vs_dev;
	ino_t	vs_ino;
	int	vs_status;
	struct verify_status *vs_next;
};

int
is_verified(struct stat *stp)
{
	struct verify_status *vsp;

	if (stp->st_ino > 0) {
		for (vsp = verified_files; vsp != NULL; vsp = vsp->vs_next) {
			if (stp->st_dev == vsp->vs_dev &&
			    stp->st_ino == vsp->vs_ino)
				return (vsp->vs_status);
		}
	}
	return (VE_NOT_CHECKED);
}

/* most recent first, since most likely to see repeated calls. */
void
add_verify_status(struct stat *stp, int status)
{
	struct verify_status *vsp;

	vsp = malloc(sizeof(struct verify_status));
	vsp->vs_next = verified_files;
	vsp->vs_dev = stp->st_dev;
	vsp->vs_ino = stp->st_ino;
	vsp->vs_status = status;
	verified_files = vsp;
}


/**
 * @brief
 * load specified manifest if verified
 */
int
load_manifest(const char *name, const char *prefix,
    const char *skip, struct stat *stp)
{
	struct stat st;
	size_t n;
	int rc;
	char *content;

	rc = VE_FINGERPRINT_NONE;
	n = strlen(name);
	if (n > 4) {
		if (!stp) {
			stp = &st;
			if (stat(name, &st) < 0 || !S_ISREG(st.st_mode))
				return (rc);
		}
		rc = is_verified(stp);
		if (rc != VE_NOT_CHECKED) {
			return (rc);
		}
		/* loader has no sense of time */
		ve_utc_set(stp->st_mtime);
		content = (char *)verify_signed(name, VEF_VERBOSE);
		if (content) {
#ifdef UNIT_TEST
			if (DestdirLen > 0 &&
			    strncmp(name, Destdir, DestdirLen) == 0) {
				name += DestdirLen;
				if (prefix &&
				    strncmp(prefix, Destdir, DestdirLen) == 0)
					prefix += DestdirLen;
			}
#endif
			fingerprint_info_add(name, prefix, skip, content, stp);
			add_verify_status(stp, VE_VERIFIED);
			loaded_manifests = 1; /* we are verifying! */
			DEBUG_PRINTF(3, ("loaded: %s %s %s\n",
				name, prefix, skip));
			rc = VE_VERIFIED;
		} else {
			rc = VE_FINGERPRINT_WRONG;
			add_verify_status(stp, rc);	/* remember */
		}
	}
	return (rc);
}

static int
find_manifest(const char *name)
{
	struct stat st;
	char buf[MAXPATHLEN];
	char *prefix;
	char *skip;
	const char **tp;
	int rc;

	strncpy(buf, name, MAXPATHLEN - 1);
	if (!(prefix = strrchr(buf, '/')))
		return (-1);
	*prefix = '\0';
	prefix = strdup(buf);
	rc = VE_FINGERPRINT_NONE;
	for (tp = manifest_names; *tp; tp++) {
		snprintf(buf, sizeof(buf), "%s/%s", prefix, *tp);
		DEBUG_PRINTF(5, ("looking for %s\n", buf));
		if (stat(buf, &st) == 0 && st.st_size > 0) {
#ifdef MANIFEST_SKIP_ALWAYS		/* very unlikely */
			skip = MANIFEST_SKIP_ALWAYS;
#else
#ifdef MANIFEST_SKIP			/* rare */
			if (*tp[0] == '.') {
				skip = MANIFEST_SKIP;
			} else
#endif
				skip = NULL;
#endif
			rc = load_manifest(buf, skip ? prefix : NULL,
			    skip, &st);
			break;
		}
	}
	free(prefix);
	return (rc);
}


#ifdef LOADER_VERIEXEC_TESTING
# define ACCEPT_NO_FP_DEFAULT	VE_MUST + 1
#else
# define ACCEPT_NO_FP_DEFAULT	VE_MUST
#endif
#ifndef VE_VERBOSE_DEFAULT
# define VE_VERBOSE_DEFAULT	0
#endif

static int
severity_guess(const char *filename)
{
	const char *cp;

	/* Some files like *.conf and *.hints may be unsigned */
	if ((cp = strrchr(filename, '.'))) {
		if (strcmp(cp, ".conf") == 0 ||
		    strcmp(cp, ".cookie") == 0 ||
			strcmp(cp, ".hints") == 0)
			return (VE_TRY);
		if (strcmp(cp, ".4th") == 0 ||
		    strcmp(cp, ".lua") == 0 ||
		    strcmp(cp, ".rc") == 0)
			return (VE_MUST);
	}
	return (VE_WANT);
}

static int Verifying = -1;		/* 0 if not verifying */

static void
verify_tweak(int fd, off_t off, struct stat *stp,
    char *tweak, int *accept_no_fp,
    int *verbose)
{
	if (strcmp(tweak, "off") == 0) {
		Verifying = 0;
	} else if (strcmp(tweak, "strict") == 0) {
		/* anything caller wants verified must be */
		*accept_no_fp = VE_WANT;
		*verbose = 1; /* warn of anything unverified */
		/* treat self test failure as fatal */
		if (!ve_self_tests()) {
			panic("verify self tests failed");
		}
	} else if (strcmp(tweak, "modules") == 0) {
		/* modules/kernel must be verified */
		*accept_no_fp = VE_MUST;
	} else if (strcmp(tweak, "try") == 0) {
		/* best effort: always accept no fp */
		*accept_no_fp = VE_MUST + 1;
	} else if (strcmp(tweak, "verbose") == 0) {
		*verbose = 1;
	} else if (strcmp(tweak, "quiet") == 0) {
		*verbose = 0;
	} else if (strncmp(tweak, "trust", 5) == 0) {
		/* content is trust anchor to add or revoke */
		unsigned char *ucp;
		size_t num;

		if (off > 0)
			lseek(fd, 0, SEEK_SET);
		ucp = read_fd(fd, stp->st_size);
		if (ucp == NULL)
			return;
		if (strstr(tweak, "revoke")) {
			num = ve_trust_anchors_revoke(ucp, stp->st_size);
			DEBUG_PRINTF(3, ("revoked %d trust anchors\n",
				(int) num));
		} else {
			num = ve_trust_anchors_add_buf(ucp, stp->st_size);
			DEBUG_PRINTF(3, ("added %d trust anchors\n",
				(int) num));
		}
	}
}

#ifndef VE_DEBUG_LEVEL
# define VE_DEBUG_LEVEL 0
#endif

static int
getenv_int(const char *var, int def)
{
	const char *cp;
	char *ep;
	long val;

	val = def;
	cp = getenv(var);
	if (cp && *cp) {
		val = strtol(cp, &ep, 0);
		if ((ep && *ep) || val != (int)val) {
			val = def;
		}
	}
	return (int)val;
}


/**
 * @brief prepare to verify an open file
 *
 * @param[in] fd
 * 	open descriptor
 *
 * @param[in] filename
 * 	path we opened and will use to lookup fingerprint
 *
 * @param[in] stp
 *	stat pointer so we can check file type
 */
int
verify_prep(int fd, const char *filename, off_t off, struct stat *stp,
    const char *caller)
{
	int rc;

	if (Verifying < 0) {
		Verifying = ve_trust_init();
#ifndef UNIT_TEST
		ve_debug_set(getenv_int("VE_DEBUG_LEVEL", VE_DEBUG_LEVEL));
#endif
		/* initialize ve_status with default result */
		rc = Verifying ? VE_NOT_CHECKED : VE_NOT_VERIFYING;
		ve_status_set(0, rc);
		ve_status_state = VE_STATUS_NONE;
		if (Verifying) {
			ve_self_tests();
			ve_anchor_verbose_set(1);
		}
	}
	if (!Verifying || fd < 0)
		return (0);
	if (stp) {
		if (fstat(fd, stp) < 0 || !S_ISREG(stp->st_mode))
			return (0);
	}
	DEBUG_PRINTF(2,
	    ("verify_prep: caller=%s,fd=%d,name='%s',off=%lld,dev=%lld,ino=%lld\n",
		caller, fd, filename, (long long)off, (long long)stp->st_dev,
		(long long)stp->st_ino));
	rc = is_verified(stp);
	DEBUG_PRINTF(4,("verify_prep: is_verified()->%d\n", rc));
	if (rc == VE_NOT_CHECKED) {
		rc = find_manifest(filename);
	} else {
		ve_status_set(fd, rc);
	}
	return (rc);
}

/**
 * @brief verify an open file
 *
 * @param[in] fd
 * 	open descriptor
 *
 * @param[in] filename
 * 	path we opened and will use to lookup fingerprint
 *
 * @param[in] off
 * 	current offset in fd, must be restored on return
 *
 * @param[in] severity
 * 	indicator of how to handle case of missing fingerprint
 *
 * We look for a signed manifest relative to the filename
 * just opened and verify/load it if needed.
 *
 * We then use verify_fd() in libve to actually verify that hash for
 * open file.  If it returns < 0 we look at the severity arg to decide
 * what to do about it.
 *
 * If verify_fd() returns VE_FINGERPRINT_NONE we accept it if severity
 * is < accept_no_fp.
 *
 * @return >= 0 on success < 0 on failure
 */
int
verify_file(int fd, const char *filename, off_t off, int severity,
    const char *caller)
{
	static int once;
	static int accept_no_fp = ACCEPT_NO_FP_DEFAULT;
	static int verbose = VE_VERBOSE_DEFAULT;
	struct stat st;
	char *cp;
	int rc;

	rc = verify_prep(fd, filename, off, &st, caller);

	if (!rc)
		return (0);

	if (!once) {
		once++;
		verbose = getenv_int("VE_VERBOSE", VE_VERBOSE_DEFAULT);
	}

	if (rc != VE_FINGERPRINT_WRONG && loaded_manifests) {
		if (severity <= VE_GUESS)
			severity = severity_guess(filename);
#ifdef VE_PCR_SUPPORT
		/*
		 * Only update pcr with things that must verify
		 * these tend to be processed in a more deterministic
		 * order, which makes our pseudo pcr more useful.
		 */
		ve_pcr_updating_set((severity == VE_MUST));
#endif
#ifdef UNIT_TEST
		if (DestdirLen > 0 &&
		    strncmp(filename, Destdir, DestdirLen) == 0) {
			filename += DestdirLen;
		}
#endif
		if ((rc = verify_fd(fd, filename, off, &st)) >= 0) {
			if (verbose || severity > VE_WANT) {
#if defined(VE_DEBUG_LEVEL) && VE_DEBUG_LEVEL > 0
				printf("%serified %s %llu,%llu\n",
				    (rc == VE_FINGERPRINT_IGNORE) ? "Unv" : "V",
				    filename,
				    (long long)st.st_dev, (long long)st.st_ino);
#else
				printf("%serified %s\n",
				    (rc == VE_FINGERPRINT_IGNORE) ? "Unv" : "V",
				    filename);
#endif
			}
			if (severity < VE_MUST) { /* not a kernel or module */
				if ((cp = strrchr(filename, '/'))) {
					cp++;
					if (strncmp(cp, "loader.ve.", 10) == 0) {
						cp += 10;
						verify_tweak(fd, off, &st, cp,
						    &accept_no_fp, &verbose);
					}
				}
			}
			add_verify_status(&st, rc);
			ve_status_set(fd, rc);
			return (rc);
		}

		if (severity || verbose || rc == VE_FINGERPRINT_WRONG)
			printf("Unverified: %s\n", ve_error_get());
		if (rc == VE_FINGERPRINT_UNKNOWN && severity < VE_MUST)
			rc = VE_UNVERIFIED_OK;
		else if (rc == VE_FINGERPRINT_NONE && severity < accept_no_fp)
			rc = VE_UNVERIFIED_OK;

		add_verify_status(&st, rc);
	}
#ifdef LOADER_VERIEXEC_TESTING
	else if (rc != VE_FINGERPRINT_WRONG) {
		/*
		 * We have not loaded any manifest and
		 * not because of verication failure.
		 * Most likely reason is we have none.
		 * Allow boot to proceed if we are just testing.
		 */
		return (VE_UNVERIFIED_OK);
	}
#endif
	if (rc == VE_FINGERPRINT_WRONG && severity > accept_no_fp)
		panic("cannot continue");
	ve_status_set(fd, rc);
	return (rc);
}

/**
 * @brief get hex string for pcr value and export
 *
 * In case we are doing measured boot, provide
 * value of the "pcr" data we have accumulated.
 */
void
verify_pcr_export(void)
{
#ifdef VE_PCR_SUPPORT
	char hexbuf[br_sha256_SIZE * 2 + 2];
	unsigned char hbuf[br_sha256_SIZE];
	char *hinfo;
	char *hex;
	ssize_t hlen;

	hlen = ve_pcr_get(hbuf, sizeof(hbuf));
	if (hlen > 0) {
		hex = hexdigest(hexbuf, sizeof(hexbuf), hbuf, hlen);
		if (hex) {
			hex[hlen*2] = '\0'; /* clobber newline */
			setenv("loader.ve.pcr", hex, 1);
			DEBUG_PRINTF(1,
			    ("%s: setenv(loader.ve.pcr, %s\n", __func__,
				hex));
			hinfo = ve_pcr_hashed_get(1);
			if (hinfo) {
				setenv("loader.ve.hashed", hinfo, 1);
				DEBUG_PRINTF(1,
				    ("%s: setenv(loader.ve.hashed, %s\n",
					__func__, hinfo));
				if ((hlen = strlen(hinfo)) > KENV_MVALLEN) {
					/*
					 * bump kenv_mvallen
					 * roundup to multiple of KENV_MVALLEN
					 */
					char mvallen[16];

					hlen += KENV_MVALLEN -
					    (hlen % KENV_MVALLEN);
					if (snprintf(mvallen, sizeof(mvallen),
						"%d", (int) hlen) < sizeof(mvallen))
						setenv("kenv_mvallen", mvallen, 1);
				}
				free(hinfo);
			}
		}
	}
#endif
}
