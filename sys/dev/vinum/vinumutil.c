/*-
 * Copyright (c) 1997, 1998, 1999
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Written by Greg Lehey
 *
 *  This software is distributed under the so-called ``Berkeley
 *  License'':
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Nan Yang Computer
 *      Services Limited.
 * 4. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even if
 * advised of the possibility of such damage.
 *
 * $Id: vinumutil.c,v 1.17 2003/04/28 02:54:43 grog Exp $
 * $FreeBSD$
 */

/* This file contains utility routines used both in kernel and user context */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/statetexts.h>
#ifndef _KERNEL
#include <stdio.h>
#include <string.h>
extern jmp_buf command_fail;				    /* return on a failed command */
#endif

static char numeric_state[32];				    /* temporary buffer for ASCII conversions */
#define STATECOUNT(x) (sizeof (x##statetext) / sizeof (char *))
/* Return drive state as a string */
char *
drive_state(enum drivestate state)
{
    if (((unsigned) state) >= STATECOUNT(drive)) {
	sprintf(numeric_state, "Invalid state %d", (int) state);
	return numeric_state;
    } else
	return drivestatetext[state];
}

/* Return volume state as a string */
char *
volume_state(enum volumestate state)
{
    if (((unsigned) state) >= STATECOUNT(vol)) {
	sprintf(numeric_state, "Invalid state %d", (int) state);
	return numeric_state;
    } else
	return volstatetext[state];
}

/* Return plex state as a string */
char *
plex_state(enum plexstate state)
{
    if (((unsigned) state) >= STATECOUNT(plex)) {
	sprintf(numeric_state, "Invalid state %d", (int) state);
	return numeric_state;
    } else
	return plexstatetext[state];
}

/* Return plex organization as a string */
char *
plex_org(enum plexorg org)
{
    switch (org) {
    case plex_disorg:					    /* disorganized */
	return "disorg";
	break;

    case plex_concat:					    /* concatenated plex */
	return "concat";
	break;

    case plex_striped:					    /* striped plex */
	return "striped";
	break;

    case plex_raid4:					    /* RAID-4 plex */
	return "raid4";

    case plex_raid5:					    /* RAID-5 plex */
	return "raid5";
	break;

    default:
	sprintf(numeric_state, "Invalid org %d", (int) org);
	return numeric_state;
    }
}

/* Return sd state as a string */
char *
sd_state(enum sdstate state)
{
    if (((unsigned) state) >= STATECOUNT(sd)) {
	sprintf(numeric_state, "Invalid state %d", (int) state);
	return numeric_state;
    } else
	return sdstatetext[state];
}

/* Now convert in the other direction */
/*
 * These are currently used only internally,
 * so we don't do too much error checking
 */
enum drivestate
DriveState(char *text)
{
    int i;
    for (i = 0; i < STATECOUNT(drive); i++)
	if (strcmp(text, drivestatetext[i]) == 0)	    /* found it */
	    return (enum drivestate) i;
    return -1;
}

enum sdstate
SdState(char *text)
{
    int i;
    for (i = 0; i < STATECOUNT(sd); i++)
	if (strcmp(text, sdstatetext[i]) == 0)		    /* found it */
	    return (enum sdstate) i;
    return -1;
}

enum plexstate
PlexState(char *text)
{
    int i;
    for (i = 0; i < STATECOUNT(plex); i++)
	if (strcmp(text, plexstatetext[i]) == 0)	    /* found it */
	    return (enum plexstate) i;
    return -1;
}

enum volumestate
VolState(char *text)
{
    int i;
    for (i = 0; i < STATECOUNT(vol); i++)
	if (strcmp(text, volstatetext[i]) == 0)		    /* found it */
	    return (enum volumestate) i;
    return -1;
}

/*
 * Take a number with an optional scale factor and convert
 * it to a number of bytes.
 *
 * The scale factors are:
 *
 * s    sectors (of 512 bytes)
 * b    blocks (of 512 bytes).  This unit is deprecated,
 *      because it's confusing, but maintained to avoid
 *      confusing Veritas users.
 * k    kilobytes (1024 bytes)
 * m    megabytes (of 1024 * 1024 bytes)
 * g    gigabytes (of 1024 * 1024 * 1024 bytes)
 */
u_int64_t
sizespec(char *spec)
{
    u_int64_t size;
    char *s;
    int sign = 1;					    /* -1 if negative */

    size = 0;
    if (spec != NULL) {					    /* we have a parameter */
	s = spec;
	if (*s == '-') {				    /* negative, */
	    sign = -1;
	    s++;					    /* skip */
	}
	if ((*s >= '0') && (*s <= '9')) {		    /* it's numeric */
	    while ((*s >= '0') && (*s <= '9'))		    /* it's numeric */
		size = size * 10 + *s++ - '0';		    /* convert it */
	    switch (*s) {
	    case '\0':
		return size * sign;

	    case 'B':
	    case 'b':
	    case 'S':
	    case 's':
		return size * sign * 512;

	    case 'K':
	    case 'k':
		return size * sign * 1024;

	    case 'M':
	    case 'm':
		return size * sign * 1024 * 1024;

	    case 'G':
	    case 'g':
		return size * sign * 1024 * 1024 * 1024;
	    }
	}
#ifdef _KERNEL
	throw_rude_remark(EINVAL, "Invalid length specification: %s", spec);
#else
	fprintf(stderr, "Invalid length specification: %s", spec);
	longjmp(command_fail, 1);
#endif
    }
#ifdef _KERNEL
    throw_rude_remark(EINVAL, "Missing length specification");
#else
    fprintf(stderr, "Missing length specification");
    longjmp(command_fail, 1);
#endif
    /* NOTREACHED */
    return -1;
}

/*
 * Extract the volume number from a device number.  Check that it's
 * the correct type, and that it isn't one of the superdevs.
 */
int
Volno(dev_t dev)
{
    int volno = minor(dev);

    if (OBJTYPE(dev) != VINUM_VOLUME_TYPE)
	return -1;
    else
	volno = ((volno & 0x3fff0000) >> 8) | (volno & 0xff);
    if ((volno == VINUM_SUPERDEV_VOL)
	|| (volno == VINUM_DAEMON_VOL))
	return -1;
    else
	return volno;
}

/*
 * Extract a plex number from a device number.
 * Don't check the major number, but check the
 * type.  Return -1 for invalid types.
 */
int
Plexno(dev_t dev)
{
    int plexno = minor(dev);

    if (OBJTYPE(dev) != VINUM_PLEX_TYPE)
	return -1;
    else
	return ((plexno & 0x3fff0000) >> 8) | (plexno & 0xff);
}

/*
 * Extract a subdisk number from a device number.
 * Don't check the major number, but check the
 * type.  Return -1 for invalid types.
 */
int
Sdno(dev_t dev)
{
    int sdno = minor(dev);

    /*
     * Care: VINUM_SD_TYPE is 2 or 3, which is why we use < instead of
     * !=.  It's not clear that this makes any sense abstracting it to
     * this level.
     */
    if (OBJTYPE(dev) < VINUM_SD_TYPE)
	return -1;
    else
/*
 * Note that the number we return includes the low-order bit of the
 * type field.  This gives us twice as many potential subdisks as
 * plexes or volumes.
 */
	return ((sdno & 0x7fff0000) >> 8) | (sdno & 0xff);
}
