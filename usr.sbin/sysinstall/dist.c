/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dist.c,v 1.45 1996/04/28 03:26:51 jkh Exp $
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
{ "X312bin",	"/usr",			&XF86Dists,	DIST_XF86_BIN,		NULL		},
{ "X312lib",	"/usr",			&XF86Dists,	DIST_XF86_LIB,		NULL		},
{ "X312doc",	"/usr",			&XF86Dists,	DIST_XF86_DOC,		NULL		},
{ "X312etc",	"/usr",			&XF86Dists,	DIST_XF86_ETC,		NULL		},
{ "XF86312",	"/usr",			&XF86Dists,	DIST_XF86_FONTS,	XF86FontDistTable },
{ "X312man",	"/usr",			&XF86Dists,	DIST_XF86_MAN,		NULL		},
{ "X312ctrb",	"/usr",			&XF86Dists,	DIST_XF86_CTRB,		NULL		},
{ "X312prog",	"/usr",			&XF86Dists,	DIST_XF86_PROG,		NULL		},
{ "X312link",	"/usr",			&XF86Dists,	DIST_XF86_LINK,		NULL		},
{ "X312pex",	"/usr",			&XF86Dists,	DIST_XF86_PEX,		NULL		},
{ "X312lbx",	"/usr",			&XF86Dists,	DIST_XF86_LBX,		NULL		},
{ "X312ubin",	"/usr",			&XF86Dists,	DIST_XF86_UBIN,		NULL		},
{ "X312xicf",	"/usr",			&XF86Dists,	DIST_XF86_XINIT,	NULL		},
{ "X312xdcf",	"/usr",			&XF86Dists,	DIST_XF86_XDMCF,	NULL		},
{ "XF86312",	"/usr",			&XF86Dists,	DIST_XF86_SERVER,	XF86ServerDistTable },
{ "XF86-xc",	"/usr/X11R6/src",	&XF86Dists,	DIST_XF86_SRC,		NULL		},
{ "XF86-co",	"/usr/X11R6/src",	&XF86Dists,	DIST_XF86_SRC,		NULL		},
{ NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
{ "X3128514",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_8514,	NULL		},
{ "X312AGX",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_AGX,	NULL		},
{ "X312Ma8",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH8,	NULL		},
{ "X312Ma32",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH32,NULL		},
{ "X312Ma64",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH64,NULL		},
{ "X312Mono",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MONO,	NULL		},
{ "X312P9K",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_P9000,	NULL		},
{ "X312S3",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_S3,	NULL		},
{ "X312SVGA",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_SVGA,	NULL		},
{ "X312VG16",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_VGA16,	NULL		},
{ "X312W32",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_W32,	NULL		},
{ "X312nest",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_NEST,	NULL		},
{ NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
{ "X312fnts",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_MISC,	NULL		},
{ "X312f100",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_100,	NULL		},
{ "X312fcyr",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_CYR,	NULL		},
{ "X312fscl",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SCALE,	NULL		},
{ "X312fnon",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_NON,	NULL		},
{ "X312fsrv",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SERVER,	NULL		},
{ NULL },
};

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
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetXDeveloper(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_DEVELOPER | DIST_XF86;
    SrcDists = DIST_SRC_ALL;
    XF86Dists = DIST_XF86_BIN | DIST_XF86_LIB | DIST_XF86_PROG | DIST_XF86_MAN | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA;
    XF86FontDists = DIST_XF86_FONTS_MISC;
    return distSetXF86(NULL);
}

int
distSetKernDeveloper(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_SYS;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetUser(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_USER;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetXUser(dialogMenuItem *self)
{
    distReset(NULL);
    Dists = _DIST_USER;
    XF86Dists = DIST_XF86_BIN | DIST_XF86_LIB | DIST_XF86_MAN | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA;
    XF86FontDists = DIST_XF86_FONTS_MISC;
    return distSetXF86(NULL);
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
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetDES(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;
 
    if (dmenuOpenSimple(&MenuDESDistributions)) {
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

int
distSetSrc(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    if (dmenuOpenSimple(&MenuSrcDistributions)) {
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

    if (dmenuOpenSimple(&MenuXF86Select)) {
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
    int i, status;
    int cpid, zpid, fd, fd2, chunk, numchunks;
    char *path, *dist, buf[10240];
    const char *tmp;
    Attribs *dist_attr;
    WINDOW *w = savescr();

    status = TRUE;
    dialog_clear();
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

	/* Recurse if actually have a sub-distribution */
	if (me[i].my_dist) {
	    status = distExtract(dist, me[i].my_dist);
	    goto done;
	}

	/* First try to get the distribution as a single file */
        snprintf(buf, 512, "%s/%s.tgz", path, dist);
	if (isDebug())
	    msgDebug("Trying to get large piece: %s\n", buf);
	fd = mediaDevice->get(mediaDevice, buf, TRUE);
	if (fd >= 0) {
	    msgNotify("Extracting %s into %s directory...", me[i].my_name, me[i].my_dir);
	    status = mediaExtractDist(me[i].my_dir, fd);
	    mediaDevice->close(mediaDevice, fd);
	    goto done;
	}
	else if (fd == -2) {	/* Hard error, can't continue */
	    mediaDevice->shutdown(mediaDevice);
	    status = FALSE;
	    goto done;
	}

	/*
	 * If we couldn't get it as one file then we need to get multiple pieces; locate and parse an
	 * info file telling us how many we need for this distribution.
	 */
	dist_attr = NULL;
	numchunks = 0;

	snprintf(buf, sizeof buf, "/stand/info/%s/%s.inf", path, dist);
	if (file_readable(buf)) {
	    if (isDebug())
		msgDebug("Parsing attributes file for distribution %s\n", dist);
	    dist_attr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	    if (DITEM_STATUS(attr_parse_file(dist_attr, buf)) == DITEM_FAILURE)
		msgConfirm("Cannot load information file for %s distribution!\n"
			   "Please verify that your media is valid and try again.", dist);
	    else {
		if (isDebug())
		    msgDebug("Looking for attribute `pieces'\n");
		tmp = attr_match(dist_attr, "pieces");
		if (tmp)
		    numchunks = strtol(tmp, 0, 0);
	    }
	    safe_free(dist_attr);
	}

	if (!numchunks)
	    continue;

	if (isDebug())
	    msgDebug("Attempting to extract distribution from %u chunks.\n", numchunks);

	/* We have one or more chunks, go pick them up */
	mediaExtractDistBegin(me[i].my_dir, &fd2, &zpid, &cpid);
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int n, retval;
	    char prompt[80];

	    snprintf(buf, 512, "%s/%s.%c%c", path, dist, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if (isDebug())
		msgDebug("trying for piece %d of %d: %s\n", chunk + 1, numchunks, buf);
	    fd = mediaDevice->get(mediaDevice, buf, FALSE);
	    if (fd < 0) {
		msgConfirm("failed to retreive piece file %s!\nAborting the transfer", buf);
		goto punt;
	    }
	    snprintf(prompt, 80, "Extracting %s into %s directory...", me[i].my_name, me[i].my_dir);
	    dialog_clear();
	    dialog_gauge("Progress", prompt, 8, 15, 6, 50, (int)((float)(chunk + 1) / numchunks * 100));
	    move(0, 0);	/* Get cursor out of the way - it makes gauges look strange */
	    while ((n = read(fd, buf, sizeof buf)) > 0) {
		retval = write(fd2, buf, n);
		if (retval != n) {
		    mediaDevice->close(mediaDevice, fd);
		    dialog_clear();
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
		    status = msgYesNo("Unable to transfer the %s distribution from %s.\n"
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
printSelected(char *buf, int selected, Distribution *me)
{
    int i;
    static int col = 0;

    /* Loop through to see if we're in our parent's plans */
    for (i = 0; me[i].my_name; i++) {

	/* If our bit isn't set, go to the next */
	if (!(me[i].my_bit & selected))
	    continue;

	/* This is shorthand for "dist currently disabled" */
	if (!me[i].my_dir)
	    continue;

	col += strlen(me[i].my_name);
	if (col > 50) {
	    col = 0;
	    strcat(buf, "\n");
	}
	sprintf(&buf[strlen(buf)], " %s", me[i].my_name);
	/* Recurse if have a sub-distribution */
	if (me[i].my_dist)
	    printSelected(buf, *(me[i].my_mask), me[i].my_dist);
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
    dialog_clear();
    msgNotify("Attempting to install all selected distributions..");
    /* Try for 3 times around the loop, then give up. */
    while (Dists && ++retries < 3)
	distExtract(NULL, DistTable);

    if (Dists) {
	printSelected(buf, Dists, DistTable);
	msgConfirm("Couldn't extract all of the distributions.  This may\n"
		   "be because the following distributions are not available on the\n"
		   "installation media you've chosen:\n\n\t%s", buf);
    }
    return DITEM_SUCCESS;
}
