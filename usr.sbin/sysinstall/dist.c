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
#include <signal.h>
#include <libutil.h>

unsigned int Dists;
unsigned int CRYPTODists;
unsigned int SrcDists;
unsigned int XF86Dists;
unsigned int XF86ServerDists;
unsigned int XF86FontDists;

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

extern Distribution DistTable[];
extern Distribution CRYPTODistTable[];
extern Distribution SrcDistTable[];
extern Distribution XF86DistTable[];
extern Distribution XF86FontDistTable[];
extern Distribution XF86ServerDistTable[];

#define	DTE_TARBALL(name, mask, flag, directory)			\
	{ name, mask, DIST_ ## flag, DT_TARBALL, { directory } }
#define	DTE_PACKAGE(name, mask, flag, package)				\
	{ name, mask, DIST_ ## flag, DT_PACKAGE, { package } }
#define	DTE_SUBDIST(name, mask, flag, subdist)				\
	{ name, mask, DIST_ ## flag, DT_SUBDIST, { my_dist: subdist } }

#define	BASE_DIST	(&DistTable[0])

/* The top-level distribution categories */
static Distribution DistTable[] = {
    DTE_TARBALL("base",	    &Dists, BASE,     "/"),
    DTE_TARBALL("doc",	    &Dists, DOC,      "/"),
    DTE_TARBALL("games",    &Dists, GAMES,    "/"),
    DTE_TARBALL("manpages", &Dists, MANPAGES, "/"),
    DTE_TARBALL("catpages", &Dists, CATPAGES, "/"),
    DTE_TARBALL("proflibs", &Dists, PROFLIBS, "/"),
    DTE_TARBALL("dict",	    &Dists, DICT,     "/"),
    DTE_TARBALL("info",	    &Dists, INFO,     "/"),
    DTE_SUBDIST("src",	    &Dists, SRC,      SrcDistTable),
    DTE_SUBDIST("crypto",   &Dists, CRYPTO,   CRYPTODistTable),
#ifdef __i386__
    DTE_TARBALL("compat1x", &Dists, COMPAT1X, "/"),
    DTE_TARBALL("compat20", &Dists, COMPAT20, "/"),
    DTE_TARBALL("compat21", &Dists, COMPAT21, "/"),
    DTE_TARBALL("compat22", &Dists, COMPAT22, "/"),
    DTE_TARBALL("compat3x", &Dists, COMPAT3X, "/"),
#endif
#if defined(__i386__) || defined(__alpha__)
    DTE_TARBALL("compat4x", &Dists, COMPAT4X, "/"),
#endif
    DTE_TARBALL("ports",    &Dists, PORTS,    "/usr"),
    DTE_TARBALL("local",    &Dists, LOCAL,    "/"),
    DTE_PACKAGE("perl",	    &Dists, PERL,     "perl"),
    DTE_SUBDIST("XFree86",  &Dists, XF86,     XF86DistTable),
    { NULL },
};

/* The CRYPTO distribution */
static Distribution CRYPTODistTable[] = {
    DTE_TARBALL("crypto",  &CRYPTODists, CRYPTO_CRYPTO,	    "/"),
    DTE_TARBALL("ssecure", &CRYPTODists, CRYPTO_SSECURE,    "/usr/src"),
    DTE_TARBALL("scrypto", &CRYPTODists, CRYPTO_SCRYPTO,    "/usr/src"),
    DTE_TARBALL("skrb5",   &CRYPTODists, CRYPTO_SKERBEROS5, "/usr/src"),
    { NULL },
};

/* The /usr/src distribution */
static Distribution SrcDistTable[] = {
    DTE_TARBALL("sbase",    &SrcDists, SRC_BASE,    "/usr/src"),
    DTE_TARBALL("scontrib", &SrcDists, SRC_CONTRIB, "/usr/src"),
    DTE_TARBALL("sgnu",	    &SrcDists, SRC_GNU,	    "/usr/src"),
    DTE_TARBALL("setc",	    &SrcDists, SRC_ETC,	    "/usr/src"),
    DTE_TARBALL("sgames",   &SrcDists, SRC_GAMES,   "/usr/src"),
    DTE_TARBALL("sinclude", &SrcDists, SRC_INCLUDE, "/usr/src"),
    DTE_TARBALL("slib",	    &SrcDists, SRC_LIB,	    "/usr/src"),
    DTE_TARBALL("slibexec", &SrcDists, SRC_LIBEXEC, "/usr/src"),
    DTE_TARBALL("srelease", &SrcDists, SRC_RELEASE, "/usr/src"),
    DTE_TARBALL("sbin",	    &SrcDists, SRC_BIN,	    "/usr/src"),
    DTE_TARBALL("ssbin",    &SrcDists, SRC_SBIN,    "/usr/src"),
    DTE_TARBALL("sshare",   &SrcDists, SRC_SHARE,   "/usr/src"),
    DTE_TARBALL("ssys",	    &SrcDists, SRC_SYS,	    "/usr/src"),
    DTE_TARBALL("subin",    &SrcDists, SRC_UBIN,    "/usr/src"),
    DTE_TARBALL("susbin",   &SrcDists, SRC_USBIN,   "/usr/src"),
    DTE_TARBALL("stools",   &SrcDists, SRC_TOOLS,   "/usr/src"),
    { NULL },
};

#ifdef X_AS_PKG
/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
    DTE_SUBDIST("XFree86", &XF86Dists, XF86_FONTS,	XF86FontDistTable),
    DTE_SUBDIST("XFree86", &XF86Dists, XF86_SERVER,	XF86ServerDistTable),
    DTE_PACKAGE("Xbin",	   &XF86Dists, XF86_CLIENTS,	"XFree86-clients"),
    DTE_PACKAGE("Xdoc",	   &XF86Dists, XF86_DOC,	"XFree86-documents"),
    DTE_PACKAGE("Xlib",	   &XF86Dists, XF86_LIB,	"XFree86-libraries"),
    DTE_PACKAGE("Xman",	   &XF86Dists, XF86_MAN,	"XFree86-manuals"),
    DTE_PACKAGE("Xprog",   &XF86Dists, XF86_PROG,	"imake"),
    { NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
    DTE_PACKAGE("Xsrv",  &XF86ServerDists, XF86_SERVER_FB,    "wrapper"),
    DTE_PACKAGE("Xnest", &XF86ServerDists, XF86_SERVER_NEST,  "XFree86-NestServer"),
    DTE_PACKAGE("Xprt",  &XF86ServerDists, XF86_SERVER_PRINT, "XFree86-PrintServer"),
    DTE_PACKAGE("Xvfb",  &XF86ServerDists, XF86_SERVER_VFB,   "XFree86-VirtualFramebufferServer"),
    { NULL }
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
    DTE_PACKAGE("Xf75",  &XF86FontDists, XF86_FONTS_75,	     "XFree86-font75dpi"),
    DTE_PACKAGE("Xf100", &XF86FontDists, XF86_FONTS_100,     "XFree86-font100dpi"),
    DTE_PACKAGE("Xfcyr", &XF86FontDists, XF86_FONTS_CYR,     "XFree86-fontCyrillic"),
    DTE_PACKAGE("Xfscl", &XF86FontDists, XF86_FONTS_SCALE,   "XFree86-fontScalable"),
    DTE_PACKAGE("Xfnts", &XF86FontDists, XF86_FONTS_BITMAPS, "XFree86-fontDefaultBitmaps"),
    DTE_PACKAGE("Xfsrv", &XF86FontDists, XF86_FONTS_SERVER,  "XFree86-FontServer"),
    { NULL },
};

#else /* !X_AS_PKG */

/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
    DTE_SUBDIST("XF86336", &XF86Dists, XF86_FONTS,	 XF86FontDistTable),
#if defined(__i386__) && defined(PC98)
    DTE_SUBDIST("XF86336/PC98-Servers", &XF86Dists, XF86_SERVER, XF86ServerDistTable),
#else
    DTE_SUBDIST("XF86336/Servers", &XF86Dists, XF86_SERVER, XF86ServerDistTable),
#endif
    DTE_TARBALL("Xbin",	   &XF86Dists, XF86_BIN,	"/usr/X11R6"),
    DTE_TARBALL("Xcfg",	   &XF86Dists, XF86_CFG,	"/usr/X11R6"),
    DTE_TARBALL("Xdoc",	   &XF86Dists, XF86_DOC,	"/usr/X11R6"),
    DTE_TARBALL("Xhtml",   &XF86Dists, XF86_HTML,	"/usr/X11R6"),
    DTE_TARBALL("Xlib",	   &XF86Dists, XF86_LIB,	"/usr/X11R6"),
#if defined(__i386__) && defined(PC98)
    DTE_TARBALL("Xlk98",   &XF86Dists, XF86_LKIT98,	"/usr/X11R6"),
#endif
    DTE_TARBALL("Xlkit",   &XF86Dists, XF86_LKIT,	"/usr/X11R6"),
    DTE_TARBALL("Xman",	   &XF86Dists, XF86_MAN,	"/usr/X11R6"),
    DTE_TARBALL("Xprog",   &XF86Dists, XF86_PROG,	"/usr/X11R6"),
    DTE_TARBALL("Xps",	   &XF86Dists, XF86_PS,		"/usr/X11R6"),
    DTE_TARBALL("Xset",	   &XF86Dists, XF86_SET,	"/usr/X11R6"),
#if defined(__i386__) && defined(PC98)
    DTE_TARBALL("X9set",   &XF86Dists, XF86_9SET,	"/usr/X11R6"),
#endif
    { NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
#if defined(__i386__) && defined(PC98)
    DTE_TARBALL("X9480", &XF86ServerDists, XF86_SERVER_9480,   "/usr/X11R6"),
    DTE_TARBALL("X9EGC", &XF86ServerDists, XF86_SERVER_9EGC,   "/usr/X11R6"),
    DTE_TARBALL("X9GA9", &XF86ServerDists, XF86_SERVER_9GA9,   "/usr/X11R6"),
    DTE_TARBALL("X9GAN", &XF86ServerDists, XF86_SERVER_9GAN,   "/usr/X11R6"),
    DTE_TARBALL("X9LPW", &XF86ServerDists, XF86_SERVER_9LPW,   "/usr/X11R6"),
    DTE_TARBALL("X9MGA", &XF86ServerDists, XF86_SERVER_9MGA,   "/usr/X11R6"),
    DTE_TARBALL("X9NKV", &XF86ServerDists, XF86_SERVER_9NKV,   "/usr/X11R6"),
    DTE_TARBALL("X9NS3", &XF86ServerDists, XF86_SERVER_9NS3,   "/usr/X11R6"),
    DTE_TARBALL("X9SPW", &XF86ServerDists, XF86_SERVER_9SPW,   "/usr/X11R6"),
    DTE_TARBALL("X9SVG", &XF86ServerDists, XF86_SERVER_9SVG,   "/usr/X11R6"),
    DTE_TARBALL("X9TGU", &XF86ServerDists, XF86_SERVER_9TGU,   "/usr/X11R6"),
    DTE_TARBALL("X9WEP", &XF86ServerDists, XF86_SERVER_9WEP,   "/usr/X11R6"),
    DTE_TARBALL("X9WS",	 &XF86ServerDists, XF86_SERVER_9WS,    "/usr/X11R6"),
    DTE_TARBALL("X9WSN", &XF86ServerDists, XF86_SERVER_9WSN,   "/usr/X11R6"),
#endif
    DTE_TARBALL("X3DL",	 &XF86ServerDists, XF86_SERVER_3DL,    "/usr/X11R6"),
#ifdef __i386__
    DTE_TARBALL("X8514", &XF86ServerDists, XF86_SERVER_8514,   "/usr/X11R6"),
    DTE_TARBALL("XAGX",	 &XF86ServerDists, XF86_SERVER_AGX,    "/usr/X11R6"),
#endif
    DTE_TARBALL("XI128", &XF86ServerDists, XF86_SERVER_I128,   "/usr/X11R6"),
#if defined(__i386__) || defined(__amd64__)
    DTE_TARBALL("XMa8",	 &XF86ServerDists, XF86_SERVER_MACH8,  "/usr/X11R6"),
    DTE_TARBALL("XMa32", &XF86ServerDists, XF86_SERVER_MACH32, "/usr/X11R6"),
#endif
    DTE_TARBALL("XMa64", &XF86ServerDists, XF86_SERVER_MACH64, "/usr/X11R6"),
    DTE_TARBALL("XMono", &XF86ServerDists, XF86_SERVER_MONO,   "/usr/X11R6"),
    DTE_TARBALL("XP9K",	 &XF86ServerDists, XF86_SERVER_P9000,  "/usr/X11R6"),
    DTE_TARBALL("XS3",	 &XF86ServerDists, XF86_SERVER_S3,     "/usr/X11R6"),
    DTE_TARBALL("XS3V",	 &XF86ServerDists, XF86_SERVER_S3V,    "/usr/X11R6"),
    DTE_TARBALL("XSVGA", &XF86ServerDists, XF86_SERVER_SVGA,   "/usr/X11R6"),
#if defined(__i386__) || defined(__amd64__)
    DTE_TARBALL("XVG16", &XF86ServerDists, XF86_SERVER_VGA16,  "/usr/X11R6"),
    DTE_TARBALL("XW32",	 &XF86ServerDists, XF86_SERVER_W32,    "/usr/X11R6"),
#endif
#ifdef __alpha__
    DTE_TARBALL("XTGA",	 &XF86ServerDists, XF86_SERVER_TGA,    "/usr/X11R6"),
#endif
    { NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
    DTE_TARBALL("Xfnts", &XF86FontDists, XF86_FONTS_MISC,   "/usr/X11R6"),
    DTE_TARBALL("Xf100", &XF86FontDists, XF86_FONTS_100,    "/usr/X11R6"),
    DTE_TARBALL("Xfcyr", &XF86FontDists, XF86_FONTS_CYR,    "/usr/X11R6"),
    DTE_TARBALL("Xfscl", &XF86FontDists, XF86_FONTS_SCALE,  "/usr/X11R6"),
    DTE_TARBALL("Xfnon", &XF86FontDists, XF86_FONTS_NON,    "/usr/X11R6"),
    DTE_TARBALL("Xfsrv", &XF86FontDists, XF86_FONTS_SERVER, "/usr/X11R6"),
    { NULL },
};
#endif /* !X_AS_PKG */

static int	distMaybeSetPorts(dialogMenuItem *self);

static void
distVerifyFlags(void)
{
    if (SrcDists)
	Dists |= DIST_SRC;
    if (CRYPTODists)
	Dists |= DIST_CRYPTO;
    else if ((Dists & DIST_CRYPTO) && !CRYPTODists)
	CRYPTODists |= DIST_CRYPTO_ALL;
#ifndef X_AS_PKG
    /* XXX : realy only for X 3.3.6 */
    if (XF86Dists & DIST_XF86_SET)
	XF86ServerDists |= DIST_XF86_SERVER_VGA16;
#endif
    if (XF86ServerDists)
	XF86Dists |= DIST_XF86_SERVER;
    if (XF86FontDists)
	XF86Dists |= DIST_XF86_FONTS;
    if (XF86Dists || XF86ServerDists || XF86FontDists)
	Dists |= DIST_XF86;
    if (isDebug()) {
	msgDebug("Dist Masks: Dists: %0x, CRYPTO: %0x, Srcs: %0x\n", Dists,
	    CRYPTODists, SrcDists);
	msgDebug("XServer: %0x, XFonts: %0x, XDists: %0x\n", XF86ServerDists,
	    XF86FontDists, XF86Dists);
    }
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
#ifdef X_AS_PKG
    XF86Dists = DIST_XF86_CLIENTS | DIST_XF86_LIB | DIST_XF86_PROG | DIST_XF86_MAN | DIST_XF86_DOC | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_FB;
    XF86FontDists = DIST_XF86_FONTS_BITMAPS | DIST_XF86_FONTS_75;
#else
    XF86Dists = DIST_XF86_BIN | DIST_XF86_SET | DIST_XF86_CFG | DIST_XF86_LIB | DIST_XF86_PROG | DIST_XF86_MAN | DIST_XF86_DOC | DIST_XF86_SERVER | DIST_XF86_FONTS;
    XF86ServerDists = DIST_XF86_SERVER_SVGA | DIST_XF86_SERVER_VGA16;
    XF86FontDists = DIST_XF86_FONTS_MISC;
#endif
    return distSetXF86(NULL);
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
    CRYPTODists |= DIST_CRYPTO_CRYPTO;
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
		  "This will give you ready access to over 8200 ported software packages,\n"
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

/*
 * Try to get distribution as multiple pieces, locating and parsing an
 * info file which tells us how many we need for this distribution.
 */
static Boolean
distExtractTarball(char *path, char *dist, char *my_dir, int is_base)
{
    char *buf = NULL, fname[PATH_MAX];
    struct timeval start, stop;
    int i, j, status, total, intr, unmounted_dev;
    int cpid, zpid, fd2, chunk, numchunks;
    properties dist_attr = NULL;
    const char *tmp;
    FILE *fp;

    status = TRUE;
    numchunks = 0;
    snprintf(fname, sizeof (fname), "%s/%s.inf", path, dist);

getinfo:
    fp = DEVICE_GET(mediaDevice, fname, TRUE);
    intr = check_for_interrupt();
    if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
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
	snprintf(fname, sizeof(fname), "%s/%s.%s", path, dist,
	    USE_GZIP ? "tgz" : "tbz");
	/*
	 * Passing TRUE as 3rd parm to get routine makes this a "probing"
	 * get, for which errors are not considered too significant.
	 */
    getsingle:
	fp = DEVICE_GET(mediaDevice, fname, TRUE);
	intr = check_for_interrupt();
	if (fp == (FILE *)IO_ERROR || intr || !mediaDevice) {
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
	} else
	    return (FALSE);
    }

    if (isDebug())
	msgDebug("Parsing attributes file for distribution %s\n", dist);

    dist_attr = properties_read(fileno(fp));
    intr = check_for_interrupt();
    if (intr || !dist_attr) {
	msgConfirm("Cannot parse information file for the %s distribution: %s\n"
		   "Please verify that your media is valid and try again.",
		   dist, !intr ? "I/O error" : "User interrupt");
    } else {
	tmp = property_find(dist_attr, "Pieces");
	if (tmp)
	    numchunks = strtol(tmp, 0, 0);
    }
    fclose(fp);
    if (!numchunks)
	return (TRUE);

    if (isDebug())
	msgDebug("Attempting to extract distribution from %u chunks.\n",
	    numchunks);

    total = 0;
    (void)gettimeofday(&start, (struct timezone *)NULL);

    if (is_base && RunningAsInit && !Fake) {
	unmounted_dev = 1;
	unmount("/dev", MNT_FORCE);
    } else
	unmounted_dev = 0;
 
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
	snprintf(fname, sizeof(fname), "%s/%s.%c%c", path, dist, (chunk / 26) + 'a',
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

    if (unmounted_dev) {
	struct iovec iov[4];

	iov[0].iov_base = "fstype";
	iov[0].iov_len = sizeof("fstype");
	iov[1].iov_base = "devfs";
	iov[1].iov_len = sizeof("devfs");
	iov[2].iov_base = "fspath";
	iov[2].iov_len = sizeof("fstype");
	iov[3].iov_base = "/dev";
	iov[3].iov_len = sizeof("/dev");
	(void)nmount(iov, 4, 0);
	unmounted_dev = 0;
    }

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
    int old_dists, retries = 0, status = DITEM_SUCCESS;
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
