/*
 * The new sysinstall program.
 *
 * This is probably the last program in the `sysinstall' line - the next
 * generation being essentially a complete rewrite.
 *
 * $Id: dist.c,v 1.34 1995/05/29 14:38:31 jkh Exp $
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
distReset(char *str)
{
    Dists = 0;
    SrcDists = 0;
    XF86Dists = 0;
    XF86ServerDists = 0;
    XF86FontDists = 0;
    return 0;
}

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
    char *my_dir;
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
{ "bin",	"/",			&Dists,		DIST_BIN,		NULL		},
{ "games",	"/",			&Dists,		DIST_GAMES,		NULL		},
{ "manpages",	"/",			&Dists,		DIST_MANPAGES,		NULL		},
{ "proflibs",	"/",			&Dists,		DIST_PROFLIBS,		NULL		},
{ "dict",	"/",			&Dists,		DIST_DICT,		NULL		},
{ "src",	"/",			&Dists,		DIST_SRC,		SrcDistTable	},
{ "des",	"/",			&Dists,		DIST_DES,		NULL		},
{ "compat1x",	"/",			&Dists,		DIST_COMPAT1X,		NULL		},
{ "compat20",	"/",			&Dists,		DIST_COMPAT20,		NULL		},
{ "XF86311",	"/usr",			&Dists,		DIST_XF86,		XF86DistTable	},
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
{ "ssbin",	"/usr/src",		&SrcDists,	DIST_SRC_SBIN,		NULL		},
{ "sshare",	"/usr/src",		&SrcDists,	DIST_SRC_SHARE,		NULL		},
{ "ssys",	"/usr/src",		&SrcDists,	DIST_SRC_SYS,		NULL		},
{ "subin",	"/usr/src",		&SrcDists,	DIST_SRC_UBIN,		NULL		},
{ "susbin",	"/usr/src",		&SrcDists,	DIST_SRC_USBIN,		NULL		},
{ "XF86-xc",	"/usr/X11R6/src",	&SrcDists,	DIST_SRC_XF86,		NULL		},
{ "XF86-co",	"/usr/X11R6/src",	&SrcDists,	DIST_SRC_XF86,		NULL		},
{ NULL },
};

/* The XFree86 distribution */
static Distribution XF86DistTable[] = {
{ "X311bin",	"/usr",			&XF86Dists,	DIST_XF86_BIN,		NULL		},
{ "X311lib",	"/usr",			&XF86Dists,	DIST_XF86_LIB,		NULL		},
{ "X311doc",	"/usr",			&XF86Dists,	DIST_XF86_DOC,		NULL		},
{ "Xf86311",	"/usr",			&XF86Dists,	DIST_XF86_FONTS,	XF86FontDistTable },
{ "X311man",	"/usr",			&XF86Dists,	DIST_XF86_MAN,		NULL		},
{ "X311prog",	"/usr",			&XF86Dists,	DIST_XF86_PROG,		NULL		},
{ "X311link",	"/usr",			&XF86Dists,	DIST_XF86_LINK,		NULL		},
{ "X311pex",	"/usr",			&XF86Dists,	DIST_XF86_PEX,		NULL		},
{ "X311lbx",	"/usr",			&XF86Dists,	DIST_XF86_LBX,		NULL		},
{ "X311xicf",	"/usr",			&XF86Dists,	DIST_XF86_XINIT,	NULL		},
{ "X311xdmcf",	"/usr",			&XF86Dists,	DIST_XF86_XDMCF,	NULL		},
{ "Xf86311",	"/usr",			&XF86Dists,	DIST_XF86_SERVER,	XF86ServerDistTable },
{ NULL },
};

/* The XFree86 server distribution */
static Distribution XF86ServerDistTable[] = {
{ "X3118514",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_8514,	NULL		},
{ "X311AGX",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_AGX,	NULL		},
{ "X311Mch3",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH32,NULL		},
{ "X311Mch8",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MACH8,	NULL		},
{ "X311Mono",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_MONO,	NULL		},
{ "X311P9K",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_P9000,	NULL		},
{ "X311S3",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_S3,	NULL		},
{ "X311SVGA",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_SVGA,	NULL		},
{ "X311VG16",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_VGA16,	NULL		},
{ "X311W32",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_W32,	NULL		},
{ "X311nest",	"/usr",		&XF86ServerDists,	DIST_XF86_SERVER_NEST,	NULL		},
{ NULL },
};

/* The XFree86 font distribution */
static Distribution XF86FontDistTable[] = {
{ "X311fnts",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_MISC,	NULL		},
{ "X311f100",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_100,	NULL		},
{ "X311fscl",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SCALE,	NULL		},
{ "X311fnon",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_NON,	NULL		},
{ "X311fsrv",	"/usr",		&XF86FontDists,		DIST_XF86_FONTS_SERVER,	NULL		},
{ NULL },
};

static Boolean
distExtract(char *parent, Distribution *me)
{
    int i, status;
    int cpid, zpid, fd, fd2, chunk, numchunks;
    char *path, *dist, buf[10240];
    const char *tmp;
    Attribs *dist_attr;

    status = FALSE;
    if (mediaDevice->init)
	if (!(*mediaDevice->init)(mediaDevice))
	    return FALSE;

    for (i = 0; me[i].my_name; i++) {
	/* If we're not doing it, we're not doing it */
	if (!(me[i].my_bit & *(me[i].my_mask)))
	    continue;

	/* Recurse if actually have a sub-distribution */
	if (me[i].my_dist) {
	    status = distExtract(me[i].my_name, me[i].my_dist);
	    goto done;
	}

	dist = me[i].my_name;
	path = parent ? parent : me[i].my_name;

        snprintf(buf, 512, "%s/%s.tgz", path, dist);
	fd = (*mediaDevice->get)(buf);
	if (fd != -1) {
	    msgNotify("Extracting %s into %s directory...", me[i].my_name, me[i].my_dir);
	    status = mediaExtractDist(me[i].my_dir, fd);
	    if (mediaDevice->close)
		(*mediaDevice->close)(mediaDevice, fd);
	    else
		close(fd);
	    goto done;
	}

	snprintf(buf, sizeof buf, "/stand/info/%s/%s.inf", path, dist);
	if (!access(buf, R_OK)) {
	    if (isDebug())
		msgDebug("Parsing attributes file for %s\n", dist);
	    dist_attr = safe_malloc(sizeof(Attribs) * MAX_ATTRIBS);
	    if (attr_parse(&dist_attr, buf) == 0) {
		msgConfirm("Cannot load information file for %s distribution!\nPlease verify that your media is valid and try again.", dist);
		return FALSE;
	    }

	    if (isDebug())
		msgDebug("Looking for attribute `pieces'\n");
	    tmp = attr_match(dist_attr, "pieces");
	    if (tmp)
		numchunks = atoi(tmp);
	    else
		numchunks = 0;
	}
	else
	    numchunks = 0;

	if (isDebug())
	    msgDebug("Attempting to extract distribution from %u chunks.\n", numchunks);

	if (numchunks < 2 ) {
	    snprintf(buf, 512, "%s/%s", path, dist);
	    if (numchunks)
		strcat(buf,".aa");
	    fd = (*mediaDevice->get)(buf);
	    if (fd == -1) {
		status = FALSE;
	    } else {
		msgNotify("Extracting %s into %s directory...", me[i].my_name, me[i].my_dir);
		status = mediaExtractDist(me[i].my_dir, fd);
		if (mediaDevice->close)
		    (*mediaDevice->close)(mediaDevice, fd);
		else
		    close(fd);
	    }
	    goto done;
	}

	mediaExtractDistBegin(me[i].my_dir, &fd2, &zpid, &cpid);
	dialog_clear();
	for (chunk = 0; chunk < numchunks; chunk++) {
	    int n, retval;
	    char prompt[80];
	    int retries = 0;

	    snprintf(buf, 512, "%s/%s.%c%c", path, dist,	(chunk / 26) + 'a', (chunk % 26) + 'a');
retry:
	    fd = (*mediaDevice->get)(buf);
	    if (fd < 0) {
		if (++retries < 5)
		    goto retry;
		msgConfirm("failed to retreive piece file %s after 5 retries!\nAborting the transfer", buf);
		goto punt;
	    }
	    snprintf(prompt, 80, "Extracting %s into %s directory...", me[i].my_name, me[i].my_dir);
	    dialog_gauge(" Progress ", prompt, 8, 15, 6, 50, (int) ((float) (chunk + 1) / numchunks * 100));
	    while ((n = read(fd, buf, sizeof buf)) > 0) {
		retval = write(fd2, buf, n);
		if (retval != n)
		{
		    if (mediaDevice->close)
			(*mediaDevice->close)(mediaDevice, fd);
		    else
			close(fd);
		    msgConfirm("Write failure on transfer! (wrote %d bytes of %d bytes)", retval, n);
		    goto punt;
		}
	    }
	    if (mediaDevice->close)
		(*mediaDevice->close)(mediaDevice, fd);
	    else
		close(fd);
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
	    if (getenv(NO_CONFIRMATION))
		status = TRUE;
	    else
		status = msgYesNo("Unable to transfer the %s distribution from %s.\nDo you want to retry this distribution later?", me[i].my_name, mediaDevice->name);
	}
	if (status) {
	    /* Extract was successful, remove ourselves from further consideration */
	    *(me[i].my_mask) &= ~(me[i].my_bit);
	}
    }
    if (mediaDevice->shutdown && parent == NULL) {
	(*mediaDevice->shutdown)(mediaDevice);
	mediaDevice = NULL;
    }
    return status;
}

void
distExtractAll(void)
{
    int retries = 0;

    /* Try for 3 times around the loop, then give up. */
    while (Dists && ++retries < 3)
	distExtract(NULL, DistTable);
    if (Dists)
	msgConfirm("Couldn't extract all of the dists.  Residue: %0x", Dists);
}
