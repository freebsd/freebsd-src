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
#include <signal.h>
#include <libutil.h>

unsigned int Dists;
unsigned int CRYPTODists;
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
extern Distribution CRYPTODistTable[];
extern Distribution SrcDistTable[];
extern Distribution XF86DistTable[];
extern Distribution XF86FontDistTable[];
extern Distribution XF86ServerDistTable[];

/* The top-level distribution categories */
static Distribution DistTable[] = {
{ "base",	"/",			&Dists,		DIST_BASE,		NULL		},
{ "doc",	"/",			&Dists,		DIST_DOC,		NULL		},
{ "games",	"/",			&Dists,		DIST_GAMES,		NULL		},
{ "manpages",	"/",			&Dists,		DIST_MANPAGES,		NULL		},
{ "catpages",	"/",			&Dists,		DIST_CATPAGES,		NULL		},
{ "proflibs",	"/",			&Dists,		DIST_PROFLIBS,		NULL		},
{ "dict",	"/",			&Dists,		DIST_DICT,		NULL		},
{ "info",	"/",			&Dists,		DIST_INFO,		NULL		},
{ "src",	"/",			&Dists,		DIST_SRC,		SrcDistTable	},
{ "crypto",	"/",			&Dists,		DIST_CRYPTO,		CRYPTODistTable	},
#ifdef __i386__
{ "compat1x",	"/",			&Dists,		DIST_COMPAT1X,		NULL		},
{ "compat20",	"/",			&Dists,		DIST_COMPAT20,		NULL		},
{ "compat21",	"/",			&Dists,		DIST_COMPAT21,		NULL		},
{ "compat22",	"/",			&Dists,		DIST_COMPAT22,		NULL		},
{ "compat3x",	"/",			&Dists,		DIST_COMPAT3X,		NULL		},
{ "compat4x",	"/",			&Dists,		DIST_COMPAT4X,		NULL		},
#endif
{ "ports",	"/usr",			&Dists,		DIST_PORTS,		NULL		},
{ "local",	"/",			&Dists,		DIST_LOCAL,		NULL		},
{ "XF86336",	"/usr",			&Dists,		DIST_XF86,		XF86DistTable	},
{ NULL },
};

/* The CRYPTO distribution */
static Distribution CRYPTODistTable[] = {
{ "crypto",     "/",                    &CRYPTODists,	DIST_CRYPTO_CRYPTO,		NULL		},
{ "krb4",	"/",			&CRYPTODists,	DIST_CRYPTO_KERBEROS4,	NULL		},
{ "krb5",	"/",			&CRYPTODists,	DIST_CRYPTO_KERBEROS5,	NULL		},
{ "ssecure",	"/usr/src",		&CRYPTODists,	DIST_CRYPTO_SSECURE,	NULL		},
{ "scrypto",	"/usr/src",		&CRYPTODists,	DIST_CRYPTO_SCRYPTO,	NULL		},
{ "skrb4",	"/usr/src",		&CRYPTODists,	DIST_CRYPTO_SKERBEROS4,	NULL		},
{ "skrb5",	"/usr/src",		&CRYPTODists,	DIST_CRYPTO_SKERBEROS5,	NULL		},
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
{ "srelease",	"/usr/src",		&SrcDists,	DIST_SRC_RELEASE,	NULL		},
{ "sbin",	"/usr/src",		&SrcDists,	DIST_SRC_BIN,		NULL		},
{ "ssbin",	"/usr/src",		&SrcDists,	DIST_SRC_SBIN,		NULL		},
{ "sshare",	"/usr/src",		&SrcDists,	DIST_SRC_SHARE,		NULL		},
{ "ssys",	"/usr/src",		&SrcDists,	DIST_SRC_SYS,		NULL		},
{ "subin",	"/usr/src",		&SrcDists,	DIST_SRC_UBIN,		NULL		},
{ "susbin",	"/usr/src",		&SrcDists,	DIST_SRC_USBIN,		NULL		},
{ "stools",	"/usr/src",		&SrcDists,	DIST_SRC_TOOLS,         NULL            },
{ NULL },
};

/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
{ "XF86336",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_FONTS,	XF86FontDistTable },
{ "XF86336",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_SERVER,	XF86ServerDistTable },
{ "Xbin",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_BIN,		NULL		},
{ "Xcfg",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_CFG,		NULL		},
{ "Xdoc",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_DOC,		NULL		},
{ "Xhtml",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_HTML,		NULL		},
{ "Xlib",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_LIB,		NULL		},
#if defined(__i386__) && defined(PC98)
{ "Xlk98",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_LKIT98,	NULL		},
#endif
{ "Xlkit",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_LKIT,		NULL		},
{ "Xman",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_MAN,		NULL		},
{ "Xprog",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_PROG,		NULL		},
{ "Xps",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_PS,		NULL		},
{ "Xset",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_SET,		NULL		},
#if defined(__i386__) && defined(PC98)
{ "X9set",	"/usr/X11R6",		&XF86Dists,	DIST_XF86_9SET,		NULL		},
#endif
{ NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
#if defined(__i386__) && defined(PC98)
{ "PC98-Servers/X9480",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9480,	NULL		},
{ "PC98-Servers/X9EGC",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9EGC,	NULL		},
{ "PC98-Servers/X9GA9",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9GA9,	NULL		},
{ "PC98-Servers/X9GAN",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9GAN,	NULL		},
{ "PC98-Servers/X9LPW",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9LPW,	NULL		},
{ "PC98-Servers/X9MGA",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9MGA,	NULL		},
{ "PC98-Servers/X9NKV",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9NKV,	NULL		},
{ "PC98-Servers/X9NS3",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9NS3,	NULL		},
{ "PC98-Servers/X9SPW",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9SPW,	NULL		},
{ "PC98-Servers/X9SVG",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9SVG,	NULL		},
{ "PC98-Servers/X9TGU",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9TGU,	NULL		},
{ "PC98-Servers/X9WEP",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9WEP,	NULL		},
{ "PC98-Servers/X9WS",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9WS,	NULL		},
{ "PC98-Servers/X9WSN",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_9WSN,	NULL		},
#endif
{ "Servers/X3DL",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_3DL,	NULL		},
#ifdef __i386__
{ "Servers/X8514",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_8514,	NULL		},
{ "Servers/XAGX",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_AGX,	NULL		},
#endif
{ "Servers/XI128",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_I128,	NULL		},
#ifdef __i386__
{ "Servers/XMa8",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_MACH8,	NULL		},
{ "Servers/XMa32",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_MACH32,NULL		},
#endif
{ "Servers/XMa64",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_MACH64,NULL		},
{ "Servers/XMono",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_MONO,	NULL		},
{ "Servers/XP9K",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_P9000,	NULL		},
{ "Servers/XS3",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_S3,	NULL		},
{ "Servers/XS3V",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_S3V,	NULL		},
{ "Servers/XSVGA",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_SVGA,	NULL		},
#ifdef __i386__
{ "Servers/XVG16",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_VGA16,	NULL		},
{ "Servers/XW32",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_W32,	NULL		},
#endif
#ifdef __alpha__
{ "Servers/XTGA",	"/usr/X11R6",		&XF86ServerDists,	DIST_XF86_SERVER_TGA,	NULL		},
#endif
{ NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
{ "Xfnts",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_MISC,	NULL		},
{ "Xf100",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_100,	NULL		},
{ "Xfcyr",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_CYR,	NULL		},
{ "Xfscl",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_SCALE,	NULL		},
{ "Xfnon",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_NON,	NULL		},
{ "Xfsrv",	"/usr/X11R6",		&XF86FontDists,		DIST_XF86_FONTS_SERVER,	NULL		},
{ NULL },
};

static int	distMaybeSetPorts(dialogMenuItem *self);

static void
distVerifyFlags(void)
{
    if (SrcDists)
	Dists |= DIST_SRC;
    if (CRYPTODists) {
	if (CRYPTODists & (DIST_CRYPTO_KERBEROS4 | DIST_CRYPTO_KERBEROS5))
	    CRYPTODists |= DIST_CRYPTO_CRYPTO;
	Dists |= DIST_CRYPTO;
    }
    else if ((Dists & DIST_CRYPTO) && !CRYPTODists)
	CRYPTODists |= DIST_CRYPTO_ALL;
    if (XF86Dists & DIST_XF86_SET)
	XF86ServerDists |= DIST_XF86_SERVER_VGA16;
    if (XF86ServerDists)
	XF86Dists |= DIST_XF86_SERVER;
    if (XF86FontDists)
	XF86Dists |= DIST_XF86_FONTS;
    if (XF86Dists || XF86ServerDists || XF86FontDists) {
	Dists |= DIST_XF86;
    }
    if (isDebug())
	msgDebug("Dist Masks: Dists: %0x, CRYPTO: %0x, Srcs: %0x\nXServer: %0x, XFonts: %0x, XDists: %0x\n",
		 Dists, CRYPTODists, SrcDists, XF86ServerDists, XF86FontDists, XF86Dists);
}

int
distReset(dialogMenuItem *self)
{
    Dists = 0;
    CRYPTODists = 0;
    SrcDists = 0;
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distConfig(dialogMenuItem *self)
{
    char *cp;

    distReset(NULL);

    if ((cp = variable_get(VAR_DIST_MAIN)) != NULL)
	Dists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_CRYPTO)) != NULL)
	CRYPTODists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_SRC)) != NULL)
	SrcDists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_X11)) != NULL)
	XF86Dists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_XSERVER)) != NULL)
	XF86ServerDists = atoi(cp);

    if ((cp = variable_get(VAR_DIST_XFONTS)) != NULL)
	XF86FontDists = atoi(cp);
    distVerifyFlags();
    return DITEM_SUCCESS | DITEM_REDRAW;
}

static int
distSetX(void)
{
    Dists |= DIST_XF86;
    XF86Dists = DIST_XF86_BIN | DIST_XF86_SET | DIST_XF86_CFG | DIST_XF86_LIB | DIST_XF86_PROG | DIST_XF86_MAN | DIST_XF86_DOC | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA | DIST_XF86_SERVER_VGA16;
    XF86FontDists = DIST_XF86_FONTS_MISC;
#ifndef X_AS_PKG
    return distSetXF86(NULL);
#endif
    return DITEM_SUCCESS;
}

int
distSetDeveloper(dialogMenuItem *self)
{
    int i;

    distReset(NULL);
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_ALL;
    CRYPTODists = DIST_CRYPTO_ALL;
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
    SrcDists = DIST_SRC_SYS;
    CRYPTODists |= DIST_CRYPTO_BIN;
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
    CRYPTODists |= DIST_CRYPTO_CRYPTO;
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
    Dists = DIST_BASE | DIST_CRYPTO;
    CRYPTODists |= DIST_CRYPTO_CRYPTO;
    distVerifyFlags();
    return DITEM_SUCCESS | DITEM_REDRAW;
}

int
distSetEverything(dialogMenuItem *self)
{
    int i;

    Dists = DIST_ALL | DIST_XF86;
    SrcDists = DIST_SRC_ALL;
    CRYPTODists = DIST_CRYPTO_ALL;
    XF86Dists = DIST_XF86_ALL;
    XF86ServerDists = DIST_XF86_SERVER_ALL;
    XF86FontDists = DIST_XF86_FONTS_ALL;
    i = distMaybeSetPorts(self);
    distVerifyFlags();
    return i;
}

static int
distMaybeSetPorts(dialogMenuItem *self)
{
    dialog_clear_norefresh();
    if (!msgYesNo("Would you like to install the FreeBSD ports collection?\n\n"
		  "This will give you ready access to over 7600 ported software packages,\n"
		  "at a cost of around 180MB of disk space when \"clean\" and possibly\n"
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
	/* This is shorthand for "dist currently disabled" */
	if (!dist[i].my_dir)
	    continue;
	if (!strcmp(dist[i].my_name, name)) {
	    *(dist[i].my_mask) |= dist[i].my_bit;
	    status = TRUE;
	}
	if (dist[i].my_dist) {
	    if (distSetByName(dist[i].my_dist, name)) {
		status = TRUE;
	    }
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
	/* This is shorthand for "dist currently disabled" */
	if (!dist[i].my_dir)
	    continue;
	if (!strcmp(dist[i].my_name, name)) {
	    *(dist[i].my_mask) &= ~(dist[i].my_bit);
	    status = TRUE;
	}
	if (dist[i].my_dist) {
	    if (distUnsetByName(dist[i].my_dist, name)) {
		status = TRUE;
	    }
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
distSetXF86(dialogMenuItem *self)
{
    int i = DITEM_SUCCESS;

    dialog_clear_norefresh();
    if (!dmenuOpenSimple(&MenuXF86Select, FALSE))
	i = DITEM_FAILURE;
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

static Boolean
distExtract(char *parent, Distribution *me)
{
    int i,j, status, total, intr, unmounted_dev;
    int cpid, zpid, fd2, chunk, numchunks;
    char *path, *dist, buf[300000];
    const char *tmp;
    FILE *fp;
    WINDOW *w = savescr();
    struct timeval start, stop;
    struct sigaction old, new;
    properties dist_attr = NULL;

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
	numchunks = 0;
	snprintf(buf, sizeof buf, "%s/%s.inf", path, dist);

    getinfo:
	fp = DEVICE_GET(mediaDevice, buf, TRUE);
	intr = check_for_interrupt();
	if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
	    /* Hard error, can't continue */
	    if (!msgYesNo("Unable to open %s: %s.\nReinitialize media?",
			  buf, !intr ? "I/O error." : "User interrupt.")) {
		DEVICE_SHUTDOWN(mediaDevice);
		if (!DEVICE_INIT(mediaDevice)) {
		    status = FALSE;
		    goto done;
		}
		else
		    goto getinfo;
	    }
	    else {
		status = FALSE;
		goto done;
	    }
	}
	else if (fp > 0) {
	    if (isDebug())
		msgDebug("Parsing attributes file for distribution %s\n", dist);

	    dist_attr = properties_read(fileno(fp));
	    intr = check_for_interrupt();
	    if (intr || !dist_attr) {
		msgConfirm("Cannot parse information file for the %s distribution: %s\n"
			   "Please verify that your media is valid and try again.",
			   dist, !intr ? "I/O error" : "User interrupt");
	    }
	    else {
		tmp = property_find(dist_attr, "Pieces");
		if (tmp)
		    numchunks = strtol(tmp, 0, 0);
	    }
	    fclose(fp);
	    if (!numchunks)
		continue;
	}
	else {
	    /* Try to get the distribution as a single file */
	    snprintf(buf, sizeof buf, "%s/%s.%s", path, dist,
		USE_GZIP ? "tgz" : "tbz");
	    /*
	     * Passing TRUE as 3rd parm to get routine makes this a "probing"
	     * get, for which errors are not considered too significant.
	     */
	getsingle:
	    fp = DEVICE_GET(mediaDevice, buf, TRUE);
	    intr = check_for_interrupt();
	    if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
		/* Hard error, can't continue */
		if (intr)	/* result of an interrupt */
		    msgConfirm("Unable to open %s: User interrupt", buf);
		else
		    msgConfirm("Unable to open %s: I/O error", buf);
		DEVICE_SHUTDOWN(mediaDevice);
		if (!DEVICE_INIT(mediaDevice)) {
		    status = FALSE;
		    goto done;
		}
		else
		    goto getsingle;
	    }
	    else if (fp > 0) {
		char *dir = root_bias(me[i].my_dir);

		dialog_clear_norefresh();
		msgNotify("Extracting %s into %s directory...", dist, dir);
		status = mediaExtractDist(dir, dist, fp);
		fclose(fp);
		goto done;
	    }
	    else {
		status = FALSE;
		goto done;
	    }
	}

	/* Fall through from "we got the attribute file, now get the pieces" step */
	if (!numchunks)
	    continue;

	if (isDebug())
	    msgDebug("Attempting to extract distribution from %u chunks.\n", numchunks);

	total = 0;
	(void)gettimeofday(&start, (struct timezone *)0);

	if (me[i].my_bit == DIST_BASE && RunningAsInit && !Fake) {
		unmounted_dev = 1;
		unmount("/dev", MNT_FORCE);
	} else
		unmounted_dev = 0;
 
	/* We have one or more chunks, initialize unpackers... */
	mediaExtractDistBegin(root_bias(me[i].my_dir), &fd2, &zpid, &cpid);

	/* And go for all the chunks */
	dialog_clear_norefresh();
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int n, retval, last_msg, chunksize, realsize;
	    char prompt[80];

	    last_msg = 0;

	getchunk:
	    snprintf(buf, sizeof buf, "cksum.%c%c",  (chunk / 26) + 'a', (chunk % 26) + 'a');
	    tmp = property_find(dist_attr, buf);
	    chunksize = 0;
	    if (tmp) {
		tmp=index(tmp, ' ');
		chunksize = strtol(tmp, 0, 0);
	    }
	    snprintf(buf, sizeof buf, "%s/%s.%c%c", path, dist, (chunk / 26) + 'a', (chunk % 26) + 'a');
	    if (isDebug())
		msgDebug("trying for piece %d of %d: %s\n", chunk + 1, numchunks, buf);
	    fp = DEVICE_GET(mediaDevice, buf, FALSE);
	    intr = check_for_interrupt();
	    if (fp <= (FILE *)0 || intr) {
		if (fp == (FILE *)0)
		    msgConfirm("Failed to find %s on this media.  Reinitializing media.", buf);
		else
		    msgConfirm("failed to retreive piece file %s.\n"
			       "%s: Reinitializing media.", buf, !intr ? "I/O error" : "User interrupt");
		DEVICE_SHUTDOWN(mediaDevice);
		if (!DEVICE_INIT(mediaDevice))
		    goto punt;
		else
		    goto getchunk;
	    }

	    snprintf(prompt, sizeof prompt, "Extracting %s into %s directory...", dist, root_bias(me[i].my_dir));
	    dialog_gauge("Progress", prompt, 8, 15, 6, 50, (int)((float)(chunk + 1) / numchunks * 100));

	    realsize = 0;
	    while (1) {
		int seconds;

		n = fread(buf + realsize, 1, BUFSIZ, fp);
		if (check_for_interrupt()) {
		    msgConfirm("Media read error:  User interrupt.");
		    fclose(fp);
		    goto punt;
		}
		else if (n <= 0)
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
			    total, dist, chunk + 1, numchunks, (total / seconds) / 1000.0);
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
	close(fd2);
	status = mediaExtractDistEnd(zpid, cpid);
	goto done;

    punt:
	close(fd2);
	mediaExtractDistEnd(zpid, cpid);
	status = FALSE;

    done:
	if (!status) {
	    dialog_clear_norefresh();
	    if (me[i].my_dist) {
		msgConfirm("Unable to transfer all components of the %s distribution.\n"
		           "You may wish to switch media types and try again.\n", me[i].my_name);
	    }
	    else if (me[i].my_bit != DIST_LOCAL) {
		status = msgYesNo("Unable to transfer the %s distribution from\n%s.\n\n"
				  "Do you want to try to retrieve it again?",
				  me[i].my_name, mediaDevice->name);
		if (!status)
		    --i;
	    }
	}
	/* If extract was successful, remove ourselves from further consideration */
	if (status)
	    *(me[i].my_mask) &= ~(me[i].my_bit);
	else
	    continue;
	if (unmounted_dev) {
	    (void)mount("devfs", "/dev", 0, NULL);
	    unmounted_dev = 0;
	}
    }
    properties_free(dist_attr);
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
    int old_dists, retries = 0, status = DITEM_SUCCESS;
    char buf[512];
    WINDOW *w;
#ifdef X_AS_PKG
    int want_x_package = 0;
#endif

    /* paranoia */
    if (!Dists) {
	if (!dmenuOpenSimple(&MenuSubDistributions, FALSE) || !Dists)
	    return DITEM_FAILURE;
    }

    if (!mediaVerify() || !DEVICE_INIT(mediaDevice))
	return DITEM_FAILURE;

    old_dists = Dists;
    distVerifyFlags();

    dialog_clear_norefresh();
    w = savescr();
    msgNotify("Attempting to install all selected distributions..");

#ifdef X_AS_PKG
    /* Clear any XFree86 dist flags, but remember they were present. */
    if(Dists & DIST_XF86)
    	want_x_package = 1;
    Dists &= ~DIST_XF86;
    /*Dists &= ~(DIST_XF86 | XF86Dists | XF86ServerDists | XF86FontDists);*/
#endif
    
    /* Try for 3 times around the loop, then give up. */
    while (Dists && ++retries < 3)
	distExtract(NULL, DistTable);

#ifdef X_AS_PKG
    if (want_x_package)
	status |= installX11package(NULL);
#endif

    dialog_clear_norefresh();
    /* Only do bin fixup if bin dist was successfully extracted */
    if ((old_dists & DIST_BASE) && !(Dists & DIST_BASE))
	status |= installFixupBin(self);
#ifndef X_AS_PKG
    if (old_dists & DIST_XF86)
	status |= installFixupXFree(self);
#endif

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
