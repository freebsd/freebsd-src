/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dist.c,v 1.2 1995/05/08 21:39:34 jkh Exp $
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
    extern DMenu MenuSrcDistributions;
    int choice, scroll, curr, max;

    choice = scroll = curr = max;
    dmenuOpen(&MenuSrcDistributions, &choice, &scroll, &curr, &max);
    if (SrcDists)
	Dists |= DIST_SRC;
    return 0;
}

static int
distSetXF86(char *str)
{
    extern DMenu MenuXF86;
    int choice, scroll, curr, max;

    choice = scroll = curr = max;
    dmenuOpen(&MenuXF86, &choice, &scroll, &curr, &max);
    return 0;
}

static struct _dist {
    char *my_name;
    unsigned int my_bit;
} DistTable[] = {
{ "bin",	DIST_BIN	},
{ "games",	DIST_GAMES	},
{ "manpages",	DIST_MANPAGES	},
{ "proflibs",	DIST_PROFLIBS	},
{ "dict",	DIST_DICT	},
{ "src",	DIST_SRC	},
{ "des",	DIST_DES	},
{ "compat1x",	DIST_COMPAT1X	},
{ "xf86311",	DIST_XF86	},
{ NULL,		0		},
};

static struct _dist SrcDistTable[] = {
{ "base",	DIST_SRC_BASE	},
{ "gnu",	DIST_SRC_GNU	},
{ "etc",	DIST_SRC_ETC	},
{ "games",	DIST_SRC_GAMES	},
{ "include",	DIST_SRC_INCLUDE},
{ "lib",	DIST_SRC_LIB	},
{ "libexec",	DIST_SRC_LIBEXEC},
{ "lkm",	DIST_SRC_LKM	},
{ "release",	DIST_SRC_RELEASE},
{ "sbin",	DIST_SRC_SBIN	},
{ "share",	DIST_SRC_SHARE	},
{ "sys",	DIST_SRC_SYS	},
{ "ubin",	DIST_SRC_UBIN	},
{ "usbin",	DIST_SRC_USBIN	},
{ NULL,		0		},
};

static struct _dist XFree86DistTable[] = {
{ "bin",	DIST_XF86_BIN	},
{ "lib",	DIST_XF86_LIB	},
{ "doc",	DIST_XF86_DOC	},
{ "man",	DIST_XF86_MAN	},
{ "prog",	DIST_XF86_PROG	},
{ "link",	DIST_XF86_LINK	},
{ "pex",	DIST_XF86_PEX	},
{ "lbx",	DIST_XF86_LBX	},
{ "xicf",	DIST_XF86_XINIT	},
{ "xdmcf",	DIST_XF86_XDMCF	},
{ NULL,		0		},
};

static struct _dist XFree86ServerDistTable[] = {
{ "8514",	DIST_XF86_SERVER_8514	},
{ "AGX",	DIST_XF86_SERVER_AGX	},
{ "Mch3",	DIST_XF86_SERVER_MACH32	},
{ "Mch8",	DIST_XF86_SERVER_MACH8	},
{ "Mono",	DIST_XF86_SERVER_MONO	},
{ "P9K",	DIST_XF86_SERVER_P9000	},
{ "S3",		DIST_XF86_SERVER_S3	},
{ "SVGA",	DIST_XF86_SERVER_SVGA	},
{ "VGA16",	DIST_XF86_SERVER_VGA16	},
{ "W32",	DIST_XF86_SERVER_W32	},
{ "nest",	DIST_XF86_SERVER_NEST	},
{ NULL,		0		},
};

static struct _dist XFree86FontDistTable[] = {
{ "fnts",	DIST_XF86_FONTS_MISC	},
{ "f100",	DIST_XF86_FONTS_100	},
{ "fscl",	DIST_XF86_FONTS_SCALE	},
{ "fnon",	DIST_XF86_FONTS_NON	},
{ "fsrv",	DIST_XF86_FONTS_SERVER	},
};

static Boolean
dist_extract(char *name)
{
    if (!strcmp(name, "src")) {
    }
    else if (!strcmp(name, "xf86311l")) {
    }
    else {
    }
    return FALSE;
}
    
void
distExtractAll(void)
{
    int i;

    while (Dists) {
	for (i = 0; DistTable[i].my_name; i++) {
	    if (Dists & DistTable[i].my_bit) {
		if (dist_extract(DistTable[i].my_name))
		    Dists &= ~DistTable[i].my_bit;
		else
		    continue;
	    }
	}
    }
}
