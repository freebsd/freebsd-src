/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $FreeBSD$
 *
 * Copyright (c) 1995
 *	Jordan Hubbard.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY JORDAN HUBBARD ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL JORDAN HUBBARD OR HIS PETS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, LIFE OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include "sysinstall.h"
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/stat.h>

static Boolean sigpipe_caught;

static void
catch_pipe(int sig)
{
    sigpipe_caught = TRUE;
}

extern PkgNode Top;

/* Like package_extract, but assumes current media device and chases deps */
int
package_add(char *name)
{
    PkgNodePtr tmp;
    int i;

    if (!mediaVerify())
	return DITEM_FAILURE;

    if (!DEVICE_INIT(mediaDevice))
	return DITEM_FAILURE;    

    i = index_initialize("packages/INDEX");
    if (DITEM_STATUS(i) != DITEM_SUCCESS)
	return i;

    tmp = index_search(&Top, name, &tmp);
    if (tmp)
	return index_extract(mediaDevice, &Top, tmp, FALSE);
    else {
	msgConfirm("Sorry, package %s was not found in the INDEX.", name);
	return DITEM_FAILURE;
    }
}

/* For use by dispatch */
int
packageAdd(dialogMenuItem *self)
{
    char *cp;

    cp = variable_get(VAR_PACKAGE);
    if (!cp) {
	msgDebug("packageAdd:  No package name passed in package variable\n");
	return DITEM_FAILURE;
    }
    else
	return package_add(cp);
}

Boolean
package_exists(char *name)
{
    char fname[FILENAME_MAX];
    int status /* = vsystem("pkg_info -e %s", name) */;

    /* XXX KLUDGE ALERT!  This makes evil assumptions about how XXX
     * packages register themselves and should *really be done with
     * `pkg_info -e <name>' except that this it's too slow for an
     * item check routine.. :-(
     */
    snprintf(fname, FILENAME_MAX, "/var/db/pkg/%s", name);
    status = access(fname, R_OK);
    if (isDebug())
	msgDebug("package check for %s returns %s.\n", name, status ? "failure" : "success");
    return !status;
}

/* Extract a package based on a namespec and a media device */
int
package_extract(Device *dev, char *name, Boolean depended)
{
    char path[511];
    int ret, last_msg = 0;
    FILE *fp;

    /* Check to make sure it's not already there */
    if (package_exists(name))
	return DITEM_SUCCESS;

    if (!DEVICE_INIT(dev)) {
	msgConfirm("Unable to initialize media type for package extract.");
	return DITEM_FAILURE;
    }

    /* If necessary, initialize the ldconfig hints */
    if (!file_readable("/var/run/ld-elf.so.hints"))
	vsystem("ldconfig /usr/lib /usr/lib/compat /usr/local/lib /usr/X11R6/lib");
    if (!file_readable("/var/run/ld.so.hints"))
	vsystem("ldconfig -aout /usr/lib/aout /usr/lib/compat/aout /usr/local/lib/aout /usr/X11R6/lib/aout");

    /* Be initially optimistic */
    ret = DITEM_SUCCESS;
    /* Make a couple of paranoid locations for temp files to live if user specified none */
    if (!variable_get(VAR_PKG_TMPDIR)) {
	/* Set it to a location with as much space as possible */
	variable_set2(VAR_PKG_TMPDIR, "/var/tmp", 0);
    }
    Mkdir(variable_get(VAR_PKG_TMPDIR));
    vsystem("chmod 1777 %s", variable_get(VAR_PKG_TMPDIR));

    if (!index(name, '/')) {
	if (!strpbrk(name, "-_"))
	    sprintf(path, "packages/Latest/%s.tgz", name);
	else
	    sprintf(path, "packages/All/%s%s", name, strstr(name, ".tgz") ? "" : ".tgz");
    }
    else
	sprintf(path, "%s%s", name, strstr(name, ".tgz") ? "" : ".tgz");

    /* We have a path, call the device strategy routine to get the file */
    fp = DEVICE_GET(dev, path, TRUE);
    if (fp) {
	int i = 0, tot, pfd[2];
	pid_t pid;
	WINDOW *w = savescr();

	sigpipe_caught = FALSE;
	signal(SIGPIPE, catch_pipe);

        dialog_clear_norefresh();
	msgNotify("Adding %s%s\nfrom %s", path, depended ? " (as a dependency)" : "", dev->name);
	pipe(pfd);
	pid = fork();
	if (!pid) {
	    dup2(pfd[0], 0); close(pfd[0]);
	    dup2(DebugFD, 1); dup2(1, 2);
	    close(pfd[1]);

	    /* Prevent pkg_add from wanting to interact in bad ways */
	    setenv("PACKAGE_BUILDING", "t", 1);
	    setenv("BATCH", "t", 1);

	    if (isDebug())
		i = execl("/usr/sbin/pkg_add", "/usr/sbin/pkg_add", "-v", "-",
		    (char *)0);
	    else
		i = execl("/usr/sbin/pkg_add", "/usr/sbin/pkg_add", "-",
		    (char *)0);
	}
	else {
	    char buf[BUFSIZ];
	    struct timeval start, stop;

	    close(pfd[0]);
	    tot = 0;
	    (void)gettimeofday(&start, (struct timezone *)0);

	    while (!sigpipe_caught && (i = fread(buf, 1, BUFSIZ, fp)) > 0) {
		int seconds;

		tot += i;
		/* Print statistics about how we're doing */
		(void) gettimeofday(&stop, (struct timezone *)0);
		stop.tv_sec = stop.tv_sec - start.tv_sec;
		stop.tv_usec = stop.tv_usec - start.tv_usec;
		if (stop.tv_usec < 0)
		    stop.tv_sec--, stop.tv_usec += 1000000;
		seconds = stop.tv_sec + (stop.tv_usec / 1000000.0);
		if (!seconds)
		    seconds = 1;
		if (seconds != last_msg) {
		    last_msg = seconds;
		    msgInfo("%10d bytes read from package %s @ %4.1f KBytes/second", tot, name, (tot / seconds) / 1000.0);
		}
		/* Write it out */
		if (sigpipe_caught || write(pfd[1], buf, i) != i) {
		    msgInfo("Write failure to pkg_add!  Package may be corrupt.");
		    break;
		}
	    }
	    close(pfd[1]);
	    fclose(fp);
	    if (sigpipe_caught)
		msgInfo("pkg_add(1) apparently did not like the %s package.", name);
	    else if (i == -1)
		msgInfo("I/O error while reading in the %s package.", name);
	    else
		msgInfo("Package %s read successfully - waiting for pkg_add(1)", name);
	    refresh();
	    i = waitpid(pid, &tot, 0);
	    dialog_clear_norefresh();
	    if (sigpipe_caught || i < 0 || WEXITSTATUS(tot)) {
		ret = DITEM_FAILURE;
		if (variable_get(VAR_NO_CONFIRM))
		    msgNotify("Add of package %s aborted, error code %d -\n"
			       "Please check the debug screen for more info.", name, WEXITSTATUS(tot));
		else
		    msgConfirm("Add of package %s aborted, error code %d -\n"
			       "Please check the debug screen for more info.", name, WEXITSTATUS(tot));
	    }
	    else
		msgNotify("Package %s was added successfully", name);

	    /* Now catch any stragglers */
	    while (wait3(&tot, WNOHANG, NULL) > 0);

	    sleep(1);
	    restorescr(w);
	}
    }
    else {
	dialog_clear_norefresh();
	if (variable_get(VAR_NO_CONFIRM))
	    msgNotify("Unable to fetch package %s from selected media.\n"
		      "No package add will be done.", name);
	else
	    msgConfirm("Unable to fetch package %s from selected media.\n"
		       "No package add will be done.", name);
	ret = DITEM_FAILURE;
    }
    signal(SIGPIPE, SIG_IGN);
    return ret;
}
