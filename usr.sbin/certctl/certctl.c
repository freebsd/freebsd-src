/*-
 * Copyright (c) 2023-2025 Dag-Erling Smørgrav <des@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/tree.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <paths.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/ssl.h>

#define info(fmt, ...)							\
	do {								\
		if (verbose)						\
			fprintf(stderr, fmt "\n", ##__VA_ARGS__);	\
	} while (0)

static char *
xasprintf(const char *fmt, ...)
{
	va_list ap;
	char *str;
	int ret;

	va_start(ap, fmt);
	ret = vasprintf(&str, fmt, ap);
	va_end(ap);
	if (ret < 0 || str == NULL)
		err(1, NULL);
	return (str);
}

static char *
xstrdup(const char *str)
{
	char *dup;

	if ((dup = strdup(str)) == NULL)
		err(1, NULL);
	return (dup);
}

static void usage(void);

static bool dryrun;
static bool longnames;
static bool nobundle;
static bool unprivileged;
static bool verbose;

static const char *localbase;
static const char *destdir;
static const char *distbase;
static const char *metalog;

static const char *uname = "root";
static const char *gname = "wheel";

static const char *const default_trusted_paths[] = {
	"/usr/share/certs/trusted",
	"%L/share/certs/trusted",
	"%L/share/certs",
	NULL
};
static char **trusted_paths;

static const char *const default_untrusted_paths[] = {
	"/usr/share/certs/untrusted",
	"%L/share/certs/untrusted",
	NULL
};
static char **untrusted_paths;

static char *trusted_dest;
static char *untrusted_dest;
static char *bundle_dest;

#define SSL_PATH		"/etc/ssl"
#define TRUSTED_DIR		"certs"
#define TRUSTED_PATH		SSL_PATH "/" TRUSTED_DIR
#define UNTRUSTED_DIR		"untrusted"
#define UNTRUSTED_PATH		SSL_PATH "/" UNTRUSTED_DIR
#define LEGACY_DIR		"blacklisted"
#define LEGACY_PATH		SSL_PATH "/" LEGACY_DIR
#define BUNDLE_FILE		"cert.pem"
#define BUNDLE_PATH		SSL_PATH "/" BUNDLE_FILE

static FILE *mlf;

/*
 * Create a directory and its parents as needed.
 */
static void
mkdirp(const char *dir)
{
	struct stat sb;
	const char *sep;
	char *parent;

	if (stat(dir, &sb) == 0)
		return;
	if ((sep = strrchr(dir, '/')) != NULL) {
		parent = xasprintf("%.*s", (int)(sep - dir), dir);
		mkdirp(parent);
		free(parent);
	}
	info("creating %s", dir);
	if (mkdir(dir, 0755) != 0)
		err(1, "mkdir %s", dir);
}

/*
 * Remove duplicate and trailing slashes from a path.
 */
static char *
normalize_path(const char *str)
{
	char *buf, *dst;

	if ((buf = malloc(strlen(str) + 1)) == NULL)
		err(1, NULL);
	for (dst = buf; *str != '\0'; dst++) {
		if ((*dst = *str++) == '/') {
			while (*str == '/')
				str++;
			if (*str == '\0')
				break;
		}
	}
	*dst = '\0';
	return (buf);
}

/*
 * Split a colon-separated list into a NULL-terminated array.
 */
static char **
split_paths(const char *str)
{
	char **paths;
	const char *p, *q;
	unsigned int i, n;

	for (p = str, n = 1; *p; p++) {
		if (*p == ':')
			n++;
	}
	if ((paths = calloc(n + 1, sizeof(*paths))) == NULL)
		err(1, NULL);
	for (p = q = str, i = 0; i < n; i++, p = q + 1) {
		q = strchrnul(p, ':');
		if ((paths[i] = strndup(p, q - p)) == NULL)
			err(1, NULL);
	}
	return (paths);
}

/*
 * Expand %L into LOCALBASE and prefix DESTDIR and DISTBASE as needed.
 */
static char *
expand_path(const char *template)
{
	if (template[0] == '%' && template[1] == 'L')
		return (xasprintf("%s%s%s", destdir, localbase, template + 2));
	return (xasprintf("%s%s%s", destdir, distbase, template));
}

/*
 * Expand an array of paths.
 */
static char **
expand_paths(const char *const *templates)
{
	char **paths;
	unsigned int i, n;

	for (n = 0; templates[n] != NULL; n++)
		continue;
	if ((paths = calloc(n + 1, sizeof(*paths))) == NULL)
		err(1, NULL);
	for (i = 0; i < n; i++)
		paths[i] = expand_path(templates[i]);
	return (paths);
}

/*
 * If destdir is a prefix of path, returns a pointer to the rest of path,
 * otherwise returns path.
 *
 * Note that this intentionally does not strip distbase from the path!
 * Unlike destdir, distbase is expected to be included in the metalog.
 */
static const char *
unexpand_path(const char *path)
{
	const char *p = path;
	const char *q = destdir;

	while (*p && *p == *q) {
		p++;
		q++;
	}
	return (*q == '\0' && *p == '/' ? p : path);
}

/*
 * X509 certificate in a rank-balanced tree.
 */
struct cert {
	RB_ENTRY(cert) entry;
	unsigned long hash;
	char *name;
	X509 *x509;
	char *path;
};

static void
free_cert(struct cert *cert)
{
	free(cert->name);
	X509_free(cert->x509);
	free(cert->path);
	free(cert);
}

static int
certcmp(const struct cert *a, const struct cert *b)
{
	return (X509_cmp(a->x509, b->x509));
}

RB_HEAD(cert_tree, cert);
static struct cert_tree trusted = RB_INITIALIZER(&trusted);
static struct cert_tree untrusted = RB_INITIALIZER(&untrusted);
RB_GENERATE_STATIC(cert_tree, cert, entry, certcmp);

static void
free_certs(struct cert_tree *tree)
{
	struct cert *cert, *tmp;

	RB_FOREACH_SAFE(cert, cert_tree, tree, tmp) {
		RB_REMOVE(cert_tree, tree, cert);
		free_cert(cert);
	}
}

static struct cert *
find_cert(struct cert_tree *haystack, X509 *x509)
{
	struct cert needle = { .x509 = x509 };

	return (RB_FIND(cert_tree, haystack, &needle));
}

/*
 * File containing a certificate in a rank-balanced tree sorted by
 * certificate hash and disambiguating counter.  This is needed because
 * the certificate hash function is prone to collisions, necessitating a
 * counter to distinguish certificates that hash to the same value.
 */
struct file {
	RB_ENTRY(file) entry;
	const struct cert *cert;
	unsigned int c;
};

static int
filecmp(const struct file *a, const struct file *b)
{
	if (a->cert->hash > b->cert->hash)
		return (1);
	if (a->cert->hash < b->cert->hash)
		return (-1);
	return (a->c - b->c);
}

RB_HEAD(file_tree, file);
RB_GENERATE_STATIC(file_tree, file, entry, filecmp);

/*
 * Lexicographical sort for scandir().
 */
static int
lexisort(const struct dirent **d1, const struct dirent **d2)
{
	return (strcmp((*d1)->d_name, (*d2)->d_name));
}

/*
 * Read certificate(s) from a single file and insert them into a tree.
 * Ignore certificates that already exist in the tree.  If exclude is not
 * null, also ignore certificates that exist in exclude.
 *
 * Returns the number certificates added to the tree, or -1 on failure.
 */
static int
read_cert(const char *path, struct cert_tree *tree, struct cert_tree *exclude)
{
	FILE *f;
	X509 *x509;
	X509_NAME *name;
	struct cert *cert;
	unsigned long hash;
	int len, ni, no;

	if ((f = fopen(path, "r")) == NULL) {
		warn("%s", path);
		return (-1);
	}
	for (ni = no = 0;
	     (x509 = PEM_read_X509(f, NULL, NULL, NULL)) != NULL;
	     ni++) {
		hash = X509_subject_name_hash(x509);
		if (exclude && find_cert(exclude, x509)) {
			info("%08lx: excluded", hash);
			X509_free(x509);
			continue;
		}
		if (find_cert(tree, x509)) {
			info("%08lx: duplicate", hash);
			X509_free(x509);
			continue;
		}
		if ((cert = calloc(1, sizeof(*cert))) == NULL)
			err(1, NULL);
		cert->x509 = x509;
		name = X509_get_subject_name(x509);
		cert->hash = X509_NAME_hash_ex(name, NULL, NULL, NULL);
		len = X509_NAME_get_text_by_NID(name, NID_commonName,
		    NULL, 0);
		if (len > 0) {
			if ((cert->name = malloc(len + 1)) == NULL)
				err(1, NULL);
			X509_NAME_get_text_by_NID(name, NID_commonName,
			    cert->name, len + 1);
		} else {
			/* fallback for certificates without CN */
			cert->name = X509_NAME_oneline(name, NULL, 0);
		}
		cert->path = xstrdup(unexpand_path(path));
		if (RB_INSERT(cert_tree, tree, cert) != NULL)
			errx(1, "unexpected duplicate");
		info("%08lx: %s", cert->hash, cert->name);
		no++;
	}
	/*
	 * ni is the number of certificates we found in the file.
	 * no is the number of certificates that weren't already in our
	 * tree or on the exclusion list.
	 */
	if (ni == 0)
		warnx("%s: no valid certificates found", path);
	fclose(f);
	return (no);
}

/*
 * Load all certificates found in the specified path into a tree,
 * optionally excluding those that already exist in a different tree.
 *
 * Returns the number of certificates added to the tree, or -1 on failure.
 */
static int
read_certs(const char *path, struct cert_tree *tree, struct cert_tree *exclude)
{
	struct stat sb;
	char *paths[] = { (char *)(uintptr_t)path, NULL };
	FTS *fts;
	FTSENT *ent;
	int fts_options = FTS_LOGICAL | FTS_NOCHDIR;
	int ret, total = 0;

	if (stat(path, &sb) != 0) {
		return (-1);
	} else if (!S_ISDIR(sb.st_mode)) {
		errno = ENOTDIR;
		return (-1);
	}
	if ((fts = fts_open(paths, fts_options, NULL)) == NULL)
		err(1, "fts_open()");
	while ((ent = fts_read(fts)) != NULL) {
		if (ent->fts_info != FTS_F) {
			if (ent->fts_info == FTS_ERR)
				warnc(ent->fts_errno, "fts_read()");
			continue;
		}
		info("found %s", ent->fts_path);
		ret = read_cert(ent->fts_path, tree, exclude);
		if (ret > 0)
			total += ret;
	}
	fts_close(fts);
	return (total);
}

/*
 * Save the contents of a cert tree to disk.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
write_certs(const char *dir, struct cert_tree *tree)
{
	struct file_tree files = RB_INITIALIZER(&files);
	struct cert *cert;
	struct file *file, *tmp;
	struct dirent **dents, **ent;
	char *path, *tmppath = NULL;
	FILE *f;
	mode_t mode = 0444;
	int cmp, d, fd, ndents, ret = 0;

	/*
	 * Start by generating unambiguous file names for each certificate
	 * and storing them in lexicographical order
	 */
	RB_FOREACH(cert, cert_tree, tree) {
		if ((file = calloc(1, sizeof(*file))) == NULL)
			err(1, NULL);
		file->cert = cert;
		for (file->c = 0; file->c < INT_MAX; file->c++)
			if (RB_INSERT(file_tree, &files, file) == NULL)
				break;
		if (file->c == INT_MAX)
			errx(1, "unable to disambiguate %08lx", cert->hash);
		free(cert->path);
		cert->path = xasprintf("%08lx.%d", cert->hash, file->c);
	}
	/*
	 * Open and scan the directory.
	 */
	if ((d = open(dir, O_DIRECTORY | O_RDONLY)) < 0 ||
#ifdef BOOTSTRAPPING
	    (ndents = scandir(dir, &dents, NULL, lexisort))
#else
	    (ndents = fdscandir(d, &dents, NULL, lexisort))
#endif
	    < 0)
		err(1, "%s", dir);
	/*
	 * Iterate over the directory listing and the certificate listing
	 * in parallel.  If the directory listing gets ahead of the
	 * certificate listing, we need to write the current certificate
	 * and advance the certificate listing.  If the certificate
	 * listing is ahead of the directory listing, we need to delete
	 * the current file and advance the directory listing.  If they
	 * are neck and neck, we have a match and could in theory compare
	 * the two, but in practice it's faster to just replace the
	 * current file with the current certificate (and advance both).
	 */
	ent = dents;
	file = RB_MIN(file_tree, &files);
	for (;;) {
		if (ent < dents + ndents) {
			/* skip directories */
			if ((*ent)->d_type == DT_DIR) {
				free(*ent++);
				continue;
			}
			if (file != NULL) {
				/* compare current dirent to current cert */
				path = file->cert->path;
				cmp = strcmp((*ent)->d_name, path);
			} else {
				/* trailing files in directory */
				path = NULL;
				cmp = -1;
			}
		} else {
			if (file != NULL) {
				/* trailing certificates */
				path = file->cert->path;
				cmp = 1;
			} else {
				/* end of both lists */
				path = NULL;
				break;
			}
		}
		if (cmp < 0) {
			/* a file on disk with no matching certificate */
			info("removing %s/%s", dir, (*ent)->d_name);
			if (!dryrun)
				(void)unlinkat(d, (*ent)->d_name, 0);
			free(*ent++);
			continue;
		}
		if (cmp == 0) {
			/* a file on disk with a matching certificate */
			info("replacing %s/%s", dir, (*ent)->d_name);
			if (dryrun) {
				fd = open(_PATH_DEVNULL, O_WRONLY);
			} else {
				tmppath = xasprintf(".%s", path);
				fd = openat(d, tmppath,
				    O_CREAT | O_WRONLY | O_TRUNC, mode);
				if (!unprivileged && fd >= 0)
					(void)fchmod(fd, mode);
			}
			free(*ent++);
		} else {
			/* a certificate with no matching file */
			info("writing %s/%s", dir, path);
			if (dryrun) {
				fd = open(_PATH_DEVNULL, O_WRONLY);
			} else {
				tmppath = xasprintf(".%s", path);
				fd = openat(d, tmppath,
				    O_CREAT | O_WRONLY | O_EXCL, mode);
			}
		}
		/* write the certificate */
		if (fd < 0 ||
		    (f = fdopen(fd, "w")) == NULL ||
		    !PEM_write_X509(f, file->cert->x509)) {
			if (tmppath != NULL && fd >= 0) {
				int serrno = errno;
				(void)unlinkat(d, tmppath, 0);
				errno = serrno;
			}
			err(1, "%s/%s", dir, tmppath ? tmppath : path);
		}
		/* rename temp file if applicable */
		if (tmppath != NULL) {
			if (ret == 0 && renameat(d, tmppath, d, path) != 0) {
				warn("%s/%s", dir, path);
				ret = -1;
			}
			if (ret != 0)
				(void)unlinkat(d, tmppath, 0);
			free(tmppath);
			tmppath = NULL;
		}
		fflush(f);
		/* emit metalog */
		if (mlf != NULL) {
			fprintf(mlf, ".%s/%s type=file "
			    "uname=%s gname=%s mode=%#o size=%ld\n",
			    unexpand_path(dir), path,
			    uname, gname, mode, ftell(f));
		}
		fclose(f);
		/* advance certificate listing */
		tmp = RB_NEXT(file_tree, &files, file);
		RB_REMOVE(file_tree, &files, file);
		free(file);
		file = tmp;
	}
	free(dents);
	close(d);
	return (ret);
}

/*
 * Save all certs in a tree to a single file (bundle).
 *
 * Returns 0 on success and -1 on failure.
 */
static int
write_bundle(const char *dir, const char *file, struct cert_tree *tree)
{
	struct cert *cert;
	char *tmpfile = NULL;
	FILE *f;
	int d, fd, ret = 0;
	mode_t mode = 0444;

	if (dir != NULL) {
		if ((d = open(dir, O_DIRECTORY | O_RDONLY)) < 0)
			err(1, "%s", dir);
	} else {
		dir = ".";
		d = AT_FDCWD;
	}
	info("writing %s/%s", dir, file);
	if (dryrun) {
		fd = open(_PATH_DEVNULL, O_WRONLY);
	} else {
		tmpfile = xasprintf(".%s", file);
		fd = openat(d, tmpfile, O_WRONLY | O_CREAT | O_EXCL, mode);
	}
	if (fd < 0 || (f = fdopen(fd, "w")) == NULL) {
		if (tmpfile != NULL && fd >= 0) {
			int serrno = errno;
			(void)unlinkat(d, tmpfile, 0);
			errno = serrno;
		}
		err(1, "%s/%s", dir, tmpfile ? tmpfile : file);
	}
	RB_FOREACH(cert, cert_tree, tree) {
		if (!PEM_write_X509(f, cert->x509)) {
			warn("%s/%s", dir, tmpfile ? tmpfile : file);
			ret = -1;
			break;
		}
	}
	if (tmpfile != NULL) {
		if (ret == 0 && renameat(d, tmpfile, d, file) != 0) {
			warn("%s/%s", dir, file);
			ret = -1;
		}
		if (ret != 0)
			(void)unlinkat(d, tmpfile, 0);
		free(tmpfile);
	}
	if (ret == 0 && mlf != NULL) {
		fprintf(mlf,
		    ".%s/%s type=file uname=%s gname=%s mode=%#o size=%ld\n",
		    unexpand_path(dir), file, uname, gname, mode, ftell(f));
	}
	fclose(f);
	if (d != AT_FDCWD)
		close(d);
	return (ret);
}

/*
 * Load trusted certificates.
 *
 * Returns the number of certificates loaded.
 */
static unsigned int
load_trusted(bool all, struct cert_tree *exclude)
{
	unsigned int i, n;
	int ret;

	/* load external trusted certs */
	for (i = n = 0; all && trusted_paths[i] != NULL; i++) {
		ret = read_certs(trusted_paths[i], &trusted, exclude);
		if (ret > 0)
			n += ret;
	}

	/* load installed trusted certs */
	ret = read_certs(trusted_dest, &trusted, exclude);
	if (ret > 0)
		n += ret;

	info("%d trusted certificates found", n);
	return (n);
}

/*
 * Load untrusted certificates.
 *
 * Returns the number of certificates loaded.
 */
static unsigned int
load_untrusted(bool all)
{
	char *path;
	unsigned int i, n;
	int ret;

	/* load external untrusted certs */
	for (i = n = 0; all && untrusted_paths[i] != NULL; i++) {
		ret = read_certs(untrusted_paths[i], &untrusted, NULL);
		if (ret > 0)
			n += ret;
	}

	/* load installed untrusted certs */
	ret = read_certs(untrusted_dest, &untrusted, NULL);
	if (ret > 0)
		n += ret;

	/* load legacy untrusted certs */
	path = expand_path(LEGACY_PATH);
	ret = read_certs(path, &untrusted, NULL);
	if (ret > 0) {
		warnx("certificates found in legacy directory %s",
		    path);
		n += ret;
	} else if (ret == 0) {
		warnx("legacy directory %s can safely be deleted",
		    path);
	}
	free(path);

	info("%d untrusted certificates found", n);
	return (n);
}

/*
 * Save trusted certificates.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
save_trusted(void)
{
	int ret;

	mkdirp(trusted_dest);
	ret = write_certs(trusted_dest, &trusted);
	return (ret);
}

/*
 * Save untrusted certificates.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
save_untrusted(void)
{
	int ret;

	mkdirp(untrusted_dest);
	ret = write_certs(untrusted_dest, &untrusted);
	return (ret);
}

/*
 * Save certificate bundle.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
save_bundle(void)
{
	char *dir, *file, *sep;
	int ret;

	if ((sep = strrchr(bundle_dest, '/')) == NULL) {
		dir = NULL;
		file = bundle_dest;
	} else {
		dir = xasprintf("%.*s", (int)(sep - bundle_dest), bundle_dest);
		file = sep + 1;
		mkdirp(dir);
	}
	ret = write_bundle(dir, file, &trusted);
	free(dir);
	return (ret);
}

/*
 * Save everything.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
save_all(void)
{
	int ret = 0;

	ret |= save_untrusted();
	ret |= save_trusted();
	if (!nobundle)
		ret |= save_bundle();
	return (ret);
}

/*
 * List the contents of a certificate tree.
 */
static void
list_certs(struct cert_tree *tree)
{
	struct cert *cert;
	char *path, *name;

	RB_FOREACH(cert, cert_tree, tree) {
		path = longnames ? NULL : strrchr(cert->path, '/');
		name = longnames ? NULL : strrchr(cert->name, '=');
		printf("%s\t%s\n", path ? path + 1 : cert->path,
		    name ? name + 1 : cert->name);
	}
}

/*
 * Load installed trusted certificates, then list them.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
certctl_list(int argc, char **argv __unused)
{
	if (argc > 1)
		usage();
	/* load trusted certificates */
	load_trusted(false, NULL);
	/* list them */
	list_certs(&trusted);
	free_certs(&trusted);
	return (0);
}

/*
 * Load installed untrusted certificates, then list them.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
certctl_untrusted(int argc, char **argv __unused)
{
	if (argc > 1)
		usage();
	/* load untrusted certificates */
	load_untrusted(false);
	/* list them */
	list_certs(&untrusted);
	free_certs(&untrusted);
	return (0);
}

/*
 * Load trusted and untrusted certificates from all sources, then
 * regenerate both the hashed directories and the bundle.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
certctl_rehash(int argc, char **argv __unused)
{
	int ret;

	if (argc > 1)
		usage();

	if (unprivileged && (mlf = fopen(metalog, "a")) == NULL) {
		warn("%s", metalog);
		return (-1);
	}

	/* load untrusted certs first */
	load_untrusted(true);

	/* load trusted certs, excluding any that are already untrusted */
	load_trusted(true, &untrusted);

	/* save everything */
	ret = save_all();

	/* clean up */
	free_certs(&untrusted);
	free_certs(&trusted);
	if (mlf != NULL)
		fclose(mlf);
	return (ret);
}

/*
 * Manually add one or more certificates to the list of trusted certificates.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
certctl_trust(int argc, char **argv)
{
	struct cert_tree extra = RB_INITIALIZER(&extra);
	struct cert *cert, *other, *tmp;
	unsigned int n;
	int i, ret;

	if (argc < 2)
		usage();

	/* load untrusted certs first */
	load_untrusted(true);

	/* load trusted certs, excluding any that are already untrusted */
	load_trusted(true, &untrusted);

	/* now load the additional trusted certificates */
	n = 0;
	for (i = 1; i < argc; i++) {
		ret = read_cert(argv[i], &extra, &trusted);
		if (ret > 0)
			n += ret;
	}
	if (n == 0) {
		warnx("no new trusted certificates found");
		free_certs(&untrusted);
		free_certs(&trusted);
		free_certs(&extra);
		return (0);
	}

	/*
	 * For each new trusted cert, move it from the extra list to the
	 * trusted list, then check if a matching certificate exists on
	 * the untrusted list.  If that is the case, warn the user, then
	 * remove the matching certificate from the untrusted list.
	 */
	RB_FOREACH_SAFE(cert, cert_tree, &extra, tmp) {
		RB_REMOVE(cert_tree, &extra, cert);
		RB_INSERT(cert_tree, &trusted, cert);
		if ((other = RB_FIND(cert_tree, &untrusted, cert)) != NULL) {
			warnx("%s was previously untrusted", cert->name);
			RB_REMOVE(cert_tree, &untrusted, other);
			free_cert(other);
		}
	}

	/* save everything */
	ret = save_all();

	/* clean up */
	free_certs(&untrusted);
	free_certs(&trusted);
	return (ret);
}

/*
 * Manually add one or more certificates to the list of untrusted
 * certificates.
 *
 * Returns 0 on success and -1 on failure.
 */
static int
certctl_untrust(int argc, char **argv)
{
	unsigned int n;
	int i, ret;

	if (argc < 2)
		usage();

	/* load untrusted certs first */
	load_untrusted(true);

	/* now load the additional untrusted certificates */
	n = 0;
	for (i = 1; i < argc; i++) {
		ret = read_cert(argv[i], &untrusted, NULL);
		if (ret > 0)
			n += ret;
	}
	if (n == 0) {
		warnx("no new untrusted certificates found");
		free_certs(&untrusted);
		return (0);
	}

	/* load trusted certs, excluding any that are already untrusted */
	load_trusted(true, &untrusted);

	/* save everything */
	ret = save_all();

	/* clean up */
	free_certs(&untrusted);
	free_certs(&trusted);
	return (ret);
}

static void
set_defaults(void)
{
	const char *value;
	char *str;
	size_t len;

	if (localbase == NULL &&
	    (localbase = getenv("LOCALBASE")) == NULL) {
		if ((str = malloc((len = PATH_MAX) + 1)) == NULL)
			err(1, NULL);
		while (sysctlbyname("user.localbase", str, &len, NULL, 0) < 0) {
			if (errno != ENOMEM)
				err(1, "sysctl(user.localbase)");
			if ((str = realloc(str, len + 1)) == NULL)
				err(1, NULL);
		}
		str[len] = '\0';
		localbase = str;
	}

	if (destdir == NULL &&
	    (destdir = getenv("DESTDIR")) == NULL)
		destdir = "";
	destdir = normalize_path(destdir);

	if (distbase == NULL &&
	    (distbase = getenv("DISTBASE")) == NULL)
		distbase = "";
	if (*distbase != '\0' && *distbase != '/')
		errx(1, "DISTBASE=%s does not begin with a slash", distbase);
	distbase = normalize_path(distbase);

	if (unprivileged && metalog == NULL &&
	    (metalog = getenv("METALOG")) == NULL)
		metalog = xasprintf("%s/METALOG", destdir);

	if (!verbose) {
		if ((value = getenv("CERTCTL_VERBOSE")) != NULL) {
			if (value[0] != '\0') {
				verbose = true;
			}
		}
	}

	if ((value = getenv("TRUSTPATH")) != NULL)
		trusted_paths = split_paths(value);
	else
		trusted_paths = expand_paths(default_trusted_paths);

	if ((value = getenv("UNTRUSTPATH")) != NULL)
		untrusted_paths = split_paths(value);
	else
		untrusted_paths = expand_paths(default_untrusted_paths);

	if ((value = getenv("TRUSTDESTDIR")) != NULL ||
	    (value = getenv("CERTDESTDIR")) != NULL)
		trusted_dest = normalize_path(value);
	else
		trusted_dest = expand_path(TRUSTED_PATH);

	if ((value = getenv("UNTRUSTDESTDIR")) != NULL)
		untrusted_dest = normalize_path(value);
	else
		untrusted_dest = expand_path(UNTRUSTED_PATH);

	if ((value = getenv("BUNDLE")) != NULL)
		bundle_dest = normalize_path(value);
	else
		bundle_dest = expand_path(BUNDLE_PATH);

	info("localbase:\t%s", localbase);
	info("destdir:\t%s", destdir);
	info("distbase:\t%s", distbase);
	info("unprivileged:\t%s", unprivileged ? "true" : "false");
	info("verbose:\t%s", verbose ? "true" : "false");
}

typedef int (*main_t)(int, char **);

static struct {
	const char	*name;
	main_t		 func;
} commands[] = {
	{ "list",	certctl_list },
	{ "untrusted",	certctl_untrusted },
	{ "rehash",	certctl_rehash },
	{ "untrust",	certctl_untrust },
	{ "trust",	certctl_trust },
	{ 0 },
};

static void
usage(void)
{
	fprintf(stderr, "usage: certctl [-lv] [-D destdir] [-d distbase] list\n"
	    "       certctl [-lv] [-D destdir] [-d distbase] untrusted\n"
	    "       certctl [-BnUv] [-D destdir] [-d distbase] [-M metalog] rehash\n"
	    "       certctl [-nv] [-D destdir] [-d distbase] untrust <file>\n"
	    "       certctl [-nv] [-D destdir] [-d distbase] trust <file>\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	const char *command;
	int opt;

	while ((opt = getopt(argc, argv, "BcD:d:g:lL:M:no:Uv")) != -1)
		switch (opt) {
		case 'B':
			nobundle = true;
			break;
		case 'c':
			/* ignored for compatibility */
			break;
		case 'D':
			destdir = optarg;
			break;
		case 'd':
			distbase = optarg;
			break;
		case 'g':
			gname = optarg;
			break;
		case 'l':
			longnames = true;
			break;
		case 'L':
			localbase = optarg;
			break;
		case 'M':
			metalog = optarg;
			break;
		case 'n':
			dryrun = true;
			break;
		case 'o':
			uname = optarg;
			break;
		case 'U':
			unprivileged = true;
			break;
		case 'v':
			verbose = true;
			break;
		default:
			usage();
		}

	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	command = *argv;

	if ((nobundle || unprivileged || metalog != NULL) &&
	    strcmp(command, "rehash") != 0)
		usage();
	if (!unprivileged && metalog != NULL) {
		warnx("-M may only be used in conjunction with -U");
		usage();
	}

	set_defaults();

	for (unsigned i = 0; commands[i].name != NULL; i++)
		if (strcmp(command, commands[i].name) == 0)
			exit(!!commands[i].func(argc, argv));
	usage();
}
