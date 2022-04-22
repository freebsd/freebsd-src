/*      $NetBSD: meta.c,v 1.200 2022/04/15 12:28:16 rillig Exp $ */

/*
 * Implement 'meta' mode.
 * Adapted from John Birrell's patches to FreeBSD make.
 * --sjg
 */
/*
 * Copyright (c) 2009-2016, Juniper Networks, Inc.
 * Portions Copyright (c) 2009, John Birrell.
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
#if defined(USE_META)

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif
#include <sys/stat.h>
#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#elif !defined(HAVE_DIRNAME)
char * dirname(char *);
#endif
#include <errno.h>
#if !defined(HAVE_CONFIG_H) || defined(HAVE_ERR_H)
#include <err.h>
#endif

#include "make.h"
#include "dir.h"
#include "job.h"

#ifdef USE_FILEMON
#include "filemon/filemon.h"
#endif

static BuildMon Mybm;			/* for compat */
static StringList metaBailiwick = LST_INIT; /* our scope of control */
static char *metaBailiwickStr;		/* string storage for the list */
static StringList metaIgnorePaths = LST_INIT; /* paths we deliberately ignore */
static char *metaIgnorePathsStr;	/* string storage for the list */

#ifndef MAKE_META_IGNORE_PATHS
#define MAKE_META_IGNORE_PATHS ".MAKE.META.IGNORE_PATHS"
#endif
#ifndef MAKE_META_IGNORE_PATTERNS
#define MAKE_META_IGNORE_PATTERNS ".MAKE.META.IGNORE_PATTERNS"
#endif
#ifndef MAKE_META_IGNORE_FILTER
#define MAKE_META_IGNORE_FILTER ".MAKE.META.IGNORE_FILTER"
#endif
#ifndef MAKE_META_CMP_FILTER
#define MAKE_META_CMP_FILTER ".MAKE.META.CMP_FILTER"
#endif

bool useMeta = false;
static bool useFilemon = false;
static bool writeMeta = false;
static bool metaMissing = false;	/* oodate if missing */
static bool filemonMissing = false;	/* oodate if missing */
static bool metaEnv = false;		/* don't save env unless asked */
static bool metaVerbose = false;
static bool metaIgnoreCMDs = false;	/* ignore CMDs in .meta files */
static bool metaIgnorePatterns = false; /* do we need to do pattern matches */
static bool metaIgnoreFilter = false;	/* do we have more complex filtering? */
static bool metaCmpFilter = false;	/* do we have CMP_FILTER ? */
static bool metaCurdirOk = false;	/* write .meta in .CURDIR Ok? */
static bool metaSilent = false;		/* if we have a .meta be SILENT */

extern bool forceJobs;
extern char    **environ;

#define MAKE_META_PREFIX	".MAKE.META.PREFIX"

#ifndef N2U
# define N2U(n, u)   (((n) + ((u) - 1)) / (u))
#endif
#ifndef ROUNDUP
# define ROUNDUP(n, u)   (N2U((n), (u)) * (u))
#endif

#if !defined(HAVE_STRSEP)
# define strsep(s, d) stresep((s), (d), '\0')
#endif
#if !defined(HAVE_STRESEP)
char * stresep(char **, const char *, int);
#endif

/*
 * Filemon is a kernel module which snoops certain syscalls.
 *
 * C chdir
 * E exec
 * F [v]fork
 * L [sym]link
 * M rename
 * R read
 * W write
 * S stat
 *
 * See meta_oodate below - we mainly care about 'E' and 'R'.
 *
 * We can still use meta mode without filemon, but
 * the benefits are more limited.
 */
#ifdef USE_FILEMON

/*
 * Open the filemon device.
 */
static void
meta_open_filemon(BuildMon *pbm)
{
    int dupfd;

    pbm->mon_fd = -1;
    pbm->filemon = NULL;
    if (!useFilemon || pbm->mfp == NULL)
	return;

    pbm->filemon = filemon_open();
    if (pbm->filemon == NULL) {
	useFilemon = false;
	warn("Could not open filemon %s", filemon_path());
	return;
    }

    /*
     * We use a file outside of '.'
     * to avoid a FreeBSD kernel bug where unlink invalidates
     * cwd causing getcwd to do a lot more work.
     * We only care about the descriptor.
     */
    if (!opts.compatMake)
	pbm->mon_fd = Job_TempFile("filemon.XXXXXX", NULL, 0);
    else
	pbm->mon_fd = mkTempFile("filemon.XXXXXX", NULL, 0);
    if ((dupfd = dup(pbm->mon_fd)) == -1) {
	Punt("Could not dup filemon output: %s", strerror(errno));
    }
    (void)fcntl(dupfd, F_SETFD, FD_CLOEXEC);
    if (filemon_setfd(pbm->filemon, dupfd) == -1) {
	Punt("Could not set filemon file descriptor: %s", strerror(errno));
    }
    /* we don't need these once we exec */
    (void)fcntl(pbm->mon_fd, F_SETFD, FD_CLOEXEC);
}

/*
 * Read the build monitor output file and write records to the target's
 * metadata file.
 */
static int
filemon_read(FILE *mfp, int fd)
{
    char buf[BUFSIZ];
    int error;

    /* Check if we're not writing to a meta data file.*/
    if (mfp == NULL) {
	if (fd >= 0)
	    close(fd);			/* not interested */
	return 0;
    }
    /* rewind */
    if (lseek(fd, (off_t)0, SEEK_SET) < 0) {
	error = errno;
	warn("Could not rewind filemon");
	fprintf(mfp, "\n");
    } else {
	ssize_t n;

	error = 0;
	fprintf(mfp, "\n-- filemon acquired metadata --\n");

	while ((n = read(fd, buf, sizeof buf)) > 0) {
	    if ((ssize_t)fwrite(buf, 1, (size_t)n, mfp) < n)
		error = EIO;
	}
    }
    if (fflush(mfp) != 0)
	Punt("Cannot write filemon data to meta file: %s",
	    strerror(errno));
    if (close(fd) < 0)
	error = errno;
    return error;
}
#endif

/*
 * when realpath() fails,
 * we use this, to clean up ./ and ../
 */
static void
eat_dots(char *buf)
{
    char *p;

    while ((p = strstr(buf, "/./")) != NULL)
	memmove(p, p + 2, strlen(p + 2) + 1);

    while ((p = strstr(buf, "/../")) != NULL) {
	char *p2 = p + 3;
	if (p > buf) {
	    do {
		p--;
	    } while (p > buf && *p != '/');
	}
	if (*p == '/')
	    memmove(p, p2, strlen(p2) + 1);
	else
	    return;		/* can't happen? */
    }
}

static char *
meta_name(char *mname, size_t mnamelen,
	  const char *dname,
	  const char *tname,
	  const char *cwd)
{
    char buf[MAXPATHLEN];
    char *rp, *cp;
    const char *tname_base;
    char *tp;
    char *dtp;
    size_t ldname;

    /*
     * Weed out relative paths from the target file name.
     * We have to be careful though since if target is a
     * symlink, the result will be unstable.
     * So we use realpath() just to get the dirname, and leave the
     * basename as given to us.
     */
    if ((tname_base = strrchr(tname, '/')) != NULL) {
	if (cached_realpath(tname, buf) != NULL) {
	    if ((rp = strrchr(buf, '/')) != NULL) {
		rp++;
		tname_base++;
		if (strcmp(tname_base, rp) != 0)
		    strlcpy(rp, tname_base, sizeof buf - (size_t)(rp - buf));
	    }
	    tname = buf;
	} else {
	    /*
	     * We likely have a directory which is about to be made.
	     * We pretend realpath() succeeded, to have a chance
	     * of generating the same meta file name that we will
	     * next time through.
	     */
	    if (tname[0] == '/') {
		strlcpy(buf, tname, sizeof buf);
	    } else {
		snprintf(buf, sizeof buf, "%s/%s", cwd, tname);
	    }
	    eat_dots(buf);
	    tname = buf;
	}
    }
    /* on some systems dirname may modify its arg */
    tp = bmake_strdup(tname);
    dtp = dirname(tp);
    if (strcmp(dname, dtp) == 0)
	snprintf(mname, mnamelen, "%s.meta", tname);
    else {
	ldname = strlen(dname);
	if (strncmp(dname, dtp, ldname) == 0 && dtp[ldname] == '/')
	    snprintf(mname, mnamelen, "%s/%s.meta", dname, &tname[ldname+1]);
	else
	    snprintf(mname, mnamelen, "%s/%s.meta", dname, tname);

	/*
	 * Replace path separators in the file name after the
	 * current object directory path.
	 */
	cp = mname + strlen(dname) + 1;

	while (*cp != '\0') {
	    if (*cp == '/')
		*cp = '_';
	    cp++;
	}
    }
    free(tp);
    return mname;
}

/*
 * Return true if running ${.MAKE}
 * Bypassed if target is flagged .MAKE
 */
static bool
is_submake(const char *cmd, GNode *gn)
{
    static const char *p_make = NULL;
    static size_t p_len;
    char *mp = NULL;
    const char *cp2;
    bool rc = false;

    if (p_make == NULL) {
	p_make = Var_Value(gn, ".MAKE").str;
	p_len = strlen(p_make);
    }
    if (strchr(cmd, '$') != NULL) {
	(void)Var_Subst(cmd, gn, VARE_WANTRES, &mp);
	/* TODO: handle errors */
	cmd = mp;
    }
    cp2 = strstr(cmd, p_make);
    if (cp2 != NULL) {
	switch (cp2[p_len]) {
	case '\0':
	case ' ':
	case '\t':
	case '\n':
	    rc = true;
	    break;
	}
	if (cp2 > cmd && rc) {
	    switch (cp2[-1]) {
	    case ' ':
	    case '\t':
	    case '\n':
		break;
	    default:
		rc = false;		/* no match */
		break;
	    }
	}
    }
    free(mp);
    return rc;
}

static bool
any_is_submake(GNode *gn)
{
    StringListNode *ln;

    for (ln = gn->commands.first; ln != NULL; ln = ln->next)
	if (is_submake(ln->datum, gn))
	    return true;
    return false;
}

static void
printCMD(const char *ucmd, FILE *fp, GNode *gn)
{
    FStr xcmd = FStr_InitRefer(ucmd);

    Var_Expand(&xcmd, gn, VARE_WANTRES);
    fprintf(fp, "CMD %s\n", xcmd.str);
    FStr_Done(&xcmd);
}

static void
printCMDs(GNode *gn, FILE *fp)
{
    StringListNode *ln;

    for (ln = gn->commands.first; ln != NULL; ln = ln->next)
	printCMD(ln->datum, fp, gn);
}

/*
 * Certain node types never get a .meta file
 */
#define SKIP_META_TYPE(flag, str) do { \
    if ((gn->type & (flag))) { \
	if (verbose) \
	    debug_printf("Skipping meta for %s: .%s\n", gn->name, str); \
	return false; \
    } \
} while (false)


/*
 * Do we need/want a .meta file ?
 */
static bool
meta_needed(GNode *gn, const char *dname,
	    char *objdir_realpath, bool verbose)
{
    struct cached_stat cst;

    if (verbose)
	verbose = DEBUG(META);

    /* This may be a phony node which we don't want meta data for... */
    /* Skip .meta for .BEGIN, .END, .ERROR etc as well. */
    /* Or it may be explicitly flagged as .NOMETA */
    SKIP_META_TYPE(OP_NOMETA, "NOMETA");
    /* Unless it is explicitly flagged as .META */
    if (!(gn->type & OP_META)) {
	SKIP_META_TYPE(OP_PHONY, "PHONY");
	SKIP_META_TYPE(OP_SPECIAL, "SPECIAL");
	SKIP_META_TYPE(OP_MAKE, "MAKE");
    }

    /* Check if there are no commands to execute. */
    if (Lst_IsEmpty(&gn->commands)) {
	if (verbose)
	    debug_printf("Skipping meta for %s: no commands\n", gn->name);
	return false;
    }
    if ((gn->type & (OP_META|OP_SUBMAKE)) == OP_SUBMAKE) {
	/* OP_SUBMAKE is a bit too aggressive */
	if (any_is_submake(gn)) {
	    DEBUG1(META, "Skipping meta for %s: .SUBMAKE\n", gn->name);
	    return false;
	}
    }

    /* The object directory may not exist. Check it.. */
    if (cached_stat(dname, &cst) != 0) {
	if (verbose)
	    debug_printf("Skipping meta for %s: no .OBJDIR\n", gn->name);
	return false;
    }

    /* make sure these are canonical */
    if (cached_realpath(dname, objdir_realpath) != NULL)
	dname = objdir_realpath;

    /* If we aren't in the object directory, don't create a meta file. */
    if (!metaCurdirOk && strcmp(curdir, dname) == 0) {
	if (verbose)
	    debug_printf("Skipping meta for %s: .OBJDIR == .CURDIR\n",
			 gn->name);
	return false;
    }
    return true;
}


static FILE *
meta_create(BuildMon *pbm, GNode *gn)
{
    FILE *fp;
    char buf[MAXPATHLEN];
    char objdir_realpath[MAXPATHLEN];
    char **ptr;
    FStr dname;
    const char *tname;
    char *fname;
    const char *cp;

    fp = NULL;

    dname = Var_Value(gn, ".OBJDIR");
    tname = GNode_VarTarget(gn);

    /* if this succeeds objdir_realpath is realpath of dname */
    if (!meta_needed(gn, dname.str, objdir_realpath, true))
	goto out;
    dname.str = objdir_realpath;

    if (metaVerbose) {
	char *mp;

	/* Describe the target we are building */
	(void)Var_Subst("${" MAKE_META_PREFIX "}", gn, VARE_WANTRES, &mp);
	/* TODO: handle errors */
	if (mp[0] != '\0')
	    fprintf(stdout, "%s\n", mp);
	free(mp);
    }
    /* Get the basename of the target */
    cp = str_basename(tname);

    fflush(stdout);

    if (!writeMeta)
	/* Don't create meta data. */
	goto out;

    fname = meta_name(pbm->meta_fname, sizeof pbm->meta_fname,
		      dname.str, tname, objdir_realpath);

#ifdef DEBUG_META_MODE
    DEBUG1(META, "meta_create: %s\n", fname);
#endif

    if ((fp = fopen(fname, "w")) == NULL)
	Punt("Could not open meta file '%s': %s", fname, strerror(errno));

    fprintf(fp, "# Meta data file %s\n", fname);

    printCMDs(gn, fp);

    fprintf(fp, "CWD %s\n", getcwd(buf, sizeof buf));
    fprintf(fp, "TARGET %s\n", tname);
    cp = GNode_VarOodate(gn);
    if (cp != NULL && *cp != '\0') {
	fprintf(fp, "OODATE %s\n", cp);
    }
    if (metaEnv) {
	for (ptr = environ; *ptr != NULL; ptr++)
	    fprintf(fp, "ENV %s\n", *ptr);
    }

    fprintf(fp, "-- command output --\n");
    if (fflush(fp) != 0)
	Punt("Cannot write expanded command to meta file: %s",
	    strerror(errno));

    Global_Append(".MAKE.META.FILES", fname);
    Global_Append(".MAKE.META.CREATED", fname);

    gn->type |= OP_META;		/* in case anyone wants to know */
    if (metaSilent) {
	    gn->type |= OP_SILENT;
    }
 out:
    FStr_Done(&dname);

    return fp;
}

static bool
boolValue(const char *s)
{
    switch (*s) {
    case '0':
    case 'N':
    case 'n':
    case 'F':
    case 'f':
	return false;
    }
    return true;
}

/*
 * Initialization we need before reading makefiles.
 */
void
meta_init(void)
{
#ifdef USE_FILEMON
	/* this allows makefiles to test if we have filemon support */
	Global_Set(".MAKE.PATH_FILEMON", filemon_path());
#endif
}


#define get_mode_bf(bf, token) \
    if ((cp = strstr(make_mode, token)) != NULL) \
	bf = boolValue(cp + sizeof (token) - 1)

/*
 * Initialization we need after reading makefiles.
 */
void
meta_mode_init(const char *make_mode)
{
    static bool once = false;
    const char *cp;

    useMeta = true;
    useFilemon = true;
    writeMeta = true;

    if (make_mode != NULL) {
	if (strstr(make_mode, "env") != NULL)
	    metaEnv = true;
	if (strstr(make_mode, "verb") != NULL)
	    metaVerbose = true;
	if (strstr(make_mode, "read") != NULL)
	    writeMeta = false;
	if (strstr(make_mode, "nofilemon") != NULL)
	    useFilemon = false;
	if (strstr(make_mode, "ignore-cmd") != NULL)
	    metaIgnoreCMDs = true;
	if (useFilemon)
	    get_mode_bf(filemonMissing, "missing-filemon=");
	get_mode_bf(metaCurdirOk, "curdirok=");
	get_mode_bf(metaMissing, "missing-meta=");
	get_mode_bf(metaSilent, "silent=");
    }
    if (metaVerbose && !Var_Exists(SCOPE_GLOBAL, MAKE_META_PREFIX)) {
	/*
	 * The default value for MAKE_META_PREFIX
	 * prints the absolute path of the target.
	 * This works be cause :H will generate '.' if there is no /
	 * and :tA will resolve that to cwd.
	 */
	Global_Set(MAKE_META_PREFIX,
	    "Building ${.TARGET:H:tA}/${.TARGET:T}");
    }
    if (once)
	return;
    once = true;
    memset(&Mybm, 0, sizeof Mybm);
    /*
     * We consider ourselves master of all within ${.MAKE.META.BAILIWICK}
     */
    (void)Var_Subst("${.MAKE.META.BAILIWICK:O:u:tA}",
		    SCOPE_GLOBAL, VARE_WANTRES, &metaBailiwickStr);
    /* TODO: handle errors */
    str2Lst_Append(&metaBailiwick, metaBailiwickStr);
    /*
     * We ignore any paths that start with ${.MAKE.META.IGNORE_PATHS}
     */
    Global_Append(MAKE_META_IGNORE_PATHS,
	       "/dev /etc /proc /tmp /var/run /var/tmp ${TMPDIR}");
    (void)Var_Subst("${" MAKE_META_IGNORE_PATHS ":O:u:tA}",
		    SCOPE_GLOBAL, VARE_WANTRES, &metaIgnorePathsStr);
    /* TODO: handle errors */
    str2Lst_Append(&metaIgnorePaths, metaIgnorePathsStr);

    /*
     * We ignore any paths that match ${.MAKE.META.IGNORE_PATTERNS}
     */
    metaIgnorePatterns = Var_Exists(SCOPE_GLOBAL, MAKE_META_IGNORE_PATTERNS);
    metaIgnoreFilter = Var_Exists(SCOPE_GLOBAL, MAKE_META_IGNORE_FILTER);
    metaCmpFilter = Var_Exists(SCOPE_GLOBAL, MAKE_META_CMP_FILTER);
}

/*
 * In each case below we allow for job==NULL
 */
void
meta_job_start(Job *job, GNode *gn)
{
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    pbm->mfp = meta_create(pbm, gn);
#ifdef USE_FILEMON_ONCE
    /* compat mode we open the filemon dev once per command */
    if (job == NULL)
	return;
#endif
#ifdef USE_FILEMON
    if (pbm->mfp != NULL && useFilemon) {
	meta_open_filemon(pbm);
    } else {
	pbm->mon_fd = -1;
	pbm->filemon = NULL;
    }
#endif
}

/*
 * The child calls this before doing anything.
 * It does not disturb our state.
 */
void
meta_job_child(Job *job MAKE_ATTR_UNUSED)
{
#ifdef USE_FILEMON
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (pbm->mfp != NULL) {
	close(fileno(pbm->mfp));
	if (useFilemon && pbm->filemon != NULL) {
	    pid_t pid;

	    pid = getpid();
	    if (filemon_setpid_child(pbm->filemon, pid) == -1) {
		Punt("Could not set filemon pid: %s", strerror(errno));
	    }
	}
    }
#endif
}

void
meta_job_parent(Job *job MAKE_ATTR_UNUSED, pid_t pid MAKE_ATTR_UNUSED)
{
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (useFilemon && pbm->filemon != NULL) {
	filemon_setpid_parent(pbm->filemon, pid);
    }
#endif
}

int
meta_job_fd(Job *job MAKE_ATTR_UNUSED)
{
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (useFilemon && pbm->filemon != NULL) {
	return filemon_readfd(pbm->filemon);
    }
#endif
    return -1;
}

int
meta_job_event(Job *job MAKE_ATTR_UNUSED)
{
#if defined(USE_FILEMON) && !defined(USE_FILEMON_DEV)
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (useFilemon && pbm->filemon != NULL) {
	return filemon_process(pbm->filemon);
    }
#endif
    return 0;
}

void
meta_job_error(Job *job, GNode *gn, bool ignerr, int status)
{
    char cwd[MAXPATHLEN];
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
	if (gn == NULL)
	    gn = job->node;
    } else {
	pbm = &Mybm;
    }
    if (pbm->mfp != NULL) {
	fprintf(pbm->mfp, "\n*** Error code %d%s\n",
		status, ignerr ? "(ignored)" : "");
    }
    if (gn != NULL)
	Global_Set(".ERROR_TARGET", GNode_Path(gn));
    getcwd(cwd, sizeof cwd);
    Global_Set(".ERROR_CWD", cwd);
    if (pbm->meta_fname[0] != '\0') {
	Global_Set(".ERROR_META_FILE", pbm->meta_fname);
    }
    meta_job_finish(job);
}

void
meta_job_output(Job *job, char *cp, const char *nl)
{
    BuildMon *pbm;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (pbm->mfp != NULL) {
	if (metaVerbose) {
	    static char *meta_prefix = NULL;
	    static size_t meta_prefix_len;

	    if (meta_prefix == NULL) {
		char *cp2;

		(void)Var_Subst("${" MAKE_META_PREFIX "}",
				SCOPE_GLOBAL, VARE_WANTRES, &meta_prefix);
		/* TODO: handle errors */
		if ((cp2 = strchr(meta_prefix, '$')) != NULL)
		    meta_prefix_len = (size_t)(cp2 - meta_prefix);
		else
		    meta_prefix_len = strlen(meta_prefix);
	    }
	    if (strncmp(cp, meta_prefix, meta_prefix_len) == 0) {
		cp = strchr(cp + 1, '\n');
		if (cp == NULL)
		    return;
		cp++;
	    }
	}
	fprintf(pbm->mfp, "%s%s", cp, nl);
    }
}

int
meta_cmd_finish(void *pbmp)
{
    int error = 0;
    BuildMon *pbm = pbmp;
#ifdef USE_FILEMON
    int x;
#endif

    if (pbm == NULL)
	pbm = &Mybm;

#ifdef USE_FILEMON
    if (pbm->filemon != NULL) {
	while (filemon_process(pbm->filemon) > 0)
	    continue;
	if (filemon_close(pbm->filemon) == -1) {
	    error = errno;
	    Punt("filemon failed: %s", strerror(errno));
	}
	x = filemon_read(pbm->mfp, pbm->mon_fd);
	if (error == 0 && x != 0)
	    error = x;
	pbm->mon_fd = -1;
	pbm->filemon = NULL;
	return error;
    }
#endif

    fprintf(pbm->mfp, "\n");	/* ensure end with newline */
    return error;
}

int
meta_job_finish(Job *job)
{
    BuildMon *pbm;
    int error = 0;
    int x;

    if (job != NULL) {
	pbm = &job->bm;
    } else {
	pbm = &Mybm;
    }
    if (pbm->mfp != NULL) {
	error = meta_cmd_finish(pbm);
	x = fclose(pbm->mfp);
	if (error == 0 && x != 0)
	    error = errno;
	pbm->mfp = NULL;
	pbm->meta_fname[0] = '\0';
    }
    return error;
}

void
meta_finish(void)
{
    Lst_Done(&metaBailiwick);
    free(metaBailiwickStr);
    Lst_Done(&metaIgnorePaths);
    free(metaIgnorePathsStr);
}

/*
 * Fetch a full line from fp - growing bufp if needed
 * Return length in bufp.
 */
static int
fgetLine(char **bufp, size_t *szp, int o, FILE *fp)
{
    char *buf = *bufp;
    size_t bufsz = *szp;
    struct stat fs;
    int x;

    if (fgets(&buf[o], (int)bufsz - o, fp) != NULL) {
    check_newline:
	x = o + (int)strlen(&buf[o]);
	if (buf[x - 1] == '\n')
	    return x;
	/*
	 * We need to grow the buffer.
	 * The meta file can give us a clue.
	 */
	if (fstat(fileno(fp), &fs) == 0) {
	    size_t newsz;
	    char *p;

	    newsz = ROUNDUP(((size_t)fs.st_size / 2), BUFSIZ);
	    if (newsz <= bufsz)
		newsz = ROUNDUP((size_t)fs.st_size, BUFSIZ);
	    if (newsz <= bufsz)
		return x;		/* truncated */
	    DEBUG2(META, "growing buffer %u -> %u\n",
		   (unsigned)bufsz, (unsigned)newsz);
	    p = bmake_realloc(buf, newsz);
	    *bufp = buf = p;
	    *szp = bufsz = newsz;
	    /* fetch the rest */
	    if (fgets(&buf[x], (int)bufsz - x, fp) == NULL)
		return x;		/* truncated! */
	    goto check_newline;
	}
    }
    return 0;
}

static bool
prefix_match(const char *prefix, const char *path)
{
    size_t n = strlen(prefix);

    return strncmp(path, prefix, n) == 0;
}

static bool
has_any_prefix(const char *path, StringList *prefixes)
{
    StringListNode *ln;

    for (ln = prefixes->first; ln != NULL; ln = ln->next)
	if (prefix_match(ln->datum, path))
	    return true;
    return false;
}

/* See if the path equals prefix or starts with "prefix/". */
static bool
path_starts_with(const char *path, const char *prefix)
{
    size_t n = strlen(prefix);

    if (strncmp(path, prefix, n) != 0)
	return false;
    return path[n] == '\0' || path[n] == '/';
}

static bool
meta_ignore(GNode *gn, const char *p)
{
    char fname[MAXPATHLEN];

    if (p == NULL)
	return true;

    if (*p == '/') {
	cached_realpath(p, fname); /* clean it up */
	if (has_any_prefix(fname, &metaIgnorePaths)) {
#ifdef DEBUG_META_MODE
	    DEBUG1(META, "meta_oodate: ignoring path: %s\n", p);
#endif
	    return true;
	}
    }

    if (metaIgnorePatterns) {
	const char *expr;
	char *pm;

	/*
	 * XXX: This variable is set on a target GNode but is not one of
	 * the usual local variables.  It should be deleted afterwards.
	 * Ideally it would not be created in the first place, just like
	 * in a .for loop.
	 */
	Var_Set(gn, ".p.", p);
	expr = "${" MAKE_META_IGNORE_PATTERNS ":@m@${.p.:M$m}@}";
	(void)Var_Subst(expr, gn, VARE_WANTRES, &pm);
	/* TODO: handle errors */
	if (pm[0] != '\0') {
#ifdef DEBUG_META_MODE
	    DEBUG1(META, "meta_oodate: ignoring pattern: %s\n", p);
#endif
	    free(pm);
	    return true;
	}
	free(pm);
    }

    if (metaIgnoreFilter) {
	char *fm;

	/* skip if filter result is empty */
	snprintf(fname, sizeof fname,
		 "${%s:L:${%s:ts:}}",
		 p, MAKE_META_IGNORE_FILTER);
	(void)Var_Subst(fname, gn, VARE_WANTRES, &fm);
	/* TODO: handle errors */
	if (*fm == '\0') {
#ifdef DEBUG_META_MODE
	    DEBUG1(META, "meta_oodate: ignoring filtered: %s\n", p);
#endif
	    free(fm);
	    return true;
	}
	free(fm);
    }
    return false;
}

/*
 * When running with 'meta' functionality, a target can be out-of-date
 * if any of the references in its meta data file is more recent.
 * We have to track the latestdir on a per-process basis.
 */
#define LCWD_VNAME_FMT ".meta.%d.lcwd"
#define LDIR_VNAME_FMT ".meta.%d.ldir"

/*
 * It is possible that a .meta file is corrupted,
 * if we detect this we want to reproduce it.
 * Setting oodate true will have that effect.
 */
#define CHECK_VALID_META(p) if (!(p != NULL && *p != '\0')) { \
    warnx("%s: %u: malformed", fname, lineno); \
    oodate = true; \
    continue; \
    }

#define DEQUOTE(p) if (*p == '\'') {	\
    char *ep; \
    p++; \
    if ((ep = strchr(p, '\'')) != NULL) \
	*ep = '\0'; \
    }

static void
append_if_new(StringList *list, const char *str)
{
    StringListNode *ln;

    for (ln = list->first; ln != NULL; ln = ln->next)
	if (strcmp(ln->datum, str) == 0)
	    return;
    Lst_Append(list, bmake_strdup(str));
}

/* A "reserved" variable to store the command to be filtered */
#define META_CMD_FILTER_VAR ".MAKE.cmd_filtered"

static char *
meta_filter_cmd(GNode *gn, char *s)
{
    Var_Set(gn, META_CMD_FILTER_VAR, s);
    Var_Subst("${" META_CMD_FILTER_VAR ":${" MAKE_META_CMP_FILTER ":ts:}}", gn, VARE_WANTRES, &s);
    return s;
}

static int
meta_cmd_cmp(GNode *gn, char *a, char *b, bool filter)
{
    int rc;

    rc = strcmp(a, b);
    if (rc == 0 || !filter)
	return rc;
    a = meta_filter_cmd(gn, a);
    b = meta_filter_cmd(gn, b);
    rc = strcmp(a, b);
    free(a);
    free(b);
    Var_Delete(gn, META_CMD_FILTER_VAR);
    return rc;
}

bool
meta_oodate(GNode *gn, bool oodate)
{
    static char *tmpdir = NULL;
    static char cwd[MAXPATHLEN];
    char lcwd_vname[64];
    char ldir_vname[64];
    char lcwd[MAXPATHLEN];
    char latestdir[MAXPATHLEN];
    char fname[MAXPATHLEN];
    char fname1[MAXPATHLEN];
    char fname2[MAXPATHLEN];
    char fname3[MAXPATHLEN];
    FStr dname;
    const char *tname;
    char *p;
    char *link_src;
    char *move_target;
    static size_t cwdlen = 0;
    static size_t tmplen = 0;
    FILE *fp;
    bool needOODATE = false;
    StringList missingFiles;
    bool have_filemon = false;
    bool cmp_filter;

    if (oodate)
	return oodate;		/* we're done */

    dname = Var_Value(gn, ".OBJDIR");
    tname = GNode_VarTarget(gn);

    /* if this succeeds fname3 is realpath of dname */
    if (!meta_needed(gn, dname.str, fname3, false))
	goto oodate_out;
    dname.str = fname3;

    Lst_Init(&missingFiles);

    /*
     * We need to check if the target is out-of-date. This includes
     * checking if the expanded command has changed. This in turn
     * requires that all variables are set in the same way that they
     * would be if the target needs to be re-built.
     */
    GNode_SetLocalVars(gn);

    meta_name(fname, sizeof fname, dname.str, tname, dname.str);

#ifdef DEBUG_META_MODE
    DEBUG1(META, "meta_oodate: %s\n", fname);
#endif

    if ((fp = fopen(fname, "r")) != NULL) {
	static char *buf = NULL;
	static size_t bufsz;
	unsigned lineno = 0;
	int lastpid = 0;
	int pid;
	int x;
	StringListNode *cmdNode;
	struct cached_stat cst;

	if (buf == NULL) {
	    bufsz = 8 * BUFSIZ;
	    buf = bmake_malloc(bufsz);
	}

	if (cwdlen == 0) {
	    if (getcwd(cwd, sizeof cwd) == NULL)
		err(1, "Could not get current working directory");
	    cwdlen = strlen(cwd);
	}
	strlcpy(lcwd, cwd, sizeof lcwd);
	strlcpy(latestdir, cwd, sizeof latestdir);

	if (tmpdir == NULL) {
	    tmpdir = getTmpdir();
	    tmplen = strlen(tmpdir);
	}

	/* we want to track all the .meta we read */
	Global_Append(".MAKE.META.FILES", fname);

	cmp_filter = metaCmpFilter || Var_Exists(gn, MAKE_META_CMP_FILTER);

	cmdNode = gn->commands.first;
	while (!oodate && (x = fgetLine(&buf, &bufsz, 0, fp)) > 0) {
	    lineno++;
	    if (buf[x - 1] == '\n')
		buf[x - 1] = '\0';
	    else {
		warnx("%s: %u: line truncated at %u", fname, lineno, x);
		oodate = true;
		break;
	    }
	    link_src = NULL;
	    move_target = NULL;
	    /* Find the start of the build monitor section. */
	    if (!have_filemon) {
		if (strncmp(buf, "-- filemon", 10) == 0) {
		    have_filemon = true;
		    continue;
		}
		if (strncmp(buf, "# buildmon", 10) == 0) {
		    have_filemon = true;
		    continue;
		}
	    }

	    /* Delimit the record type. */
	    p = buf;
#ifdef DEBUG_META_MODE
	    DEBUG3(META, "%s: %u: %s\n", fname, lineno, buf);
#endif
	    strsep(&p, " ");
	    if (have_filemon) {
		/*
		 * We are in the 'filemon' output section.
		 * Each record from filemon follows the general form:
		 *
		 * <key> <pid> <data>
		 *
		 * Where:
		 * <key> is a single letter, denoting the syscall.
		 * <pid> is the process that made the syscall.
		 * <data> is the arguments (of interest).
		 */
		switch(buf[0]) {
		case '#':		/* comment */
		case 'V':		/* version */
		    break;
		default:
		    /*
		     * We need to track pathnames per-process.
		     *
		     * Each process run by make starts off in the 'CWD'
		     * recorded in the .meta file, if it chdirs ('C')
		     * elsewhere we need to track that - but only for
		     * that process.  If it forks ('F'), we initialize
		     * the child to have the same cwd as its parent.
		     *
		     * We also need to track the 'latestdir' of
		     * interest.  This is usually the same as cwd, but
		     * not if a process is reading directories.
		     *
		     * Each time we spot a different process ('pid')
		     * we save the current value of 'latestdir' in a
		     * variable qualified by 'lastpid', and
		     * re-initialize 'latestdir' to any pre-saved
		     * value for the current 'pid' and 'CWD' if none.
		     */
		    CHECK_VALID_META(p);
		    pid = atoi(p);
		    if (pid > 0 && pid != lastpid) {
			FStr ldir;

			if (lastpid > 0) {
			    /* We need to remember these. */
			    Global_Set(lcwd_vname, lcwd);
			    Global_Set(ldir_vname, latestdir);
			}
			snprintf(lcwd_vname, sizeof lcwd_vname, LCWD_VNAME_FMT, pid);
			snprintf(ldir_vname, sizeof ldir_vname, LDIR_VNAME_FMT, pid);
			lastpid = pid;
			ldir = Var_Value(SCOPE_GLOBAL, ldir_vname);
			if (ldir.str != NULL) {
			    strlcpy(latestdir, ldir.str, sizeof latestdir);
			    FStr_Done(&ldir);
			}
			ldir = Var_Value(SCOPE_GLOBAL, lcwd_vname);
			if (ldir.str != NULL) {
			    strlcpy(lcwd, ldir.str, sizeof lcwd);
			    FStr_Done(&ldir);
			}
		    }
		    /* Skip past the pid. */
		    if (strsep(&p, " ") == NULL)
			continue;
#ifdef DEBUG_META_MODE
		    if (DEBUG(META))
			debug_printf("%s: %u: %d: %c: cwd=%s lcwd=%s ldir=%s\n",
				     fname, lineno,
				     pid, buf[0], cwd, lcwd, latestdir);
#endif
		    break;
		}

		CHECK_VALID_META(p);

		/* Process according to record type. */
		switch (buf[0]) {
		case 'X':		/* eXit */
		    Var_Delete(SCOPE_GLOBAL, lcwd_vname);
		    Var_Delete(SCOPE_GLOBAL, ldir_vname);
		    lastpid = 0;	/* no need to save ldir_vname */
		    break;

		case 'F':		/* [v]Fork */
		    {
			char cldir[64];
			int child;

			child = atoi(p);
			if (child > 0) {
			    snprintf(cldir, sizeof cldir, LCWD_VNAME_FMT, child);
			    Global_Set(cldir, lcwd);
			    snprintf(cldir, sizeof cldir, LDIR_VNAME_FMT, child);
			    Global_Set(cldir, latestdir);
#ifdef DEBUG_META_MODE
			    if (DEBUG(META))
				debug_printf(
					"%s: %u: %d: cwd=%s lcwd=%s ldir=%s\n",
					fname, lineno,
					child, cwd, lcwd, latestdir);
#endif
			}
		    }
		    break;

		case 'C':		/* Chdir */
		    /* Update lcwd and latest directory. */
		    strlcpy(latestdir, p, sizeof latestdir);
		    strlcpy(lcwd, p, sizeof lcwd);
		    Global_Set(lcwd_vname, lcwd);
		    Global_Set(ldir_vname, lcwd);
#ifdef DEBUG_META_MODE
		    DEBUG4(META, "%s: %u: cwd=%s ldir=%s\n",
			   fname, lineno, cwd, lcwd);
#endif
		    break;

		case 'M':		/* renaMe */
		    /*
		     * For 'M'oves we want to check
		     * the src as for 'R'ead
		     * and the target as for 'W'rite.
		     */
		    {
			char *cp = p;	/* save this for a second */
			/* now get target */
			if (strsep(&p, " ") == NULL)
			    continue;
			CHECK_VALID_META(p);
			move_target = p;
			p = cp;
		    }
		    /* 'L' and 'M' put single quotes around the args */
		    DEQUOTE(p);
		    DEQUOTE(move_target);
		    /* FALLTHROUGH */
		case 'D':		/* unlink */
		    if (*p == '/') {
			/* remove any missingFiles entries that match p */
			StringListNode *ln = missingFiles.first;
			while (ln != NULL) {
			    StringListNode *next = ln->next;
			    if (path_starts_with(ln->datum, p)) {
				free(ln->datum);
				Lst_Remove(&missingFiles, ln);
			    }
			    ln = next;
			}
		    }
		    if (buf[0] == 'M') {
			/* the target of the mv is a file 'W'ritten */
#ifdef DEBUG_META_MODE
			DEBUG2(META, "meta_oodate: M %s -> %s\n",
			       p, move_target);
#endif
			p = move_target;
			goto check_write;
		    }
		    break;
		case 'L':		/* Link */
		    /*
		     * For 'L'inks check
		     * the src as for 'R'ead
		     * and the target as for 'W'rite.
		     */
		    link_src = p;
		    /* now get target */
		    if (strsep(&p, " ") == NULL)
			continue;
		    CHECK_VALID_META(p);
		    /* 'L' and 'M' put single quotes around the args */
		    DEQUOTE(p);
		    DEQUOTE(link_src);
#ifdef DEBUG_META_MODE
		    DEBUG2(META, "meta_oodate: L %s -> %s\n", link_src, p);
#endif
		    /* FALLTHROUGH */
		case 'W':		/* Write */
		check_write:
		    /*
		     * If a file we generated within our bailiwick
		     * but outside of .OBJDIR is missing,
		     * we need to do it again.
		     */
		    /* ignore non-absolute paths */
		    if (*p != '/')
			break;

		    if (Lst_IsEmpty(&metaBailiwick))
			break;

		    /* ignore cwd - normal dependencies handle those */
		    if (strncmp(p, cwd, cwdlen) == 0)
			break;

		    if (!has_any_prefix(p, &metaBailiwick))
			break;

		    /* tmpdir might be within */
		    if (tmplen > 0 && strncmp(p, tmpdir, tmplen) == 0)
			break;

		    /* ignore anything containing the string "tmp" */
		    /* XXX: The arguments to strstr must be swapped. */
		    if (strstr("tmp", p) != NULL)
			break;

		    if ((link_src != NULL && cached_lstat(p, &cst) < 0) ||
			(link_src == NULL && cached_stat(p, &cst) < 0)) {
			if (!meta_ignore(gn, p))
			    append_if_new(&missingFiles, p);
		    }
		    break;
		check_link_src:
		    p = link_src;
		    link_src = NULL;
#ifdef DEBUG_META_MODE
		    DEBUG1(META, "meta_oodate: L src %s\n", p);
#endif
		    /* FALLTHROUGH */
		case 'R':		/* Read */
		case 'E':		/* Exec */
		    /*
		     * Check for runtime files that can't
		     * be part of the dependencies because
		     * they are _expected_ to change.
		     */
		    if (meta_ignore(gn, p))
			break;

		    /*
		     * The rest of the record is the file name.
		     * Check if it's not an absolute path.
		     */
		    {
			char *sdirs[4];
			char **sdp;
			int sdx = 0;
			bool found = false;

			if (*p == '/') {
			    sdirs[sdx++] = p; /* done */
			} else {
			    if (strcmp(".", p) == 0)
				continue; /* no point */

			    /* Check vs latestdir */
			    snprintf(fname1, sizeof fname1, "%s/%s", latestdir, p);
			    sdirs[sdx++] = fname1;

			    if (strcmp(latestdir, lcwd) != 0) {
				/* Check vs lcwd */
				snprintf(fname2, sizeof fname2, "%s/%s", lcwd, p);
				sdirs[sdx++] = fname2;
			    }
			    if (strcmp(lcwd, cwd) != 0) {
				/* Check vs cwd */
				snprintf(fname3, sizeof fname3, "%s/%s", cwd, p);
				sdirs[sdx++] = fname3;
			    }
			}
			sdirs[sdx++] = NULL;

			for (sdp = sdirs; *sdp != NULL && !found; sdp++) {
#ifdef DEBUG_META_MODE
			    DEBUG3(META, "%s: %u: looking for: %s\n",
				   fname, lineno, *sdp);
#endif
			    if (cached_stat(*sdp, &cst) == 0) {
				found = true;
				p = *sdp;
			    }
			}
			if (found) {
#ifdef DEBUG_META_MODE
			    DEBUG3(META, "%s: %u: found: %s\n",
				   fname, lineno, p);
#endif
			    if (!S_ISDIR(cst.cst_mode) &&
				cst.cst_mtime > gn->mtime) {
				DEBUG3(META, "%s: %u: file '%s' is newer than the target...\n",
				       fname, lineno, p);
				oodate = true;
			    } else if (S_ISDIR(cst.cst_mode)) {
				/* Update the latest directory. */
				cached_realpath(p, latestdir);
			    }
			} else if (errno == ENOENT && *p == '/' &&
				   strncmp(p, cwd, cwdlen) != 0) {
			    /*
			     * A referenced file outside of CWD is missing.
			     * We cannot catch every eventuality here...
			     */
			    append_if_new(&missingFiles, p);
			}
		    }
		    if (buf[0] == 'E') {
			/* previous latestdir is no longer relevant */
			strlcpy(latestdir, lcwd, sizeof latestdir);
		    }
		    break;
		default:
		    break;
		}
		if (!oodate && buf[0] == 'L' && link_src != NULL)
		    goto check_link_src;
	    } else if (strcmp(buf, "CMD") == 0) {
		/*
		 * Compare the current command with the one in the
		 * meta data file.
		 */
		if (cmdNode == NULL) {
		    DEBUG2(META, "%s: %u: there were more build commands in the meta data file than there are now...\n",
			   fname, lineno);
		    oodate = true;
		} else {
		    const char *cp;
		    char *cmd = cmdNode->datum;
		    bool hasOODATE = false;

		    if (strstr(cmd, "$?") != NULL)
			hasOODATE = true;
		    else if ((cp = strstr(cmd, ".OODATE")) != NULL) {
			/* check for $[{(].OODATE[:)}] */
			if (cp > cmd + 2 && cp[-2] == '$')
			    hasOODATE = true;
		    }
		    if (hasOODATE) {
			needOODATE = true;
			DEBUG2(META, "%s: %u: cannot compare command using .OODATE\n",
			       fname, lineno);
		    }
		    (void)Var_Subst(cmd, gn, VARE_UNDEFERR, &cmd);
		    /* TODO: handle errors */

		    if ((cp = strchr(cmd, '\n')) != NULL) {
			int n;

			/*
			 * This command contains newlines, we need to
			 * fetch more from the .meta file before we
			 * attempt a comparison.
			 */
			/* first put the newline back at buf[x - 1] */
			buf[x - 1] = '\n';
			do {
			    /* now fetch the next line */
			    if ((n = fgetLine(&buf, &bufsz, x, fp)) <= 0)
				break;
			    x = n;
			    lineno++;
			    if (buf[x - 1] != '\n') {
				warnx("%s: %u: line truncated at %u", fname, lineno, x);
				break;
			    }
			    cp = strchr(cp + 1, '\n');
			} while (cp != NULL);
			if (buf[x - 1] == '\n')
			    buf[x - 1] = '\0';
		    }
		    if (p != NULL &&
			!hasOODATE &&
			!(gn->type & OP_NOMETA_CMP) &&
			(meta_cmd_cmp(gn, p, cmd, cmp_filter) != 0)) {
			DEBUG4(META, "%s: %u: a build command has changed\n%s\nvs\n%s\n",
			       fname, lineno, p, cmd);
			if (!metaIgnoreCMDs)
			    oodate = true;
		    }
		    free(cmd);
		    cmdNode = cmdNode->next;
		}
	    } else if (strcmp(buf, "CWD") == 0) {
		/*
		 * Check if there are extra commands now
		 * that weren't in the meta data file.
		 */
		if (!oodate && cmdNode != NULL) {
		    DEBUG2(META, "%s: %u: there are extra build commands now that weren't in the meta data file\n",
			   fname, lineno);
		    oodate = true;
		}
		CHECK_VALID_META(p);
		if (strcmp(p, cwd) != 0) {
		    DEBUG4(META, "%s: %u: the current working directory has changed from '%s' to '%s'\n",
			   fname, lineno, p, curdir);
		    oodate = true;
		}
	    }
	}

	fclose(fp);
	if (!Lst_IsEmpty(&missingFiles)) {
	    DEBUG2(META, "%s: missing files: %s...\n",
		   fname, (char *)missingFiles.first->datum);
	    oodate = true;
	}
	if (!oodate && !have_filemon && filemonMissing) {
	    DEBUG1(META, "%s: missing filemon data\n", fname);
	    oodate = true;
	}
    } else {
	if (writeMeta && (metaMissing || (gn->type & OP_META))) {
	    const char *cp = NULL;

	    /* if target is in .CURDIR we do not need a meta file */
	    if (gn->path != NULL && (cp = strrchr(gn->path, '/')) != NULL &&
		(cp > gn->path)) {
		if (strncmp(curdir, gn->path, (size_t)(cp - gn->path)) != 0) {
		    cp = NULL;		/* not in .CURDIR */
		}
	    }
	    if (cp == NULL) {
		DEBUG1(META, "%s: required but missing\n", fname);
		oodate = true;
		needOODATE = true;	/* assume the worst */
	    }
	}
    }

    Lst_DoneCall(&missingFiles, free);

    if (oodate && needOODATE) {
	/*
	 * Target uses .OODATE which is empty; or we wouldn't be here.
	 * We have decided it is oodate, so .OODATE needs to be set.
	 * All we can sanely do is set it to .ALLSRC.
	 */
	Var_Delete(gn, OODATE);
	Var_Set(gn, OODATE, GNode_VarAllsrc(gn));
    }

 oodate_out:
    FStr_Done(&dname);
    return oodate;
}

/* support for compat mode */

static int childPipe[2];

void
meta_compat_start(void)
{
#ifdef USE_FILEMON_ONCE
    /*
     * We need to re-open filemon for each cmd.
     */
    BuildMon *pbm = &Mybm;

    if (pbm->mfp != NULL && useFilemon) {
	meta_open_filemon(pbm);
    } else {
	pbm->mon_fd = -1;
	pbm->filemon = NULL;
    }
#endif
    if (pipe(childPipe) < 0)
	Punt("Cannot create pipe: %s", strerror(errno));
    /* Set close-on-exec flag for both */
    (void)fcntl(childPipe[0], F_SETFD, FD_CLOEXEC);
    (void)fcntl(childPipe[1], F_SETFD, FD_CLOEXEC);
}

void
meta_compat_child(void)
{
    meta_job_child(NULL);
    if (dup2(childPipe[1], 1) < 0 || dup2(1, 2) < 0)
	execDie("dup2", "pipe");
}

void
meta_compat_parent(pid_t child)
{
    int outfd, metafd, maxfd, nfds;
    char buf[BUFSIZ+1];
    fd_set readfds;

    meta_job_parent(NULL, child);
    close(childPipe[1]);			/* child side */
    outfd = childPipe[0];
#ifdef USE_FILEMON
    metafd = Mybm.filemon != NULL ? filemon_readfd(Mybm.filemon) : -1;
#else
    metafd = -1;
#endif
    maxfd = -1;
    if (outfd > maxfd)
	    maxfd = outfd;
    if (metafd > maxfd)
	    maxfd = metafd;

    while (outfd != -1 || metafd != -1) {
	FD_ZERO(&readfds);
	if (outfd != -1) {
	    FD_SET(outfd, &readfds);
	}
	if (metafd != -1) {
	    FD_SET(metafd, &readfds);
	}
	nfds = select(maxfd + 1, &readfds, NULL, NULL, NULL);
	if (nfds == -1) {
	    if (errno == EINTR)
		continue;
	    err(1, "select");
	}

	if (outfd != -1 && FD_ISSET(outfd, &readfds) != 0) do {
	    /* XXX this is not line-buffered */
	    ssize_t nread = read(outfd, buf, sizeof buf - 1);
	    if (nread == -1)
		err(1, "read");
	    if (nread == 0) {
		close(outfd);
		outfd = -1;
		break;
	    }
	    fwrite(buf, 1, (size_t)nread, stdout);
	    fflush(stdout);
	    buf[nread] = '\0';
	    meta_job_output(NULL, buf, "");
	} while (false);
	if (metafd != -1 && FD_ISSET(metafd, &readfds) != 0) {
	    if (meta_job_event(NULL) <= 0)
		metafd = -1;
	}
    }
}

#endif /* USE_META */
