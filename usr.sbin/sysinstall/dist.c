/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dist.c,v 1.9 1995/05/19 17:11:07 jkh Exp $
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Jordan Hubbard
 *	for the FreeBSD Project.
 * 4. The name of Jordan Hubbard or the FreeBSD project may not be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
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
unsigned int SrcDists;
unsigned int XF86Dists;
unsigned int XF86ServerDists;
unsigned int XF86FontDists;

static int	distSetXF86(char *str);

int
distSetDeveloper(char *str)
{
    Dists = _DIST_DEVELOPER;
    SrcDists = DIST_SRC_ALL;
    return 0;
}

int
distSetXDeveloper(char *str)
{
    Dists = _DIST_XDEVELOPER;
    SrcDists = DIST_SRC_ALL;
    distSetXF86(NULL);
    return 0;
}

int
distSetUser(char *str)
{
    Dists = _DIST_USER;
    return 0;
}

int
distSetXUser(char *str)
{
    Dists = _DIST_XUSER;
    distSetXF86(NULL);
    return 0;
}

int
distSetMinimum(char *str)
{
    Dists = DIST_BIN;
    return 0;
}

int
distSetEverything(char *str)
{
    Dists = DIST_ALL;
    SrcDists = DIST_SRC_ALL;
    distSetXF86(NULL);
    return 0;
}

int
distSetSrc(char *str)
{
    dmenuOpenSimple(&MenuSrcDistributions);
    if (SrcDists)
	Dists |= DIST_SRC;
    return 0;
}

static int
distSetXF86(char *str)
{
    dmenuOpenSimple(&MenuXF86Select);
    return 0;
}

typedef struct _dist {
    char *my_name;
    unsigned int *my_mask;
    unsigned int my_bit;
    struct _dist *my_dist;
} Distribution;

extern Distribution SrcDistTable[];
extern Distribution XF86DistTable[];
extern Distribution XF86FontDistTable[];
extern Distribution XF86ServerDistTable[];


/* The top-level distribution categories */
static Distribution DistTable[] = {
{ "bin",	&Dists,		DIST_BIN,		NULL		},
{ "games",	&Dists,		DIST_GAMES,		NULL		},
{ "manpages",	&Dists,		DIST_MANPAGES,		NULL		},
{ "proflibs",	&Dists,		DIST_PROFLIBS,		NULL		},
{ "dict",	&Dists,		DIST_DICT,		NULL		},
{ "src/",	&Dists,		DIST_SRC,		SrcDistTable	},
{ "des",	&Dists,		DIST_DES,		NULL		},
{ "compat1x",	&Dists,		DIST_COMPAT1X,		NULL		},
{ "compat20",	&Dists,		DIST_COMPAT20,		NULL		},
{ "xf86311/",	&Dists,		DIST_XF86,		XF86DistTable	},
{ NULL },
};

/* The /usr/src distribution */
static Distribution SrcDistTable[] = {
{ "base",	&SrcDists,	DIST_SRC_BASE,		NULL		},
{ "gnu",	&SrcDists,	DIST_SRC_GNU,		NULL		},
{ "etc",	&SrcDists,	DIST_SRC_ETC,		NULL		},
{ "games",	&SrcDists,	DIST_SRC_GAMES,		NULL		},
{ "include",	&SrcDists,	DIST_SRC_INCLUDE,	NULL		},
{ "lib",	&SrcDists,	DIST_SRC_LIB,		NULL		},
{ "libexec",	&SrcDists,	DIST_SRC_LIBEXEC,	NULL		},
{ "lkm",	&SrcDists,	DIST_SRC_LKM,		NULL		},
{ "release",	&SrcDists,	DIST_SRC_RELEASE,	NULL		},
{ "sbin",	&SrcDists,	DIST_SRC_SBIN,		NULL		},
{ "share",	&SrcDists,	DIST_SRC_SHARE,		NULL		},
{ "sys",	&SrcDists,	DIST_SRC_SYS,		NULL		},
{ "ubin",	&SrcDists,	DIST_SRC_UBIN,		NULL		},
{ "usbin",	&SrcDists,	DIST_SRC_USBIN,		NULL		},
{ "xf86",	&SrcDists,	DIST_SRC_XF86,		NULL		},
{ NULL },
};

/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
{ "bin",	&XF86Dists,	DIST_XF86_BIN,		NULL		},
{ "lib",	&XF86Dists,	DIST_XF86_LIB,		NULL		},
{ "doc",	&XF86Dists,	DIST_XF86_DOC,		NULL		},
{ "xf86311/",	&XF86Dists,	DIST_XF86_FONTS,	XF86FontDistTable },
{ "man",	&XF86Dists,	DIST_XF86_MAN,		NULL		},
{ "prog",	&XF86Dists,	DIST_XF86_PROG,		NULL		},
{ "link",	&XF86Dists,	DIST_XF86_LINK,		NULL		},
{ "pex",	&XF86Dists,	DIST_XF86_PEX,		NULL		},
{ "lbx",	&XF86Dists,	DIST_XF86_LBX,		NULL		},
{ "xicf",	&XF86Dists,	DIST_XF86_XINIT,	NULL		},
{ "xdmcf",	&XF86Dists,	DIST_XF86_XDMCF,	NULL		},
{ "xf86311/",	&XF86Dists,	DIST_XF86_SERVER,	XF86ServerDistTable },
{ NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
{ "X3118514",	&XF86ServerDists,	DIST_XF86_SERVER_8514,	NULL		},
{ "X311AGX",	&XF86ServerDists,	DIST_XF86_SERVER_AGX,	NULL		},
{ "X311Mch3",	&XF86ServerDists,	DIST_XF86_SERVER_MACH32,NULL		},
{ "X311Mch8",	&XF86ServerDists,	DIST_XF86_SERVER_MACH8,	NULL		},
{ "X311Mono",	&XF86ServerDists,	DIST_XF86_SERVER_MONO,	NULL		},
{ "X311P9K",	&XF86ServerDists,	DIST_XF86_SERVER_P9000,	NULL		},
{ "X311S3",	&XF86ServerDists,	DIST_XF86_SERVER_S3,	NULL		},
{ "X311SVGA",	&XF86ServerDists,	DIST_XF86_SERVER_SVGA,	NULL		},
{ "X311VG16",	&XF86ServerDists,	DIST_XF86_SERVER_VGA16,	NULL		},
{ "X311W32",	&XF86ServerDists,	DIST_XF86_SERVER_W32,	NULL		},
{ "X311nest",	&XF86ServerDists,	DIST_XF86_SERVER_NEST,	NULL		},
{ NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
{ "X311fnts",	&XF86FontDists,		DIST_XF86_FONTS_MISC,	NULL		},
{ "X311f100",	&XF86FontDists,		DIST_XF86_FONTS_100,	NULL		},
{ "X311fscl",	&XF86FontDists,		DIST_XF86_FONTS_SCALE,	NULL		},
{ "X311fnon",	&XF86FontDists,		DIST_XF86_FONTS_NON,	NULL		},
{ "X311fsrv",	&XF86FontDists,		DIST_XF86_FONTS_SERVER,	NULL		},
{ NULL },
};

static int
distExtract(char *parent, Distribution *me)
{
    int i, status;
    FILE *fp;

    status = 0;
    for (i = 0; me[i].my_name; i++) {
	if (me[i].my_bit & *(me[i].my_mask)) {
	    if (me[i].my_dist)
		status = distExtract(me[i].my_name, me[i].my_dist);
	    else {
		msgNotify("Attempting to open %s%s distribution", parent ? parent : "", me[i].my_name);
		fp = mediaOpen(parent, me[i].my_name);
		if (fp) {
		    msgNotify("Extracting %s%s distribution", parent ? parent : "", me[i].my_name);
		    status = mediaExtractDist(fp);
		    fclose(fp);
		}
		else {
		    if (getenv(NO_CONFIRMATION))
			status = 0;
		    else
			status = msgYesNo("Unable to transfer the %s distribution from %s.\nDo you want to retry this distribution later?");
		}
	    }
	    if (!status) {
		/*
		 * Extract was successful, remove ourselves from further
		 * consideration
		 */
		*(me[i].my_mask) &= ~(me[i].my_bit);
	    }
	}
    }
    return status;
}

void
distExtractAll(void)
{
    distExtract(NULL, DistTable);
}
