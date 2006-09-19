/*-
 * Copyright (c) 2004-2006 Pawel Jakub Dawidek <pjd@FreeBSD.org>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <readpassphrase.h>
#include <string.h>
#include <strings.h>
#include <libgeom.h>
#include <paths.h>
#include <errno.h>
#include <assert.h>

#include <sys/param.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <opencrypto/cryptodev.h>
#include <geom/eli/g_eli.h>
#include <geom/eli/pkcs5v2.h>

#include "core/geom.h"
#include "misc/subr.h"


uint32_t lib_version = G_LIB_VERSION;
uint32_t version = G_ELI_VERSION;

static char aalgo[] = "none";
static char ealgo[] = "aes";
static intmax_t keylen = 0;
static intmax_t keyno = -1;
static intmax_t iterations = -1;
static intmax_t sectorsize = 0;
static char keyfile[] = "", newkeyfile[] = "";

static void eli_main(struct gctl_req *req, unsigned flags);
static void eli_init(struct gctl_req *req);
static void eli_attach(struct gctl_req *req);
static void eli_configure(struct gctl_req *req);
static void eli_setkey(struct gctl_req *req);
static void eli_delkey(struct gctl_req *req);
static void eli_kill(struct gctl_req *req);
static void eli_backup(struct gctl_req *req);
static void eli_restore(struct gctl_req *req);
static void eli_clear(struct gctl_req *req);
static void eli_dump(struct gctl_req *req);

/*
 * Available commands:
 *
 * init [-bhPv] [-a aalgo] [-e ealgo] [-i iterations] [-l keylen] [-K newkeyfile] prov
 * label - alias for 'init'
 * attach [-dprv] [-k keyfile] prov
 * detach [-fl] prov ...
 * stop - alias for 'detach'
 * onetime [-d] [-a aalgo] [-e ealgo] [-l keylen] prov ...
 * configure [-bB] prov ...
 * setkey [-pPv] [-n keyno] [-k keyfile] [-K newkeyfile] prov
 * delkey [-afv] [-n keyno] prov
 * kill [-av] [prov ...]
 * backup [-v] prov file
 * restore [-v] file prov
 * clear [-v] prov ...
 * dump [-v] prov ...
 */
struct g_command class_commands[] = {
	{ "init", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'a', "aalgo", aalgo, G_TYPE_STRING },
		{ 'b', "boot", NULL, G_TYPE_NONE },
		{ 'e', "ealgo", ealgo, G_TYPE_STRING },
		{ 'i', "iterations", &iterations, G_TYPE_NUMBER },
		{ 'K', "newkeyfile", newkeyfile, G_TYPE_STRING },
		{ 'l', "keylen", &keylen, G_TYPE_NUMBER },
		{ 'P', "nonewpassphrase", NULL, G_TYPE_NONE },
		{ 's', "sectorsize", &sectorsize, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-bPv] [-a aalgo] [-e ealgo] [-i iterations] [-l keylen] [-K newkeyfile] [-s sectorsize] prov"
	},
	{ "label", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'a', "aalgo", aalgo, G_TYPE_STRING },
		{ 'b', "boot", NULL, G_TYPE_NONE },
		{ 'e', "ealgo", ealgo, G_TYPE_STRING },
		{ 'i', "iterations", &iterations, G_TYPE_NUMBER },
		{ 'K', "newkeyfile", newkeyfile, G_TYPE_STRING },
		{ 'l', "keylen", &keylen, G_TYPE_NUMBER },
		{ 'P', "nonewpassphrase", NULL, G_TYPE_NONE },
		{ 's', "sectorsize", &sectorsize, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "- an alias for 'init'"
	},
	{ "attach", G_FLAG_VERBOSE | G_FLAG_LOADKLD, eli_main,
	    {
		{ 'd', "detach", NULL, G_TYPE_NONE },
		{ 'k', "keyfile", keyfile, G_TYPE_STRING },
		{ 'p', "nopassphrase", NULL, G_TYPE_NONE },
		{ 'r', "readonly", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "[-dprv] [-k keyfile] prov"
	},
	{ "detach", 0, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		{ 'l', "last", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "[-fl] prov ..."
	},
	{ "stop", 0, NULL,
	    {
		{ 'f', "force", NULL, G_TYPE_NONE },
		{ 'l', "last", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "- an alias for 'detach'"
	},
	{ "onetime", G_FLAG_VERBOSE | G_FLAG_LOADKLD, NULL,
	    {
		{ 'a', "aalgo", aalgo, G_TYPE_STRING },
		{ 'd', "detach", NULL, G_TYPE_NONE },
		{ 'e', "ealgo", ealgo, G_TYPE_STRING },
		{ 'l', "keylen", &keylen, G_TYPE_NUMBER },
		{ 's', "sectorsize", &sectorsize, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-d] [-a aalgo] [-e ealgo] [-l keylen] [-s sectorsize] prov ..."
	},
	{ "configure", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'b', "boot", NULL, G_TYPE_NONE },
		{ 'B', "noboot", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "[-bB] prov ..."
	},
	{ "setkey", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'i', "iterations", &iterations, G_TYPE_NUMBER },
		{ 'k', "keyfile", keyfile, G_TYPE_STRING },
		{ 'K', "newkeyfile", newkeyfile, G_TYPE_STRING },
		{ 'n', "keyno", &keyno, G_TYPE_NUMBER },
		{ 'p', "nopassphrase", NULL, G_TYPE_NONE },
		{ 'P', "nonewpassphrase", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "[-pPv] [-n keyno] [-i iterations] [-k keyfile] [-K newkeyfile] prov"
	},
	{ "delkey", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'a', "all", NULL, G_TYPE_NONE },
		{ 'f', "force", NULL, G_TYPE_NONE },
		{ 'n', "keyno", &keyno, G_TYPE_NUMBER },
		G_OPT_SENTINEL
	    },
	    "[-afv] [-n keyno] prov"
	},
	{ "kill", G_FLAG_VERBOSE, eli_main,
	    {
		{ 'a', "all", NULL, G_TYPE_NONE },
		G_OPT_SENTINEL
	    },
	    "[-av] [prov ...]"
	},
	{ "backup", G_FLAG_VERBOSE, eli_main, G_NULL_OPTS,
	    "[-v] prov file"
	},
	{ "restore", G_FLAG_VERBOSE, eli_main, G_NULL_OPTS,
	    "[-v] file prov"
	},
	{ "clear", G_FLAG_VERBOSE, eli_main, G_NULL_OPTS,
	    "[-v] prov ..."
	},
	{ "dump", G_FLAG_VERBOSE, eli_main, G_NULL_OPTS,
	    "[-v] prov ..."
	},
	G_CMD_SENTINEL
};

static int verbose = 0;

static int
eli_protect(struct gctl_req *req)
{
	struct rlimit rl;

	/* Disable core dumps. */
	rl.rlim_cur = 0;
	rl.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &rl) == -1) {
		gctl_error(req, "Cannot disable core dumps: %s.",
		    strerror(errno));
		return (-1);
	}
	/* Disable swapping. */
	if (mlockall(MCL_FUTURE) == -1) {
		gctl_error(req, "Cannot lock memory: %s.", strerror(errno));
		return (-1);
	}
	return (0);
}

static void
eli_main(struct gctl_req *req, unsigned flags)
{
	const char *name;

	if (eli_protect(req) == -1)
		return;

	if ((flags & G_FLAG_VERBOSE) != 0)
		verbose = 1;

	name = gctl_get_ascii(req, "verb");
	if (name == NULL) {
		gctl_error(req, "No '%s' argument.", "verb");
		return;
	}
	if (strcmp(name, "init") == 0 || strcmp(name, "label") == 0)
		eli_init(req);
	else if (strcmp(name, "attach") == 0)
		eli_attach(req);
	else if (strcmp(name, "configure") == 0)
		eli_configure(req);
	else if (strcmp(name, "setkey") == 0)
		eli_setkey(req);
	else if (strcmp(name, "delkey") == 0)
		eli_delkey(req);
	else if (strcmp(name, "kill") == 0)
		eli_kill(req);
	else if (strcmp(name, "backup") == 0)
		eli_backup(req);
	else if (strcmp(name, "restore") == 0)
		eli_restore(req);
	else if (strcmp(name, "dump") == 0)
		eli_dump(req);
	else if (strcmp(name, "clear") == 0)
		eli_clear(req);
	else
		gctl_error(req, "Unknown command: %s.", name);
}

static void
arc4rand(unsigned char *buf, size_t size)
{
	uint32_t *buf4;
	size_t size4;
	unsigned i;

	buf4 = (uint32_t *)buf;
	size4 = size / 4;

	for (i = 0; i < size4; i++)
		buf4[i] = arc4random();
	for (i *= 4; i < size; i++)
		buf[i] = arc4random() % 0xff;
}

static int
eli_is_attached(const char *prov)
{
	char name[MAXPATHLEN];
	unsigned secsize;

	/*
	 * Not the best way to do it, but the easiest.
	 * We try to open provider and check if it is a GEOM provider
	 * by asking about its sectorsize.
	 */
	snprintf(name, sizeof(name), "%s%s", prov, G_ELI_SUFFIX);
	secsize = g_get_sectorsize(name);
	if (secsize > 0)
		return (1);
	return (0);
}

static unsigned char *
eli_genkey(struct gctl_req *req, struct g_eli_metadata *md, unsigned char *key,
    int new)
{
	struct hmac_ctx ctx;
	const char *str;
	int error, nopassphrase;

	nopassphrase =
	    gctl_get_int(req, new ? "nonewpassphrase" : "nopassphrase");

	g_eli_crypto_hmac_init(&ctx, NULL, 0);

	str = gctl_get_ascii(req, new ? "newkeyfile" : "keyfile");
	if (str[0] == '\0' && nopassphrase) {
		gctl_error(req, "No key components given.");
		return (NULL);
	} else if (str[0] != '\0') {
		char buf[MAXPHYS];
		ssize_t done;
		int fd;

		if (strcmp(str, "-") == 0)
			fd = STDIN_FILENO;
		else {
			fd = open(str, O_RDONLY);
			if (fd == -1) {
				gctl_error(req, "Cannot open keyfile %s: %s.",
				    str, strerror(errno));
				return (NULL);
			}
		}
		while ((done = read(fd, buf, sizeof(buf))) > 0)
			g_eli_crypto_hmac_update(&ctx, buf, done);
		error = errno;
		if (strcmp(str, "-") != 0)
			close(fd);
		bzero(buf, sizeof(buf));
		if (done == -1) {
			gctl_error(req, "Cannot read keyfile %s: %s.", str,
			    strerror(error));
			return (NULL);
		}
	}

	if (!nopassphrase) {
		char buf1[BUFSIZ], buf2[BUFSIZ], *p;

		if (!new && md->md_iterations == -1) {
			gctl_error(req, "Missing -p flag.");
			return (NULL);
		}
		for (;;) {
			p = readpassphrase(
			    new ? "Enter new passphrase:" : "Enter passphrase:",
			    buf1, sizeof(buf1), RPP_ECHO_OFF | RPP_REQUIRE_TTY);
			if (p == NULL) {
				bzero(buf1, sizeof(buf1));
				gctl_error(req, "Cannot read passphrase: %s.",
				    strerror(errno));
				return (NULL);
			}
	
			if (new) {
				p = readpassphrase("Reenter new passphrase: ",
				    buf2, sizeof(buf2),
				    RPP_ECHO_OFF | RPP_REQUIRE_TTY);
				if (p == NULL) {
					bzero(buf1, sizeof(buf1));
					gctl_error(req,
					    "Cannot read passphrase: %s.",
					    strerror(errno));
					return (NULL);
				}
	
				if (strcmp(buf1, buf2) != 0) {
					bzero(buf2, sizeof(buf2));
					fprintf(stderr, "They didn't match.\n");
					continue;
				}
				bzero(buf2, sizeof(buf2));
			}
			break;
		}
		/*
		 * Field md_iterations equal to -1 means "choose some sane
		 * value for me".
		 */
		if (md->md_iterations == -1) {
			assert(new);
			if (verbose)
				printf("Calculating number of iterations...\n");
			md->md_iterations = pkcs5v2_calculate(2000000);
			assert(md->md_iterations > 0);
			if (verbose) {
				printf("Done, using %d iterations.\n",
				    md->md_iterations);
			}
		}
		/*
		 * If md_iterations is equal to 0, user don't want PKCS#5v2.
		 */
		if (md->md_iterations == 0) {
			g_eli_crypto_hmac_update(&ctx, md->md_salt,
			    sizeof(md->md_salt));
			g_eli_crypto_hmac_update(&ctx, buf1, strlen(buf1));
		} else /* if (md->md_iterations > 0) */ {
			unsigned char dkey[G_ELI_USERKEYLEN];

			pkcs5v2_genkey(dkey, sizeof(dkey), md->md_salt,
			    sizeof(md->md_salt), buf1, md->md_iterations);
			g_eli_crypto_hmac_update(&ctx, dkey, sizeof(dkey));
			bzero(dkey, sizeof(dkey));
		}
		bzero(buf1, sizeof(buf1));
	}
	g_eli_crypto_hmac_final(&ctx, key, 0);
	return (key);
}

static int
eli_metadata_read(struct gctl_req *req, const char *prov,
    struct g_eli_metadata *md)
{
	unsigned char sector[sizeof(struct g_eli_metadata)];
	int error;

	if (g_get_sectorsize(prov) == 0) {
		int fd;

		/* This is a file probably. */
		fd = open(prov, O_RDONLY);
		if (fd == -1) {
			gctl_error(req, "Cannot open %s: %s.", prov,
			    strerror(errno));
			return (-1);
		}
		if (read(fd, sector, sizeof(sector)) != sizeof(sector)) {
			gctl_error(req, "Cannot read metadata from %s: %s.",
			    prov, strerror(errno));
			close(fd);
			return (-1);
		}
		close(fd);
	} else {
		/* This is a GEOM provider. */
		error = g_metadata_read(prov, sector, sizeof(sector),
		    G_ELI_MAGIC);
		if (error != 0) {
			gctl_error(req, "Cannot read metadata from %s: %s.",
			    prov, strerror(error));
			return (-1);
		}
	}
	if (eli_metadata_decode(sector, md) != 0) {
		gctl_error(req, "MD5 hash mismatch for %s.", prov);
		return (-1);
	}
	return (0);
}

static int
eli_metadata_store(struct gctl_req *req, const char *prov,
    struct g_eli_metadata *md)
{
	unsigned char sector[sizeof(struct g_eli_metadata)];
	int error;

	eli_metadata_encode(md, sector);
	if (g_get_sectorsize(prov) == 0) {
		int fd;

		/* This is a file probably. */
		fd = open(prov, O_WRONLY | O_TRUNC);
		if (fd == -1) {
			gctl_error(req, "Cannot open %s: %s.", prov,
			    strerror(errno));
			bzero(sector, sizeof(sector));
			return (-1);
		}
		if (write(fd, sector, sizeof(sector)) != sizeof(sector)) {
			gctl_error(req, "Cannot write metadata to %s: %s.",
			    prov, strerror(errno));
			bzero(sector, sizeof(sector));
			close(fd);
			return (-1);
		}
		close(fd);
	} else {
		/* This is a GEOM provider. */
		error = g_metadata_store(prov, sector, sizeof(sector));
		if (error != 0) {
			gctl_error(req, "Cannot write metadata to %s: %s.",
			    prov, strerror(errno));
			bzero(sector, sizeof(sector));
			return (-1);
		}
	}
	bzero(sector, sizeof(sector));
	return (0);
}

static void
eli_init(struct gctl_req *req)
{
	struct g_eli_metadata md;
	unsigned char sector[sizeof(struct g_eli_metadata)];
	unsigned char key[G_ELI_USERKEYLEN];
	const char *str, *prov;
	unsigned secsize;
	off_t mediasize;
	intmax_t val;
	int error, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	prov = gctl_get_ascii(req, "arg0");
	mediasize = g_get_mediasize(prov);
	secsize = g_get_sectorsize(prov);
	if (mediasize == 0 || secsize == 0) {
		gctl_error(req, "Cannot get informations about %s: %s.", prov,
		    strerror(errno));
		return;
	}

	bzero(&md, sizeof(md));
	strlcpy(md.md_magic, G_ELI_MAGIC, sizeof(md.md_magic));
	md.md_version = G_ELI_VERSION;
	md.md_flags = 0;
	if (gctl_get_int(req, "boot"))
		md.md_flags |= G_ELI_FLAG_BOOT;
	md.md_ealgo = CRYPTO_ALGORITHM_MIN - 1;
	str = gctl_get_ascii(req, "aalgo");
	if (strcmp(str, "none") != 0) {
		md.md_aalgo = g_eli_str2aalgo(str);
		if (md.md_aalgo >= CRYPTO_ALGORITHM_MIN &&
		    md.md_aalgo <= CRYPTO_ALGORITHM_MAX) {
			md.md_flags |= G_ELI_FLAG_AUTH;
		} else {
			/*
			 * For backward compatibility, check if the -a option
			 * was used to provide encryption algorithm.
			 */
			md.md_ealgo = g_eli_str2ealgo(str);
			if (md.md_ealgo < CRYPTO_ALGORITHM_MIN ||
			    md.md_ealgo > CRYPTO_ALGORITHM_MAX) {
				gctl_error(req,
				    "Invalid authentication algorithm.");
				return;
			} else {
				fprintf(stderr, "warning: The -e option, not "
				    "the -a option is now used to specify "
				    "encryption algorithm to use.\n");
			}
		}
	}
	if (md.md_ealgo < CRYPTO_ALGORITHM_MIN ||
	    md.md_ealgo > CRYPTO_ALGORITHM_MAX) {
		str = gctl_get_ascii(req, "ealgo");
		md.md_ealgo = g_eli_str2ealgo(str);
		if (md.md_ealgo < CRYPTO_ALGORITHM_MIN ||
		    md.md_ealgo > CRYPTO_ALGORITHM_MAX) {
			gctl_error(req, "Invalid encryption algorithm.");
			return;
		}
	}
	val = gctl_get_intmax(req, "keylen");
	md.md_keylen = val;
	md.md_keylen = g_eli_keylen(md.md_ealgo, md.md_keylen);
	if (md.md_keylen == 0) {
		gctl_error(req, "Invalid key length.");
		return;
	}
	md.md_provsize = mediasize;

	val = gctl_get_intmax(req, "iterations");
	if (val != -1) {
		int nonewpassphrase;

		/*
		 * Don't allow to set iterations when there will be no
		 * passphrase.
		 */
		nonewpassphrase = gctl_get_int(req, "nonewpassphrase");
		if (nonewpassphrase) {
			gctl_error(req,
			    "Options -i and -P are mutually exclusive.");
			return;
		}
	}
	md.md_iterations = val;

	val = gctl_get_intmax(req, "sectorsize");
	if (val == 0)
		md.md_sectorsize = secsize;
	else {
		if (val < 0 || (val % secsize) != 0) {
			gctl_error(req, "Invalid sector size.");
			return;
		}
		md.md_sectorsize = val;
	}

	md.md_keys = 0x01;
	arc4rand(md.md_salt, sizeof(md.md_salt));
	arc4rand(md.md_mkeys, sizeof(md.md_mkeys));

	/* Generate user key. */
	if (eli_genkey(req, &md, key, 1) == NULL) {
		bzero(key, sizeof(key));
		bzero(&md, sizeof(md));
		return;
	}

	/* Encrypt the first and the only Master Key. */
	error = g_eli_mkey_encrypt(md.md_ealgo, key, md.md_keylen, md.md_mkeys);
	bzero(key, sizeof(key));
	if (error != 0) {
		bzero(&md, sizeof(md));
		gctl_error(req, "Cannot encrypt Master Key: %s.",
		    strerror(error));
		return;
	}

	eli_metadata_encode(&md, sector);
	bzero(&md, sizeof(md));
	error = g_metadata_store(prov, sector, sizeof(sector));
	bzero(sector, sizeof(sector));
	if (error != 0) {
		gctl_error(req, "Cannot store metadata on %s: %s.", prov,
		    strerror(error));
		return;
	}
	if (verbose)
		printf("Metadata value stored on %s.\n", prov);
}

static void
eli_attach(struct gctl_req *req)
{
	struct g_eli_metadata md;
	unsigned char key[G_ELI_USERKEYLEN];
	const char *prov;
	int nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	prov = gctl_get_ascii(req, "arg0");

	if (eli_metadata_read(req, prov, &md) == -1)
		return;

	if (eli_genkey(req, &md, key, 0) == NULL) {
		bzero(key, sizeof(key));
		return;
	}

	gctl_ro_param(req, "key", sizeof(key), key);
	if (gctl_issue(req) == NULL) {
		if (verbose)
			printf("Attched to %s.\n", prov);
	}
	bzero(key, sizeof(key));
}

static void
eli_configure_detached(struct gctl_req *req, const char *prov, int boot)
{
	struct g_eli_metadata md;

	if (eli_metadata_read(req, prov, &md) == -1)
		return;

	if (boot && (md.md_flags & G_ELI_FLAG_BOOT)) {
		if (verbose)
			printf("BOOT flag already configured for %s.\n", prov);
	} else if (!boot && !(md.md_flags & G_ELI_FLAG_BOOT)) {
		if (verbose)
			printf("BOOT flag not configured for %s.\n", prov);
	} else {
		if (boot)
			md.md_flags |= G_ELI_FLAG_BOOT;
		else
			md.md_flags &= ~G_ELI_FLAG_BOOT;
		eli_metadata_store(req, prov, &md);
	}
	bzero(&md, sizeof(md));
}

static void
eli_configure(struct gctl_req *req)
{
	const char *prov;
	int i, nargs, boot, noboot;

	nargs = gctl_get_int(req, "nargs");
	if (nargs == 0) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	boot = gctl_get_int(req, "boot");
	noboot = gctl_get_int(req, "noboot");

	if (boot && noboot) {
		gctl_error(req, "Options -b and -B are mutually exclusive.");
		return;
	}
	if (!boot && !noboot) {
		gctl_error(req, "No option given.");
		return;
	}

	/* First attached providers. */
	gctl_issue(req);
	/* Now the rest. */
	for (i = 0; i < nargs; i++) {
		prov = gctl_get_ascii(req, "arg%d", i);
		if (!eli_is_attached(prov))
			eli_configure_detached(req, prov, boot);
	}
}

static void
eli_setkey_attached(struct gctl_req *req, struct g_eli_metadata *md)
{
	unsigned char key[G_ELI_USERKEYLEN];
	intmax_t val;

	val = gctl_get_intmax(req, "iterations");
	/* Check if iterations number should be changed. */
	if (val != -1)
		md->md_iterations = val;

	/* Generate key for Master Key encryption. */
	if (eli_genkey(req, md, key, 1) == NULL) {
		bzero(key, sizeof(key));
		return;
	}

	gctl_ro_param(req, "key", sizeof(key), key);
	gctl_issue(req);
	bzero(key, sizeof(key));
}

static void
eli_setkey_detached(struct gctl_req *req, const char *prov,
 struct g_eli_metadata *md)
{
	unsigned char key[G_ELI_USERKEYLEN], mkey[G_ELI_DATAIVKEYLEN];
	unsigned char *mkeydst;
	intmax_t val;
	unsigned nkey;
	int error;

	if (md->md_keys == 0) {
		gctl_error(req, "No valid keys on %s.", prov);
		return;
	}

	/* Generate key for Master Key decryption. */
	if (eli_genkey(req, md, key, 0) == NULL) {
		bzero(key, sizeof(key));
		return;
	}

	/* Decrypt Master Key. */
	error = g_eli_mkey_decrypt(md, key, mkey, &nkey);
	bzero(key, sizeof(key));
	if (error != 0) {
		bzero(md, sizeof(*md));
		if (error == -1)
			gctl_error(req, "Wrong key for %s.", prov);
		else /* if (error > 0) */ {
			gctl_error(req, "Cannot decrypt Master Key: %s.",
			    strerror(error));
		}
		return;
	}
	if (verbose)
		printf("Decrypted Master Key %u.\n", nkey);

	val = gctl_get_intmax(req, "keyno");
	if (val != -1)
		nkey = val;
#if 0
	else
		; /* Use the key number which was found during decryption. */
#endif
	if (nkey >= G_ELI_MAXMKEYS) {
		gctl_error(req, "Invalid '%s' argument.", "keyno");
		return;
	}

	val = gctl_get_intmax(req, "iterations");
	/* Check if iterations number should and can be changed. */
	if (val != -1) {
		if (bitcount32(md->md_keys) != 1) {
			gctl_error(req, "To be able to use '-i' option, only "
			    "one key can be defined.");
			return;
		}
		if (md->md_keys != (1 << nkey)) {
			gctl_error(req, "Only already defined key can be "
			    "changed when '-i' option is used.");
			return;
		}
		md->md_iterations = val;
	}

	mkeydst = md->md_mkeys + nkey * G_ELI_MKEYLEN;
	md->md_keys |= (1 << nkey);

	bcopy(mkey, mkeydst, sizeof(mkey));
	bzero(mkey, sizeof(mkey));

	/* Generate key for Master Key encryption. */
	if (eli_genkey(req, md, key, 1) == NULL) {
		bzero(key, sizeof(key));
		bzero(md, sizeof(*md));
		return;
	}

	/* Encrypt the Master-Key with the new key. */
	error = g_eli_mkey_encrypt(md->md_ealgo, key, md->md_keylen, mkeydst);
	bzero(key, sizeof(key));
	if (error != 0) {
		bzero(md, sizeof(*md));
		gctl_error(req, "Cannot encrypt Master Key: %s.",
		    strerror(error));
		return;
	}

	/* Store metadata with fresh key. */
	eli_metadata_store(req, prov, md);
	bzero(md, sizeof(*md));
}

static void
eli_setkey(struct gctl_req *req)
{
	struct g_eli_metadata md;
	const char *prov;
	int nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	prov = gctl_get_ascii(req, "arg0");

	if (eli_metadata_read(req, prov, &md) == -1)
		return;

	if (eli_is_attached(prov))
		eli_setkey_attached(req, &md);
	else
		eli_setkey_detached(req, prov, &md);
}

static void
eli_delkey_attached(struct gctl_req *req, const char *prov __unused)
{

	gctl_issue(req);
}

static void
eli_delkey_detached(struct gctl_req *req, const char *prov)
{
	struct g_eli_metadata md;
	unsigned char *mkeydst;
	intmax_t val;
	unsigned nkey;
	int all, force;

	if (eli_metadata_read(req, prov, &md) == -1)
		return;

	all = gctl_get_int(req, "all");
	if (all)
		arc4rand(md.md_mkeys, sizeof(md.md_mkeys));
	else {
		force = gctl_get_int(req, "force");
		val = gctl_get_intmax(req, "keyno");
		if (val == -1) {
			gctl_error(req, "Key number has to be specified.");
			return;
		}
		nkey = val;
		if (nkey >= G_ELI_MAXMKEYS) {
			gctl_error(req, "Invalid '%s' argument.", "keyno");
			return;
		}
		if (!(md.md_keys & (1 << nkey)) && !force) {
			gctl_error(req, "Master Key %u is not set.", nkey);
			return;
		}
		md.md_keys &= ~(1 << nkey);
		if (md.md_keys == 0 && !force) {
			gctl_error(req, "This is the last Master Key. Use '-f' "
			    "option if you really want to remove it.");
			return;
		}
		mkeydst = md.md_mkeys + nkey * G_ELI_MKEYLEN;
		arc4rand(mkeydst, G_ELI_MKEYLEN);
	}

	eli_metadata_store(req, prov, &md);
	bzero(&md, sizeof(md));
}

static void
eli_delkey(struct gctl_req *req)
{
	const char *prov;
	int nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 1) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	prov = gctl_get_ascii(req, "arg0");

	if (eli_is_attached(prov))
		eli_delkey_attached(req, prov);
	else
		eli_delkey_detached(req, prov);
}

static void
eli_kill_detached(struct gctl_req *req, const char *prov)
{
	struct g_eli_metadata md;
	int error;

	/*
	 * NOTE: Maybe we should verify if this is geli provider first,
	 *       but 'kill' command is quite critical so better don't waste
	 *       the time.
	 */
#if 0
	error = g_metadata_read(prov, (unsigned char *)&md, sizeof(md),
	    G_ELI_MAGIC);
	if (error != 0) {
		gctl_error(req, "Cannot read metadata from %s: %s.", prov,
		    strerror(error));
		return;
	}
#endif

	arc4rand((unsigned char *)&md, sizeof(md));
	error = g_metadata_store(prov, (unsigned char *)&md, sizeof(md));
	if (error != 0) {
		gctl_error(req, "Cannot write metadata to %s: %s.", prov,
		    strerror(error));
	}
}

static void
eli_kill(struct gctl_req *req)
{
	const char *prov;
	int i, nargs, all;

	nargs = gctl_get_int(req, "nargs");
	all = gctl_get_int(req, "all");
	if (!all && nargs == 0) {
		gctl_error(req, "Too few arguments.");
		return;
	}
	/*
	 * How '-a' option combine with a list of providers:
	 * Delete Master Keys from all attached providers:
	 * geli kill -a
	 * Delete Master Keys from all attached provider and from
	 * detached da0 and da1:
	 * geli kill -a da0 da1
	 * Delete Master Keys from (attached or detached) da0 and da1:
	 * geli kill da0 da1
	 */

	/* First detached provider. */
	for (i = 0; i < nargs; i++) {
		prov = gctl_get_ascii(req, "arg%d", i);
		if (!eli_is_attached(prov))
			eli_kill_detached(req, prov);
	}
	/* Now attached providers. */
	gctl_issue(req);
}

static void
eli_backup(struct gctl_req *req)
{
	struct g_eli_metadata md;
	const char *file, *prov;
	unsigned secsize;
	unsigned char *sector;
	off_t mediasize;
	int nargs, filefd, provfd;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 2) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	prov = gctl_get_ascii(req, "arg0");
	file = gctl_get_ascii(req, "arg1");

	provfd = filefd = -1;
	sector = NULL;
	secsize = 0;

	provfd = open(prov, O_RDONLY);
	if (provfd == -1 && errno == ENOENT && prov[0] != '/') {
		char devprov[MAXPATHLEN];

		snprintf(devprov, sizeof(devprov), "%s%s", _PATH_DEV, prov);
		provfd = open(devprov, O_RDONLY);
	}
	if (provfd == -1) {
		gctl_error(req, "Cannot open %s: %s.", prov, strerror(errno));
		return;
	}
	filefd = open(file, O_WRONLY | O_TRUNC | O_CREAT, 0600);
	if (filefd == -1) {
		gctl_error(req, "Cannot open %s: %s.", file, strerror(errno));
		goto out;
	}

	mediasize = g_get_mediasize(prov);
	secsize = g_get_sectorsize(prov);
	if (mediasize == 0 || secsize == 0) {
		gctl_error(req, "Cannot get informations about %s: %s.", prov,
		    strerror(errno));
		return;
	}

	sector = malloc(secsize);
	if (sector == NULL) {
		gctl_error(req, "Cannot allocate memory.");
		return;
	}

	/* Read metadata from the provider. */
	if (pread(provfd, sector, secsize, mediasize - secsize) !=
	    (ssize_t)secsize) {
		gctl_error(req, "Cannot read metadata: %s.", strerror(errno));
		goto out;
	}
	/* Check if this is geli provider. */
	if (eli_metadata_decode(sector, &md) != 0) {
		gctl_error(req, "MD5 hash mismatch: not a geli provider?");
		goto out;
	}
	/* Write metadata to the destination file. */
	if (write(filefd, sector, secsize) != (ssize_t)secsize) {
		gctl_error(req, "Cannot write to %s: %s.", file,
		    strerror(errno));
		goto out;
	}
out:
	if (provfd > 0)
		close(provfd);
	if (filefd > 0)
		close(filefd);
	if (sector != NULL) {
		bzero(sector, secsize);
		free(sector);
	}
}

static void
eli_restore(struct gctl_req *req)
{
	struct g_eli_metadata md;
	const char *file, *prov;
	unsigned char *sector;
	unsigned secsize;
	off_t mediasize;
	int nargs, filefd, provfd;

	nargs = gctl_get_int(req, "nargs");
	if (nargs != 2) {
		gctl_error(req, "Invalid number of arguments.");
		return;
	}
	file = gctl_get_ascii(req, "arg0");
	prov = gctl_get_ascii(req, "arg1");

	provfd = filefd = -1;
	sector = NULL;
	secsize = 0;

	filefd = open(file, O_RDONLY);
	if (filefd == -1) {
		gctl_error(req, "Cannot open %s: %s.", file, strerror(errno));
		goto out;
	}
	provfd = open(prov, O_WRONLY);
	if (provfd == -1 && errno == ENOENT && prov[0] != '/') {
		char devprov[MAXPATHLEN];

		snprintf(devprov, sizeof(devprov), "%s%s", _PATH_DEV, prov);
		provfd = open(devprov, O_WRONLY);
	}
	if (provfd == -1) {
		gctl_error(req, "Cannot open %s: %s.", prov, strerror(errno));
		return;
	}

	mediasize = g_get_mediasize(prov);
	secsize = g_get_sectorsize(prov);
	if (mediasize == 0 || secsize == 0) {
		gctl_error(req, "Cannot get informations about %s: %s.", prov,
		    strerror(errno));
		return;
	}

	sector = malloc(secsize);
	if (sector == NULL) {
		gctl_error(req, "Cannot allocate memory.");
		return;
	}

	/* Read metadata from the backup file. */
	if (read(filefd, sector, secsize) != (ssize_t)secsize) {
		gctl_error(req, "Cannot read from %s: %s.", file,
		    strerror(errno));
		goto out;
	}
	/* Check if this file contains geli metadata. */
	if (eli_metadata_decode(sector, &md) != 0) {
		gctl_error(req, "MD5 hash mismatch: not a geli backup file?");
		goto out;
	}
	/* Write metadata from the provider. */
	if (pwrite(provfd, sector, secsize, mediasize - secsize) !=
	    (ssize_t)secsize) {
		gctl_error(req, "Cannot write metadata: %s.", strerror(errno));
		goto out;
	}
out:
	if (provfd > 0)
		close(provfd);
	if (filefd > 0)
		close(filefd);
	if (sector != NULL) {
		bzero(sector, secsize);
		free(sector);
	}
}

static void
eli_clear(struct gctl_req *req)
{
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_clear(name, G_ELI_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Cannot clear metadata on %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (verbose)
			printf("Metadata cleared on %s.\n", name);
	}
}

static void
eli_dump(struct gctl_req *req)
{
	struct g_eli_metadata md, tmpmd;
	const char *name;
	int error, i, nargs;

	nargs = gctl_get_int(req, "nargs");
	if (nargs < 1) {
		gctl_error(req, "Too few arguments.");
		return;
	}

	for (i = 0; i < nargs; i++) {
		name = gctl_get_ascii(req, "arg%d", i);
		error = g_metadata_read(name, (unsigned char *)&tmpmd,
		    sizeof(tmpmd), G_ELI_MAGIC);
		if (error != 0) {
			fprintf(stderr, "Cannot read metadata from %s: %s.\n",
			    name, strerror(error));
			gctl_error(req, "Not fully done.");
			continue;
		}
		if (eli_metadata_decode((unsigned char *)&tmpmd, &md) != 0) {
			fprintf(stderr, "MD5 hash mismatch for %s, skipping.\n",
			    name);
			gctl_error(req, "Not fully done.");
			continue;
		}
		printf("Metadata on %s:\n", name);
		eli_metadata_dump(&md);
		printf("\n");
	}
}
