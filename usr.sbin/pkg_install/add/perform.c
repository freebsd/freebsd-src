#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.18 1995/04/22 07:40:54 jkh Exp $";
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
 * 18 July 1993
 *
 * This is the main body of the add module.
 *
 */

#include "lib.h"
#include "add.h"

#include <signal.h>
#include <sys/wait.h>

static int pkg_do(char *);
static int sanity_check(char *);
static char LogDir[FILENAME_MAX];


int
pkg_perform(char **pkgs)
{
    int i, err_cnt = 0;

    signal(SIGINT, cleanup);
    signal(SIGHUP, cleanup);

    if (AddMode == SLAVE)
	err_cnt = pkg_do(NULL);
    else {
	for (i = 0; pkgs[i]; i++)
	    err_cnt += pkg_do(pkgs[i]);
    }
    return err_cnt;
}

static Package Plist;

/* This is seriously ugly code following.  Written very fast! */
static int
pkg_do(char *pkg)
{
    char pkg_fullname[FILENAME_MAX];
    char home[FILENAME_MAX];
    char extract_contents[FILENAME_MAX];
    char *where_to, *tmp;
    FILE *cfile;
    int code = 0;
    PackingList p;
    struct stat sb;

    /* Reset some state */
    if (Plist.head)
	free_plist(&Plist);
    LogDir[0] = '\0';
    if (AddMode == SLAVE) {
	char tmp_dir[FILENAME_MAX];

	fgets(tmp_dir, FILENAME_MAX, stdin);
	tmp_dir[strlen(tmp_dir) - 1] = '\0'; /* pesky newline! */
	if (chdir(tmp_dir) == FAIL) {
	    whinge("pkg_add in SLAVE mode can't chdir to %s.", tmp_dir);
	    return 1;
	}
	read_plist(&Plist, stdin);
    }
    else {
	if (!getcwd(home, FILENAME_MAX))
	    upchuck("getcwd"); 

	if (pkg[0] == '/')	/* full pathname? */
	    strcpy(pkg_fullname, pkg);
	else
	    sprintf(pkg_fullname, "%s/%s", home, pkg);
	if (!fexists(pkg_fullname)) {
	    whinge("Can't find package '%s'.", pkg_fullname);
	    return 1;
	}
	Home = make_playpen(PlayPen, 0);
	sprintf(extract_contents, "--fast-read %s", CONTENTS_FNAME);
	if (unpack(pkg_fullname, extract_contents)) {
	    whinge("Unable to extract table of contents file from `%s' - not a package?.", pkg_fullname);
	    goto bomb;
	}
	cfile = fopen(CONTENTS_FNAME, "r");
	if (!cfile) {
	    whinge("Unable to open table of contents file `%s' - not a package?", CONTENTS_FNAME);
	    goto bomb;
	}
	read_plist(&Plist, cfile);
	fclose(cfile);

	/*
	 * If we have a prefix, delete the first one we see and add this
	 * one in place of it.
	 */
	if (Prefix) {
	    delete_plist(&Plist, FALSE, PLIST_CWD, NULL);
	    add_plist_top(&Plist, PLIST_CWD, Prefix);
	}

	/* Extract directly rather than moving?  Oh goodie! */
	if (find_plist_option(&Plist, "extract-in-place")) {
	    if (Verbose)
		printf("Doing in-place extraction for %s\n", pkg_fullname);
	    p = find_plist(&Plist, PLIST_CWD);
	    if (p) {
		if (!isdir(p->name) && !NoInstall) {
		    if (Verbose)
			printf("Desired prefix of %s does not exist, creating..\n", p->name);
		    vsystem("mkdir -p %s", p->name);
		    if (chdir(p->name)) {
			whinge("Unable to change directory to `%s' - no permission?", p->name);
			perror("chdir");
			leave_playpen();
			return 1;
		    }
		}
		where_to = p->name;
	    }
	    else {
		whinge("No prefix specified in `%s' - this is a bad package!",
		       pkg_fullname);
		leave_playpen();
		return 1;
	    }
	}
	else
	    where_to = PlayPen;
	/*
	 * Apply a crude heuristic to see how much space the package will
	 * take up once it's unpacked.  I've noticed that most packages
	 * compress an average of 75%, so multiply by 4 for good measure.
	 */
	if (stat(pkg_fullname, &sb) == FAIL) {
	    whinge("Can't stat package file '%s'.", pkg_fullname);
	    return 1;
	}

	if (min_free(where_to) < sb.st_size * 4) {
	    whinge("Projected size of %d exceeds free space in %s.",
		   sb.st_size * 4, where_to);
	    whinge("Not extracting %s, sorry!", pkg_fullname);
	    goto bomb;
	}

	/* If this is a direct extract and we didn't want it, stop now */
	if (where_to != PlayPen && NoInstall)
	    goto success;

	setenv(PKG_PREFIX_VNAME,
	       (p = find_plist(&Plist, PLIST_CWD)) ? p->name : NULL, 1);
	/* Protect against old packages with bogus @name fields */
	PkgName = (p = find_plist(&Plist, PLIST_NAME)) ? p->name : "anonymous";

	/* See if we're already registered */
	sprintf(LogDir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
		basename_of(PkgName));
	if (isdir(LogDir)) {
	    char tmp[FILENAME_MAX];

	    whinge("Package `%s' already recorded as installed.\n", PkgName);
	    code = 1;
	    goto success;	/* close enough for government work */
        }

	/* Finally unpack the whole mess */
	if (unpack(pkg_fullname, NULL)) {
	    whinge("Unable to extract `%s'!", pkg_fullname);
	    goto bomb;
	}

	if (sanity_check(pkg_fullname))
	    goto bomb;

	/* If we're running in MASTER mode, just output the plist and return */
	if (AddMode == MASTER) {
	    printf("%s\n", where_playpen());
	    write_plist(&Plist, stdout);
	    return 0;
	}
    }
    for (p = Plist.head; p ; p = p->next) {
	if (p->type != PLIST_PKGDEP)
	    continue;
	if (Verbose)
	    printf("Package `%s' depends on `%s'", PkgName, p->name);
	if (!Fake && vsystem("pkg_info -e %s", p->name)) {
	    char *cp, tmp[FILENAME_MAX], path[FILENAME_MAX*2];

	    if (Verbose)
		printf(" which is not currently loaded");
	    cp = getenv("PKG_PATH");
	    if (!cp)
		cp = Home;
	    strcpy(path, cp);
	    cp = path;
	    while (cp) {
		char *cp2 = strsep(&cp, ":");

		sprintf(tmp, "%s/%s.tgz", cp2 ? cp2 : cp, p->name);
		if (fexists(tmp))
		    break;
	    }
	    if (fexists(tmp)) {
		if (Verbose)
		    printf(" but was found - loading:\n");
		if (vsystem("pkg_add %s", tmp)) {
		    whinge("Autoload of dependency package `%s' failed!%s",
			   p->name, Force ? " (proceeding anyway)" : "");
		    if (!Force)
			code++;
		}
		else if (Verbose)
		    printf("\t`%s' loaded successfully.\n", p->name);
	    }
	    else {
	    	printf("and was not found%s.\n",
	    	       Force ? " (proceeding anyway)" : "");
	    	if (!Force)
		    code++;
	    }
	}
        else if (Verbose)
	    printf(" - already installed.\n");
    }
    if (code != 0)
	goto success;	/* close enough for government work */

    if (fexists(REQUIRE_FNAME)) {
	vsystem("chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (Verbose)
	    printf("Running requirements file first for %s..\n", PkgName);
	if (!Fake && vsystem("./%s %s INSTALL", REQUIRE_FNAME, PkgName)) {
	    whinge("Package %s fails requirements %s",
		   pkg_fullname,
		   Force ? "installing anyway" : "- not installed.");
	    if (!Force) {
		code = 1;
		goto success;	/* close enough for government work */
	    }
	}
    }
    if (!NoInstall && fexists(INSTALL_FNAME)) {
	vsystem("chmod +x %s", INSTALL_FNAME);	/* make sure */
	if (Verbose)
	    printf("Running install with PRE-INSTALL for %s..\n", PkgName);
	if (!Fake && vsystem("./%s %s PRE-INSTALL", INSTALL_FNAME, PkgName)) {
	    whinge("Install script returned error status.");
	    unlink(INSTALL_FNAME);
	    code = 1;
	    goto success;		/* nothing to uninstall yet */
	}
    }
    extract_plist(home, &Plist);
    if (!NoInstall && fexists(MTREE_FNAME)) {
	if (Verbose)
	    printf("Running mtree for %s..\n", PkgName);
	p = find_plist(&Plist, PLIST_CWD);
	if (Verbose)
	    printf("mtree -u -f %s -d -e -p %s\n", MTREE_FNAME,
		   p ? p->name : "/");
	if (!Fake) {
	    if (vsystem("/usr/sbin/mtree -u -f %s -d -e -p %s",
			MTREE_FNAME, p ? p->name : "/")) {
		perror("error in the execution of mtree");
		unlink(MTREE_FNAME);
		goto fail;
	    }
	}
	unlink(MTREE_FNAME);
    }
    if (!NoInstall && fexists(INSTALL_FNAME)) {
	if (Verbose)
	    printf("Running install with POST-INSTALL for %s..\n", PkgName);
	if (!Fake && vsystem("./%s %s POST-INSTALL", INSTALL_FNAME, PkgName)) {
	    whinge("Install script returned error status.");
	    unlink(INSTALL_FNAME);
	    code = 1;
	    goto fail;
	}
	unlink(INSTALL_FNAME);
    }
    if (!NoRecord && !Fake) {
	char contents[FILENAME_MAX];
	FILE *cfile;

	umask(022);
	if (getuid() != 0)
	    whinge("Not running as root - trying to record install anyway.");
	if (!PkgName) {
	    whinge("No package name!  Can't record package, sorry.");
	    code = 1;
	    goto success;	/* well, partial anyway */
	}
	sprintf(LogDir, "%s/%s",
		(tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
    	    	basename_of(PkgName));
	if (Verbose)
	    printf("Attempting to record package into %s..\n", LogDir);
	if (make_hierarchy(LogDir)) {
	    whinge("Can't record package into '%s', you're on your own!",
		   LogDir);
	    bzero(LogDir, FILENAME_MAX);
	    code = 1;
	    goto success;	/* close enough for government work */
	}
	/* Make sure pkg_info can read the entry */
	vsystem("chmod a+rx %s", LogDir);
	if (fexists(DEINSTALL_FNAME))
	    move_file(".", DEINSTALL_FNAME, LogDir);
	if (fexists(REQUIRE_FNAME))
	    move_file(".", REQUIRE_FNAME, LogDir);
	sprintf(contents, "%s/%s", LogDir, CONTENTS_FNAME);
	cfile = fopen(contents, "w");
	if (!cfile) {
	    whinge("Can't open new contents file '%s'!  Can't register pkg.",
		   contents);
	    goto success; /* can't log, but still keep pkg */
	}
	write_plist(&Plist, cfile);
	fclose(cfile);
	move_file(".", DESC_FNAME, LogDir);
	move_file(".", COMMENT_FNAME, LogDir);
	if (fexists(DISPLAY_FNAME))
	    move_file(".", DISPLAY_FNAME, LogDir);
	for (p = Plist.head; p ; p = p->next) {
	    if (p->type != PLIST_PKGDEP)
		continue;
	    if (Verbose)
		printf("Attempting to record dependency on package `%s'\n",
		       p->name);
	    sprintf(contents, "%s/%s/%s",
	    	    (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
	    	    basename_of(p->name), REQUIRED_BY_FNAME);
	    cfile = fopen(contents, "a");
	    if (!cfile) {
		whinge("Can't open dependency file '%s'!\n\tDependency registration incomplete.",
		   contents);
		continue;
	    }
	    fprintf(cfile, "%s\n", basename_of(PkgName));
	    if (fclose(cfile) == EOF)
		warn("Cannot properly close file %s", contents);
	}
	if (Verbose)
	    printf("Package %s registered in %s\n", PkgName, LogDir);
    }
    
    if (p = find_plist(&Plist, PLIST_DISPLAY)) {
	FILE *fp;
	char buf[BUFSIZ];
	fp = fopen(p->name, "r");
	if (fp) {
	    putc('\n', stdout);
	    while (fgets(buf, sizeof(buf), fp))
		fputs(buf, stdout);
	    putc('\n', stdout);
	    (void) fclose(fp);
	} else
	    warn("Cannot open display file `%s'.", p->name);
    }

    goto success;

 bomb:
    code = 1;
    goto success;

 fail:
    /* Nuke the whole (installed) show, XXX but don't clean directories */
    if (!Fake)
	delete_package(FALSE, FALSE, &Plist);

 success:
    /* delete the packing list contents */
    leave_playpen();

    return code;
}

static int
sanity_check(char *pkg)
{
    if (!fexists(CONTENTS_FNAME)) {
	whinge("Package %s has no CONTENTS file!", pkg);
	return 1;
    }
    if (!fexists(COMMENT_FNAME)) {
	whinge("Package %s has no COMMENT file!", pkg);
	return 1;
    }
    if (!fexists(DESC_FNAME)) {
	whinge("Package %s has no DESC file!", pkg);
	return 1;
    }
    return 0;
}

void
cleanup(int signo)
{
    if (signo)
	printf("Signal %d received, cleaning up..\n", signo);
    if (Plist.head) {
	if (!Fake)
	    delete_package(FALSE, FALSE, &Plist);
	free_plist(&Plist);
    }
    if (!Fake && LogDir[0])
	vsystem("%s -rf %s", REMOVE_CMD, LogDir);
    leave_playpen();
}
