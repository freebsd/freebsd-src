#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
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
#include <err.h>
#include <signal.h>

static int pkg_do(char *);
static int find_pkg(char *, struct which_head *);
static int cmp_path(const char *, const char *, const char *);

int
pkg_perform(char **pkgs)
{
    char **matched;
    char *tmp;
    int err_cnt = 0;
    int i, errcode;

    signal(SIGINT, cleanup);

    tmp = getenv(PKG_DBDIR);
    if (!tmp)
	tmp = DEF_LOG_DIR;

    /* Overriding action? */
    if (CheckPkg) {
	char buf[FILENAME_MAX];

	snprintf(buf, FILENAME_MAX, "%s/%s", tmp, CheckPkg);
	return abs(access(buf, R_OK));
	/* Not reached */
    } else if (!TAILQ_EMPTY(whead)) {
	return find_pkg(tmp, whead);
    }

    if (MatchType != MATCH_EXACT) {
	matched = matchinstalled(MatchType, pkgs, &errcode);
	if (errcode != 0)
	    return 1;
	    /* Not reached */

	if (matched != NULL)
	    pkgs = matched;
	else switch (MatchType) {
	    case MATCH_GLOB:
		break;
	    case MATCH_ALL:
		warnx("no packages installed");
		return 0;
		/* Not reached */
	    case MATCH_REGEX:
		warnx("no packages match pattern(s)");
		return 1;
		/* Not reached */
	    default:
		break;
	}
    }

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
	if ((Flags & SHOW_CKSUM) && installed)
	    show_cksum("Mismatched Checksums:\n", &plist);
	if (Flags & SHOW_ORIGIN)
	    show_origin("Origin:\n", &plist);
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

/*
 * Comparison to see if the path we're on matches the
 * one we are looking for.
 */
static int
cmp_path(const char *target, const char *current, const char *cwd) 
{
    char *loc, *temp;
    int rval;

    asprintf(&temp, "%s/%s", cwd, current);
    if (temp == NULL)
        errx(2, NULL);

    /*
     * Make sure there's no multiple /'s, since some plists
     * seem to have them and it could screw up our strncmp.
     */
    while ((loc = strstr(temp, "//")) != NULL)
	strcpy(loc, loc + 1);

    if (strcmp(target, temp) == 0)
	rval = 1;
    else
	rval = 0;

    free(temp);
    return rval;
}

/* 
 * Look through package dbs in db_dir and find which
 * packages installed the files in which_list.
 */
static int 
find_pkg(char *db_dir, struct which_head *which_list)
{
    char **installed;
    int errcode, i;
    struct which_entry *wp;

    TAILQ_FOREACH(wp, which_list, next) {
	/* If it's not a file, we'll see if it's an executable. */
	if (isfile(wp->file) == FALSE) {
	    if (strchr(wp->file, '/') == NULL) {
		char *tmp;
		tmp = vpipe("/usr/bin/which %s", wp->file);
		if (tmp == NULL) {
		    warnx("%s: file is not in PATH", wp->file);
		    wp->skip = TRUE;
		} else
		    strlcpy(wp->file, tmp, PATH_MAX);
		free(tmp);
	    } else {
		warnx("%s: file cannot be found", wp->file);
		wp->skip = TRUE;
	    }
	} else if (wp->file[0] != '/') {
	    /*
	     * If it is a file, and it doesn't start with a /, then it's a 
	     * relative path.  in order to give us some chance of getting a 
	     * successful match, tack the current working directory on the 
	     * beginning.  this won't work for filenames that include .. or . 
	     * or extra /'s, but it's better than nothing).
	     */
	    char *curdir, *tmp;

	    curdir = getcwd(NULL, PATH_MAX);
	    if (curdir == NULL)
		err(2, NULL);

	    asprintf(&tmp, "%s/%s", curdir, wp->file);
	    if (tmp == NULL)
		err(2, NULL);

	    if (!isfile(tmp)) {
		warnx("%s: file cannot be found", tmp);
		wp->skip = TRUE;
	    } else
		strlcpy(wp->file, tmp, PATH_MAX);

	    free(tmp);
	    free(curdir);
	}
    }

    installed = matchinstalled(MATCH_ALL, NULL, &errcode);
    if (installed == NULL)
        return errcode;
 
    for (i = 0; installed[i] != NULL; i++) {
     	FILE *fp;
     	Package pkg;
     	PackingList itr;
     	char *cwd = NULL;
     	char tmp[PATH_MAX];

	snprintf(tmp, PATH_MAX, "%s/%s/%s", db_dir, installed[i],
		 CONTENTS_FNAME);
	fp = fopen(tmp, "r");
	if (fp == NULL) {
	    warn("%s", tmp);
	    return 1;
	}

	pkg.head = pkg.tail = NULL;
	read_plist(&pkg, fp);
	fclose(fp);
	for (itr = pkg.head; itr != pkg.tail; itr = itr->next) {
	    if (itr->type == PLIST_CWD) {
		cwd = itr->name;
	    } else if (itr->type == PLIST_FILE) {
		TAILQ_FOREACH(wp, which_list, next) {
		    if (wp->skip == TRUE)
			continue;
		    if (!cmp_path(wp->file, itr->name, cwd))
			continue;
		    if (wp->package[0] != '\0') {
			warnx("Both %s and %s claim to have installed %s\n",
			      wp->package, installed[i], wp->file);
		    } else {
			strlcpy(wp->package, installed[i], PATH_MAX);
		    }
		}
	    }
	}
	free_plist(&pkg);
    }

    TAILQ_FOREACH(wp, which_list, next) {
	if (wp->package[0] != '\0') {
	    if (Quiet)
		puts(wp->package);
	    else
		printf("%s was installed by package %s\n", \
		       wp->file, wp->package);
	}
    }
    while (!TAILQ_EMPTY(which_list)) {
	wp = TAILQ_FIRST(which_list);
	TAILQ_REMOVE(which_list, wp, next);
	free(wp);
    }

    free(which_list);
    return 0;
}
