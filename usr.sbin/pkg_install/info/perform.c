#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/usr.sbin/pkg_install/info/perform.c,v 1.29.2.2 2000/09/25 07:35:08 marko Exp $";
#endif

/*
 * FreeBSD install - a package for the installation and maintainance
 * of non-core utilities.
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
 * Jordan K. Hubbard
 * 23 Aug 1993
 *
 * This is the main body of the info module.
 *
 */

#include "lib.h"
#include "info.h"

#include <fts.h>
#include <signal.h>
#include <err.h>

static int fname_cmp(const FTSENT **, const FTSENT **);
static int pkg_do(char *);

int
pkg_perform(char **pkgs)
{
    int i, err_cnt = 0;
    char *tmp;

    signal(SIGINT, cleanup);

    tmp = getenv(PKG_DBDIR);
    if (!tmp)
	tmp = DEF_LOG_DIR;
    /* Overriding action? */
    if (CheckPkg) {
	char buf[FILENAME_MAX];

	snprintf(buf, FILENAME_MAX, "%s/%s", tmp, CheckPkg);
	return abs(access(buf, R_OK));
    }
    else if (AllInstalled) {
	FTS *ftsp;
	FTSENT *f;
	char *paths[2];

	if (!isdir(tmp))
	    return 1;
	paths[0] = tmp;
	paths[1] = NULL;
	ftsp = fts_open(paths, FTS_LOGICAL | FTS_NOCHDIR | FTS_NOSTAT,
	  fname_cmp);
	if (ftsp != NULL) {
	    while ((f = fts_read(ftsp)) != NULL) {
		if (f->fts_info == FTS_D && f->fts_level == 1) {
		    err_cnt += pkg_do(f->fts_name);
		    fts_set(ftsp, f, FTS_SKIP);
		}
	    }
	    fts_close(ftsp);
	}
    }
    else
	for (i = 0; pkgs[i]; i++)
	    err_cnt += pkg_do(pkgs[i]);
    return err_cnt;
}

static char *Home;

static int
pkg_do(char *pkg)
{
    Boolean installed = FALSE, isTMP = FALSE;
    char log_dir[FILENAME_MAX];
    char fname[FILENAME_MAX];
    Package plist;
    FILE *fp;
    struct stat sb;
    char *cp = NULL;
    int code = 0;

    if (isURL(pkg)) {
	if ((cp = fileGetURL(NULL, pkg)) != NULL) {
	    strcpy(fname, cp);
	    isTMP = TRUE;
	}
    }
    else if (fexists(pkg) && isfile(pkg)) {
	int len;

	if (*pkg != '/') {
	    if (!getcwd(fname, FILENAME_MAX))
		upchuck("getcwd");
	    len = strlen(fname);
	    snprintf(&fname[len], FILENAME_MAX - len, "/%s", pkg);
	}
	else
	    strcpy(fname, pkg);
	cp = fname;
    }
    else {
	if ((cp = fileFindByPath(NULL, pkg)) != NULL)
	    strncpy(fname, cp, FILENAME_MAX);
    }
    if (cp) {
	/*
	 * Apply a crude heuristic to see how much space the package will
	 * take up once it's unpacked.  I've noticed that most packages
	 * compress an average of 75%, but we're only unpacking the + files so
	 * be very optimistic.
	 */
	if (stat(fname, &sb) == FAIL) {
	    warnx("can't stat package file '%s'", fname);
	    code = 1;
	    goto bail;
	}
	Home = make_playpen(PlayPen, sb.st_size / 2);
	if (unpack(fname, "+*")) {
	    warnx("error during unpacking, no info for '%s' available", pkg);
	    code = 1;
	    goto bail;
	}
    }
    /* It's not an ininstalled package, try and find it among the installed */
    else {
	char *tmp;

	sprintf(log_dir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
		pkg);
	if (!fexists(log_dir)) {
	    warnx("can't find package `%s' installed or in a file!", pkg);
	    return 1;
	}
	if (chdir(log_dir) == FAIL) {
	    warnx("can't change directory to '%s'!", log_dir);
	    return 1;
	}
	installed = TRUE;
    }

    /* Suck in the contents list */
    plist.head = plist.tail = NULL;
    fp = fopen(CONTENTS_FNAME, "r");
    if (!fp) {
	warnx("unable to open %s file", CONTENTS_FNAME);
	code = 1;
	goto bail;
    }
    /* If we have a prefix, add it now */
    read_plist(&plist, fp);
    fclose(fp);

    /*
     * Index is special info type that has to override all others to make
     * any sense.
     */
    if (Flags & SHOW_INDEX) {
	char tmp[FILENAME_MAX];

	snprintf(tmp, FILENAME_MAX, "%-19s ", pkg);
	show_index(tmp, COMMENT_FNAME);
    }
    else {
	/* Start showing the package contents */
	if (!Quiet)
	    printf("%sInformation for %s:\n\n", InfoPrefix, pkg);
	if (Flags & SHOW_COMMENT)
	    show_file("Comment:\n", COMMENT_FNAME);
	if (Flags & SHOW_REQUIRE)
	    show_plist("Depends on:\n", &plist, PLIST_PKGDEP);
	if ((Flags & SHOW_REQBY) && !isemptyfile(REQUIRED_BY_FNAME))
	    show_file("Required by:\n", REQUIRED_BY_FNAME);
	if (Flags & SHOW_DESC)
	    show_file("Description:\n", DESC_FNAME);
	if ((Flags & SHOW_DISPLAY) && fexists(DISPLAY_FNAME))
	    show_file("Install notice:\n", DISPLAY_FNAME);
	if (Flags & SHOW_PLIST)
	    show_plist("Packing list:\n", &plist, (plist_t)-1);
	if ((Flags & SHOW_INSTALL) && fexists(INSTALL_FNAME))
	    show_file("Install script:\n", INSTALL_FNAME);
	if ((Flags & SHOW_INSTALL) && fexists(POST_INSTALL_FNAME))
	    show_file("Post-Install script:\n", POST_INSTALL_FNAME);
	if ((Flags & SHOW_DEINSTALL) && fexists(DEINSTALL_FNAME))
	    show_file("De-Install script:\n", DEINSTALL_FNAME);
	if ((Flags & SHOW_DEINSTALL) && fexists(POST_DEINSTALL_FNAME))
	    show_file("Post-DeInstall script:\n", POST_DEINSTALL_FNAME);
	if ((Flags & SHOW_MTREE) && fexists(MTREE_FNAME))
	    show_file("mtree file:\n", MTREE_FNAME);
	if (Flags & SHOW_PREFIX)
	    show_plist("Prefix(s):\n", &plist, PLIST_CWD);
	if (Flags & SHOW_FILES)
	    show_files("Files:\n", &plist);
	if ((Flags & SHOW_SIZE) && installed)
	    show_size("Package Size:\n", &plist);
	if (!Quiet)
	    puts(InfoPrefix);
    }
    free_plist(&plist);
 bail:
    leave_playpen();
    if (isTMP)
	unlink(fname);
    return code;
}

void
cleanup(int sig)
{
    static int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup = 1;
    	leave_playpen();
    }
    if (sig)
	exit(1);
}

static int
fname_cmp(const FTSENT **a, const FTSENT **b)
{
    return strcmp((*a)->fts_name, (*b)->fts_name);
}
