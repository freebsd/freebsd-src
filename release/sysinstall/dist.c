/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dist.c,v 1.75 1996/11/06 19:15:22 jkh Exp $
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

#include <sys/time.h>
#include "sysinstall.h"

unsigned int Dists;
unsigned int DESDists;
unsigned int SrcDists;
unsigned int XF86Dists;
unsigned int XF86ServerDists;
unsigned int XF86FontDists;

typedef struct _dist {
    char *my_name;
    char *my_dir;
    unsigned int *my_mask;
    unsigned int my_bit;
    struct _dist *my_dist;
} Distribution;

extern Distribution DistTable[];
extern Distribution DESDistTable[];
extern Distribution SrcDistTable[];
extern Distribution XF86DistTable[];
extern Distribution XF86FontDistTable[];
extern Distribution XF86ServerDistTable[];

/* The top-level distribution categories */
static Distribution DistTable[] = {
{ "bin",	"/",			&Dists,		DIST_BIN,		NULL		},
{ "doc",	"/",			&Dists,		DIST_DOC,		NULL		},
{ "games",	"/",			&Dists,		DIST_GAMES,		NULL		},
{ "manpages",	"/",			&Dists,		DIST_MANPAGES,		NULL		},
{ "proflibs",	"/",			&Dists,		DIST_PROFLIBS,		NULL		},
{ "dict",	"/",			&Dists,		DIST_DICT,		NULL		},
{ "info",	"/",			&Dists,		DIST_INFO,		NULL		},
{ "src",	"/",			&Dists,		DIST_SRC,		SrcDistTable	},
{ "des",	"/",			&Dists,		DIST_DES,		DESDistTable	},
{ "compat1x",	"/",			&Dists,		DIST_COMPAT1X,		NULL		},
{ "compat20",	"/",			&Dists,		DIST_COMPAT20,		NULL		},
{ "compat21",	"/",			&Dists,		DIST_COMPAT21,		NULL		},
{ "commerce",	"/usr/local",		&Dists,		DIST_COMMERCIAL,	NULL		},
{ "xperimnt",	"/usr/local",		&Dists,		DIST_EXPERIMENTAL,	NULL		},
{ "XF86312",	"/usr",			&Dists,		DIST_XF86,		XF86DistTable	},
{ NULL },
};

/* The DES distribution (not for export!) */
static Distribution DESDistTable[] = {
{ "des",        "/",                    &DESDists,	DIST_DES_DES,		NULL		},
{ "krb",	"/",			&DESDists,	DIST_DES_KERBEROS,	NULL		},
{ "ssecure",	"/usr/src",		&DESDists,	DIST_DES_SSECURE,	NULL		},
{ "sebones",	"/usr/src",		&DESDists,	DIST_DES_SEBONES,	NULL		},
{ NULL },
};

/* The /usr/src distribution */
static Distribution SrcDistTable[] = {
{ "sbase",	"/usr/src",		&SrcDists,	DIST_SRC_BASE,		NULL		},
{ "scontrib",	"/usr/src",		&SrcDists,	DIST_SRC_CONTRIB,	NULL		},
{ "sgnu",	"/usr/src",		&SrcDists,	DIST_SRC_GNU,		NULL		},
{ "setc",	"/usr/src",		&SrcDists,	DIST_SRC_ETC,		NULL		},
{ "sgames",	"/usr/src",		&SrcDists,	DIST_SRC_GAMES,		NULL		},
{ "sinclude",	"/usr/src",		&SrcDists,	DIST_SRC_INCLUDE,	NULL		},
{ "slib",	"/usr/src",		&SrcDists,	DIST_SRC_LIB,		NULL		},
{ "slibexec",	"/usr/src",		&SrcDists,	DIST_SRC_LIBEXEC,	NULL		},
{ "slkm",	"/usr/src",		&SrcDists,	DIST_SRC_LKM,		NULL		},
{ "srelease",	"/usr/src",		&SrcDists,	DIST_SRC_RELEASE,	NULL		},
{ "sbin",	"/usr/src",		&SrcDists,	DIST_SRC_BIN,		NULL		},
{ "ssbin",	"/usr/src",		&SrcDists,	DIST_SRC_SBIN,		NULL		},
{ "sshare",	"/usr/src",		&SrcDists,	DIST_SRC_SHARE,		NULL		},
{ "ssys",	"/usr/src",		&SrcDists,	DIST_SRC_SYS,		NULL		},
{ "subin",	"/usr/src",		&SrcDists,	DIST_SRC_UBIN,		NULL		},
{ "susbin",	"/usr/src",		&SrcDists,	DIST_SRC_USBIN,		NULL		},
{ "ssmailcf",	"/usr/src",		&SrcDists,	DIST_SRC_SMAILCF,	NULL		},
{ NULL },
};

/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
{ "XF8632",	"/usr",			&XF86Dists,	DIST_XF86_FONTS,	XF86FontDistTable },
{ "XF8632",	"/usr",			&XF86Dists,	DIST_XF86_SERVER,	XF86ServerDistTable },
{ "XF86-xc",	"/usr/X11R6/src",	&XF86Dists,	DIST_XF86_SRC,		NULL		},
{ "XF86-co",	"/usr/X11R6/src",	&XF86Dists,	DIST_XF86_CSRC,		NULL		},
{ "X32bin",	"/usr",			&XF86Dists,	DIST_XF86_BIN,		NULL		},
{ "X32cfg",	"/usr",			&XF86Dists,	DIST_XF86_CFG,		NULL		},
{ "X32doc",	"/usr",			&XF86Dists,	DIST_XF86_DOC,		NULL		},
{ "X32html",	"/usr",			&XF86Dists,	DIST_XF86_HTML,		NULL		},
{ "X32lib",	"/usr",			&XF86Dists,	DIST_XF86_LIB,		NULL		},
{ "X32lk98",	"/usr",			&XF86Dists,	DIST_XF86_LKIT98,	NULL		},
{ "X32lkit",	"/usr",			&XF86Dists,	DIST_XF86_LKIT,		NULL		},
{ "X32man",	"/usr",			&XF86Dists,	DIST_XF86_MAN,		NULL		},
{ "X32prog",	"/usr",			&XF86Dists,	DIST_XF86_PROG,		NULL		},
{ "X32ps",	"/usr",			&XF86Dists,	DIST_XF86_PS,		NULL		},
{ "X32set",	"/usr",			&XF86Dists,	DIST_XF86_SET,		NULL		},
{ NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
{ "X328514",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_8514,	NULL		},
{ "X329480",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9480,	NULL		},
{ "X329EGC",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9EGC,	NULL		},
{ "X329GA9",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9GA9,	NULL		},
{ "X329GAN",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9GAN,	NULL		},
{ "X329LPW",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9LPW,	NULL		},
{ "X329NKV",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9NKV,	NULL		},
{ "X329NS3",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9NS3,	NULL		},
{ "X329SPW",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9SPW,	NULL		},
{ "X329TGU",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9TGU,	NULL		},
{ "X329WEP",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9WEP,	NULL		},
{ "X329WS",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9WS,	NULL		},
{ "X329WSN",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_9WSN,	NULL		},
{ "X32AGX",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_AGX,	NULL		},
{ "X32I128",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_I128,	NULL		},
{ "X32Ma8",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH8,	NULL		},
{ "X32Ma32",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH32,NULL		},
{ "X32Ma64",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH64,NULL		},
{ "X32Mono",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MONO,	NULL		},
{ "X32P9K",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_P9000,	NULL		},
{ "X32S3",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_S3,	NULL		},
{ "X32S3V",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_S3V,	NULL		},
{ "X32SVGA",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_SVGA,	NULL		},
{ "X32VG16",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_VGA16,	NULL		},
{ "X32W32",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_W32,	NULL		},
{ "X32nest",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_NEST,	NULL		},
{ "X32vfb",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_VFB,	NULL		},
{ NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
{ "X32fnts",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_MISC,	NULL		},
{ "X32f100",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_100,	NULL		},
{ "X32fcyr",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_CYR,	NULL		},
{ "X32fscl",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SCALE,	NULL		},
{ "X32fnon",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_NON,	NULL		},
{ "X32fsrv",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SERVER,	NULL		},
{ NULL },
};

static int	distMaybeSetDES(dialogMenuItem *self);

int
distReset(dialogMenuItem *self)
{
    Dists = 0;
    DESDists = 0;
    SrcDists = 0;
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetDeveloper(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_ALL;
    return distMaybeSetDES(self);
}

int
distSetXDeveloper(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_ALL;
    XF86Dists = DIST_XF86_BIN | DIST_XF86_LIB | DIST_XF86_PROG | DIST_XF86_MAN | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA;
    XF86FontDists = DIST_XF86_FONTS_MISC;
    return distSetXF86(NULL) | distMaybeSetDES(self);
}

int
distSetKernDeveloper(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_SYS;
    return distMaybeSetDES(self);
}

int
distSetUser(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_USER;
    return distMaybeSetDES(self);
}

int
distSetXUser(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_USER;
    XF86Dists = DIST_XF86_BIN | DIST_XF86_LIB | DIST_XF86_MAN | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA;
    XF86FontDists = DIST_XF86_FONTS_MISC;
    return distSetXF86(NULL) | distMaybeSetDES(self);
}

int
distSetMinimum(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = DIST_BIN;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetEverything(dialogMenuItem *self)
{
    Dists = DIST_ALL;
    SrcDists = DIST_SRC_ALL;
    XF86Dists = DIST_XF86_ALL;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    return distMaybeSetDES(self);
}

int
distSetDES(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;
 
    if (dmenuOpenSimple(&MenuDESDistributions, FALSE)) {
	if (DESDists) {
	    if (DESDists & DIST_DES_KERBEROS)
		DESDists |= DIST_DES_DES;
	    Dists |= DIST_DES;
	    msgDebug("SetDES Masks: DES: %0x, Dists: %0x\n", DESDists, Dists);
	}
    }
    else
	i = DITEM_FAILURE;
    return i | DITEM_RECREATE | DITEM_RESTORE;
}

static int
distMaybeSetDES(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    dialog_clear_norefresh();
    if (!msgYesNo("Do wish to install DES cryptographic software?\n\n"
		  "If you choose No, FreeBSD will use an MD5 based password scheme which,\n"
		  "while perhaps more secure, is not interoperable with the traditional\n"
		  "UNIX DES passwords on other non-FreeBSD systems.\n\n"
		  "Please do NOT choose Yes at this point if you are outside the\n"
		  "United States and Canada yet are installing from a U.S. FTP server.\n"
		  "This will violate U.S. export restrictions and possibly get the\n"
		  "server site into trouble!  In such cases, install everything but the\n"
		  "DES distribution from the U.S. server then switch your media type to\n"
		  "point to an international FTP server, using the Custom installation\n"
		  "option to select and extract the DES distribution in a second pass.")) {
	if (dmenuOpenSimple(&MenuDESDistributions, FALSE)) {
	    if (DESDists) {
		if (DESDists & DIST_DES_KERBEROS)
		    DESDists |= DIST_DES_DES;
		Dists |= DIST_DES;
		msgDebug("SetDES Masks: DES: %0x, Dists: %0x\n", DESDists, Dists);
	    }
	}
	else
	    i = DITEM_FAILURE;
    }
    return i | DITEM_RECREATE | DITEM_RESTORE;
}

int
distSetSrc(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    if (dmenuOpenSimple(&MenuSrcDistributions, FALSE)) {
	if (SrcDists) {
	    Dists |= DIST_SRC;
	    msgDebug("SetSrc Masks: Srcs: %0x, Dists: %0x\n", SrcDists, Dists);
	}
    }
    else
	i = DITEM_FAILURE;
    return i | DITEM_RECREATE | DITEM_RESTORE;
}

int
distSetXF86(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    if (dmenuOpenSimple(&MenuXF86Select, FALSE)) {
	if (XF86ServerDists)
	    XF86Dists |= DIST_XF86_SERVER;
	if (XF86FontDists)
	    XF86Dists |= DIST_XF86_FONTS;
	if (XF86Dists)
	    Dists |= DIST_XF86;
	msgDebug("SetXF86 Masks: Server: %0x, Fonts: %0x, XDists: %0x, Dists: %0x\n",
		 XF86ServerDists, XF86FontDists, XF86Dists, Dists);
    }
    else
	i = DITEM_FAILURE;
    return i | DITEM_RECREATE | DITEM_RESTORE;
}

static Boolean
distExtract(char *parent, Distribution *me)
{
    int i, status, total;
    int cpid, zpid, fd, fd2, chunk, numchunks;
    char *path, *dist, buf[BUFSIZ];
    const char *tmp;
    Attribs *dist_attr;
    WINDOW *w = savescr();
    struct timeval start, stop;

    status = TRUE;
    dialog_clear_norefresh();
    if (isDebug())
	msgDebug("distExtract: parent: %s, me: %s\n", parent ? parent : "(none)", me->my_name);

    /* Loop through to see if we're in our parent's plans */
    for (i = 0; me[i].my_name; i++) {
	dist = me[i].my_name;
	path = parent ? parent : dist;

	/* If our bit isn't set, go to the next */
	if (!(me[i].my_bit & *(me[i].my_mask)))
	    continue;

	/* This is shorthand for "dist currently disabled" */
	if (!me[i].my_dir) {
	    *(me[i].my_mask) &= ~(me[i].my_bit);
	    continue;
	}

	/* Recurse if we actually have a sub-distribution */
	if (me[i].my_dist) {
	    if ((status = distExtract(dist, me[i].my_dist)) == TRUE)
		*(me[i].my_mask) &= ~(me[i].my_bit);
	    goto done;
	}

	/*
	 * Try to get distribution as multiple pieces, locating and parsing an
	 * info file which tells us how many we need for this distribution.
	 */
	dist_attr = NULL;
	numchunks = 0;

	snprintf(buf, sizeof buf, "%s/%s.inf", path, dist);
	fd = mediaDevice->get(mediaDevice, buf, TRUE);
	if (fd >= 0) {
	    if (isDebug())
		msgDebug("Parsing attributes file for distribution %s\n", dist);
	    dist_attr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	    if (DITEM_STATUS(attr_parse(dist_attr, fd)) == DITEM_FAILURE)
		msgConfirm("Cannot parse information file for the %s distribution!\n"
			   "Please verify that your media is valid and try again.", dist);
	    else {
		if (isDebug())
		    msgDebug("Looking for attribute `pieces'\n");
		tmp = attr_match(dist_attr, "pieces");
		if (tmp)
		    numchunks = strtol(tmp, 0, 0);
	    }
	    safe_free(dist_attr);
	    mediaDevice->close(mediaDevice, fd);
	    if (!numchunks)
		continue;
	}
	else if (fd == IO_ERROR) {	/* Hard error, can't continue */
	    mediaDevice->shutdown(mediaDevice);
	    status = FALSE;
	    goto done;
	}
	else {
	    /* Try to get the distribution as a single file */
	    snprintf(buf, 512, "%s/%s.tgz", path, dist);
	    /*
	     * Passing TRUE as 3rd parm to get routine makes this a "probing" get, for which errors
	     * are not considered too significant.
	     */
	    fd = mediaDevice->get(mediaDevice, buf, TRUE);
	    if (fd >= 0) {
		char *dir = root_bias(me[i].my_dir);

		msgNotify("Extracting %s into %s directory...", dist, dir);
		status = mediaExtractDist(dir, fd);
		mediaDevice->close(mediaDevice, fd);
		goto done;
	    }
	    else if (fd == IO_ERROR) {	/* Hard error, can't continue */
		mediaDevice->shutdown(mediaDevice);
		status = FALSE;
		goto done;
	    }
	    else
		numchunks = 0;
	}

	/* Fall through from "we got the attribute file, now get the pieces" step */
	if (!numchunks)
	    continue;

	if (isDebug())
	    msgDebug("Attempting to extract distribution from %u chunks.\n", numchunks);

	total = 0;
	(void)gettimeofday(&start, (struct timezone *)0);

	/* We have one or more chunks, go pick them up */
	mediaExtractDistBegin(root_bias(me[i].my_dir), &fd2, &zpid, &cpid);
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int n, retval, last_msg;
	    char prompt[80];

	    last_msg = 0;

	    snprintf(buf, 512, "%s/%s.%c%c", path, dist, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if (isDebug())
		msgDebug("trying for piece %d of %d: %s\n", chunk + 1, numchunks, buf);
	    fd = mediaDevice->get(mediaDevice, buf, FALSE);
	    if (fd < 0) {
		msgConfirm("failed to retreive piece file %s!\n"
			   "Aborting the transfer", buf);
		goto punt;
	    }
	    snprintf(prompt, 80, "Extracting %s into %s directory...", dist, root_bias(me[i].my_dir));
	    dialog_gauge("Progress", prompt, 8, 15, 6, 50, (int)((float)(chunk + 1) / numchunks * 100));
	    while (1) {
		int seconds;

		n = read(fd, buf, BUFSIZ);
		if (n <= 0)
		    break;
		total += n;

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
		    msgInfo("%10d bytes read from %s dist, chunk %2d of %2d @ %.1f KB/sec.",
			    total, dist, chunk + 1, numchunks, (total / seconds) / 1024.0);
		}
		retval = write(fd2, buf, n);
		if (retval != n) {
		    mediaDevice->close(mediaDevice, fd);
		    dialog_clear_norefresh();
		    msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", retval, n);
		    goto punt;
		}
	    }
	    mediaDevice->close(mediaDevice, fd);
	}
	close(fd2);
	status = mediaExtractDistEnd(zpid, cpid);
        goto done;

    punt:
	close(fd2);
	mediaExtractDistEnd(zpid, cpid);
	status = FALSE;

    done:
	if (!status) {
	    if (variable_get(VAR_NO_CONFIRM))
		status = TRUE;
	    else {
		if (me[i].my_dist) {
		    msgConfirm("Unable to transfer all components of the %s distribution.\n"
			       "If this is a CDROM install, it may be because export restrictions prohibit\n"
			       "DES code from being shipped from the U.S.  Try to get this code from a\n"
			       "local FTP site instead!", me[i].my_name);
		    status = TRUE;
		}
		else {
		    status = msgYesNo("Unable to transfer the %s distribution from\n%s.\n\n"
				      "Do you want to try to retrieve it again?",
				      me[i].my_name, mediaDevice->name);
		}
	    }
	}
	/* Extract was successful, remove ourselves from further consideration */
	if (status)
	    *(me[i].my_mask) &= ~(me[i].my_bit);
    }
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

	/* This is shorthand for "dist currently disabled" */
	if (!me[i].my_dir)
	    continue;

	*col += strlen(me[i].my_name);
	if (*col > 50) {
	    *col = 0;
	    strcat(buf, "\n");
	}
	sprintf(&buf[strlen(buf)], " %s", me[i].my_name);
	/* Recurse if have a sub-distribution */
	if (me[i].my_dist)
	    printSelected(buf, *(me[i].my_mask), me[i].my_dist, col);
    }
}

int
distExtractAll(dialogMenuItem *self)
{
    int retries = 0;
    char buf[512];

    /* First try to initialize the state of things */
    if (!Dists) {
	msgConfirm("You haven't selected any distributions to extract.");
	return DITEM_FAILURE;
    }
    if (!mediaVerify())
	return DITEM_FAILURE;

    if (!mediaDevice->init(mediaDevice))
	return DITEM_FAILURE;
    dialog_clear_norefresh();
    msgNotify("Attempting to install all selected distributions..");
    /* Try for 3 times around the loop, then give up. */
    while (Dists && ++retries < 3)
	distExtract(NULL, DistTable);

    if (Dists) {
	int col = 0;

	buf[0] = '\0';
	printSelected(buf, Dists, DistTable, &col);
	dialog_clear_norefresh();
	msgConfirm("Couldn't extract the following distributions.  This may\n"
		   "be because they were not available on the installation\n"
		   "media you've chosen:\n\n\t%s", buf);
	return DITEM_FAILURE | DITEM_RESTORE;
    }
    return DITEM_SUCCESS;
}

