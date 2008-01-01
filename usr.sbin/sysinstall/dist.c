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
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <ctype.h>
#include <signal.h>
#include <libutil.h>

unsigned int Dists;
unsigned int SrcDists;
unsigned int XOrgDists;
unsigned int KernelDists;

enum _disttype { DT_TARBALL, DT_SUBDIST, DT_PACKAGE };

typedef struct _dist {
    char *my_name;
    unsigned int *my_mask;
    unsigned int my_bit;
    enum _disttype my_type;
    union {
	char *my_string;	/* DT_TARBALL & DT_PACKAGE */
	struct _dist *my_dist;	/* DT_SUBDIST */
    } my_data;
} Distribution;

static Distribution KernelDistTable[];
static Distribution SrcDistTable[];
static Distribution XOrgDistTable[];

#define	DTE_TARBALL(name, mask, flag, directory)			\
	{ name, mask, DIST_ ## flag, DT_TARBALL, { directory } }
#define	DTE_PACKAGE(name, mask, flag, package)				\
	{ name, mask, DIST_ ## flag, DT_PACKAGE, { package } }
#define	DTE_SUBDIST(name, mask, flag, subdist)				\
	{ name, mask, DIST_ ## flag, DT_SUBDIST, { .my_dist = subdist } }
#define	DTE_END			{ NULL, NULL, 0, 0, { NULL } }

#define	BASE_DIST	(&DistTable[0])

/* The top-level distribution categories */
static Distribution DistTable[] = {
    DTE_TARBALL("base",	    &Dists, BASE,     "/"),
    DTE_SUBDIST("kernels",  &Dists, KERNEL,   KernelDistTable),
    DTE_TARBALL("doc",	    &Dists, DOC,      "/"),
    DTE_TARBALL("games",    &Dists, GAMES,    "/"),
    DTE_TARBALL("manpages", &Dists, MANPAGES, "/"),
    DTE_TARBALL("catpages", &Dists, CATPAGES, "/"),
    DTE_TARBALL("proflibs", &Dists, PROFLIBS, "/"),
    DTE_TARBALL("dict",	    &Dists, DICT,     "/"),
    DTE_TARBALL("info",	    &Dists, INFO,     "/"),
#ifdef __amd64__
    DTE_TARBALL("lib32",    &Dists, LIB32,    "/"),
#endif
    DTE_SUBDIST("src",	    &Dists, SRC,      SrcDistTable),
    DTE_TARBALL("ports",    &Dists, PORTS,    "/usr"),
    DTE_TARBALL("local",    &Dists, LOCAL,    "/"),
    DTE_PACKAGE("X.Org",    &Dists, XORG,     "xorg"),
    DTE_END,
};

/* The kernel distributions */
static Distribution KernelDistTable[] = {
    DTE_TARBALL("GENERIC",  &KernelDists, KERNEL_GENERIC, "/boot"),
#ifdef WITH_SMP
    DTE_TARBALL("SMP", 	    &KernelDists, KERNEL_SMP,	  "/boot"),
#endif
    DTE_END,
};

/* The /usr/src distribution */
static Distribution SrcDistTable[] = {
    DTE_TARBALL("sbase",    &SrcDists, SRC_BASE,    "/usr/src"),
    DTE_TARBALL("scddl",    &SrcDists, SRC_CDDL,    "/usr/src"),
    DTE_TARBALL("scompat",  &SrcDists, SRC_COMPAT,  "/usr/src"),
    DTE_TARBALL("scontrib", &SrcDists, SRC_CONTRIB, "/usr/src"),
    DTE_TARBALL("scrypto",  &SrcDists, SRC_SCRYPTO, "/usr/src"),
    DTE_TARBALL("sgnu",	    &SrcDists, SRC_GNU,	    "/usr/src"),
    DTE_TARBALL("setc",	    &SrcDists, SRC_ETC,	    "/usr/src"),
    DTE_TARBALL("sgames",   &SrcDists, SRC_GAMES,   "/usr/src"),
    DTE_TARBALL("sinclude", &SrcDists, SRC_INCLUDE, "/usr/src"),
    DTE_TARBALL("skrb5",    &SrcDists, SRC_SKERBEROS5, "/usr/src"),
    DTE_TARBALL("slib",	    &SrcDists, SRC_LIB,	    "/usr/src"),
    DTE_TARBALL("slibexec", &SrcDists, SRC_LIBEXEC, "/usr/src"),
    DTE_TARBALL("srelease", &SrcDists, SRC_RELEASE, "/usr/src"),
    DTE_TARBALL("sbin",	    &SrcDists, SRC_BIN,	    "/usr/src"),
    DTE_TARBALL("ssecure",  &SrcDists, SRC_SSECURE, "/usr/src"),
    DTE_TARBALL("ssbin",    &SrcDists, SRC_SBIN,    "/usr/src"),
    DTE_TARBALL("sshare",   &SrcDists, SRC_SHARE,   "/usr/src"),
    DTE_TARBALL("ssys",	    &SrcDists, SRC_SYS,	    "/usr/src"),
    DTE_TARBALL("subin",    &SrcDists, SRC_UBIN,    "/usr/src"),
    DTE_TARBALL("susbin",   &SrcDists, SRC_USBIN,   "/usr/src"),
    DTE_TARBALL("stools",   &SrcDists, SRC_TOOLS,   "/usr/src"),
    DTE_TARBALL("srescue",  &SrcDists, SRC_RESCUE,  "/usr/src"),
    DTE_END,
};

static int	distMaybeSetPorts(dialogMenuItem *self);

static void
distVerifyFlags(void)
{
    if (SrcDists)
	Dists |= DIST_SRC;
    if (XOrgDists)
	Dists |= DIST_XORG;
    if (KernelDists)
	Dists |= DIST_KERNEL;
    if (isDebug()) {
	msgDebug("Dist Masks: Dists: %0x, Srcs: %0x Kernels: %0x\n", Dists,
	    SrcDists, KernelDists);
	msgDebug("XServer: %0x\n", XOrgDists);
    }
}

int
distReset(dialogMenuItem *self)
{
    Dists = 0;
    SrcDists = 0;
    XOrgDists = 0;
    KernelDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distConfig(dialogMenuItem *self)
{
    char *cp;

    distReset(NULL);

    if ((cp = variable_get(VAR_DIST_MAIN)) != NULL)
	Dists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_SRC)) != NULL)
	SrcDists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_X11)) != NULL)
	XOrgDists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_KERNEL)) != NULL)
	KernelDists = atoi(cp);

    distVerifyFlags();
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
distSetX(void)
{
    Dists |= DIST_XORG;
    XOrgDists = DIST_XORG_ALL;
    return DITEM_SUCCESS;
}

int
selectKernel(void)
{
#ifdef WITH_SMP
    /* select default kernel based on deduced cpu count */
    return NCpus > 1 ? DIST_KERNEL_SMP : DIST_KERNEL_GENERIC;
#else
    return DIST_KERNEL_GENERIC;
#endif
}

int
distSetDeveloper(dialogMenuItem *self)
{
    int i;

    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_ALL;
    KernelDists = selectKernel();
    i = distMaybeSetPorts(self);
    distVerifyFlags();
    return i;
}

int
distSetXDeveloper(dialogMenuItem *self)
{
    int i;

    i = distSetDeveloper(self);
    i |= distSetX();
    distVerifyFlags();
    return i;
}

int
distSetKernDeveloper(dialogMenuItem *self)
{
    int i;

    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_SYS | DIST_SRC_BASE;
    KernelDists = selectKernel();
    i = distMaybeSetPorts(self);
    distVerifyFlags();
    return i;
}

int
distSetXKernDeveloper(dialogMenuItem *self)
{
    int i;

    i = distSetKernDeveloper(self);
    i |= distSetX();
    distVerifyFlags();
    return i;
}

int
distSetUser(dialogMenuItem *self)
{
    int i;

    distReset(NULL);
    Dists = _DIST_USER;
    KernelDists = selectKernel();
    i = distMaybeSetPorts(self);
    distVerifyFlags();
    return i;
}

int
distSetXUser(dialogMenuItem *self)
{
    int i;

    i = distSetUser(self);
    i |= distSetX();
    distVerifyFlags();
    return i;
}

int
distSetMinimum(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = DIST_BASE | DIST_KERNEL;
    KernelDists = selectKernel();
    distVerifyFlags();
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetEverything(dialogMenuItem *self)
{
    int i;

    Dists = DIST_ALL;
    SrcDists = DIST_SRC_ALL;
    XOrgDists = DIST_XORG_ALL;
    KernelDists = DIST_KERNEL_ALL;
    i = distMaybeSetPorts(self);
    distVerifyFlags();
    return i | DITEM_REDRAW;
}

static int
distMaybeSetPorts(dialogMenuItem *self)
{
    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to install the FreeBSD ports collection?\n\n"
		  "This will give you ready access to over 17,000 ported software packages,\n"
		  "at a cost of around 400MB of disk space when \"clean\" and possibly\n"
		  "much more than that when a lot of the distribution tarballs are loaded\n"
		  "(unless you have the extra discs available from a FreeBSD CD/DVD distribution\n"
		  "and can mount them on /cdrom, in which case this is far less of a problem).\n\n"
		  "The ports collection is a very valuable resource and well worth having\n"
		  "on your /usr partition, so it is advisable to say Yes to this option.\n\n"
		  "For more information on the ports collection & the latest ports, visit:\n"
		  "    http://www.freebsd.org/ports\n"))
	Dists |= DIST_PORTS;
    else
	Dists &= ~DIST_PORTS;
    return DITEM_SUCCESS | DITEM_RESTORE;
}

static Boolean
distSetByName(Distribution *dist, char *name)
{
    int i, status = FALSE;
    
    /* Loop through current set */
    for (i = 0; dist[i].my_name; i++) {
	switch (dist[i].my_type) {
	case DT_TARBALL:
	case DT_PACKAGE:
	    if (!strcmp(dist[i].my_name, name)) {
		*(dist[i].my_mask) |= dist[i].my_bit;
		status = TRUE;
	    }
	    break;
	case DT_SUBDIST:
	    if (distSetByName(dist[i].my_data.my_dist, name)) {
		status = TRUE;
	    }
	    break;
	}
    }
    distVerifyFlags();
    return status;
}

static Boolean
distUnsetByName(Distribution *dist, char *name)
{
    int i, status = FALSE;
    
    /* Loop through current set */
    for (i = 0; dist[i].my_name; i++) {
	switch (dist[i].my_type) {
	case DT_TARBALL:
	case DT_PACKAGE:
	    if (!strcmp(dist[i].my_name, name)) {
		*(dist[i].my_mask) &= ~(dist[i].my_bit);
		status = TRUE;
	    }
	    break;
	case DT_SUBDIST:
	    if (distUnsetByName(dist[i].my_data.my_dist, name)) {
		status = TRUE;
	    }
	    break;
	}
    }
    return status;
}

/* Just for the dispatch stuff */
int
distSetCustom(dialogMenuItem *self)
{
    char *cp, *cp2, *tmp;

    if (!(tmp = variable_get(VAR_DISTS))) {
	msgDebug("distSetCustom() called without %s variable set.\n", VAR_DISTS);
	return DITEM_FAILURE;
    }

    cp = alloca(strlen(tmp) + 1);
    if (!cp)
	msgFatal("Couldn't alloca() %d bytes!\n", (int)(strlen(tmp) + 1));
    strcpy(cp, tmp);
    while (cp) {
	if ((cp2 = index(cp, ' ')) != NULL)
	    *(cp2++) = '\0';
	if (!distSetByName(DistTable, cp))
	    msgDebug("distSetCustom: Warning, no such release \"%s\"\n", cp);
	cp = cp2;
    }
    distVerifyFlags();
    return DITEM_SUCCESS;
}
    
/* Just for the dispatch stuff */
int
distUnsetCustom(dialogMenuItem *self)
{
    char *cp, *cp2, *tmp;

    if (!(tmp = variable_get(VAR_DISTS))) {
	msgDebug("distUnsetCustom() called without %s variable set.\n", VAR_DISTS);
	return DITEM_FAILURE;
    }

    cp = alloca(strlen(tmp) + 1);
    if (!cp)
	msgFatal("Couldn't alloca() %d bytes!\n", (int)(strlen(tmp) + 1));
    strcpy(cp, tmp);
    while (cp) {
	if ((cp2 = index(cp, ' ')) != NULL)
	    *(cp2++) = '\0';
	if (!distUnsetByName(DistTable, cp))
	    msgDebug("distUnsetCustom: Warning, no such release \"%s\"\n", cp);
	cp = cp2;
    }
    return DITEM_SUCCESS;
}

int
distSetSrc(dialogMenuItem *self)
{
    int i;

    dialog_clear_norefresh();
    if (!dmenuOpenSimple(&MenuSrcDistributions, FALSE))
	i = DITEM_FAILURE;
    else
	i = DITEM_SUCCESS;
    distVerifyFlags();
    return i | DITEM_RESTORE;
}

int
distSetKernel(dialogMenuItem *self)
{
    int i;

    dialog_clear_norefresh();
    if (!dmenuOpenSimple(&MenuKernelDistributions, FALSE))
	i = DITEM_FAILURE;
    else
	i = DITEM_SUCCESS;
    distVerifyFlags();
    return i | DITEM_RESTORE;
}

static Boolean got_intr = FALSE;

/* timeout handler */
static void
handle_intr(int sig)
{
    msgDebug("User generated interrupt.\n");
    got_intr = TRUE;
}

static int
check_for_interrupt(void)
{
    if (got_intr) {
	got_intr = FALSE;
	return TRUE;
    }
    return FALSE;
}

/*
 * translate distribution filename to lower case
 * as doTARBALL does in release/Makefile
 */
static void
translateDist(char trdist[PATH_MAX], const char *dist)
{
    int j;

    /*
     * translate distribution filename to lower case
     * as doTARBALL does in release/Makefile
     */
    for (j = 0; j < PATH_MAX-1 && dist[j] != '\0'; j++)
	trdist[j] = tolower(dist[j]);
    trdist[j] = '\0';
}

/*
 * Try to get distribution as multiple pieces, locating and parsing an
 * info file which tells us how many we need for this distribution.
 */
static Boolean
distExtractTarball(char *path, char *dist, char *my_dir, int is_base)
{
    char *buf = NULL, trdist[PATH_MAX], fname[PATH_MAX];
    struct timeval start, stop;
    int j, status, total, intr;
    int cpid, zpid, fd2, chunk, numchunks;
    properties dist_attr = NULL;
    const char *tmp;
    FILE *fp;

    translateDist(trdist, dist);
    if (isDebug())
	msgDebug("%s: path \"%s\" dist \"%s\" trdist \"%s\" "
		"my_dir \"%s\" %sis_base\n",
		__func__, path, dist, trdist, my_dir, is_base ? "" : "!");

    status = TRUE;
    numchunks = 0;
    snprintf(fname, sizeof (fname), "%s/%s.inf", path, trdist);

getinfo:
    fp = DEVICE_GET(mediaDevice, fname, TRUE);
    intr = check_for_interrupt();
    if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
	if (isDebug())
	    msgDebug("%s: fname %s fp: %p, intr: %d mediaDevice: %p\n",
		__func__, fname, fp, intr, mediaDevice);
	/* Hard error, can't continue */
	if (!msgYesNo("Unable to open %s: %s.\nReinitialize media?",
		fname, !intr ? "I/O error." : "User interrupt.")) {
	    DEVICE_SHUTDOWN(mediaDevice);
	    if (!DEVICE_INIT(mediaDevice))
		return (FALSE);
	    goto getinfo;
	} else
	    return (FALSE);
    } else if (fp == NULL) {
	/* No attributes file, so try as a single file. */
	snprintf(fname, sizeof(fname), "%s/%s.%s", path, trdist,
	    USE_GZIP ? "tgz" : "tbz");
	if (isDebug())
	    msgDebug("%s: fp is NULL (1) fname: %s\n", __func__, fname);
	/*
	 * Passing TRUE as 3rd parm to get routine makes this a "probing"
	 * get, for which errors are not considered too significant.
	 */
    getsingle:
	fp = DEVICE_GET(mediaDevice, fname, TRUE);
	intr = check_for_interrupt();
	if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
	    if (isDebug())
		msgDebug("%s: fname %s fp: %p, intr: %d mediaDevice: %p\n",
		    __func__, fname, fp, intr, mediaDevice);
	    /* Hard error, can't continue */
	    msgConfirm("Unable to open %s: %s", fname,
		!intr ? "I/O error" : "User interrupt");
	    DEVICE_SHUTDOWN(mediaDevice);
	    if (!DEVICE_INIT(mediaDevice))
		return (FALSE);
	    goto getsingle;
	} else if (fp != NULL) {
	    char *dir = root_bias(my_dir);

	    dialog_clear_norefresh();
	    msgNotify("Extracting %s into %s directory...", dist, dir);
	    status = mediaExtractDist(dir, dist, fp);
	    fclose(fp);
	    return (status);
	} else {
	    if (isDebug())
		msgDebug("%s: fp is NULL (2) fname %s\n", __func__, fname);
	    return (FALSE);
	}
    }

    if (isDebug())
	msgDebug("Parsing attributes file for distribution %s\n", dist);

    dist_attr = properties_read(fileno(fp));
    intr = check_for_interrupt();
    if (intr || !dist_attr) {
	if (isDebug())
	    msgDebug("%s: intr %d dist_attr %p\n", __func__, intr, dist_attr);
	msgConfirm("Cannot parse information file for the %s distribution: %s\n"
		   "Please verify that your media is valid and try again.",
		   dist, !intr ? "I/O error" : "User interrupt");
    } else {
	tmp = property_find(dist_attr, "Pieces");
	if (tmp)
	    numchunks = strtol(tmp, 0, 0);
    }
    fclose(fp);
    if (!numchunks) {
	if (isDebug())
	    msgDebug("%s: numchunks is zero\n", __func__);
	return (TRUE);
    }

    if (isDebug())
	msgDebug("Attempting to extract distribution from %u chunks.\n",
	    numchunks);

    total = 0;
    (void)gettimeofday(&start, (struct timezone *)NULL);

    /* We have one or more chunks, initialize unpackers... */
    mediaExtractDistBegin(root_bias(my_dir), &fd2, &zpid, &cpid);

    /* And go for all the chunks */
    dialog_clear_norefresh();
    for (chunk = 0; chunk < numchunks; chunk++) {
	int n, retval, last_msg, chunksize, realsize;
	char prompt[80];

	last_msg = 0;

    getchunk:
	snprintf(fname, sizeof(fname), "cksum.%c%c",  (chunk / 26) + 'a',
	    (chunk % 26) + 'a');
	tmp = property_find(dist_attr, fname);
	chunksize = 0;
	if (tmp) {
	    tmp = index(tmp, ' ');
	    chunksize = strtol(tmp, 0, 0);
	}
	snprintf(fname, sizeof(fname), "%s/%s.%c%c", path, trdist, (chunk / 26) + 'a',
	    (chunk % 26) + 'a');
	if (isDebug())
	    msgDebug("trying for piece %d of %d: %s\n", chunk + 1, numchunks,
		fname);
	fp = DEVICE_GET(mediaDevice, fname, FALSE);
	intr = check_for_interrupt();
	/* XXX: this can't work if we get an I/O error */
	if (fp <= (FILE *)NULL || intr) {
	    if (fp == NULL)
		msgConfirm("Failed to find %s on this media.  Reinitializing media.", fname);
	    else
		msgConfirm("Failed to retreive piece file %s.\n"
			   "%s: Reinitializing media.",
			   fname, !intr ? "I/O error" : "User interrupt");
	    DEVICE_SHUTDOWN(mediaDevice);
	    if (!DEVICE_INIT(mediaDevice))
		goto punt;
	    else
		goto getchunk;
	}

	snprintf(prompt, sizeof(prompt), "Extracting %s into %s directory...",
	    dist, root_bias(my_dir));
	dialog_gauge("Progress", prompt, 8, 15, 6, 50,
	    (chunk + 1) * 100 / numchunks);

	buf = safe_realloc(buf, chunksize);
	realsize = 0;
	while (1) {
	    int seconds;

	    n = fread(buf + realsize, 1, BUFSIZ, fp);
	    if (check_for_interrupt()) {
		msgConfirm("Media read error:  User interrupt.");
		fclose(fp);
		goto punt;
	    } else if (n <= 0)
		break;
	    total += n;
	    realsize += n;

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
		msgInfo("%10d bytes read from %s dist, chunk %2d of %2d @ %.1f KBytes/sec.",
			total, dist, chunk + 1, numchunks,
		        (total / seconds) / 1000.0);
	    }
	}
	fclose(fp);

	if (!chunksize || (realsize == chunksize)) {
	    /* No substitution necessary */
	    retval = write(fd2, buf, realsize);
	    if (retval != realsize) {
		fclose(fp);
		dialog_clear_norefresh();
		msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", retval, realsize);
		    goto punt;
	    }
	} else {
	    for (j = 0; j < realsize; j++) {
		/* On finding CRLF, skip the CR; don't exceed end of buffer. */
		if ((buf[j] != 0x0d) || (j == total - 1) || (buf[j + 1] != 0x0a)) {
		    retval = write(fd2, buf + j, 1);
		    if (retval != 1) {
			fclose(fp);
			dialog_clear_norefresh();
			msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", j, chunksize);
			goto punt;
		    }
		}
	    }
	}
    }
    goto done;

punt:
    status = FALSE;
done:
    properties_free(dist_attr);
    close(fd2);
    if (status != FALSE)
	status = mediaExtractDistEnd(zpid, cpid);
    else
	(void)mediaExtractDistEnd(zpid, cpid);

    safe_free(buf);
    return (status);
}

static Boolean
distExtract(char *parent, Distribution *me)
{
    int i, status;
    char *path, *dist;
    WINDOW *w = savescr();
    struct sigaction old, new;

    status = TRUE;
    if (isDebug())
	msgDebug("distExtract: parent: %s, me: %s\n", parent ? parent : "(none)", me->my_name);

    /* Make ^C fake a sudden timeout */
    new.sa_handler = handle_intr;
    new.sa_flags = 0;
    (void)sigemptyset(&new.sa_mask);
    dialog_clear_norefresh();
    dialog_msgbox("Please Wait", "Extracting all requested distributions...", -1, -1, 0);
    sigaction(SIGINT, &new, &old);

    /* Loop through to see if we're in our parent's plans */
    for (i = 0; me[i].my_name; i++) {
	dist = me[i].my_name;
	path = parent ? parent : dist;

	/* If our bit isn't set, go to the next */
	if (!(me[i].my_bit & *(me[i].my_mask)))
	    continue;

	switch (me[i].my_type) {
	case DT_SUBDIST:
	    /* Recurse if we actually have a sub-distribution */
	    status = distExtract(dist, me[i].my_data.my_dist);
	    if (!status) {
		dialog_clear_norefresh();
		msgConfirm("Unable to transfer all components of the %s distribution.\n"
		    "You may wish to switch media types and try again.\n",
		    me[i].my_name);
	    }
	    break;
	case DT_PACKAGE:
	    dialog_clear_norefresh();
	    msgNotify("Installing %s distribution...", dist);
	    status = (package_add(me[i].my_data.my_string) == DITEM_SUCCESS);
	    if (!status)
		dialog_clear_norefresh();
	    break;
	case DT_TARBALL:
	    status = distExtractTarball(path, dist, me[i].my_data.my_string,
		&me[i] == BASE_DIST);
	    if (!status) {
		dialog_clear_norefresh();
		if (me[i].my_bit != DIST_LOCAL) {
		    status = msgYesNo("Unable to transfer the %s distribution from\n%s.\n\n"
			              "Do you want to try to retrieve it again?",
				      me[i].my_name, mediaDevice->name);
		    if (!status)
			--i;
		    status = FALSE;
		}
	    }
	    break;
	}
	
	/*
	 * If extract was successful, remove ourselves from further
	 * consideration.
	 */
	if (status)
	    *(me[i].my_mask) &= ~(me[i].my_bit);
    }
    sigaction(SIGINT, &old, NULL);	/* Restore signal handler */
    restorescr(w);
    return status;
}

static void
printSelected(char *buf, int selected, Distribution *me, int *col)
{
    int i;

    /* Loop through to see if we're in our parent's plans */
    for (i = 0; me[i].my_name; i++) {

	/* If our bit isn't set, go to the next */
	if (!(me[i].my_bit & selected))
	    continue;

	*col += strlen(me[i].my_name);
	if (*col > 50) {
	    *col = 0;
	    strcat(buf, "\n");
	}
	sprintf(&buf[strlen(buf)], " %s", me[i].my_name);

	/* Recurse if have a sub-distribution */
	if (me[i].my_type == DT_SUBDIST)
	    printSelected(buf, *(me[i].my_mask), me[i].my_data.my_dist, col);
    }
}

int
distExtractAll(dialogMenuItem *self)
{
    int old_dists, old_kernel, retries = 0, status = DITEM_SUCCESS;
    char buf[512];
    WINDOW *w;

    /* paranoia */
    if (!Dists) {
	if (!dmenuOpenSimple(&MenuSubDistributions, FALSE) || !Dists)
	    return DITEM_FAILURE;
    }

    if (!mediaVerify() || !DEVICE_INIT(mediaDevice))
	return DITEM_FAILURE;

    old_dists = Dists;
    old_kernel = KernelDists;
    distVerifyFlags();

    dialog_clear_norefresh();
    w = savescr();
    msgNotify("Attempting to install all selected distributions..");

    /* Try for 3 times around the loop, then give up. */
    while (Dists && ++retries < 3)
	distExtract(NULL, DistTable);

    dialog_clear_norefresh();
    /* Only do base fixup if base dist was successfully extracted */
    if ((old_dists & DIST_BASE) && !(Dists & DIST_BASE))
	status |= installFixupBase(self);
    /* Only do kernel fixup if kernel dist was successfully extracted */
    if ((old_dists & DIST_KERNEL) && !(Dists & DIST_KERNEL))
	status |= installFixupKernel(self, old_kernel);

    /* Clear any local dist flags now */
    Dists &= ~DIST_LOCAL;

    if (Dists) {
	int col = 0;

	buf[0] = '\0';
	dialog_clear_norefresh();
	printSelected(buf, Dists, DistTable, &col);
	dialog_clear_norefresh();
	if (col) {
	    msgConfirm("Couldn't extract the following distributions.  This may\n"
		       "be because they were not available on the installation\n"
		       "media you've chosen:\n\n\t%s", buf);
	}
    }
    restorescr(w);
    return status;
}
