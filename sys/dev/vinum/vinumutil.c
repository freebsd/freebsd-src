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
 * $Id: vinumutil.c,v 1.14 1999/12/30 07:04:02 grog Exp grog $
 * $FreeBSD$
 */

/* This file contains utility routines used both in kernel and user context */

#include <dev/vinum/vinumhdr.h>
#include <dev/vinum/statetexts.h>
#ifndef _KERNEL
#include <stdio.h>
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
	longjmp(command_fail, -1);
#endif
    }
#ifdef _KERNEL
    throw_rude_remark(EINVAL, "Missing length specification");
#else
    fprintf(stderr, "Missing length specification");
    longjmp(command_fail, -1);
#endif
    /* NOTREACHED */
    return -1;
}

/*
 * Extract the volume number from a device number.
 * Perform no checking.
 */
int
Volno(dev_t dev)
{
    return (minor(dev) & MASK(VINUM_VOL_WIDTH)) >> VINUM_VOL_SHIFT;
}

/*
 * Extract a plex number from a device number.
 * Don't check the major number, but check the
 * type.  Return -1 for invalid types.
 */
int
Plexno(dev_t dev)
{
    switch (DEVTYPE(dev)) {
    case VINUM_VOLUME_TYPE:
    case VINUM_DRIVE_TYPE:
    case VINUM_SUPERDEV_TYPE:				    /* ordinary super device */
    case VINUM_RAWSD_TYPE:
	return -1;

    case VINUM_PLEX_TYPE:
    case VINUM_SD_TYPE:
	return VOL[Volno(dev)].plex[(minor(dev) >> VINUM_PLEX_SHIFT) & (MASK(VINUM_PLEX_WIDTH))];

    case VINUM_RAWPLEX_TYPE:
	return ((minor(dev) & MASK(VINUM_VOL_WIDTH)) >> VINUM_VOL_SHIFT) /* low order 8 bits */
	|((minor(dev) >> VINUM_RAWPLEX_SHIFT)
	    & (MASK(VINUM_RAWPLEX_WIDTH)
		<< (VINUM_VOL_SHIFT + VINUM_VOL_WIDTH)));   /* upper 12 bits */
    }
    return 0;						    /* compiler paranoia */
}

/*
 * Extract a subdisk number from a device number.
 * Don't check the major number, but check the
 * type.  Return -1 for invalid types.
 */
int
Sdno(dev_t dev)
{
    switch (DEVTYPE(dev)) {
    case VINUM_VOLUME_TYPE:
    case VINUM_DRIVE_TYPE:
    case VINUM_SUPERDEV_TYPE:				    /* ordinary super device */
    case VINUM_PLEX_TYPE:
    case VINUM_RAWPLEX_TYPE:
	return -1;

    case VINUM_SD_TYPE:
	return PLEX[Plexno(dev)].sdnos[(minor(dev) >> VINUM_SD_SHIFT) & (MASK(VINUM_SD_WIDTH))];

    case VINUM_RAWSD_TYPE:
	return ((minor(dev) & MASK(VINUM_VOL_WIDTH)) >> VINUM_VOL_SHIFT) /* low order 8 bits */
	|((minor(dev) >> VINUM_RAWPLEX_SHIFT) & (MASK(VINUM_RAWPLEX_WIDTH)
		<< (VINUM_VOL_SHIFT + VINUM_VOL_WIDTH)));   /* upper 12 bits */
    }
    return -1;						    /* compiler paranoia */
}
