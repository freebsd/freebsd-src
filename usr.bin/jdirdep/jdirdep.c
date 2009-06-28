/* $FreeBSD$ */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <dirent.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "jdirdep.h"

#ifdef JDIRDEP
#define	MAKEFILE	"Buildfile"
#define	MAKEFILED	"Buildfile.dirdep"
#define	MAKEFILEDEP	"Buildfile.dep"
#else
#define	MAKEFILE	"Makefile"
#define	MAKEFILED	"Makefile.dirdep"
#define	MAKEFILEDEP	"Makefile.dep"
#endif

struct dirdep {
	char *srcrel;
	int subcnt;
	TAILQ_ENTRY(dirdep) link;
};

struct march {
	char *machine;
	char *machine_arch;
	TAILQ_ENTRY(march) link;
};

struct incmk {
	char *s;
	size_t l;
	TAILQ_ENTRY(incmk) link;
};

struct metas {
	char *s;
	TAILQ_ENTRY(metas) link;
};

struct file_track {
	int64_t *filids;
	int max_idx;
	int next_idx;
};

static TAILQ_HEAD(, dirdep) dirdeps = TAILQ_HEAD_INITIALIZER(dirdeps);
static TAILQ_HEAD(, dirdep) srcdirdeps = TAILQ_HEAD_INITIALIZER(srcdirdeps);
static TAILQ_HEAD(, incmk) incmks = TAILQ_HEAD_INITIALIZER(incmks);
static TAILQ_HEAD(, march) marchs = TAILQ_HEAD_INITIALIZER(marchs);
static TAILQ_HEAD(, metas) metass = TAILQ_HEAD_INITIALIZER(metass);
static int f_db = 0;
static int f_quiet = 1;
static struct file_track read_filids = { NULL, 0, 0 };
static struct file_track write_filids = { NULL, 0, 0 };

static int
meta_lookup(const char *mname)
{
	struct metas *m;

	TAILQ_FOREACH(m, &metass, link) {
		if (strcmp(m->s, mname) == 0)
			return(1);
	}

	return(0);
}

static void
file_track_add(struct file_track *ft, int64_t filid)
{
	int i;

	for (i = 0; i < ft->max_idx; i++)
		if (ft->filids[i] == filid)
			return;;

	if (ft->next_idx == ft->max_idx) {
		int max_idx = ft->max_idx + 256;
		int64_t *filids;

		if ((filids = realloc(ft->filids, sizeof(int64_t) * max_idx)) == NULL)
			errx(1, "Error reallocating memory");

		ft->filids = filids;
		ft->max_idx = max_idx;
	}

	ft->filids[ft->next_idx++] = filid;
}

static void
file_track_reset(void)
{
	read_filids.next_idx = 0;
	write_filids.next_idx = 0;
}

/*
 * Clean the directory dependency list ready to process another file.
 */
static void
clean_subdep(void)
{
	struct dirdep *dd;
	struct dirdep *dt;

	TAILQ_FOREACH_SAFE(dd, &dirdeps, link, dt) {
		TAILQ_REMOVE(&dirdeps, dd, link);
		if (dd->srcrel != NULL)
			free(dd->srcrel);
		free(dd);
	}

	TAILQ_FOREACH_SAFE(dd, &srcdirdeps, link, dt) {
		TAILQ_REMOVE(&srcdirdeps, dd, link);
		if (dd->srcrel != NULL)
			free(dd->srcrel);
		free(dd);
	}
}

/*
 * Check if a source relative directory path is in the list and add it
 * if not. Keep the list sorted.
 */
static void
srcdirdep_add(const char *srcrel)
{
	int v = 0;
	struct dirdep *dd;
	struct dirdep *dn = NULL;

	TAILQ_FOREACH(dd, &srcdirdeps, link) {
		if ((v = strcmp(srcrel, dd->srcrel)) == 0)
			return;

		if (v < 0 && dn == NULL) {
			dn = dd;
			break;
		}
	}

	if ((dd = malloc(sizeof(struct dirdep))) == NULL)
		err(1, "Could not allocate memory for struct dirdep");

	if ((dd->srcrel = strdup(srcrel)) == NULL)
		err(1, "Could not duplicate srcrel string");

	dd->subcnt = 0;

	if (dn == NULL)
		TAILQ_INSERT_TAIL(&srcdirdeps, dd, link);
	else
		TAILQ_INSERT_BEFORE(dn, dd, link);
}

/*
 * Check if a source relative directory path is in the list and add it
 * if not. Keep the list sorted.
 */
static void
dirdep_add(const char *srcrel, const char *from __unused, struct march *march)
{
	char *p2;
	char s[MAXPATHLEN];
	const char *p1;
	int v = 0;
	size_t len = 0;
	struct dirdep *dd;
	struct dirdep *dn = NULL;

	if (march != NULL)
		len = strlen(march->machine_arch);

	for (p1 = srcrel, p2 = s; *p1 != '\0'; ) {
		if (march != NULL &&
		    strcmp(march->machine_arch, "common") != 0 &&
		    strncmp(p1, march->machine_arch, len) == 0) {
			strcpy(p2, "MACHINE_ARCH");
			p1 += len;
			p2 += 12;
		} else
			*p2++ = *p1++;
	}
	*p2 = '\0';

	TAILQ_FOREACH(dd, &dirdeps, link) {
		if ((v = strcmp(s, dd->srcrel)) == 0)
			return;

		if (v < 0 && dn == NULL) {
			dn = dd;
			break;
		}
	}

#ifdef DEBUG
	if (!f_quiet) {
		printf("%s from %s\n", s, from);
		fflush(stdout);
	}
#endif

	if ((dd = malloc(sizeof(struct dirdep))) == NULL)
		err(1, "Could not allocate memory for struct dirdep");

	if ((dd->srcrel = strdup(s)) == NULL)
		err(1, "Could not duplicate srcrel string");

	dd->subcnt = 0;

	if (dn == NULL)
		TAILQ_INSERT_TAIL(&dirdeps, dd, link);
	else
		TAILQ_INSERT_BEFORE(dn, dd, link);
}

static int
recid_lookup(void *thing, int argc __unused, char **argv __unused, char **colname __unused)
{
	int64_t *recid = thing;

	*recid = strtoimax(argv[0], NULL, 10);

	return(0);
}

static int64_t
reldir_table_add(const char *reldir)
{
	int64_t relid = 0;

	jdirdep_db_command(recid_lookup, &relid, "SELECT relid FROM reldir WHERE name='%s';", reldir);

	if (relid == 0) {
		jdirdep_db_command(NULL, NULL, "INSERT INTO reldir ( name ) VALUES ('%s');", reldir);

		relid = jdirdep_db_rowid();
	}

	return(relid);
}

static int64_t
file_table_add(const char *relname, const char *reldir)
{
	int64_t filid = 0;
	int64_t relid = 0;

	if (reldir != NULL && f_db)
		relid = reldir_table_add(reldir);

	jdirdep_db_command(recid_lookup, &filid, "SELECT filid FROM file WHERE name='%s';", relname);

	if (filid == 0) {
		jdirdep_db_command(NULL, NULL, "INSERT INTO file VALUES (0, '%s', %jd);", relname, relid);

		filid = jdirdep_db_rowid();
	} else if (relid != 0) {
		jdirdep_db_command(NULL, NULL, "UPDATE file SET relid=%jd WHERE filid=%jd;", relid, filid);

#ifdef DOODAD
		/* Delete the files_using tuples for this file. */
		jdirdep_db_command(NULL, NULL, "DELETE FROM files_using WHERE filid_used=%jd;", filid);

		/* Now delete the files_used by this file. */
		jdirdep_db_command(NULL, NULL, "DELETE FROM files_used WHERE filid=%jd;", filid);
#endif
	}

	return(filid);
}

static void
file_dep_add(int64_t filid_using, int64_t filid_used)
{
	int64_t filid = 0;

	if (!f_db)
		return;

	if (filid_using == filid_used)
		return;

	jdirdep_db_command(recid_lookup, &filid, "SELECT filid_used FROM files_using WHERE filid=%jd AND filid_used=%jd;", filid_using, filid_used);

	if (filid == 0)
		jdirdep_db_command(NULL, NULL, "INSERT INTO files_using VALUES (%jd, %jd);", filid_using, filid_used);

	filid = 0;

	jdirdep_db_command(recid_lookup, &filid, "SELECT filid_using FROM files_used WHERE filid=%jd AND filid_using=%jd;", filid_used, filid_using);

	if (filid == 0)
		jdirdep_db_command(NULL, NULL, "INSERT INTO files_used VALUES (%jd, %jd);", filid_used, filid_using);
}

static void
meta_dep_add(void)
{
	int i;
	int j;

	if (!f_db)
		return;

	for (i = 0; i < write_filids.next_idx; i++) {
		for (j = 0; j < read_filids.next_idx; j++) {
			file_dep_add(write_filids.filids[i], read_filids.filids[j]);
		}
	}
}

static void
delete_dep(const char *relname __unused)
{
	/* Don't need this info (yet). */
}

static void
move_dep(const char *srcname, const char *dstname, const char *reldir)
{
	int64_t filid_used;
	int64_t filid_using;

	if (!f_db)
		return;

	filid_using = file_table_add(dstname, reldir);
	filid_used = file_table_add(srcname, NULL);

	file_dep_add(filid_using, filid_used);
}

static void
read_dep(const char *relname)
{
	int64_t filid;

	if (!f_db)
		return;

	filid = file_table_add(relname, NULL);

	file_track_add(&read_filids, filid);
}

static void
write_dep(const char *relname, const char *reldir)
{
	int64_t filid;

	if (!f_db)
		return;

	filid = file_table_add(relname, reldir);

	file_track_add(&write_filids, filid);
}

/*
 * Read a meta data file looking for referenced files. For each referenced
 * file, open the associated source relative directory file (.srcrel) if
 * it exists and read the source relative directory from which the file
 * was released.
 */
static void
parse_meta(const char *srctop, const char *thissrcrel, const char *objtop, const char *objroot,
    const char *sharedobj, const char *mname, struct march *march)
{
	FILE *fp;
	FILE *fp1;
	char fname[MAXPATHLEN];
	char srcrel[MAXPATHLEN];
	char tname[MAXPATHLEN];
	char tname1[MAXPATHLEN];
	char tname2[MAXPATHLEN];
	size_t l_objroot;
	size_t l_objtop;
	size_t l_sharedobj;
	size_t l_srctop;
	struct stat fs;

	l_objroot = strlen(objroot);
	l_objtop = strlen(objtop);
	l_sharedobj = strlen(sharedobj);
	l_srctop = strlen(srctop);

	if ((fp = fopen(mname, "r")) != NULL) {
		char *bufr;
		char *p;
		char *p1;
		char *p2;
		int f = 0;
		size_t len;
		size_t s_bufr = 128 * 1024;

		if ((bufr = malloc(s_bufr)) == NULL)
			err(1, "Cannot allocate memory for a read buffer");

		while (fgets(bufr, s_bufr, fp) != NULL) {
			/* Whack the trailing newline. */
			bufr[strlen(bufr) - 1] = '\0';

			/* Find the start of the build monitor section. */
			if (strncmp(bufr, "-- buildmon", 11) == 0) {
				f = 1;
				continue;
			}

			/* Check for the end of the build monitor section. */
			if (strncmp(bufr, "# Session completed", 19) == 0) {
				f = 0;
				continue;
			}

			/* Delimit the record type. */
			p = bufr;
			strsep(&p, " ");

			if (f) {
				/* Process according to record type. */
				switch (bufr[0]) {
				case 'D':
				case 'E':
				case 'R':
				case 'S':
				case 'W':
					/* Skip the pid. */
					if (strsep(&p, " ") == NULL)
						break;

					if (*p != '/')
						snprintf(tname1, sizeof(tname1), "%s/%s/%s",
							objtop, thissrcrel, p);
					else
						strlcpy(tname1, p, sizeof(tname1));

					if (realpath(tname1, tname) == NULL)
						break;

					/* Deleted file: */
					if (bufr[0] == 'D') {
						if ((len = strlen(tname)) < l_objroot)
							break;

						if (strncmp(tname, objroot, l_objroot) != 0)
							break;

						delete_dep(tname + l_objroot - 3);
						break;

					/* Written file: */
					} else if (bufr[0] == 'W') {
						len = strlen(tname);

						if (strcmp(tname + len - 7, ".srcrel") == 0)
							break;

						if ((len = strlen(tname)) < l_objroot)
							break;

						if (strncmp(tname, objroot, l_objroot) != 0)
							break;

						write_dep(tname + l_objroot - 3, thissrcrel);
						break;
					}

					if (stat(tname, &fs) == 0 && !S_ISREG(fs.st_mode))
						break;

					/*
					 * If there is a source relative directory file
					 * for the referenced file, then read it.
					 */
					snprintf(fname, sizeof(fname), "%s.srcrel", tname);

					if ((fp1 = fopen(fname, "r")) != NULL) {
						if (l_sharedobj > 0 && strncmp(tname, sharedobj, l_sharedobj) == 0)
							read_dep(tname + l_sharedobj - 3);
						else if (l_objroot > 0 && strncmp(tname, objroot, l_objroot) == 0)
							read_dep(tname + l_objroot - 3);

						if (fgets(srcrel, sizeof(srcrel), fp1) != NULL) {
							char *sp;

							/* Whack the trailing newline. */
							srcrel[strlen(srcrel) - 1] = '\0';

							/* Avoid bogus additional slashes. */
							sp = srcrel;
							while (*sp == '/' || *sp == '#' || isspace(*sp))
								sp++;

							/*
							 * Watch out for the current directory
							 * because that can confuse things!
							 */
							if (strcmp(sp, thissrcrel) != 0)
								dirdep_add(sp, p, march);
						}
						fclose(fp1);

					/* Check if the file is in the object tree. */
					} else if (strncmp(tname, objtop, l_objtop) == 0) {
						char *sp;

						read_dep(tname + l_objroot - 3);

						/* Get the parent directory path. */
						strlcpy(srcrel, dirname(tname), sizeof(srcrel));

						/* Avoid bogus additional slashes. */
						sp = srcrel + l_objtop + 1;
						while (*sp == '/')
							sp++;

						/*
						 * Watch out for the current directory
						 * because that can confuse things!
						 */
						if (strcmp(sp, thissrcrel) != 0)
							dirdep_add(sp, p, march);

					/*
					 * Check if the file is in the source tree, but not
					 * in the current source directory.
					 */
					} else if (strncmp(tname, srctop, l_srctop) == 0) {
						read_dep(tname + l_srctop - 3);

						if (strcmp(dirname(tname + l_srctop + 1), thissrcrel) != 0)
							srcdirdep_add(dirname(tname + l_srctop + 1));
					}
					break;

				case 'M':
					/* Skip the pid. */
					if (strsep(&p, " ") == NULL)
						break;

					/* Get the src file name. */
					if (strsep(&p, "'") == NULL)
						break;
					p1 = p;
					if (strsep(&p, "'") == NULL)
						break;
					if (strsep(&p, " ") == NULL)
						break;

					/* Get the dst file name. */
					if (strsep(&p, "'") == NULL)
						break;
					p2 = p;
					if (strsep(&p, "'") == NULL)
						break;

					if (*p1 != '/')
						snprintf(tname1, sizeof(tname1), "%s/%s/%s",
							objtop, thissrcrel, p1);
					else
						strlcpy(tname1, p1, sizeof(tname1));

					if (realpath(tname1, tname) == NULL)
						break;

					if (*p2 != '/')
						snprintf(tname1, sizeof(tname1), "%s/%s/%s",
							objtop, thissrcrel, p2);
					else
						strlcpy(tname1, p2, sizeof(tname1));

					if (realpath(tname1, tname2) == NULL)
						break;

					move_dep(tname + l_objroot - 3, tname2 + l_objroot - 3, thissrcrel);
					break;
				default:
					break;
				}
			}
		}

		free(bufr);
		fclose(fp);
	}

	meta_dep_add();

	file_track_reset();
}

static char lockf_created[MAXPATHLEN];
static int lockf_fd = -1;

static void
lockf_create(const char *curdir)
{
	int i;

	if (lockf_fd >= 0)
		return;

	snprintf(lockf_created, sizeof(lockf_created), "%s/.jdirdep_lockf", curdir);

	for (i = 0; i < 300; i++) {
		if ((lockf_fd = open(lockf_created, O_CREAT | O_EXCL, 0600)) >= 0) {
			return;
		}

		if (errno != EEXIST)
			err(1, "Could not create lock file '%s'", lockf_created);

		sleep(1);
	}

	err(1, "Could not create lock file '%s'", lockf_created);
}

static void
lockf_delete(void)
{
	if (lockf_fd >= 0) {
		close(lockf_fd);
		if (unlink(lockf_created) != 0)
			err(1, "Could not unlink file '%s'", lockf_created);
		lockf_fd = -1;
	}
	lockf_created[0] = '\0';
}

static void
do_dirdep(const char *srctop, const char *curdir, const char *srcrel, const char *objroot,
    const char *sharedobj, int options)
{
	DIR *d;
	FILE *fp;
	FILE *fp1;
	char *bufr;
	char cmd[MAXPATHLEN];
	char *dirdep = NULL;
	char fname[MAXPATHLEN];
	char fname1[MAXPATHLEN];
	char fname2[MAXPATHLEN];
	char mname[MAXPATHLEN];
	char objdir[MAXPATHLEN];
	char objtop[MAXPATHLEN];
	char *s = NULL;
	char *s1 = NULL;
	char *srcdirdep = NULL;
	int f_add = (options & JDIRDEP_OPT_ADD);
	int f_dirdep = 1;
	int f_doit = 0;
	int f_error = 0;
	int f_force = (options & JDIRDEP_OPT_FORCE);
	int f_meta = (options & JDIRDEP_OPT_META);
	int f_rewrite = 0;
	int f_src = (options & JDIRDEP_OPT_SOURCE);
	int f_update = (options & JDIRDEP_OPT_UPDATE);
	int fd;
	size_t l;
	size_t ss;
	size_t ss1;
	struct dirdep *dd;
	struct dirent *de;
	struct march *march;
	struct stat fs;

	if (strcmp(srcrel, "stage") != 0)
		dirdep_add("stage", "", NULL);
	snprintf(fname, sizeof(fname), "%s/%s", curdir, MAKEFILE);
	snprintf(fname2, sizeof(fname2), "%s/%s", curdir, MAKEFILEDEP);

	if ((fp = fopen(fname, "r")) == NULL)
		return;

	if (fstat(fileno(fp), &fs) != 0)
		err(1, "Could not get file status of file '%s'", fname);

	if (f_db)
		reldir_table_add(srcrel);

	if ((bufr = malloc(fs.st_size)) == NULL)
		err(1, "Could not allocate %zd bytes for the dirdep buffer", (size_t) fs.st_size);

	/*
	 * Read through the Makefile/Buildfile to look for the .include system makefile
	 * to determine if this directory needs to have a .dep file.
	 */
	while (fgets(bufr, fs.st_size, fp) != NULL) {
		/* Whack the trailing newline. */
		bufr[strlen(bufr) - 1] = '\0';

		if (bufr[0] != '#' && strstr(bufr, "<bsd.dirdep.mk>") != NULL) {
			f_dirdep = 0;
			f_rewrite = 0;
			break;
		}

		if (bufr[0] != '#' && strstr(bufr, "<bsd.subdir.mk>") != NULL) {
			f_dirdep = 0;
			f_rewrite = 0;
			break;
		}

		/*
		 * Check if this file needs to be rewritten to remove the old DIRDEP and/or
		 * SRCDIRDEP lines.
		 */
		if (strncmp(bufr, "DIRDEP", 6) == 0 || strncpy(bufr, "SRCDIRDEP", 9) == 0) {
			f_rewrite = 1;
			f_force = 1;
		}
	}

	fclose(fp);

	/*
	 * If this directory needs a .dep file, then try to read the current one.
	 * This initialises the global lists for DIRDEP and SRCDIRDEP.
	 */
	if (f_dirdep && (fp = fopen(fname2, "r")) != NULL) {
		char *p = NULL;
		char *p1;
		char *p2;

		/*
		 * Get the size of the current .dep file to use it as a maximum
		 * length for the DIRDEP and SRCDIRDEP strings.
		 */
		if (fstat(fileno(fp), &fs) != 0)
			err(1, "Could not get file status of file '%s'", fname2);

		if ((dirdep = malloc(fs.st_size)) == NULL)
			err(1, "Could not allocate %zd bytes for the dirdep buffer", (size_t) fs.st_size);

		if ((srcdirdep = malloc(fs.st_size)) == NULL)
			err(1, "Could not allocate %zd bytes for the dirdep buffer", (size_t) fs.st_size);

		*dirdep = '\0';
		*srcdirdep = '\0';

		/* Always add the stage directory since there is nothing in the meta data to
		 * indicate that it is always needed. The stage directory itself obviously can't
		 * depend on itself, but this is handled in bsd.dirdep.mk.
		 */
		dirdep_add("stage", "", NULL);

		/*
		 * Read through the existing .dep file and add paths to the DIRDEP and
		 * SRCDIRDEP global lists.
		 */
		while (fgets(bufr, fs.st_size, fp) != NULL) {
			if (bufr[0] == '#' || bufr[0] == '\n')
				continue;
			else if (strncmp(bufr, "DIRDEP", 6) == 0)
				p = dirdep;
			else if (strncmp(bufr, "SRCDIRDEP", 9) == 0)
				p = srcdirdep;
			else if (bufr[0] == '\t' && p != NULL) {
				/*
				 * Found a continution line which should contain a source
				 * relative directory path. Skip the tab and point to the
				 * start of the path.
				 */
				p1 = bufr + 1;

				/* Get the path with always has a space after it. */
				if ((p2 = strsep(&p1, " ")) != NULL) {
					if (p == dirdep) {
						/* Append to the reference dirdep string. */
						strcat(dirdep, " ");
						strcat(dirdep, p2);

						if (f_add)
							dirdep_add(p2, "", NULL);
					}

					if (p == srcdirdep) {
						/* Append to the reference srcdirdep string. */
						strcat(srcdirdep, " ");
						strcat(srcdirdep, p2);

						if (f_add)
							srcdirdep_add(p2);
					}
				}
			}
		}

		fclose(fp);
	}

	/* Check if the DIRDEP= isn't needed for this directory. */
	if (!f_force && !f_dirdep) {
		if (dirdep != NULL)
			free(dirdep);
		if (srcdirdep != NULL)
			free(srcdirdep);
		return;
	} else {
		/* Loop through the supported machine types... */
		TAILQ_FOREACH(march, &marchs, link) {
			/*
			 * Format the path to the current machine-specific object
			 * directory:
			 */
			snprintf(objdir, sizeof(objdir), "%s/%s/%s",
			   objroot, march->machine, srcrel);
			snprintf(objtop, sizeof(objtop), "%s/%s",
			   objroot, march->machine);

			/*
			 * Open the object directory and parse all meta data files we
			 * can find in there. These tell us what was referenced when the
			 * objects were built.
			 */
			if ((d = opendir(objdir)) != NULL) {
				while((de = readdir(d)) != NULL) {
					if ((l = strlen(de->d_name)) < 6)
						continue;

					if (strcmp(de->d_name + l - 5, ".meta") != 0)
						continue;

					snprintf(mname, sizeof(mname), "%s/%s", objdir, de->d_name);

					/*
					 * If a list of meta data files has been specified,
					 * then only parse the meata data file if it is in the list.
					 */
					if (f_meta && !meta_lookup(mname))
						continue;

					/*
					 * Parse the meta data file and watch out that we don't get
					 * confused with references to the current directory -- they
					 * are almost certain to occur if there are include files
					 * released from here because that happens early in the
					 * build (so that there are no differences between the
					 * released headers and the local ones).
					 */
					parse_meta(srctop, srcrel, objtop, objroot, sharedobj, mname, march);
				}

				closedir(d);
			}
		}

		ss = 1;
		TAILQ_FOREACH(dd, &dirdeps, link) {
			ss += strlen(dd->srcrel) + 1;
		}

		if ((s = malloc(ss)) == NULL)
			err(1, "Could not allocate %zd bytes for the dirdep buffer", ss);

		ss1 = 1;
		TAILQ_FOREACH(dd, &srcdirdeps, link) {
			ss1 += strlen(dd->srcrel) + 1;
		}

		if ((s1 = malloc(ss1)) == NULL)
			err(1, "Could not allocate %zd bytes for the srcdirdep buffer", ss1);

		l = 0;
		*s = '\0';

		/*
		 * There are a couple of special cases we have to handle here. If we've
		 * got source relative directory elements, then we simply report
		 * that list, but if we have none, then we still have to report that we
		 * depend on the stage directory (which doesn't have a .srcrel file
		 * for us to discover). And finally we have to avoid making the 'stage'
		 * directory depend on itself.
		 */
		if (!TAILQ_EMPTY(&dirdeps)) {
			TAILQ_FOREACH(dd, &dirdeps, link) {
				l += snprintf(s + l, ss - l, " %s", dd->srcrel);
			}
		} else if (strcmp(srcrel, "stage") != 0)
			l += snprintf(s + l, ss - l, " stage");

		l = 0;
		*s1 = '\0';

		TAILQ_FOREACH(dd, &srcdirdeps, link) {
			l += snprintf(s1 + l, ss1 - l, " %s", dd->srcrel);
		}

		if (f_update && dirdep != NULL && strcmp(s, dirdep) != 0) {
			printf("Directory dependencies are being updated:\n");
			printf("%s\n", s);
			fflush(stdout);
			f_doit = 1;
		}

		if (f_update && f_src && srcdirdep != NULL && strcmp(s1, srcdirdep) != 0) {
			printf("Source dependencies are being updated:\n");
			printf("%s\n", s1);
			fflush(stdout);
			f_doit = 1;
		}

		if (f_update && dirdep == NULL) {
			printf("Directory dependencies are being added:\n");
			printf("%s\n", s);
			fflush(stdout);
			f_doit = 1;
		}

		if (f_update && f_src && srcdirdep == NULL && ss1 > 11) {
			printf("Source dependencies are being added:\n");
			printf("%s\n", s1);
			fflush(stdout);
			f_doit = 1;
		}

		if (!f_update && f_src) {
			printf("%s\n", s1);
			fflush(stdout);
		}

		if (f_update && f_doit) {
			lockf_create(curdir);

			if (f_rewrite) {
				snprintf(fname1, sizeof(fname1), "%s.XXXXXX", fname);
				if ((fd = mkstemp(fname1)) < 0)
					err(1, "Could not create temporary file '%s'", fname1);

				if ((fp1 = fdopen(fd, "w")) == NULL)
					err(1, "Could not open stream for temporary file '%s'", fname1);

				if ((fp = fopen(fname, "r")) == NULL) {
					f_error = 1;
					warn("Could not open '%s' again", fname);
				} else {
					while (fgets(bufr, fs.st_size, fp) != NULL) {
						/* Whack the trailing newline. */
						bufr[strlen(bufr) - 1] = '\0';

						/* Ignore lines that start with DIRDEP. */
						if (strncmp(bufr, "DIRDEP", 6) == 0)
							continue;

						if (f_src && strncmp(bufr, "SRCDIRDEP", 9) == 0)
							continue;

						fprintf(fp1, "%s\n", bufr);
					}

					fclose(fp);
				}

				fclose(fp1);

				if (f_error)
					unlink(fname1);
				else {
					if (unlink(fname) != 0)
						err(1, "Could not delete old file '%s'", fname);

					if (rename(fname1, fname) != 0)
						err(1, "Could not rename file '%s' to '%s'", fname1, fname);

					if (chmod(fname, fs.st_mode) != 0)
						err(1, "Could not chmod file '%s'", fname);
				}
			}

			/*
			 * Only (re)write the .dep file in system .include is one which
			 * requires it.
			 */
			if (f_dirdep) {
				if ((fp1 = fopen(fname2, "w")) == NULL)
					err(1, "Could not open stream for new file '%s'", fname2);

				fprintf(fp1, "# This file is automatically generated. DO NOT EDIT!\n\n");

				fprintf(fp1, "DIRDEP = \\\n");

				if (!TAILQ_EMPTY(&dirdeps)) {
					TAILQ_FOREACH(dd, &dirdeps, link) {
						fprintf(fp1, "\t%s \\\n", dd->srcrel);
					}
				} else if (strcmp(srcrel, "stage") != 0)
					fprintf(fp1, "\tstage \\\n");

				fprintf(fp1, "\n\n");

				fprintf(fp1, "SRCDIRDEP = \\\n");

				TAILQ_FOREACH(dd, &srcdirdeps, link) {
					fprintf(fp1, "\t%s \\\n", dd->srcrel);
				}

				fprintf(fp1, "\n\n");

				fclose(fp1);
			} else
				unlink(fname2);

			lockf_delete();
		}
	}

	if ((f_update && f_doit) || f_force) {
		if (chdir(curdir) != 0)
			err(1, "Could not change directory to '%s'", curdir);

		if (unlink(MAKEFILED) != 0 && errno != ENOENT)
			err(1, "Could not delete '%s/%s", curdir, MAKEFILED);

#ifdef JDIRDEP
		snprintf(cmd, sizeof(cmd), "jbuild gendirdep");
#else
		snprintf(cmd, sizeof(cmd), "build gendirdep");
#endif
		if (system(cmd) == -1)
			err(1, "Could not run command '%s'", cmd);
	}

	if (dirdep != NULL)
		free(dirdep);
	if (srcdirdep != NULL)
		free(srcdirdep);
	if (s != NULL)
		free(s);
	if (s1 != NULL)
		free(s1);

	free(bufr);

	clean_subdep();

	fflush(stdout);
}

static void
do_recurse(const char *srctop, const char *curdir, const char *srcrel, const char *objroot,
    const char *sharedobj, int options)
{
	DIR *d;
	char path[MAXPATHLEN];
	struct dirent *de;
	struct stat fs;

	if (!f_quiet) {
		printf("Searching : %s\n", curdir);
		fflush(stdout);
	}

	/* Open the current directory and find make files and subdirectories. */
	if ((d = opendir(curdir)) == NULL)
		err(1, "Could not open the current directory (%s)", curdir);

	while((de = readdir(d)) != NULL) {
		/* Ignore the current and parent directories. */
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		/* Don't recurse into a 'contrib' directory. */
		if (strcmp(de->d_name, "contrib") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", curdir, de->d_name);

		if (strcmp(de->d_name, MAKEFILE) == 0) {
			printf("Processing: %s\n", path);
			fflush(stdout);
			do_dirdep(srctop, curdir, srcrel, objroot, sharedobj, options);
			continue;
		}

		if (stat(path, &fs) != 0)
			err(1, "Could not stat file '%s'", path);

		if (S_ISDIR(fs.st_mode)) {
			char xsrcrel[MAXPATHLEN];

			snprintf(xsrcrel, sizeof(xsrcrel), "%s%s%s", srcrel,
			    *srcrel == '\0' ? "" : "/", de->d_name);

			do_recurse(srctop, path, xsrcrel, objroot, sharedobj, options);
		}
	}

	closedir(d);
}

static void
do_graph(FILE *fp, const char *curdir, const char *srcrel)
{
	DIR *d;
	FILE *fp1;
	char bufr[8192];
	char *p;
	char path[MAXPATHLEN];
	struct dirent *de;
	struct stat fs;

	/* Open the current directory and find make files and subdirectories. */
	if ((d = opendir(curdir)) == NULL)
		err(1, "Could not open the current directory (%s)", curdir);

	while((de = readdir(d)) != NULL) {
		/* Ignore the current and parent directories. */
		if (strcmp(de->d_name, ".") == 0 || strcmp(de->d_name, "..") == 0)
			continue;

		/* Don't recurse into a 'contrib' directory. */
		if (strcmp(de->d_name, "contrib") == 0)
			continue;

		snprintf(path, sizeof(path), "%s/%s", curdir, de->d_name);

		if (stat(path, &fs) != 0)
			err(1, "Could not stat file '%s'", path);

		if (strcmp(de->d_name, MAKEFILED) == 0) {
			printf("Graphing : %s\n", curdir);
			fflush(stdout);

			if ((fp1 = fopen(path, "r")) != NULL) {
				while (fgets(bufr, sizeof(bufr), fp1) != NULL) {
					/* Whack the trailing newline. */
					bufr[strlen(bufr) - 1] = '\0';

					if (strncmp(bufr, "# DIRDEP= ", 10) == 0) {
						char *bp = bufr + 10;

						while (*bp == ' ')
							bp++;

						if (*bp == '\0')
							break;

						while ((p = strsep(&bp, " ")) != NULL)
							fprintf(fp, "\t\"%s\" -> \"%s\";\n", srcrel, p);
						break;
					}
				}
				fclose(fp1);
			}
		}

		if (S_ISDIR(fs.st_mode)) {
			char xsrcrel[MAXPATHLEN];

			snprintf(xsrcrel, sizeof(xsrcrel), "%s%s%s", srcrel,
			    *srcrel == '\0' ? "" : "/", de->d_name);

			do_graph(fp, path, xsrcrel);
		}
	}

	closedir(d);
}

void
jdirdep_supmac_add(const char *machine, const char *machine_arch)
{
	struct march *march;

	if ((march = malloc(sizeof(struct march))) == NULL)
		err(1, "Could not allocate memory for struct march");

	march->machine = strdup(machine);
	if (machine_arch == NULL)
		march->machine_arch = strdup(machine);
	else
		march->machine_arch = strdup(machine_arch);

	TAILQ_INSERT_TAIL(&marchs, march, link);
}

void
jdirdep_supmac(const char *p)
{
	char envname[MAXPATHLEN];
	char supmac[MAXPATHLEN];
	char *s;
	char *s1;
	char *supmacp = supmac;

	strlcpy(supmac, p, sizeof(supmac));

	for (s = supmac; (s = strsep(&supmacp, " ")) != NULL; ) {
		snprintf(envname, sizeof(envname), "MACHINE_ARCH.%s", s);
		for (s1 = envname; *s1 != '\0'; s1++)
			if (*s1 == '-')
				*s1 = '_';
		jdirdep_supmac_add(s, getenv(envname));
	}
}

void
jdirdep_incmk(const char *p)
{
	char *incmk;
	char *s;
	struct incmk *incmkp;

	incmk = strdup(p);

	for (s = incmk; (s = strsep(&incmk, " \t")) != NULL; ) {
		if (*s == '\0')
			continue;
		if ((incmkp = malloc(sizeof(struct incmk))) == NULL)
			err(1, "Could not allocate memory for struct incmk");
		asprintf(&incmkp->s, ".include <%s>", s);
		incmkp->l = strlen(incmkp->s);
		TAILQ_INSERT_TAIL(&incmks, incmkp, link);
	}
}

/* This is the public function. */
int
jdirdep(const char *srctop, const char *curdir, const char *srcrel, const char *objroot,
    const char *objdir, const char *sharedobj, const char *filedep_name,
    const char *meta_created, int options)
{
	FILE *fp;
	char *meta_str = NULL;
	char *s;
	char *str;
	int ret = 0;
	struct metas *metasp;

	if (filedep_name != NULL) {
		f_db = 1;

		jdirdep_db_open(filedep_name);
	}

	if (meta_created != NULL) {
		meta_str = strdup(meta_created);
		str = meta_str;
		for (s = str; (s = strsep(&str, " ")) != NULL; ) {
			if ((metasp = malloc(sizeof(struct metas))) == NULL)
				err(1, "Could not allocate memory for struct metas");
			metasp->s = s;
			TAILQ_INSERT_TAIL(&metass, metasp, link);
		}

		options |= JDIRDEP_OPT_META;
	}

	if ((options & JDIRDEP_OPT_GRAPH) != 0) {
		char gvfile[MAXPATHLEN];

		snprintf(gvfile, sizeof(gvfile), "%s/dirdep.gv", objdir);

		printf("Creating : %s\n", gvfile);
		fflush(stdout);

		if ((fp = fopen(gvfile, "w")) == NULL)
			err(1, "Could not open graphviz file '%s'", gvfile);

		fprintf(fp, "digraph dirdep {\n");
		fprintf(fp, "\tpage=\"8.5,11\";\n");
		fprintf(fp, "\trotate=90;\n");
		fprintf(fp, "\tsize=\"300,300\";\n");
		fprintf(fp, "\tnode [color=lightblue2, style=filled];\n");

		do_graph(fp, curdir, srcrel);

		fprintf(fp, "}\n");

		fclose(fp);
	} else if ((options & JDIRDEP_OPT_RECURSE) != 0)
		do_recurse(srctop, curdir, srcrel, objroot, sharedobj, options);
	else
		do_dirdep(srctop, curdir, srcrel, objroot, sharedobj, options);

	jdirdep_db_close();

	if (meta_str != NULL)
		free(meta_str);

	return(ret);
}

#ifdef JDIRDEP_MAIN
int
main(int argc, char *argv[])
{
	char curdir[MAXPATHLEN];
	char *filedep_name = NULL;
	char objdir[MAXPATHLEN];
	char objroot[MAXPATHLEN];
	char objtop[MAXPATHLEN];
	char sharedobj[MAXPATHLEN];
	char srctop[MAXPATHLEN];
	char tname[MAXPATHLEN];
	const char *p;
	const char *srcrel;
	char *s;
	int c;
	int options = 0;
	struct stat fs;

	/*
	 * We expect CURDIR, OBJDIR, SRCTOP and MACHINE_ARCH to be defined in the
	 * environment and set in the build command line which executes this program.
	 */
	if ((p = getenv("CURDIR")) == NULL)
		errx(1, "CURDIR is missing from the environment");

	strlcpy(curdir, p, sizeof(curdir));

	if ((p = getenv("OBJDIR")) == NULL)
		errx(1, "OBJDIR is missing from the environment");

	strlcpy(objdir, p, sizeof(objdir));

	if ((p = getenv("SRCTOP")) == NULL)
		errx(1, "SRCTOP is missing from the environment");

	strlcpy(srctop, p, sizeof(srctop));

	if ((p = getenv("SUPMAC")) == NULL)
		errx(1, "SUPMAC is missing from the environment");

	jdirdep_supmac(p);

	if ((p = getenv("INCMK")) == NULL)
		errx(1, "INCMK is missing from the environment");

	jdirdep_incmk(p);

	/*
	 * The current source relative directory is the bit after the
	 * source top bit has been removed.
	 */
	if (strcmp(curdir, srctop) == 0)
		srcrel = "\0";
	else
		srcrel = curdir + strlen(srctop) + 1;

	strlcpy(objtop, objdir, sizeof(objtop));
	if (*srcrel != '\0')
		objtop[strlen(objdir) - strlen(srcrel) - 1] = '\0';

	strlcpy(objroot, objtop, sizeof(objroot));
	if ((s = strrchr(objroot, '/')) != NULL)
		*s = '\0';

	strlcpy(tname, objroot, sizeof(tname));
	if ((s = strrchr(tname, '/')) != NULL) {
		s++;
		*s = '\0';
	}
	strlcat(tname, "shared", sizeof(tname));

	if (stat(tname, &fs) == 0) {
		if (realpath(tname, sharedobj) == NULL)
			errx(1, "Could not get real path for shared objdir");
	} else
		sharedobj[0] = '\0';

	for (optind = 1; optind < argc; optind++) {
		while ((c = getopt(argc, argv, "ad:fgrsu")) != -1) {
			switch (c) {
			/* Add (not replace) */
			case 'a':
				options |= JDIRDEP_OPT_ADD;
				break;

			/* Update file dependency database */
			case 'd':
				options |= JDIRDEP_OPT_DB;
				filedep_name = strdup(optarg);
				break;

			/* Force */
			case 'f':
				options |= JDIRDEP_OPT_FORCE;
				break;

			/* Graph */
			case 'g':
				options |= JDIRDEP_OPT_GRAPH;
				break;

			/* Recursive */
			case 'r':
				options |= JDIRDEP_OPT_RECURSE;
				break;

			/* Source usage */
			case 's':
				options |= JDIRDEP_OPT_SOURCE;
				break;

			/* Update makefile(s) */
			case 'u':
				options |= JDIRDEP_OPT_UPDATE;
				break;

			default:
				break;
			}
		}
	}

	return(jdirdep(srctop, curdir, srcrel, objroot, objdir, sharedobj, filedep_name, NULL, options));
}
#endif
