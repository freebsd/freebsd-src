/*
 * The new sysinstall program.
 *
 * This is probably the last attempt in the `sysinstall' line, the next
 * generation being slated to essentially a complete rewrite.
 *
 * $Id: media.c,v 1.7 1995/05/20 00:13:11 jkh Exp $
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

#include <stdio.h>
#include "sysinstall.h"

/*
 * Return 1 if we successfully found and set the installation type to
 * be a CD.
 */
int
mediaSetCDROM(char *str)
{
    Device **devs;
    int cnt;

    if (OnCDROM == TRUE)
	return 1;
    else {
	devs = deviceFind(NULL, DEVICE_TYPE_CDROM);
	cnt = deviceCount(devs);
	if (!cnt) {
	    msgConfirm("No CDROM devices found!  Please check that your system's\nconfiguration is correct and that the CDROM drive is of a supported\ntype.  For more information, consult the hardware guide\nin the Doc menu.");
	    return 0;
        }
	else if (cnt > 1) {
	    /* put up a menu */
	}
	else {
	    mediaDevice = devs[0];
	}
    }
    return 0;
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a floppy
 */
int
mediaSetFloppy(char *str)
{
    dmenuOpenSimple(&MenuMediaFloppy);
    return 0;
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a DOS partition.
 */
int
mediaSetDOS(char *str)
{
    Device **devs;

    devs = deviceFind(NULL, DEVICE_TYPE_DISK);
    if (!devs)
	msgConfirm("No disk devices found!");
    return 0;
}

/*
 * Return 1 if we successfully found and set the installation type to
 * be a tape drive.
 */
int
mediaSetTape(char *str)
{
    return 0;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be an ftp server
 */
int
mediaSetFTP(char *str)
{
    dmenuOpenSimple(&MenuMediaFTP);
    return 0;
}

/*
 * Return 0 if we successfully found and set the installation type to
 * be some sort of mounted filesystem (it's also mounted at this point)
 */
int
mediaSetFS(char *str)
{
    return 0;
}

FILE *
mediaOpen(char *parent, char *me)
{
    char fname[FILENAME_MAX];

    if (!mediaVerify())
	return NULL;

    if (parent)
	snprintf(fname, FILENAME_MAX, "%s%s", parent, me);
    else
	snprintf(fname, FILENAME_MAX, "%s/%s", me, me);
#if 0
	strncpy(fname, me, FILENAME_MAX);
#endif
    /* XXX mediaDevice points to where we want to get it from */
    return NULL;
}

Boolean
mediaExtractDist(FILE *fp)
{
    return TRUE;
}

Boolean
mediaGetType(void)
{
    char *cp;

    dmenuOpenSimple(&MenuMedia);
#if 0
    cp = getenv(MEDIA_TYPE);
    if (!cp)
	return FALSE;
#endif
    return TRUE;
}

/* Return TRUE if all the media variables are set up correctly */
Boolean
mediaVerify(void)
{
    if (!mediaDevice) {
	msgConfirm("Media type not set!  Please select a media type\nfrom the Installation menu before proceeding.");
	return FALSE;
    }
    return TRUE;
}

void
mediaClose(void)
{
}
