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
 * URL file access utilities.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "lib.h"
#include <err.h>
#include <fetch.h>
#include <libgen.h>
#include <sys/wait.h>
#include <stdio.h>

/*
 * Try and fetch a file by URL, returning the directory name for where
 * it's unpacked, if successful.
 */
char *
fileGetURL(const char *base, const char *spec, int keep_package)
{
    char *cp, *rp, *tmp;
    char fname[FILENAME_MAX];
    char pen[FILENAME_MAX];
    char pkg[FILENAME_MAX];
    char buf[8192];
    FILE *ftp;
    pid_t tpid;
    int pfd[2], pstat, r, w = 0;
    char *hint;
    int fd, pkgfd = 0;

    rp = NULL;
    /* Special tip that sysinstall left for us */
    hint = getenv("PKG_ADD_BASE");
    if (!isURL(spec)) {
	if (!base && !hint)
	    return NULL;
	/*
	 * We've been given an existing URL (that's known-good) and now we need
	 * to construct a composite one out of that and the basename we were
	 * handed as a dependency.
	 */
	if (base) {
	    strcpy(fname, base);
	    /*
	     * Advance back two slashes to get to the root of the package
	     * hierarchy
	     */
	    cp = strrchr(fname, '/');
	    if (cp) {
		*cp = '\0';	/* chop name */
		cp = strrchr(fname, '/');
	    }
	    if (cp) {
		*(cp + 1) = '\0';
		strcat(cp, "All/");
		strcat(cp, spec);
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500039
		strcat(cp, ".tbz");
#else
		strcat(cp, ".tgz");
#endif
	    }
	    else
		return NULL;
	}
	else {
	    /*
	     * Otherwise, we've been given an environment variable hinting
	     * at the right location from sysinstall
	     */
	    strcpy(fname, hint);
	    strcat(fname, spec);
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500039
	    strcat(fname, ".tbz");
#else
	    strcat(fname, ".tgz");
#endif
	}
    }
    else
	strcpy(fname, spec);

    if (keep_package) {
	tmp = getenv("PKGDIR");
	strlcpy(pkg, tmp ? tmp : ".", sizeof(pkg));
	tmp = basename(fname);
	strlcat(pkg, "/", sizeof(pkg));
	strlcat(pkg, tmp, sizeof(pkg));
	if ((pkgfd = open(pkg, O_WRONLY|O_CREAT|O_TRUNC, 0644)) == -1) {
	    printf("Error: Unable to open %s\n", pkg);
	    perror("open");
	    return NULL;
	}
    }

    fetchDebug = (Verbose > 0);
    if ((ftp = fetchGetURL(fname, Verbose ? "v" : NULL)) == NULL) {
	printf("Error: FTP Unable to get %s: %s\n",
	       fname, fetchLastErrString);
	return NULL;
    }

    if (isatty(0) || Verbose)
	printf("Fetching %s...", fname), fflush(stdout);
    pen[0] = '\0';
    if ((rp = make_playpen(pen, 0)) == NULL) {
	printf("Error: Unable to construct a new playpen for FTP!\n");
	fclose(ftp);
	return NULL;
    }
    if (pipe(pfd) == -1) {
	warn("pipe()");
	cleanup(0);
	exit(2);
    }
    if ((tpid = fork()) == -1) {
	warn("pipe()");
	cleanup(0);
	exit(2);
    }
    if (!tpid) {
	dup2(pfd[0], 0);
	for (fd = getdtablesize() - 1; fd >= 3; --fd)
	    close(fd);
	execl("/usr/bin/tar", "tar",
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500039
	    Verbose ? "-xpjvf" : "-xpjf",
#else
	    Verbose ? "-xpzvf" : "-xpzf",
#endif
	    "-", (char *)0);
	_exit(2);
    }
    close(pfd[0]);
    for (;;) {
	if ((r = fread(buf, 1, sizeof buf, ftp)) < 1)
	    break;
	if ((w = write(pfd[1], buf, r)) != r)
	    break;
	if (keep_package) {
	    if ((w = write(pkgfd, buf, r)) != r)
		break;
	}    
    }
    if (ferror(ftp))
	warn("warning: error reading from server");
    fclose(ftp);
    if (keep_package) {
	close(pkgfd);
    } 
    close(pfd[1]);
    if (w == -1)
	warn("warning: error writing to tar");
    tpid = waitpid(tpid, &pstat, 0);
    if (Verbose)
	printf("tar command returns %d status\n", WEXITSTATUS(pstat));
    if (rp && (isatty(0) || Verbose))
	printf(" Done.\n");
    return rp;
}
