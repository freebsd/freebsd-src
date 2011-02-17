/*-
 * Copyright (c) 2002 John Rochester
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/utsname.h>

#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <fcntl.h>
#include <locale.h>
#include <langinfo.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define DEFAULT_MANPATH		"/usr/share/man"

#define TOP_LEVEL_DIR	0	/* signifies a top-level man directory */
#define MAN_SECTION_DIR	1	/* signifies a man section directory */
#define UNKNOWN		2	/* signifies an unclassifiable directory */

#define TEST_EXISTS	0x01
#define TEST_DIR	0x02
#define TEST_FILE	0x04
#define TEST_READABLE	0x08
#define TEST_WRITABLE	0x10

static int verbose;		/* -v flag: be verbose with warnings */
static int pretend;		/* -n, -p flags: print out what would be done
				   instead of actually doing it */
static int force;		/* -f flag: force overwriting all cat pages */
static int rm_junk;		/* -r flag: remove garbage pages */
static char *locale;		/* user's locale if -L is used */
static char *lang_locale;	/* short form of locale */
static const char *machine, *machine_arch;
static int exit_code;		/* exit code to use when finished */

/*
 * -T argument for nroff
 */
static const char *nroff_device = "ascii";

/*
 * Mapping from locale to nroff device
 */
static const char *locale_device[] = {
	"KOI8-R",	"koi8-r",
	"ISO8859-1",	"latin1",
	"ISO8859-15",	"latin1",
	NULL
};

#define	BZ2_CMD		"bzip2"
#define	BZ2_EXT		".bz2"
#define	BZ2CAT_CMD	"bz"
#define	GZ_CMD		"gzip"
#define	GZ_EXT		".gz"
#define	GZCAT_CMD	"z"
enum Ziptype {NONE, BZIP, GZIP};

static uid_t uid;
static int starting_dir;
static char tmp_file[MAXPATHLEN];
struct stat test_st;

/*
 * A hashtable is an array of chains composed of this entry structure.
 */
struct hash_entry {
	ino_t		inode_number;
	dev_t		device_number;
	const char	*data;
	struct hash_entry *next;
};

#define HASHTABLE_ALLOC	16384	/* allocation for hashtable (power of 2) */
#define HASH_MASK	(HASHTABLE_ALLOC - 1)

static struct hash_entry *visited[HASHTABLE_ALLOC];
static struct hash_entry *links[HASHTABLE_ALLOC];

/*
 * Inserts a string into a hashtable keyed by inode & device number.
 */
static void
insert_hashtable(struct hash_entry **table,
    ino_t inode_number,
    dev_t device_number,
    const char *data)
{
	struct hash_entry *new_entry;
	struct hash_entry **chain;

	new_entry = (struct hash_entry *) malloc(sizeof(struct hash_entry));
	if (new_entry == NULL)
		err(1, "can't insert into hashtable");
	chain = &table[inode_number & HASH_MASK];
	new_entry->inode_number = inode_number;
	new_entry->device_number = device_number;
	new_entry->data = data;
	new_entry->next = *chain;
	*chain = new_entry;
}

/*
 * Finds a string in a hashtable keyed by inode & device number.
 */
static const char *
find_hashtable(struct hash_entry **table,
    ino_t inode_number,
    dev_t device_number)
{
	struct hash_entry *chain;

	chain = table[inode_number & HASH_MASK];
	while (chain != NULL) {
		if (chain->inode_number == inode_number &&
		    chain->device_number == device_number)
			return chain->data;
		chain = chain->next;
	}
	return NULL;
}

static void
trap_signal(int sig __unused)
{
	if (tmp_file[0] != '\0')
		unlink(tmp_file);
	exit(1);
}

/*
 * Deals with junk files in the man or cat section directories.
 */
static void
junk(const char *mandir, const char *name, const char *reason)
{
	if (verbose)
		fprintf(stderr, "%s/%s: %s\n", mandir, name, reason);
	if (rm_junk) {
		fprintf(stderr, "rm %s/%s\n", mandir, name);
		if (!pretend && unlink(name) < 0)
			warn("%s/%s", mandir, name);
	}
}

/*
 * Returns TOP_LEVEL_DIR for .../man, MAN_SECTION_DIR for .../manXXX,
 * and UNKNOWN for everything else.
 */
static int
directory_type(char *dir)
{
	char *p;

	for (;;) {
		p = strrchr(dir, '/');
		if (p == NULL || p[1] != '\0')
			break;
		*p = '\0';
	}
	if (p == NULL)
		p = dir;
	else
		p++;
	if (strncmp(p, "man", 3) == 0) {
		p += 3;
		if (*p == '\0')
			return TOP_LEVEL_DIR;
		while (isalnum((unsigned char)*p) || *p == '_') {
			if (*++p == '\0')
				return MAN_SECTION_DIR;
		}
	}
	return UNKNOWN;
}

/*
 * Tests whether the given file name (without a preceding path)
 * is a proper man page name (like "mk-amd-map.8.gz").
 * Only alphanumerics and '_' are allowed after the last '.' and
 * the last '.' can't be the first or last characters.
 */
static int
is_manpage_name(char *name)
{
	char *lastdot = NULL;
	char *n = name;

	while (*n != '\0') {
		if (!isalnum((unsigned char)*n)) {
			switch (*n) {
			case '_':
				break;
			case '-':
			case '+':
			case '[':
			case ':':
				lastdot = NULL;
				break;
			case '.':
				lastdot = n;
				break;
			default:
				return 0;
			}
		}
		n++;
	}
	return lastdot > name && lastdot + 1 < n;
}

static int
is_bzipped(char *name)
{
	int len = strlen(name);
	return len >= 5 && strcmp(&name[len - 4], BZ2_EXT) == 0;
}

static int
is_gzipped(char *name)
{
	int len = strlen(name);
	return len >= 4 && strcmp(&name[len - 3], GZ_EXT) == 0;
}

/*
 * Converts manXXX to catXXX.
 */
static char *
get_cat_section(char *section)
{
	char *cat_section;

	cat_section = strdup(section);
	strncpy(cat_section, "cat", 3);
	return cat_section;
}

/*
 * Tests to see if the given directory has already been visited.
 */
static int
already_visited(char *mandir, char *dir, int count_visit)
{
	struct stat st;

	if (stat(dir, &st) < 0) {
		if (mandir != NULL)
			warn("%s/%s", mandir, dir);
		else
			warn("%s", dir);
		exit_code = 1;
		return 1;
	}
	if (find_hashtable(visited, st.st_ino, st.st_dev) != NULL) {
		if (mandir != NULL)
			warnx("already visited %s/%s", mandir, dir);
		else
			warnx("already visited %s", dir);
		return 1;
	}
	if (count_visit)
		insert_hashtable(visited, st.st_ino, st.st_dev, "");
	return 0;
}

/*
 * Returns a set of TEST_* bits describing a file's type and permissions.
 * If mod_time isn't NULL, it will contain the file's modification time.
 */
static int
test_path(char *name, time_t *mod_time)
{
	int result;

	if (stat(name, &test_st) < 0)
		return 0;
	result = TEST_EXISTS;
	if (mod_time != NULL)
		*mod_time = test_st.st_mtime;
	if (S_ISDIR(test_st.st_mode))
		result |= TEST_DIR;
	else if (S_ISREG(test_st.st_mode))
		result |= TEST_FILE;
	if (access(name, R_OK))
		result |= TEST_READABLE;
	if (access(name, W_OK))
		result |= TEST_WRITABLE;
	return result;
}

/*
 * Checks whether a file is a symbolic link.
 */
static int
is_symlink(char *path)
{
	struct stat st;

	return lstat(path, &st) >= 0 && S_ISLNK(st.st_mode);
}

/*
 * Tests to see if the given directory can be written to.
 */
static void
check_writable(char *mandir)
{
	if (verbose && !(test_path(mandir, NULL) & TEST_WRITABLE))
		fprintf(stderr, "%s: not writable - will only be able to write to existing cat directories\n", mandir);
}

/*
 * If the directory exists, attempt to make it writable, otherwise
 * attempt to create it.
 */
static int
make_writable_dir(char *mandir, char *dir)
{
	int test;

	if ((test = test_path(dir, NULL)) != 0) {
		if (!(test & TEST_WRITABLE) && chmod(dir, 0755) < 0) {
			warn("%s/%s: chmod", mandir, dir);
			exit_code = 1;
			return 0;
		}
	} else {
		if (verbose || pretend)
			fprintf(stderr, "mkdir %s\n", dir);
		if (!pretend) {
			unlink(dir);
			if (mkdir(dir, 0755) < 0) {
				warn("%s/%s: mkdir", mandir, dir);
				exit_code = 1;
				return 0;
			}
		}
	}
	return 1;
}

/*
 * Processes a single man page source by using nroff to create
 * the preformatted cat page.
 */
static void
process_page(char *mandir, char *src, char *cat, enum Ziptype zipped)
{
	int src_test, cat_test;
	time_t src_mtime, cat_mtime;
	char cmd[MAXPATHLEN];
	dev_t src_dev;
	ino_t src_ino;
	const char *link_name;

	src_test = test_path(src, &src_mtime);
	if (!(src_test & (TEST_FILE|TEST_READABLE))) {
		if (!(src_test & TEST_DIR)) {
			warnx("%s/%s: unreadable", mandir, src);
			exit_code = 1;
			if (rm_junk && is_symlink(src))
				junk(mandir, src, "bogus symlink");
		}
		return;
	}
	src_dev = test_st.st_dev;
	src_ino = test_st.st_ino;
	cat_test = test_path(cat, &cat_mtime);
	if (cat_test & (TEST_FILE|TEST_READABLE)) {
		if (!force && cat_mtime >= src_mtime) {
			if (verbose) {
				fprintf(stderr, "\t%s/%s: up to date\n",
				    mandir, src);
			}
			return;
		}
	}
	/*
	 * Is the man page a link to one we've already processed?
	 */
	if ((link_name = find_hashtable(links, src_ino, src_dev)) != NULL) {
		if (verbose || pretend) {
			fprintf(stderr, "%slink %s -> %s\n",
			    verbose ? "\t" : "", cat, link_name);
		}
		if (!pretend)
			link(link_name, cat);
		return;
	}
	insert_hashtable(links, src_ino, src_dev, strdup(cat));
	if (verbose || pretend) {
		fprintf(stderr, "%sformat %s -> %s\n",
		    verbose ? "\t" : "", src, cat);
		if (pretend)
			return;
	}
	snprintf(tmp_file, sizeof tmp_file, "%s.tmp", cat);
	snprintf(cmd, sizeof cmd,
	    "%scat %s | tbl | nroff -T%s -man | col | %s > %s.tmp",
	    zipped == BZIP ? BZ2CAT_CMD : zipped == GZIP ? GZCAT_CMD : "",
	    src, nroff_device,
	    zipped == BZIP ? BZ2_CMD : zipped == GZIP ? GZ_CMD : "cat",
	    cat);
	if (system(cmd) != 0)
		err(1, "formatting pipeline");
	if (rename(tmp_file, cat) < 0)
		warn("%s", cat);
	tmp_file[0] = '\0';
}

/*
 * Scan the man section directory for pages and process each one,
 * then check for junk in the corresponding cat section.
 */
static void
scan_section(char *mandir, char *section, char *cat_section)
{
	struct dirent **entries;
	char **expected = NULL;
	int npages;
	int nexpected = 0;
	int i, e;
	enum Ziptype zipped;
	char *page_name;
	char page_path[MAXPATHLEN];
	char cat_path[MAXPATHLEN];
	char zip_path[MAXPATHLEN];

	/*
	 * scan the man section directory for pages
	 */
	npages = scandir(section, &entries, NULL, alphasort);
	if (npages < 0) {
		warn("%s/%s", mandir, section);
		exit_code = 1;
		return;
	}
	if (verbose || rm_junk) {
		/*
		 * Maintain a list of all cat pages that should exist,
		 * corresponding to existing man pages.
		 */
		expected = (char **) calloc(npages, sizeof(char *));
	}
	for (i = 0; i < npages; free(entries[i++])) {
		page_name = entries[i]->d_name;
		snprintf(page_path, sizeof page_path, "%s/%s", section,
		    page_name);
		if (!is_manpage_name(page_name)) {
			if (!(test_path(page_path, NULL) & TEST_DIR)) {
				junk(mandir, page_path,
				    "invalid man page name");
			}
			continue;
		}
		zipped = is_bzipped(page_name) ? BZIP :
		    is_gzipped(page_name) ? GZIP : NONE;
		if (zipped != NONE) {
			snprintf(cat_path, sizeof cat_path, "%s/%s",
			    cat_section, page_name);
			if (expected != NULL)
				expected[nexpected++] = strdup(page_name);
			process_page(mandir, page_path, cat_path, zipped);
		} else {
			/*
			 * We've got an uncompressed man page,
			 * check to see if there's a (preferred)
			 * compressed one.
			 */
			snprintf(zip_path, sizeof zip_path, "%s%s",
			    page_path, GZ_EXT);
			if (test_path(zip_path, NULL) != 0) {
				junk(mandir, page_path,
				    "man page unused due to existing " GZ_EXT);
			} else {
				if (verbose) {
					fprintf(stderr,
						"warning, %s is uncompressed\n",
						page_path);
				}
				snprintf(cat_path, sizeof cat_path, "%s/%s",
				    cat_section, page_name);
				if (expected != NULL) {
					asprintf(&expected[nexpected++],
					    "%s", page_name);
				}
				process_page(mandir, page_path, cat_path, NONE);
			}
		}
	}
	free(entries);
	if (expected == NULL)
	    return;
	/*
	 * scan cat sections for junk
	 */
	npages = scandir(cat_section, &entries, NULL, alphasort);
	e = 0;
	for (i = 0; i < npages; free(entries[i++])) {
		const char *junk_reason;
		int cmp = 1;

		page_name = entries[i]->d_name;
		if (strcmp(page_name, ".") == 0 || strcmp(page_name, "..") == 0)
			continue;
		/*
		 * Keep the index into the expected cat page list
		 * ahead of the name we've found.
		 */
		while (e < nexpected &&
		    (cmp = strcmp(page_name, expected[e])) > 0)
			free(expected[e++]);
		if (cmp == 0)
			continue;
		/* we have an unexpected page */
		snprintf(cat_path, sizeof cat_path, "%s/%s", cat_section,
		    page_name);
		if (!is_manpage_name(page_name)) {
			if (test_path(cat_path, NULL) & TEST_DIR)
				continue;
			junk_reason = "invalid cat page name";
		} else if (!is_gzipped(page_name) && e + 1 < nexpected &&
		    strncmp(page_name, expected[e + 1], strlen(page_name)) == 0 &&
		    strlen(expected[e + 1]) == strlen(page_name) + 3) {
			junk_reason = "cat page unused due to existing " GZ_EXT;
		} else
			junk_reason = "cat page without man page";
		junk(mandir, cat_path, junk_reason);
	}
	free(entries);
	while (e < nexpected)
		free(expected[e++]);
	free(expected);
}


/*
 * Processes a single man section.
 */
static void
process_section(char *mandir, char *section)
{
	char *cat_section;

	if (already_visited(mandir, section, 1))
		return;
	if (verbose)
		fprintf(stderr, "  section %s\n", section);
	cat_section = get_cat_section(section);
	if (make_writable_dir(mandir, cat_section))
		scan_section(mandir, section, cat_section);
	free(cat_section);
}

static int
select_sections(const struct dirent *entry)
{
	char *name;
	int ret;

	name = strdup(entry->d_name);
	ret = directory_type(name) == MAN_SECTION_DIR;
	free(name);
	return (ret);
}

/*
 * Processes a single top-level man directory.  If section isn't NULL,
 * it will only process that section sub-directory, otherwise it will
 * process all of them.
 */
static void
process_mandir(char *dir_name, char *section)
{
	fchdir(starting_dir);
	if (already_visited(NULL, dir_name, section == NULL))
		return;
	check_writable(dir_name);
	if (verbose)
		fprintf(stderr, "man directory %s\n", dir_name);
	if (pretend)
		fprintf(stderr, "cd %s\n", dir_name);
	if (chdir(dir_name) < 0) {
		warn("%s: chdir", dir_name);
		exit_code = 1;
		return;
	}
	if (section != NULL) {
		process_section(dir_name, section);
	} else {
		struct dirent **entries;
		char *machine_dir, *arch_dir;
		int nsections;
		int i;

		nsections = scandir(".", &entries, select_sections, alphasort);
		if (nsections < 0) {
			warn("%s", dir_name);
			exit_code = 1;
			return;
		}
		for (i = 0; i < nsections; i++) {
			process_section(dir_name, entries[i]->d_name);
			asprintf(&machine_dir, "%s/%s", entries[i]->d_name,
			    machine);
			if (test_path(machine_dir, NULL) & TEST_DIR)
				process_section(dir_name, machine_dir);
			free(machine_dir);
			if (strcmp(machine_arch, machine) != 0) {
				asprintf(&arch_dir, "%s/%s", entries[i]->d_name,
				    machine_arch);
				if (test_path(arch_dir, NULL) & TEST_DIR)
					process_section(dir_name, arch_dir);
				free(arch_dir);
			}
			free(entries[i]);
		}
		free(entries);
	}
}

/*
 * Processes one argument, which may be a colon-separated list of
 * directories.
 */
static void
process_argument(const char *arg)
{
	char *dir;
	char *mandir;
	char *section;
	char *parg;

	parg = strdup(arg);
	if (parg == NULL)
		err(1, "out of memory");
	while ((dir = strsep(&parg, ":")) != NULL) {
		switch (directory_type(dir)) {
		case TOP_LEVEL_DIR:
			if (locale != NULL) {
				asprintf(&mandir, "%s/%s", dir, locale);
				process_mandir(mandir, NULL);
				free(mandir);
				if (lang_locale != NULL) {
					asprintf(&mandir, "%s/%s", dir,
					    lang_locale);
					process_mandir(mandir, NULL);
					free(mandir);
				}
			} else {
				process_mandir(dir, NULL);
			}
			break;
		case MAN_SECTION_DIR: {
			mandir = strdup(dirname(dir));
			section = strdup(basename(dir));
			process_mandir(mandir, section);
			free(mandir);
			free(section);
			break;
			}
		default:
			warnx("%s: directory name not in proper man form", dir);
			exit_code = 1;
		}
	}
	free(parg);
}

static void
determine_locale(void)
{
	char *sep;

	if ((locale = setlocale(LC_CTYPE, "")) == NULL) {
		warnx("-L option used, but no locale found\n");
		return;
	}
	sep = strchr(locale, '_');
	if (sep != NULL && isupper((unsigned char)sep[1])
			&& isupper((unsigned char)sep[2])) {
		asprintf(&lang_locale, "%.*s%s", (int)(sep - locale),
		    locale, &sep[3]);
	}
	sep = nl_langinfo(CODESET);
	if (sep != NULL && *sep != '\0' && strcmp(sep, "US-ASCII") != 0) {
		int i;

		for (i = 0; locale_device[i] != NULL; i += 2) {
			if (strcmp(sep, locale_device[i]) == 0) {
				nroff_device = locale_device[i + 1];
				break;
			}
		}
	}
	if (verbose) {
		if (lang_locale != NULL)
			fprintf(stderr, "short locale is %s\n", lang_locale);
		fprintf(stderr, "nroff device is %s\n", nroff_device);
	}
}

static void
usage(void)
{
	fprintf(stderr, "usage: %s [-fLnrv] [directories ...]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char **argv)
{
	int opt;

	if ((uid = getuid()) == 0) {
		fprintf(stderr, "don't run %s as root, use:\n   echo", argv[0]);
		for (optind = 0; optind < argc; optind++) {
			fprintf(stderr, " %s", argv[optind]);
		}
		fprintf(stderr, " | nice -5 su -m man\n");
		exit(1);
	}
	while ((opt = getopt(argc, argv, "vnfLrh")) != -1) {
		switch (opt) {
		case 'f':
			force++;
			break;
		case 'L':
			determine_locale();
			break;
		case 'n':
			pretend++;
			break;
		case 'r':
			rm_junk++;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	if ((starting_dir = open(".", 0)) < 0) {
		err(1, ".");
	}
	umask(022);
	signal(SIGINT, trap_signal);
	signal(SIGHUP, trap_signal);
	signal(SIGQUIT, trap_signal);
	signal(SIGTERM, trap_signal);

	if ((machine = getenv("MACHINE")) == NULL) {
		static struct utsname utsname;

		if (uname(&utsname) == -1)
			err(1, "uname");
		machine = utsname.machine;
	}

	if ((machine_arch = getenv("MACHINE_ARCH")) == NULL)
		machine_arch = MACHINE_ARCH;

	if (optind == argc) {
		const char *manpath = getenv("MANPATH");
		if (manpath == NULL)
			manpath = DEFAULT_MANPATH;
		process_argument(manpath);
	} else {
		while (optind < argc)
			process_argument(argv[optind++]);
	}
	exit(exit_code);
}
