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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <paths.h>
#include "lib.h"
#include "add.h"

#include <libgen.h>
#include <signal.h>
#include <sys/wait.h>

static int pkg_do(char *);
static int sanity_check(char *);
static char LogDir[FILENAME_MAX];
static int zapLogDir;		/* Should we delete LogDir? */

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

/*
 * This is seriously ugly code following.  Written very fast!
 * [And subsequently made even worse..  Sigh!  This code was just born
 * to be hacked, I guess.. :) -jkh]
 */
static int
pkg_do(char *pkg)
{
    Package Plist;
    char pkg_fullname[FILENAME_MAX];
    char playpen[FILENAME_MAX];
    char extract_contents[FILENAME_MAX];
    char *extract;
    const char *where_to;
    FILE *cfile;
    int code;
    PackingList p;
    struct stat sb;
    int inPlace, conflictsfound, errcode;
    /* support for separate pre/post install scripts */
    int new_m = 0;
    char pre_script[FILENAME_MAX] = INSTALL_FNAME;
    char post_script[FILENAME_MAX];
    char pre_arg[FILENAME_MAX], post_arg[FILENAME_MAX];
    char *conflict[2];
    char **matched;

    conflictsfound = 0;
    code = 0;
    zapLogDir = 0;
    LogDir[0] = '\0';
    strcpy(playpen, FirstPen);
    inPlace = 0;

    memset(&Plist, '\0', sizeof(Plist));

    /* Are we coming in for a second pass, everything already extracted? */
    if (!pkg) {
	fgets(playpen, FILENAME_MAX, stdin);
	playpen[strlen(playpen) - 1] = '\0'; /* pesky newline! */
	if (chdir(playpen) == FAIL) {
	    warnx("pkg_add in SLAVE mode can't chdir to %s", playpen);
	    return 1;
	}
	read_plist(&Plist, stdin);
	where_to = playpen;
    }
    /* Nope - do it now */
    else {
	/* Is it an ftp://foo.bar.baz/file.t[bg]z specification? */
	if (isURL(pkg)) {
	    if (!(where_to = fileGetURL(NULL, pkg, KeepPackage))) {
		warnx("unable to fetch '%s' by URL", pkg);
		return 1;
	    }
	    strcpy(pkg_fullname, pkg);
	    cfile = fopen(CONTENTS_FNAME, "r");
	    if (!cfile) {
		warnx(
		"unable to open table of contents file '%s' - not a package?",
		CONTENTS_FNAME);
		goto bomb;
	    }
	    read_plist(&Plist, cfile);
	    fclose(cfile);
	}
	else {
	    strcpy(pkg_fullname, pkg);		/*
						 * Copy for sanity's sake,
						 * could remove pkg_fullname
						 */
	    if (strcmp(pkg, "-")) {
		if (stat(pkg_fullname, &sb) == FAIL) {
		    warnx("can't stat package file '%s'", pkg_fullname);
		    goto bomb;
		}
		sprintf(extract_contents, "--fast-read %s", CONTENTS_FNAME);
		extract = extract_contents;
	    }
	    else {
		extract = NULL;
		sb.st_size = 100000;	/* Make up a plausible average size */
	    }
	    if (!(where_to = make_playpen(playpen, sb.st_size * 4)))
		errx(1, "unable to make playpen for %lld bytes", (long long)sb.st_size * 4);
	    /* Since we can call ourselves recursively, keep notes on where we came from */
	    if (!getenv("_TOP"))
		setenv("_TOP", where_to, 1);
	    if (unpack(pkg_fullname, extract)) {
		warnx(
	"unable to extract table of contents file from '%s' - not a package?",
		pkg_fullname);
		goto bomb;
	    }
	    cfile = fopen(CONTENTS_FNAME, "r");
	    if (!cfile) {
		warnx(
	"unable to open table of contents file '%s' - not a package?",
		CONTENTS_FNAME);
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
			vsystem("/bin/mkdir -p %s", p->name);
			if (chdir(p->name) == -1) {
			    warn("unable to change directory to '%s'", p->name);
			    goto bomb;
			}
		    }
		    where_to = p->name;
		    inPlace = 1;
		}
		else {
		    warnx(
		"no prefix specified in '%s' - this is a bad package!",
			pkg_fullname);
		    goto bomb;
		}
	    }

	    /*
	     * Apply a crude heuristic to see how much space the package will
	     * take up once it's unpacked.  I've noticed that most packages
	     * compress an average of 75%, so multiply by 4 for good measure.
	     */

	    if (!extract && !inPlace && min_free(playpen) < sb.st_size * 4) {
		warnx("projected size of %lld exceeds available free space.\n"
"Please set your PKG_TMPDIR variable to point to a location with more\n"
		       "free space and try again", (long long)sb.st_size * 4);
		warnx("not extracting %s\ninto %s, sorry!",
			pkg_fullname, where_to);
		goto bomb;
	    }

	    /* If this is a direct extract and we didn't want it, stop now */
	    if (inPlace && Fake)
		goto success;

	    /* Finally unpack the whole mess.  If extract is null we
	       already + did so so don't bother doing it again. */
	    if (extract && unpack(pkg_fullname, NULL)) {
		warnx("unable to extract '%s'!", pkg_fullname);
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
    /* Protect against old packages with bogus @name and origin fields */
    if (Plist.name == NULL)
	Plist.name = "anonymous";
    if (Plist.origin == NULL)
	Plist.origin = "anonymous/anonymous";

    /*
     * See if we're already registered either with the same name (the same
     * version) or some other version with the same origin.
     */
    if ((isinstalledpkg(Plist.name) > 0 ||
         matchbyorigin(Plist.origin, NULL) != NULL) && !Force) {
	warnx("package '%s' or its older version already installed%s",
	      Plist.name, FailOnAlreadyInstalled ? "" : " (ignored)");
	code = FailOnAlreadyInstalled != FALSE;
	goto success;	/* close enough for government work */
    }

    /* Now check the packing list for conflicts */
    if (!IgnoreDeps){
    for (p = Plist.head; p != NULL; p = p->next) {
	if (p->type == PLIST_CONFLICTS) {
	    int i;
	    conflict[0] = strdup(p->name);
	    conflict[1] = NULL;
	    matched = matchinstalled(MATCH_GLOB, conflict, &errcode);
	    free(conflict[0]);
	    if (errcode == 0 && matched != NULL)
		for (i = 0; matched[i] != NULL; i++)
		    if (isinstalledpkg(matched[i]) > 0) {
			warnx("package '%s' conflicts with %s", Plist.name,
				matched[i]);
			conflictsfound = 1;
		    }

	    continue;
	}
    }
    if(conflictsfound) {
	if(!Force) {
	    warnx("please use pkg_delete first to remove conflicting package(s) or -f to force installation");
	    code = 1;
	    goto bomb;
	} else
	    warnx("-f specified; proceeding anyway");
    }

#if ENSURE_THAT_ALL_REQUIREMENTS_ARE_MET
    /*
     * Before attempting to do the slave mode bit, ensure that we've
     * downloaded & processed everything we need.
     * It's possible that we haven't already installed all of our
     * dependencies if the dependency list was misgenerated due to
     * other dynamic dependencies or if a dependency was added to a
     * package without all REQUIRED_BY packages being regenerated.
     */
    for (p = pkg ? Plist.head : NULL; p; p = p->next) {
	const char *ext;
	char *deporigin;

	if (p->type != PLIST_PKGDEP)
	    continue;
	deporigin = (p->next->type == PLIST_DEPORIGIN) ? p->next->name : NULL;

	if (isinstalledpkg(p->name) <= 0 &&
	    !(deporigin != NULL && matchbyorigin(deporigin, NULL) != NULL)) {
	    char subpkg[FILENAME_MAX], *sep;

	    strlcpy(subpkg, pkg, sizeof subpkg);
	    if ((sep = strrchr(subpkg, '/')) != NULL) {
		*sep = '\0';
		if ((sep = strrchr(subpkg, '/')) != NULL) {
		    *sep = '\0';
		    strlcat(subpkg, "/All/", sizeof subpkg);
		    strlcat(subpkg, p->name, sizeof subpkg);
		    if ((ext = strrchr(pkg, '.')) == NULL)
			ext = ".tbz";
		    strlcat(subpkg, ext, sizeof subpkg);
		    pkg_do(subpkg);
		}
	    }
	}
    }
#endif

    /* Now check the packing list for dependencies */
    for (p = Plist.head; p ; p = p->next) {
	char *deporigin;

	if (p->type != PLIST_PKGDEP)
	    continue;
	deporigin = (p->next->type == PLIST_DEPORIGIN) ? p->next->name : NULL;
	if (Verbose) {
	    printf("Package '%s' depends on '%s'", Plist.name, p->name);
	    if (deporigin != NULL)
		printf(" with '%s' origin", deporigin);
	    printf(".\n");
	}
	if (isinstalledpkg(p->name) <= 0 &&
	    !(deporigin != NULL && matchbyorigin(deporigin, NULL) != NULL)) {
	    char path[FILENAME_MAX];
	    const char *cp = NULL;

	    if (!Fake) {
		char prefixArg[2 + MAXPATHLEN]; /* "-P" + Prefix */
		if (PrefixRecursive) {
		    strlcpy(prefixArg, "-P", sizeof(prefixArg));
		    strlcat(prefixArg, Prefix, sizeof(prefixArg));
		}
		if (!isURL(pkg) && !getenv("PKG_ADD_BASE")) {
		    const char *ext;

		    ext = strrchr(pkg_fullname, '.');
		    if (ext == NULL)
			ext = ".tbz";
		    snprintf(path, FILENAME_MAX, "%s/%s%s", getenv("_TOP"), p->name, ext);
		    if (fexists(path))
			cp = path;
		    else
			cp = fileFindByPath(pkg, p->name);
		    if (cp) {
			if (Verbose)
			    printf("Loading it from %s.\n", cp);
			if (vsystem("%s %s %s '%s'", PkgAddCmd, Verbose ? "-v " : "", PrefixRecursive ? prefixArg : "", cp)) {
			    warnx("autoload of dependency '%s' failed%s",
				cp, Force ? " (proceeding anyway)" : "!");
			    if (!Force)
				++code;
			}
		    }
		    else {
			warnx("could not find package %s %s",
			      p->name, Force ? " (proceeding anyway)" : "!");
			if (!Force)
			    ++code;
		    }
		}
		else if ((cp = fileGetURL(pkg, p->name, KeepPackage)) != NULL) {
		    if (Verbose)
			printf("Finished loading %s via a URL\n", p->name);
		    if (!fexists("+CONTENTS")) {
			warnx("autoloaded package %s has no +CONTENTS file?",
				p->name);
			if (!Force)
			    ++code;
		    }
		    else if (vsystem("(pwd; /bin/cat +CONTENTS) | %s %s %s %s -S", PkgAddCmd, Verbose ? "-v" : "", PrefixRecursive ? prefixArg : "", KeepPackage ? "-K" : "")) {
			warnx("pkg_add of dependency '%s' failed%s",
				p->name, Force ? " (proceeding anyway)" : "!");
			if (!Force)
			    ++code;
		    }
		    else if (Verbose)
			printf("\t'%s' loaded successfully.\n", p->name);
		    /* Nuke the temporary playpen */
		    leave_playpen();
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
    } /* if (!IgnoreDeps) */

    if (code != 0)
	goto bomb;

    /* Look for the requirements file */
    if (fexists(REQUIRE_FNAME)) {
	vsystem("/bin/chmod +x %s", REQUIRE_FNAME);	/* be sure */
	if (Verbose)
	    printf("Running requirements file first for %s..\n", Plist.name);
	if (!Fake && vsystem("./%s %s INSTALL", REQUIRE_FNAME, Plist.name)) {
	    warnx("package %s fails requirements %s", pkg_fullname,
		   Force ? "installing anyway" : "- not installed");
	    if (!Force) {
		code = 1;
		goto success;	/* close enough for government work */
	    }
	}
    }

    /*
     * Test whether to use the old method of passing tokens to installation
     * scripts, and set appropriate variables..
     */

    if (fexists(POST_INSTALL_FNAME)) {
	new_m = 1;
	sprintf(post_script, "%s", POST_INSTALL_FNAME);
	pre_arg[0] = '\0';
	post_arg[0] = '\0';
    } else {
	if (fexists(INSTALL_FNAME)) {
	    sprintf(post_script, "%s", INSTALL_FNAME);
	    sprintf(pre_arg, "PRE-INSTALL");
	    sprintf(post_arg, "POST-INSTALL");
	}
    }

    /* If we're really installing, and have an installation file, run it */
    if (!NoInstall && fexists(pre_script)) {
	vsystem("/bin/chmod +x %s", pre_script);	/* make sure */
	if (Verbose)
	    printf("Running pre-install for %s..\n", Plist.name);
	if (!Fake && vsystem("./%s %s %s", pre_script, Plist.name, pre_arg)) {
	    warnx("install script returned error status");
	    unlink(pre_script);
	    code = 1;
	    goto success;		/* nothing to uninstall yet */
	}
    }

    /* Now finally extract the entire show if we're not going direct */
    if (!inPlace && !Fake)
	extract_plist(".", &Plist);

    if (!Fake && fexists(MTREE_FNAME)) {
	if (Verbose)
	    printf("Running mtree for %s..\n", Plist.name);
	p = find_plist(&Plist, PLIST_CWD);
	if (Verbose)
	    printf("mtree -U -f %s -d -e -p %s >%s\n", MTREE_FNAME, p ? p->name : "/", _PATH_DEVNULL);
	if (!Fake) {
	    if (vsystem("/usr/sbin/mtree -U -f %s -d -e -p %s >%s", MTREE_FNAME, p ? p->name : "/", _PATH_DEVNULL))
		warnx("mtree returned a non-zero status - continuing");
	}
    }

    /* Run the installation script one last time? */
    if (!NoInstall && fexists(post_script)) {
	vsystem("/bin/chmod +x %s", post_script);	/* make sure */
	if (Verbose)
	    printf("Running post-install for %s..\n", Plist.name);
	if (!Fake && vsystem("./%s %s %s", post_script, Plist.name, post_arg)) {
	    warnx("install script returned error status");
	    unlink(post_script);
	    code = 1;
	    goto fail;
	}
    }

    /* Time to record the deed? */
    if (!NoRecord && !Fake) {
	char contents[FILENAME_MAX];
	char **depnames = NULL, **deporigins = NULL, ***depmatches;
	int i, dep_count = 0;
	FILE *contfile;

	if (getuid() != 0)
	    warnx("not running as root - trying to record install anyway");
	sprintf(LogDir, "%s/%s", LOG_DIR, Plist.name);
	zapLogDir = 1;
	if (Verbose)
	    printf("Attempting to record package into %s..\n", LogDir);
	if (make_hierarchy(LogDir)) {
	    warnx("can't record package into '%s', you're on your own!",
		   LogDir);
	    bzero(LogDir, FILENAME_MAX);
	    code = 1;
	    goto success;	/* close enough for government work */
	}
	/* Make sure pkg_info can read the entry */
	vsystem("/bin/chmod a+rx %s", LogDir);
	move_file(".", DESC_FNAME, LogDir);
	move_file(".", COMMENT_FNAME, LogDir);
	if (fexists(INSTALL_FNAME))
	    move_file(".", INSTALL_FNAME, LogDir);
	if (fexists(POST_INSTALL_FNAME))
	    move_file(".", POST_INSTALL_FNAME, LogDir);
	if (fexists(DEINSTALL_FNAME))
	    move_file(".", DEINSTALL_FNAME, LogDir);
	if (fexists(POST_DEINSTALL_FNAME))
	    move_file(".", POST_DEINSTALL_FNAME, LogDir);
	if (fexists(REQUIRE_FNAME))
	    move_file(".", REQUIRE_FNAME, LogDir);
	if (fexists(DISPLAY_FNAME))
	    move_file(".", DISPLAY_FNAME, LogDir);
	if (fexists(MTREE_FNAME))
	    move_file(".", MTREE_FNAME, LogDir);
	sprintf(contents, "%s/%s", LogDir, CONTENTS_FNAME);
	contfile = fopen(contents, "w");
	if (!contfile) {
	    warnx("can't open new contents file '%s'! can't register pkg",
		contents);
	    goto success; /* can't log, but still keep pkg */
	}
	write_plist(&Plist, contfile);
	fclose(contfile);
	for (p = Plist.head; p ; p = p->next) {
	    char *deporigin;

	    if (p->type != PLIST_PKGDEP)
		continue;
	    deporigin = (p->next->type == PLIST_DEPORIGIN) ? p->next->name :
							     NULL;
	    if (Verbose) {
		printf("Trying to record dependency on package '%s'", p->name);
		if (deporigin != NULL)
		    printf(" with '%s' origin", deporigin);
		printf(".\n");
	    }

	    if (deporigin) {
		/* Defer to origin lookup */
		depnames = realloc(depnames, (dep_count + 1) * sizeof(*depnames));
		depnames[dep_count] = p->name;
		deporigins = realloc(deporigins, (dep_count + 2) * sizeof(*deporigins));
		deporigins[dep_count] = deporigin;
		deporigins[dep_count + 1] = NULL;
		dep_count++;
	    } else {
	       /* No origin recorded, try to register on literal package name */
	       sprintf(contents, "%s/%s/%s", LOG_DIR, p->name,
		     REQUIRED_BY_FNAME);
	       contfile = fopen(contents, "a");
	       if (!contfile) {
		  warnx("can't open dependency file '%s'!\n"
			"dependency registration is incomplete", contents);
	       } else {
		  fprintf(contfile, "%s\n", Plist.name);
		  if (fclose(contfile) == EOF) {
		     warnx("cannot properly close file %s", contents);
		  }
	       }
	    }
	}
	if (dep_count > 0) {
	    depmatches = matchallbyorigin((const char **)deporigins, NULL);
	    free(deporigins);
	    if (!IgnoreDeps && depmatches) {
		for (i = 0; i < dep_count; i++) {
		    if (depmatches[i]) {
			int j;
			char **tmp = depmatches[i];
			for (j = 0; tmp[j] != NULL; j++) {
			    /* Origin looked up */
			    sprintf(contents, "%s/%s/%s", LOG_DIR, tmp[j],
				REQUIRED_BY_FNAME);
			    if (depnames[i] && strcmp(depnames[i], tmp[j]) != 0)
				warnx("warning: package '%s' requires '%s', but '%s' "
				    "is installed", Plist.name, depnames[i], tmp[j]);
			    contfile = fopen(contents, "a");
			    if (!contfile) {
				warnx("can't open dependency file '%s'!\n"
				    "dependency registration is incomplete", contents);
			    } else {
				fprintf(contfile, "%s\n", Plist.name);
				if (fclose(contfile) == EOF)
				    warnx("cannot properly close file %s", contents);
			    }
			}
		    } else if (depnames[i]) {
			/* No package present with this origin, try literal package name */
			sprintf(contents, "%s/%s/%s", LOG_DIR, depnames[i],
			    REQUIRED_BY_FNAME);
			contfile = fopen(contents, "a");
			if (!contfile) {
			    warnx("can't open dependency file '%s'!\n"
				"dependency registration is incomplete", contents);
			} else {
			    fprintf(contfile, "%s\n", Plist.name);
			    if (fclose(contfile) == EOF) {
				warnx("cannot properly close file %s", contents);
			    }
			}
		    }
		}
	    }
	}
	if (Verbose)
	    printf("Package %s registered in %s\n", Plist.name, LogDir);
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
	} else {
    	    if (!Fake) {
		warnx("cannot open %s as display file", buf);  
	    }
	}
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
    leave_playpen();
    return code;
}

static int
sanity_check(char *pkg)
{
    int code = 0;

    if (!fexists(CONTENTS_FNAME)) {
	warnx("package %s has no CONTENTS file!", pkg);
	code = 1;
    }
    else if (!fexists(COMMENT_FNAME)) {
	warnx("package %s has no COMMENT file!", pkg);
	code = 1;
    }
    else if (!fexists(DESC_FNAME)) {
	warnx("package %s has no DESC file!", pkg);
	code = 1;
    }
    return code;
}

void
cleanup(int sig)
{
    static int in_cleanup = 0;

    if (!in_cleanup) {
	in_cleanup = 1;
    	if (sig)
	    printf("Signal %d received, cleaning up..\n", sig);
    	if (!Fake && zapLogDir && LogDir[0])
	    vsystem("%s -rf %s", REMOVE_CMD, LogDir);
    	while (leave_playpen())
	    ;
    }
    if (sig)
	exit(1);
}
