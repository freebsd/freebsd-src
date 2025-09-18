/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012-2014 Baptiste Daroussin <bapt@FreeBSD.org>
 * Copyright (c) 2013 Bryan Drewery <bdrewery@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <archive.h>
#include <archive_entry.h>
#include <assert.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fetch.h>
#include <getopt.h>
#include <libutil.h>
#include <paths.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ucl.h>

#include "pkg.h"

#include "dns_utils.h"
#include "config.h"
#include "hash.h"

#define	PKGSIGN_MARKER	"$PKGSIGN:"

static const struct pkgsign_impl {
	const char			*pi_name;
	const struct pkgsign_ops	*pi_ops;
} pkgsign_builtins[] = {
	{
		.pi_name = "rsa",
		.pi_ops = &pkgsign_rsa,
	},
	{
		.pi_name = "ecc",
		.pi_ops = &pkgsign_ecc,
	},
	{
		.pi_name = "ecdsa",
		.pi_ops = &pkgsign_ecc,
	},
	{
		.pi_name = "eddsa",
		.pi_ops = &pkgsign_ecc,
	},
};

typedef enum {
	HASH_UNKNOWN,
	HASH_SHA256,
} hash_t;

struct fingerprint {
	hash_t type;
	char *name;
	char hash[BUFSIZ];
	STAILQ_ENTRY(fingerprint) next;
};

static const char *bootstrap_name = "pkg.pkg";

STAILQ_HEAD(fingerprint_list, fingerprint);

static int debug;

static int
pkgsign_new(const char *name, struct pkgsign_ctx **ctx)
{
	const struct pkgsign_impl *impl;
	const struct pkgsign_ops *ops;
	struct pkgsign_ctx *nctx;
	size_t ctx_size;
	int ret;

	assert(*ctx == NULL);
	ops = NULL;
	for (size_t i = 0; i < nitems(pkgsign_builtins); i++) {
		impl = &pkgsign_builtins[i];

		if (strcmp(name, impl->pi_name) == 0) {
			ops = impl->pi_ops;
			break;
		}
	}

	if (ops == NULL)
		return (ENOENT);

	ctx_size = ops->pkgsign_ctx_size;
	if (ctx_size == 0)
		ctx_size = sizeof(*nctx);
	assert(ctx_size >= sizeof(*nctx));

	nctx = calloc(1, ctx_size);
	if (nctx == NULL)
		err(EXIT_FAILURE, "calloc");
	nctx->impl = impl;

	ret = 0;
	if (ops->pkgsign_new != NULL)
		ret = (*ops->pkgsign_new)(name, nctx);

	if (ret != 0) {
		free(nctx);
		return (ret);
	}

	*ctx = nctx;
	return (0);
}

static bool
pkgsign_verify_cert(const struct pkgsign_ctx *ctx, int fd, const char *sigfile,
    const unsigned char *key, int keylen, unsigned char *sig, int siglen)
{

	return ((*ctx->impl->pi_ops->pkgsign_verify_cert)(ctx, fd, sigfile,
	    key, keylen, sig, siglen));
}

static bool
pkgsign_verify_data(const struct pkgsign_ctx *ctx, const char *data,
    size_t datasz, const char *sigfile, const unsigned char *key, int keylen,
    unsigned char *sig, int siglen)
{

	return ((*ctx->impl->pi_ops->pkgsign_verify_data)(ctx, data, datasz,
	    sigfile, key, keylen, sig, siglen));
}


static int
extract_pkg_static(int fd, char *p, int sz)
{
	struct archive *a;
	struct archive_entry *ae;
	char *end;
	int ret, r;

	ret = -1;
	a = archive_read_new();
	if (a == NULL) {
		warn("archive_read_new");
		return (ret);
	}
	archive_read_support_filter_all(a);
	archive_read_support_format_tar(a);

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		goto cleanup;
	}

	if (archive_read_open_fd(a, fd, 4096) != ARCHIVE_OK) {
		warnx("archive_read_open_fd: %s", archive_error_string(a));
		goto cleanup;
	}

	ae = NULL;
	while ((r = archive_read_next_header(a, &ae)) == ARCHIVE_OK) {
		end = strrchr(archive_entry_pathname(ae), '/');
		if (end == NULL)
			continue;

		if (strcmp(end, "/pkg-static") == 0) {
			r = archive_read_extract(a, ae,
			    ARCHIVE_EXTRACT_OWNER | ARCHIVE_EXTRACT_PERM |
			    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_ACL |
			    ARCHIVE_EXTRACT_FFLAGS | ARCHIVE_EXTRACT_XATTR);
			strlcpy(p, archive_entry_pathname(ae), sz);
			break;
		}
	}

	if (r == ARCHIVE_OK)
		ret = 0;
	else
		warnx("failed to extract pkg-static: %s",
		    archive_error_string(a));

cleanup:
	archive_read_free(a);
	return (ret);

}

static int
install_pkg_static(const char *path, const char *pkgpath, bool force)
{
	int pstat;
	pid_t pid;

	switch ((pid = fork())) {
	case -1:
		return (-1);
	case 0:
		if (force)
			execl(path, "pkg-static", "add", "-f", pkgpath,
			    (char *)NULL);
		else
			execl(path, "pkg-static", "add", pkgpath,
			    (char *)NULL);
		_exit(1);
	default:
		break;
	}

	while (waitpid(pid, &pstat, 0) == -1)
		if (errno != EINTR)
			return (-1);

	if (WEXITSTATUS(pstat))
		return (WEXITSTATUS(pstat));
	else if (WIFSIGNALED(pstat))
		return (128 & (WTERMSIG(pstat)));
	return (pstat);
}

static int
fetch_to_fd(struct repository *repo, const char *url, char *path, const char *fetchOpts)
{
	struct url *u;
	struct dns_srvinfo *mirrors, *current;
	struct url_stat st;
	FILE *remote;
	/* To store _https._tcp. + hostname + \0 */
	int fd;
	int retry, max_retry;
	ssize_t r;
	char buf[10240];
	char zone[MAXHOSTNAMELEN + 13];

	max_retry = 3;
	current = mirrors = NULL;
	remote = NULL;

	if ((fd = mkstemp(path)) == -1) {
		warn("mkstemp()");
		return (-1);
	}

	retry = max_retry;

	if ((u = fetchParseURL(url)) == NULL) {
		warn("fetchParseURL('%s')", url);
		return (-1);
	}

	while (remote == NULL) {
		if (retry == max_retry) {
			if (strcmp(u->scheme, "file") != 0 &&
			    repo->mirror_type == MIRROR_SRV) {
				snprintf(zone, sizeof(zone),
				    "_%s._tcp.%s", u->scheme, u->host);
				mirrors = dns_getsrvinfo(zone);
				current = mirrors;
			}
		}

		if (mirrors != NULL) {
			strlcpy(u->host, current->host, sizeof(u->host));
			u->port = current->port;
		}

		remote = fetchXGet(u, &st, fetchOpts);
		if (remote == NULL) {
			--retry;
			if (retry <= 0)
				goto fetchfail;
			if (mirrors != NULL) {
				current = current->next;
				if (current == NULL)
					current = mirrors;
			}
		}
	}

	while ((r = fread(buf, 1, sizeof(buf), remote)) > 0) {
		if (write(fd, buf, r) != r) {
			warn("write()");
			goto fetchfail;
		}
	}

	if (r != 0) {
		warn("An error occurred while fetching pkg(8)");
		goto fetchfail;
	}

	if (ferror(remote))
		goto fetchfail;

	goto cleanup;

fetchfail:
	if (fd != -1) {
		close(fd);
		fd = -1;
		unlink(path);
	}

cleanup:
	if (remote != NULL)
		fclose(remote);

	return fd;
}

static struct fingerprint *
parse_fingerprint(ucl_object_t *obj)
{
	const ucl_object_t *cur;
	ucl_object_iter_t it = NULL;
	const char *function, *fp, *key;
	struct fingerprint *f;
	hash_t fct = HASH_UNKNOWN;

	function = fp = NULL;

	while ((cur = ucl_iterate_object(obj, &it, true))) {
		key = ucl_object_key(cur);
		if (cur->type != UCL_STRING)
			continue;
		if (strcasecmp(key, "function") == 0) {
			function = ucl_object_tostring(cur);
			continue;
		}
		if (strcasecmp(key, "fingerprint") == 0) {
			fp = ucl_object_tostring(cur);
			continue;
		}
	}

	if (fp == NULL || function == NULL)
		return (NULL);

	if (strcasecmp(function, "sha256") == 0)
		fct = HASH_SHA256;

	if (fct == HASH_UNKNOWN) {
		warnx("Unsupported hashing function: %s", function);
		return (NULL);
	}

	f = calloc(1, sizeof(struct fingerprint));
	f->type = fct;
	strlcpy(f->hash, fp, sizeof(f->hash));

	return (f);
}

static void
free_fingerprint_list(struct fingerprint_list* list)
{
	struct fingerprint *fingerprint, *tmp;

	STAILQ_FOREACH_SAFE(fingerprint, list, next, tmp) {
		free(fingerprint->name);
		free(fingerprint);
	}
	free(list);
}

static struct fingerprint *
load_fingerprint(const char *dir, const char *filename)
{
	ucl_object_t *obj = NULL;
	struct ucl_parser *p = NULL;
	struct fingerprint *f;
	char path[MAXPATHLEN];

	f = NULL;

	snprintf(path, MAXPATHLEN, "%s/%s", dir, filename);

	p = ucl_parser_new(0);
	if (!ucl_parser_add_file(p, path)) {
		warnx("%s: %s", path, ucl_parser_get_error(p));
		ucl_parser_free(p);
		return (NULL);
	}

	obj = ucl_parser_get_object(p);

	if (obj->type == UCL_OBJECT)
		f = parse_fingerprint(obj);

	if (f != NULL)
		f->name = strdup(filename);

	ucl_object_unref(obj);
	ucl_parser_free(p);

	return (f);
}

static struct fingerprint_list *
load_fingerprints(const char *path, int *count)
{
	DIR *d;
	struct dirent *ent;
	struct fingerprint *finger;
	struct fingerprint_list *fingerprints;

	*count = 0;

	fingerprints = calloc(1, sizeof(struct fingerprint_list));
	if (fingerprints == NULL)
		return (NULL);
	STAILQ_INIT(fingerprints);

	if ((d = opendir(path)) == NULL) {
		free(fingerprints);

		return (NULL);
	}

	while ((ent = readdir(d))) {
		if (strcmp(ent->d_name, ".") == 0 ||
		    strcmp(ent->d_name, "..") == 0)
			continue;
		finger = load_fingerprint(path, ent->d_name);
		if (finger != NULL) {
			STAILQ_INSERT_TAIL(fingerprints, finger, next);
			++(*count);
		}
	}

	closedir(d);

	return (fingerprints);
}

char *
pkg_read_fd(int fd, size_t *osz)
{
	char *obuf;
	char buf[4096];
	FILE *fp;
	ssize_t r;

	obuf = NULL;
	*osz = 0;
	fp = open_memstream(&obuf, osz);
	if (fp == NULL)
		err(EXIT_FAILURE, "open_memstream()");

	while ((r = read(fd, buf, sizeof(buf))) >0) {
		fwrite(buf, 1, r, fp);
	}

	if (ferror(fp))
		errx(EXIT_FAILURE, "reading file");

	fclose(fp);

	return (obuf);
}

/*
 * Returns a copy of the signature type stored on the heap, and advances *bufp
 * past the type.
 */
static char *
parse_sigtype(char **bufp, size_t *bufszp)
{
	char *buf = *bufp;
	char *endp;
	char *sigtype;
	size_t bufsz = *bufszp;

	if (bufsz <= sizeof(PKGSIGN_MARKER) - 1 ||
	    strncmp(buf, PKGSIGN_MARKER, sizeof(PKGSIGN_MARKER) - 1) != 0)
		goto dflt;

	buf += sizeof(PKGSIGN_MARKER) - 1;
	endp = strchr(buf, '$');
	if (endp == NULL)
		goto dflt;

	sigtype = strndup(buf, endp - buf);
	*bufp = endp + 1;
	*bufszp -= *bufp - buf;

	return (sigtype);
dflt:
	return (strdup("rsa"));
}

static struct pubkey *
read_pubkey(int fd)
{
	struct pubkey *pk;
	char *osigb, *sigb, *sigtype;
	size_t sigsz;

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		return (NULL);
	}

	osigb = sigb = pkg_read_fd(fd, &sigsz);
	sigtype = parse_sigtype(&sigb, &sigsz);

	pk = calloc(1, sizeof(struct pubkey));
	pk->siglen = sigsz;
	pk->sig = calloc(1, pk->siglen);
	memcpy(pk->sig, sigb, pk->siglen);
	pk->sigtype = sigtype;
	free(osigb);

	return (pk);
}

static struct sig_cert *
parse_cert(int fd) {
	int my_fd;
	struct sig_cert *sc;
	FILE *fp, *sigfp, *certfp, *tmpfp, *typefp;
	char *line;
	char *sig, *cert, *type;
	size_t linecap, sigsz, certsz, typesz;
	ssize_t linelen;
	bool end_seen;

	sc = NULL;
	line = NULL;
	linecap = 0;
	sig = cert = type = NULL;
	sigfp = certfp = tmpfp = typefp = NULL;

	if (lseek(fd, 0, 0) == -1) {
		warn("lseek");
		return (NULL);
	}

	/* Duplicate the fd so that fclose(3) does not close it. */
	if ((my_fd = dup(fd)) == -1) {
		warnx("dup");
		return (NULL);
	}

	if ((fp = fdopen(my_fd, "rb")) == NULL) {
		warn("fdopen");
		close(my_fd);
		return (NULL);
	}

	sigsz = certsz = typesz = 0;
	sigfp = open_memstream(&sig, &sigsz);
	if (sigfp == NULL)
		err(EXIT_FAILURE, "open_memstream()");
	certfp = open_memstream(&cert, &certsz);
	if (certfp == NULL)
		err(EXIT_FAILURE, "open_memstream()");
	typefp = open_memstream(&type, &typesz);
	if (typefp == NULL)
		err(EXIT_FAILURE, "open_memstream()");

	end_seen = false;
	while ((linelen = getline(&line, &linecap, fp)) > 0) {
		if (strcmp(line, "SIGNATURE\n") == 0) {
			tmpfp = sigfp;
			continue;
		} else if (strcmp(line, "TYPE\n") == 0) {
			tmpfp = typefp;
			continue;
		} else if (strcmp(line, "CERT\n") == 0) {
			tmpfp = certfp;
			continue;
		} else if (strcmp(line, "END\n") == 0) {
			end_seen = true;
			break;
		}
		if (tmpfp != NULL)
			fwrite(line, 1, linelen, tmpfp);
	}

	fclose(fp);
	fclose(sigfp);
	fclose(certfp);
	fclose(typefp);

	sc = calloc(1, sizeof(struct sig_cert));
	sc->siglen = sigsz -1; /* Trim out unrelated trailing newline */
	sc->sig = sig;

	if (typesz == 0) {
		sc->type = strdup("rsa");
		free(type);
	} else {
		assert(type[typesz - 1] == '\n');
		type[typesz - 1] = '\0';
		sc->type = type;
	}

	/*
	 * cert could be DER-encoded rather than PEM, so strip off any trailing
	 * END marker if we ran over it.
	 */
	if (!end_seen && certsz > 4 &&
	    strcmp(&cert[certsz - 4], "END\n") == 0)
		certsz -= 4;
	sc->certlen = certsz;
	sc->cert = cert;

	return (sc);
}

static bool
verify_pubsignature(int fd_pkg, int fd_sig, struct repository *r)
{
	struct pubkey *pk;
	char *data;
	struct pkgsign_ctx *sctx;
	size_t datasz;
	bool ret;
	const char *pubkey;

	pk = NULL;
	sctx = NULL;
	data = NULL;
	ret = false;

	if (r != NULL) {
		if (r->pubkey == NULL) {
			warnx("No CONFIG_PUBKEY defined for %s", r->name);
			goto cleanup;
		}
		pubkey = r->pubkey;
	} else {
		if (config_string(PUBKEY, &pubkey) != 0) {
			warnx("No CONFIG_PUBKEY defined");
			goto cleanup;
		}
	}

	if ((pk = read_pubkey(fd_sig)) == NULL) {
		warnx("Error reading signature");
		goto cleanup;
	}

	if (lseek(fd_pkg, 0, SEEK_SET) == -1) {
		warn("lseek");
		goto cleanup;
	}

	if (strcmp(pk->sigtype, "rsa") == 0) {
		/* Future types shouldn't do this. */
		if ((data = sha256_fd(fd_pkg)) == NULL) {
			warnx("Error creating SHA256 hash for package");
			goto cleanup;
		}

		datasz = strlen(data);
	} else {
		if ((data = pkg_read_fd(fd_pkg, &datasz)) == NULL) {
			warnx("Failed to read package data");
			goto cleanup;
		}
	}

	if (pkgsign_new(pk->sigtype, &sctx) != 0) {
		warnx("Failed to fetch '%s' signer", pk->sigtype);
		goto cleanup;
	}

	/* Verify the signature. */
	printf("Verifying signature with public key %s.a.. ", r->pubkey);
	if (pkgsign_verify_data(sctx, data, datasz, r->pubkey, NULL, 0, pk->sig,
	    pk->siglen) == false) {
		fprintf(stderr, "Signature is not valid\n");
		goto cleanup;
	}

	ret = true;

cleanup:
	free(data);
	if (pk) {
		free(pk->sig);
		free(pk);
	}

	return (ret);
}

static bool
verify_signature(int fd_pkg, int fd_sig, struct repository *r)
{
	struct fingerprint_list *trusted, *revoked;
	struct fingerprint *fingerprint;
	struct sig_cert *sc;
	struct pkgsign_ctx *sctx;
	bool ret;
	int trusted_count, revoked_count;
	const char *fingerprints;
	char path[MAXPATHLEN];
	char *hash;

	hash = NULL;
	sc = NULL;
	sctx = NULL;
	trusted = revoked = NULL;
	ret = false;

	/* Read and parse fingerprints. */
	if (r != NULL) {
		if (r->fingerprints == NULL) {
			warnx("No FINGERPRINTS defined for %s", r->name);
			goto cleanup;
		}
		fingerprints = r->fingerprints;
	} else {
		if (config_string(FINGERPRINTS, &fingerprints) != 0) {
			warnx("No FINGERPRINTS defined");
			goto cleanup;
		}
	}

	snprintf(path, MAXPATHLEN, "%s/trusted", fingerprints);
	if ((trusted = load_fingerprints(path, &trusted_count)) == NULL) {
		warnx("Error loading trusted certificates");
		goto cleanup;
	}

	if (trusted_count == 0 || trusted == NULL) {
		fprintf(stderr, "No trusted certificates found.\n");
		goto cleanup;
	}

	snprintf(path, MAXPATHLEN, "%s/revoked", fingerprints);
	if ((revoked = load_fingerprints(path, &revoked_count)) == NULL) {
		warnx("Error loading revoked certificates");
		goto cleanup;
	}

	/* Read certificate and signature in. */
	if ((sc = parse_cert(fd_sig)) == NULL) {
		warnx("Error parsing certificate");
		goto cleanup;
	}
	/* Explicitly mark as non-trusted until proven otherwise. */
	sc->trusted = false;

	/* Parse signature and pubkey out of the certificate */
	hash = sha256_buf(sc->cert, sc->certlen);

	/* Check if this hash is revoked */
	if (revoked != NULL) {
		STAILQ_FOREACH(fingerprint, revoked, next) {
			if (strcasecmp(fingerprint->hash, hash) == 0) {
				fprintf(stderr, "The package was signed with "
				    "revoked certificate %s\n",
				    fingerprint->name);
				goto cleanup;
			}
		}
	}

	STAILQ_FOREACH(fingerprint, trusted, next) {
		if (strcasecmp(fingerprint->hash, hash) == 0) {
			sc->trusted = true;
			sc->name = strdup(fingerprint->name);
			break;
		}
	}

	if (sc->trusted == false) {
		fprintf(stderr, "No trusted fingerprint found matching "
		    "package's certificate\n");
		goto cleanup;
	}

	if (pkgsign_new(sc->type, &sctx) != 0) {
		fprintf(stderr, "Failed to fetch 'rsa' signer\n");
		goto cleanup;
	}

	/* Verify the signature. */
	printf("Verifying signature with trusted certificate %s... ", sc->name);
	if (pkgsign_verify_cert(sctx, fd_pkg, NULL, sc->cert, sc->certlen,
	    sc->sig, sc->siglen) == false) {
		fprintf(stderr, "Signature is not valid\n");
		goto cleanup;
	}

	ret = true;

cleanup:
	free(hash);
	if (trusted)
		free_fingerprint_list(trusted);
	if (revoked)
		free_fingerprint_list(revoked);
	if (sc) {
		free(sc->cert);
		free(sc->sig);
		free(sc->name);
		free(sc);
	}

	return (ret);
}

static int
bootstrap_pkg(bool force, const char *fetchOpts, struct repository *repo)
{
	int fd_pkg, fd_sig;
	int ret;
	char url[MAXPATHLEN];
	char tmppkg[MAXPATHLEN];
	char tmpsig[MAXPATHLEN];
	const char *packagesite;
	char pkgstatic[MAXPATHLEN];

	fd_sig = -1;
	ret = -1;

	printf("Bootstrapping pkg from %s, please wait...\n", repo->url);

	/* Support pkg+http:// for PACKAGESITE which is the new format
	   in 1.2 to avoid confusion on why http://pkg.FreeBSD.org has
	   no A record. */
	packagesite = repo->url;
	if (strncmp(URL_SCHEME_PREFIX, packagesite,
	    strlen(URL_SCHEME_PREFIX)) == 0)
		packagesite += strlen(URL_SCHEME_PREFIX);

	snprintf(url, MAXPATHLEN, "%s/Latest/%s", packagesite, bootstrap_name);
	snprintf(tmppkg, MAXPATHLEN, "%s/%s.XXXXXX",
	    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP,
	    bootstrap_name);
	if ((fd_pkg = fetch_to_fd(repo, url, tmppkg, fetchOpts)) == -1)
		goto fetchfail;

	if (repo->signature_type == SIGNATURE_FINGERPRINT) {
		snprintf(tmpsig, MAXPATHLEN, "%s/%s.sig.XXXXXX",
		    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP,
		    bootstrap_name);
		snprintf(url, MAXPATHLEN, "%s/Latest/%s.sig",
		    packagesite, bootstrap_name);

		if ((fd_sig = fetch_to_fd(repo, url, tmpsig, fetchOpts)) == -1) {
			fprintf(stderr, "Signature for pkg not "
			    "available.\n");
			goto fetchfail;
		}

		if (verify_signature(fd_pkg, fd_sig, repo) == false)
			goto cleanup;
	} else if (repo->signature_type == SIGNATURE_PUBKEY) {
		snprintf(tmpsig, MAXPATHLEN,
		    "%s/%s.pubkeysig.XXXXXX",
		    getenv("TMPDIR") ? getenv("TMPDIR") : _PATH_TMP,
		    bootstrap_name);
		snprintf(url, MAXPATHLEN, "%s/Latest/%s.pubkeysig",
		    packagesite, bootstrap_name);

		if ((fd_sig = fetch_to_fd(repo, url, tmpsig, fetchOpts)) == -1) {
			fprintf(stderr, "Signature for pkg not "
			    "available.\n");
			goto fetchfail;
		}

		if (verify_pubsignature(fd_pkg, fd_sig, repo) == false)
			goto cleanup;
	}

	if ((ret = extract_pkg_static(fd_pkg, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, tmppkg, force);

	goto cleanup;

fetchfail:
	warnx("Error fetching %s: %s", url, fetchLastErrString);
	if (fetchLastErrCode == FETCH_RESOLV) {
		fprintf(stderr, "Address resolution failed for %s.\n", packagesite);
	} else {
		fprintf(stderr, "A pre-built version of pkg could not be found for "
		    "your system.\n");
	}

cleanup:
	if (fd_sig != -1) {
		close(fd_sig);
		unlink(tmpsig);
	}

	if (fd_pkg != -1) {
		close(fd_pkg);
		unlink(tmppkg);
	}

	return (ret);
}

static const char confirmation_message[] =
"The package management tool is not yet installed on your system.\n"
"Do you want to fetch and install it now? [y/N]: ";

static const char non_interactive_message[] =
"The package management tool is not yet installed on your system.\n"
"Please set ASSUME_ALWAYS_YES=yes environment variable to be able to bootstrap "
"in non-interactive (stdin not being a tty)\n";

static const char args_bootstrap_message[] =
"Too many arguments\n"
"Usage: pkg [-4|-6] bootstrap [-f] [-y]\n";

static int
pkg_query_yes_no(void)
{
	int ret, c;

	fflush(stdout);
	c = getchar();

	if (c == 'y' || c == 'Y')
		ret = 1;
	else
		ret = 0;

	while (c != '\n' && c != EOF)
		c = getchar();

	return (ret);
}

static int
bootstrap_pkg_local(const char *pkgpath, bool force)
{
	char path[MAXPATHLEN];
	char pkgstatic[MAXPATHLEN];
	const char *signature_type;
	int fd_pkg, fd_sig, ret;

	fd_sig = -1;
	ret = -1;

	fd_pkg = open(pkgpath, O_RDONLY);
	if (fd_pkg == -1)
		err(EXIT_FAILURE, "Unable to open %s", pkgpath);

	if (config_string(SIGNATURE_TYPE, &signature_type) != 0) {
		warnx("Error looking up SIGNATURE_TYPE");
		goto cleanup;
	}
	if (signature_type != NULL &&
	    strcasecmp(signature_type, "NONE") != 0) {
		if (strcasecmp(signature_type, "FINGERPRINTS") == 0) {

			snprintf(path, sizeof(path), "%s.sig", pkgpath);

			if ((fd_sig = open(path, O_RDONLY)) == -1) {
				fprintf(stderr, "Signature for pkg not "
				    "available.\n");
				goto cleanup;
			}

			if (verify_signature(fd_pkg, fd_sig, NULL) == false)
				goto cleanup;

		} else if (strcasecmp(signature_type, "PUBKEY") == 0) {

			snprintf(path, sizeof(path), "%s.pubkeysig", pkgpath);

			if ((fd_sig = open(path, O_RDONLY)) == -1) {
				fprintf(stderr, "Signature for pkg not "
				    "available.\n");
				goto cleanup;
			}

			if (verify_pubsignature(fd_pkg, fd_sig, NULL) == false)
				goto cleanup;

		} else {
			warnx("Signature type %s is not supported for "
			    "bootstrapping.", signature_type);
			goto cleanup;
		}
	}

	if ((ret = extract_pkg_static(fd_pkg, pkgstatic, MAXPATHLEN)) == 0)
		ret = install_pkg_static(pkgstatic, pkgpath, force);

cleanup:
	close(fd_pkg);
	if (fd_sig != -1)
		close(fd_sig);

	return (ret);
}

#define	PKG_NAME	"pkg"
#define	PKG_DEVEL_NAME	PKG_NAME "-devel"
#define	PKG_PKG		PKG_NAME "."

static bool
pkg_is_pkg_pkg(const char *pkg)
{
	char *vstart, *basename;
	size_t namelen;

	/* Strip path. */
	if ((basename = strrchr(pkg, '/')) != NULL)
		pkg = basename + 1;

	/*
	 * Chop off the final "-" (version delimiter) and check the name that
	 * precedes it.  If we didn't have a version delimiter, it must be the
	 * pkg.$archive short form but we'll check it anyways.  pkg-devel short
	 * form will look like a pkg archive with 'devel' version, but that's
	 * OK.  We otherwise assumed that non-pkg packages will always have a
	 * version component.
	 */
	vstart = strrchr(pkg, '-');
	if (vstart == NULL) {
		return (strlen(pkg) > sizeof(PKG_PKG) - 1 &&
		    strncmp(pkg, PKG_PKG, sizeof(PKG_PKG) - 1) == 0);
	}

	namelen = vstart - pkg;
	if (namelen == sizeof(PKG_NAME) - 1 &&
	    strncmp(pkg, PKG_NAME, sizeof(PKG_NAME) - 1) == 0)
		return (true);
	if (namelen == sizeof(PKG_DEVEL_NAME) - 1 &&
	    strncmp(pkg, PKG_DEVEL_NAME, sizeof(PKG_DEVEL_NAME) - 1) == 0)
		return (true);
	return (false);
}

int
main(int argc, char *argv[])
{
	char pkgpath[MAXPATHLEN];
	char **original_argv;
	const char *pkgarg, *repo_name;
	bool activation_test, add_pkg, bootstrap_only, force, yes;
	signed char ch;
	const char *fetchOpts;
	struct repositories *repositories;

	activation_test = false;
	add_pkg = false;
	bootstrap_only = false;
	fetchOpts = "";
	force = false;
	original_argv = argv;
	pkgarg = NULL;
	repo_name = NULL;
	yes = false;

	struct option longopts[] = {
		{ "debug",		no_argument,		NULL,	'd' },
		{ "only-ipv4",		no_argument,		NULL,	'4' },
		{ "only-ipv6",		no_argument,		NULL,	'6' },
		{ NULL,			0,			NULL,	0   },
	};

	snprintf(pkgpath, MAXPATHLEN, "%s/sbin/pkg", getlocalbase());

	while ((ch = getopt_long(argc, argv, "+:dN46", longopts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			debug++;
			break;
		case 'N':
			activation_test = true;
			break;
		case '4':
			fetchOpts = "4";
			break;
		case '6':
			fetchOpts = "6";
			break;
		default:
			break;
		}
	}
	if (debug > 1)
		fetchDebug = 1;

	argc -= optind;
	argv += optind;

	if (argc >= 1) {
		if (strcmp(argv[0], "bootstrap") == 0) {
			bootstrap_only = true;
		} else if (strcmp(argv[0], "add") == 0) {
			add_pkg = true;
		}

		optreset = 1;
		optind = 1;
		if (bootstrap_only || add_pkg) {
			struct option sub_longopts[] = {
				{ "force",	no_argument,	NULL,	'f' },
				{ "yes",	no_argument,	NULL,	'y' },
				{ NULL,		0,		NULL,	0   },
			};
			while ((ch = getopt_long(argc, argv, "+:fr:y",
			    sub_longopts, NULL)) != -1) {
				switch (ch) {
				case 'f':
					force = true;
					break;
				case 'r':
					repo_name = optarg;
					break;
				case 'y':
					yes = true;
					break;
				case ':':
					fprintf(stderr, "Option -%c requires an argument\n", optopt);
					exit(EXIT_FAILURE);
					break;
				default:
					break;
				}
			}
		} else {
			/*
			 * Parse -y and --yes regardless of the pkg subcommand
			 * specified. This is necessary to make, for example,
			 * `pkg install -y foobar` work as expected when pkg is
			 * not yet bootstrapped.
			 */
			struct option sub_longopts[] = {
				{ "yes",	no_argument,	NULL,	'y' },
				{ NULL,		0,		NULL,	0   },
			};
			while ((ch = getopt_long(argc, argv, "+:y",
			    sub_longopts, NULL)) != -1) {
				switch (ch) {
				case 'y':
					yes = true;
					break;
				default:
					break;
				}
			}

		}
		argc -= optind;
		argv += optind;

		if (bootstrap_only && argc > 0) {
			fprintf(stderr, args_bootstrap_message);
			exit(EXIT_FAILURE);
		}

		if (add_pkg) {
			if (argc < 1) {
				fprintf(stderr, "Path to pkg.pkg required\n");
				exit(EXIT_FAILURE);
			} else if (argc == 1 && pkg_is_pkg_pkg(argv[0])) {
				pkgarg = argv[0];
			} else {
				/*
				 * If the target package is not pkg.pkg
				 * or there is more than one target package,
				 * this is not a local bootstrap request.
				 */
				add_pkg = false;
			}
		}
	}

	if ((bootstrap_only && force) || access(pkgpath, X_OK) == -1) {
		struct repository *repo;
		int ret = 0;
		/*
		 * To allow 'pkg -N' to be used as a reliable test for whether
		 * a system is configured to use pkg, don't bootstrap pkg
		 * when that option is passed.
		 */
		if (activation_test)
			errx(EXIT_FAILURE, "pkg is not installed");

		config_init(repo_name);

		if (add_pkg) {
			assert(pkgarg != NULL);
			if (access(pkgarg, R_OK) == -1) {
				fprintf(stderr, "No such file: %s\n", pkgarg);
				exit(EXIT_FAILURE);
			}
			if (bootstrap_pkg_local(pkgarg, force) != 0)
				exit(EXIT_FAILURE);
			exit(EXIT_SUCCESS);
		}
		/*
		 * Do not ask for confirmation if either of stdin or stdout is
		 * not tty. Check the environment to see if user has answer
		 * tucked in there already.
		 */
		if (!yes)
			config_bool(ASSUME_ALWAYS_YES, &yes);
		if (!yes) {
			if (!isatty(fileno(stdin))) {
				fprintf(stderr, non_interactive_message);
				exit(EXIT_FAILURE);
			}

			printf("%s", confirmation_message);
			if (pkg_query_yes_no() == 0)
				exit(EXIT_FAILURE);
		}
		repositories = config_get_repositories();
		STAILQ_FOREACH(repo, repositories, next) {
			if ((ret = bootstrap_pkg(force, fetchOpts, repo)) == 0)
				break;
		}
		if (ret != 0)
			exit(EXIT_FAILURE);
		config_finish();

		if (bootstrap_only)
			exit(EXIT_SUCCESS);
	} else if (bootstrap_only) {
		printf("pkg already bootstrapped at %s\n", pkgpath);
		exit(EXIT_SUCCESS);
	}

	execv(pkgpath, original_argv);

	/* NOT REACHED */
	return (EXIT_FAILURE);
}
