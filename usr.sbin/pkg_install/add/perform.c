#ifndef lint
static const char *rcsid = "$Id: perform.c,v 1.37.2.2 1997/02/15 14:19:04 jkh Exp $";
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
static char *Home;

/*
 * This is seriously ugly code following.  Written very fast!
 * [And subsequently made even worse..  Sigh!  This code was just born
 * to be hacked, I guess.. :) -jkh]
 */
static int
pkg_do(char *pkg)
{
    char pkg_fullname[FILENAME_MAX];
    char playpen[FILENAME_MAX];
    char extract_contents[FILENAME_MAX];
    char *where_to, *tmp, *extract;
    FILE *cfile;
    int code;
    PackingList p;
    struct stat sb;
    int inPlace;

    code = 0;
    LogDir[0] = '\0';
    strcpy(playpen, FirstPen);
    inPlace = 0;

    /* Are we coming in for a second pass, everything already extracted? */
    if (!pkg) {
	fgets(playpen, FILENAME_MAX, stdin);
	playpen[strlen(playpen) - 1] = '\0'; /* pesky newline! */
	if (chdir(playpen) == FAIL) {
	    whinge("pkg_add in SLAVE mode can't chdir to %s.", playpen);
	    return 1;
	}
	read_plist(&Plist, stdin);
	where_to = playpen;
    }
    /* Nope - do it now */
    else {
	/* Is it an ftp://foo.bar.baz/file.tgz specification? */
	if (isURL(pkg)) {
	    if (!(Home = fileGetURL(NULL, pkg))) {
		whinge("Unable to fetch `%s' by URL.", pkg);
		return 1;
	    }
	    where_to = Home;
	    strcpy(pkg_fullname, pkg);
	    cfile = fopen(CONTENTS_FNAME, "r");
	    if (!cfile) {
		whinge("Unable to open table of contents file `%s' - not a package?", CONTENTS_FNAME);
		goto bomb;
	    }
	    read_plist(&Plist, cfile);
	    fclose(cfile);
	}
	else {
	    strcpy(pkg_fullname, pkg);		/* copy for sanity's sake, could remove pkg_fullname */
	    if (strcmp(pkg, "-")) {
		if (stat(pkg_fullname, &sb) == FAIL) {
		    whinge("Can't stat package file '%s'.", pkg_fullname);
		    goto bomb;
		}
		sprintf(extract_contents, "--fast-read %s", CONTENTS_FNAME);
		extract = extract_contents;
	    }
	    else {
		extract = NULL;
		sb.st_size = 100000;	/* Make up a plausible average size */
	    }
	    Home = make_playpen(playpen, sb.st_size * 4);
	    if (!Home)
		whinge("Unable to make playpen for %d bytes.\n", sb.st_size * 4);
	    where_to = Home;
	    if (unpack(pkg_fullname, extract)) {
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

	    /* Extract directly rather than moving?  Oh goodie! */
	    if (find_plist_option(&Plist, "extract-in-place")) {
		if (Verbose)
		    printf("Doing in-place extraction for %s\n", pkg_fullname);
		p = find_plist(&Plist, PLIST_CWD);
		if (p) {
		    if (!isdir(p->name) && !Fake) {
			if (Verbose)
			    printf("Desired prefix of %s does not exist, creating..\n", p->name);
			vsystem("mkdir -p %s", p->name);
			if (chdir(p->name) == -1) {
			    whinge("Unable to change directory to `%s' - no permission?", p->name);
			    perror("chdir");
			    goto bomb;
			}
		    }
		    where_to = p->name;
		    inPlace = 1;
		}
		else {
		    whinge("No prefix specified in `%s' - this is a bad package!", pkg_fullname);
		    goto bomb;
		}
	    }

	    /*
	     * Apply a crude heuristic to see how much space the package will
	     * take up once it's unpacked.  I've noticed that most packages
	     * compress an average of 75%, so multiply by 4 for good measure.
	     */

	    if (!inPlace && min_free(playpen) < sb.st_size * 4) {
		whinge("Projected size of %d exceeds available free space.\n"
		       "Please set your PKG_TMPDIR variable to point to a location with more\n"
		       "free space and try again.", sb.st_size * 4);
		whinge("Not extracting %s\ninto %s, sorry!", pkg_fullname, where_to);
		goto bomb;
	    }

	    /* If this is a direct extract and we didn't want it, stop now */
	    if (inPlace && Fake)
		goto success;

	    /* Finally unpack the whole mess */
	    if (unpack(pkg_fullname, NULL)) {
		whinge("Unable to extract `%s'!", pkg_fullname);
		goto bomb;
	    }
	}

	/* Check for sanity and dependencies */
	if (sanity_check(pkg))
	    goto bomb;

	/* If we're running in MASTER mode, just output the plist and return */
	if (AddMode == MASTER) {
	    printf("%s\n", where_playpen());
	    write_plist(&Plist, stdout);
	    return 0;
	}
    }

    /*
     * If we have a prefix, delete the first one we see and add this
     * one in place of it.
     */
    if (Prefix) {
	delete_plist(&Plist, FALSE, PLIST_CWD, NULL);
	add_plist_top(&Plist, PLIST_CWD, Prefix);
    }

    setenv(PKG_PREFIX_VNAME, (p = find_plist(&Plist, PLIST_CWD)) ? p->name : ".", 1);
    /* Protect against old packages with bogus @name fields */
    PkgName = (p = find_plist(&Plist, PLIST_NAME)) ? p->name : "anonymous";

    /* See if we're already registered */
    sprintf(LogDir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR, PkgName);
    if (isdir(LogDir) && !Force) {
	whinge("Package `%s' already recorded as installed.\n", PkgName);
	code = 1;
	goto success;	/* close enough for government work */
    }

    /* Now check the packing list for dependencies */
    for (p = Plist.head; p ; p = p->next) {
	if (p->type != PLIST_PKGDEP)
	    continue;
	if (Verbose)
	    printf("Package `%s' depends on `%s'.\n", PkgName, p->name);
	if (vsystem("pkg_info -e %s", p->name)) {
	    char path[FILENAME_MAX], *cp = NULL;

	    if (!Fake) {
		if (!isURL(pkg) && !getenv("PKG_ADD_BASE")) {
		    snprintf(path, FILENAME_MAX, "%s/%s.tgz", Home, p->name);
		    if (fexists(path))
			cp = path;
		    else
			cp = fileFindByPath(pkg, p->name);
		    if (cp) {
			if (Verbose)
			    printf("Loading it from %s.\n", cp);
			if (vsystem("pkg_add %s", cp)) {
			    whinge("Autoload of dependency `%s' failed%s", cp, Force ? " (proceeding anyway)" : "!");
			    if (!Force)
				++code;
			}
		    }
		}
		else if ((cp = fileGetURL(pkg, p->name)) != NULL) {
		    if (Verbose)
			printf("Finished loading %s over FTP.\n", p->name);
		    if (!fexists("+CONTENTS")) {
			whinge("Autoloaded package %s has no +CONTENTS file?", p->name);
			if (!Force)
			    ++code;
		    }
		    else if (vsystem("(pwd; cat +CONTENTS) | pkg_add %s-S", Verbose ? "-v " : "")) {
			whinge("pkg_add of dependency `%s' failed%s", p->name, Force ? " (proceeding anyway)" : "!");
			if (!Force)
			    ++code;
		    }
		    else if (Verbose)
			printf("\t`%s' loaded successfully.\n", p->name);
		    /* Nuke the temporary playpen */
		    leave_playpen(cp);
		}
	    }
	    else {
		if (Verbose)
		    printf("and was not found%s.\n", Force ? " (proceeding anyway)" : "");
		else
		    printf("Package dependency %s for %s not found%s\n", p->name, pkg,
			   Force ? " (proceeding anyway)" : "!");
		if (!Force)
		    ++code;
	    }
	}
	else if (Verbose)
	    printf(" - already installed.\n");
    }

    if (code != 0)
	goto bomb;

    /* Look for the requirements file */
    if (fexists(REQUIRE_FNAME)) {
	vsystem("chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (Verbose)
	    printf("Running requirements file first for %s..\n", PkgName);
	if (!Fake && vsystem("./%s %s INSTALL", REQUIRE_FNAME, PkgName)) {
	    whinge("Package %s fails requirements %s", pkg_fullname,
		   Force ? "installing anyway" : "- not installed.");
	    if (!Force) {
		code = 1;
		goto success;	/* close enough for government work */
	    }
	}
    }

    /* If we're really installing, and have an installation file, run it */
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

    /* Now finally extract the entire show if we're not going direct */
    if (!inPlace && !Fake)
	extract_plist(".", &Plist);

    if (!Fake && fexists(MTREE_FNAME)) {
	if (Verbose)
	    printf("Running mtree for %s..\n", PkgName);
	p = find_plist(&Plist, PLIST_CWD);
	if (Verbose)
	    printf("mtree -U -f %s -d -e -p %s\n", MTREE_FNAME, p ? p->name : "/");
	if (!Fake) {
	    if (vsystem("/usr/sbin/mtree -U -f %s -d -e -p %s", MTREE_FNAME, p ? p->name : "/"))
		whinge("mtree returned a non-zero status - continuing.");
	}
	unlink(MTREE_FNAME);
    }

    /* Run the installation script one last time? */
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

    /* Time to record the deed? */
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
	sprintf(LogDir, "%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR, PkgName);
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
	    whinge("Can't open new contents file '%s'!  Can't register pkg.", contents);
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
		printf("Attempting to record dependency on package `%s'\n", p->name);
	    sprintf(contents, "%s/%s/%s", (tmp = getenv(PKG_DBDIR)) ? tmp : DEF_LOG_DIR,
	    	    basename_of(p->name), REQUIRED_BY_FNAME);
	    cfile = fopen(contents, "a");
	    if (!cfile)
		whinge("Warning: Can't open dependency file '%s'!\n"
		       "\tDependency registration is incomplete.", contents);
	    else {
		fprintf(cfile, "%s\n", PkgName);
		if (fclose(cfile) == EOF)
		    whinge("Cannot properly close file %s", contents);
	    }
	}
	if (Verbose)
	    printf("Package %s registered in %s\n", PkgName, LogDir);
    }

    if ((p = find_plist(&Plist, PLIST_DISPLAY)) != NULL) {
	FILE *fp;
	char buf[BUFSIZ];

	snprintf(buf, sizeof buf, "%s/%s", LogDir, p->name);
	fp = fopen(buf, "r");
	if (fp) {
	    putc('\n', stdout);
	    while (fgets(buf, sizeof(buf), fp))
		fputs(buf, stdout);
	    putc('\n', stdout);
	    (void) fclose(fp);
	} else
	    whinge("Cannot open %s as display file.", buf);
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
    free_plist(&Plist);
    leave_playpen(Home);
    return code;
}

static int
sanity_check(char *pkg)
{
    int code = 0;

    if (!fexists(CONTENTS_FNAME)) {
	whinge("Package %s has no CONTENTS file!", pkg);
	code = 1;
    }
    else if (!fexists(COMMENT_FNAME)) {
	whinge("Package %s has no COMMENT file!", pkg);
	code = 1;
    }
    else if (!fexists(DESC_FNAME)) {
	whinge("Package %s has no DESC file!", pkg);
	code = 1;
    }
    return code;
}

void
cleanup(int signo)
{
    if (signo)
	printf("Signal %d received, cleaning up..\n", signo);
    if (!Fake && LogDir[0])
	vsystem("%s -rf %s", REMOVE_CMD, LogDir);
    leave_playpen(Home);
}
