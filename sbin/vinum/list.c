/*      list.c: vinum interface program, list routines
 */
/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
 *
 *  Parts copyright (c) 1997, 1998 Cybernet Corporation, NetMAX project.
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
 * $Id: list.c,v 1.25 2000/12/20 03:38:43 grog Exp grog $
 * $FreeBSD$
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netdb.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include "vext.h"
#include <dev/vinum/request.h>
/* #include <dev/vinum/vinumhdr.h> */
#include <devstat.h>

/*
 * When a subdisk is reviving or initializing, we
 * check to see whether it is still progressing
 * and print a warning if not.  We check every 50
 * ms, up to a maximum of 5 seconds.  This is the
 * counter value.
 */
#define STALLCOUNT	100

/*
 * Take a size in sectors and return a pointer to
 * a string which represents the size best.  If lj
 * is != 0, return left justified, otherwise in a
 * fixed 10 character field suitable for columnar
 * printing.
 *
 * Note this uses a static string: it's only
 * intended to be used immediately for printing.
 */
char *
roughlength(int64_t bytes, int lj)
{
    static char description[16];

    if (bytes > (int64_t) MEGABYTE * 10000)		    /* gigabytes */
	sprintf(description, lj ? "%lld GB" : "%10d GB", bytes / GIGABYTE);
    else if (bytes > KILOBYTE * 10000)			    /* megabytes */
	sprintf(description, lj ? "%lld MB" : "%10d MB", bytes / MEGABYTE);
    else if (bytes > 10000)				    /* kilobytes */
	sprintf(description, lj ? "%lld kB" : "%10d kB", bytes / KILOBYTE);
    else						    /* bytes */
	sprintf(description, lj ? "%lld  B" : "%10d  B", bytes);
    return description;
}

void
vinum_list(int argc, char *argv[], char *argv0[])
{
    int object;
    int i;
    enum objecttype type;

    if (sflag & (!vflag))				    /* just summary stats, */
	printf("Object\t\t  Reads\t\tBytes\tAverage\tRecover\t Writes"
	    "\t\tBytes\tAverage\t  Mblock  Mstripe\n\n");
    if (argc == 0)
	listconfig();					    /* list everything */
    else {
	for (i = 0; i < argc; i++) {
	    object = find_object(argv[i], &type);	    /* look for it */
	    if (vinum_li(object, type))
		fprintf(stderr, "Can't find object: %s\n", argv[i]);
	}
    }
}

/* List an object */
int
vinum_li(int object, enum objecttype type)
{
    switch (type) {
    case drive_object:
	vinum_ldi(object, recurse);
	break;

    case sd_object:
	vinum_lsi(object, recurse);
	break;

    case plex_object:
	vinum_lpi(object, recurse);
	break;

    case volume_object:
	vinum_lvi(object, recurse);
	break;

    default:
	return -1;
    }
    return 0;
}

void
vinum_ldi(int driveno, int recurse)
{
    time_t t;						    /* because Bruce says so */
    int sdno;						    /* for recursion */

    get_drive_info(&drive, driveno);
    if (drive.state != drive_unallocated) {
	if (vflag) {
	    printf("Drive %s:\tDevice %s\n",
		drive.label.name,
		drive.devicename);
	    t = drive.label.date_of_birth.tv_sec;
	    printf("\t\tCreated on %s at %s",
		drive.label.sysname,
		ctime(&t));
	    t = drive.label.last_update.tv_sec;
	    printf("\t\tConfig last updated %s",	    /* care: \n at end */
		ctime(&t));
	    printf("\t\tSize: %16lld bytes (%lld MB)\n\t\tUsed: %16lld bytes (%lld MB)\n"
		"\t\tAvailable: %11qd bytes (%d MB)\n",
		(long long) drive.label.drive_size,	    /* bytes used */
		(long long) (drive.label.drive_size / MEGABYTE),
		(long long) (drive.label.drive_size - drive.sectors_available
		    * DEV_BSIZE),
		(long long) (drive.label.drive_size - drive.sectors_available
		    * DEV_BSIZE) / MEGABYTE,
		(long long) drive.sectors_available * DEV_BSIZE,
		(int) (drive.sectors_available * DEV_BSIZE / MEGABYTE));
	    printf("\t\tState: %s\n", drive_state(drive.state));
	    if (drive.lasterror != 0)
		printf("\t\tLast error: %s\n", strerror(drive.lasterror));
	    else
		printf("\t\tLast error: none\n");
	    printf("\t\tActive requests:\t%d\n\t\tMaximum active:\t\t%d\n",
		drive.active,
		drive.maxactive);
	    if (Verbose) {				    /* print the free list */
		int fe;					    /* freelist entry */
		struct drive_freelist freelist;
		struct ferq {				    /* request to pass to ioctl */
		    int driveno;
		    int fe;
		} *ferq = (struct ferq *) &freelist;

		printf("\t\tFree list contains %d entries:\n\t\t   Offset\t     Size\n",
		    drive.freelist_entries);
		for (fe = 0; fe < drive.freelist_entries; fe++) {
		    ferq->driveno = drive.driveno;
		    ferq->fe = fe;
		    if (ioctl(superdev, VINUM_GETFREELIST, &freelist) < 0) {
			fprintf(stderr,
			    "Can't get free list element %d: %s\n",
			    fe,
			    strerror(errno));
			longjmp(command_fail, -1);
		    }
		    printf("\t\t%9lld\t%9lld\n",
			(long long) freelist.offset,
			(long long) freelist.sectors);
		}
	    }
	} else if (!sflag) {
	    printf("D %-21s State: %s\t%s\tA: %lld/%lld MB",
		drive.label.name,
		drive_state(drive.state),
		drive.devicename,
		(long long) drive.sectors_available * DEV_BSIZE / MEGABYTE,
		(long long) (drive.label.drive_size / MEGABYTE));
	    if (drive.label.drive_size != 0)
		printf(" (%d%%)",
		    (int) ((drive.sectors_available * 100 * DEV_BSIZE)
			/ (drive.label.drive_size - (DATASTART * DEV_BSIZE))));
	}
	if (sflag) {
	    if (vflag || Verbose) {
		printf("\t\tReads:  \t%16lld\n\t\tBytes read:\t%16lld (%s)\n",
		    (long long) drive.reads,
		    (long long) drive.bytes_read,
		    roughlength(drive.bytes_read, 1));
		if (drive.reads != 0)
		    printf("\t\tAverage read:\t%16lld bytes\n",
			(long long) drive.bytes_read / drive.reads);
		printf("\t\tWrites: \t%16lld\n\t\tBytes written:\t%16lld (%s)\n",
		    (long long) drive.writes,
		    (long long) drive.bytes_written,
		    roughlength(drive.bytes_written, 1));
		if (drive.writes != 0)
		    printf("\t\tAverage write:\t%16lld bytes\n",
			(long long) (drive.bytes_written / drive.writes));
	    } else {					    /* non-verbose stats */
		printf("%-15s\t%7lld\t%15lld\t",
		    drive.label.name,
		    (long long) drive.reads,
		    (long long) drive.bytes_read);
		if (drive.reads != 0)
		    printf("%7lld\t\t",
			(long long) (drive.bytes_read / drive.reads));
		else
		    printf("\t\t");
		printf("%7lld\t%15lld\t",
		    (long long) drive.writes,
		    (long long) drive.bytes_written);
		if (drive.writes != 0)
		    printf("%7lld",
			(long long) (drive.bytes_written / drive.writes));
	    }
	}
	if (recurse) {
	    printf("\n");
	    for (sdno = 0; sdno < vinum_conf.subdisks_allocated; sdno++) {
		get_sd_info(&sd, sdno);
		if ((sd.state != sd_unallocated)
		    && (sd.driveno == drive.driveno))
		    vinum_lsi(sd.sdno, 0);
	    }
	}
	printf("\n");
    }
}

void
vinum_ld(int argc, char *argv[], char *argv0[])
{
    int i;
    int driveno;
    enum objecttype type;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    if (argc == 0) {
	for (driveno = 0; driveno < vinum_conf.drives_allocated; driveno++)
	    vinum_ldi(driveno, recurse);
    } else {
	for (i = 0; i < argc; i++) {
	    driveno = find_object(argv[i], &type);
	    if (type == drive_object)
		vinum_ldi(driveno, recurse);
	    else
		fprintf(stderr, "%s is not a drive\n", argv[i]);
	}
    }
}

void
vinum_lvi(int volno, int recurse)
{
    get_volume_info(&vol, volno);
    if (vol.state != volume_unallocated) {
	if (vflag) {
	    printf("Volume %s:\tSize: %lld bytes (%lld MB)\n"
		"\t\tState: %s\n\t\tFlags: %s%s%s\n",
		vol.name,
		((long long) vol.size) * DEV_BSIZE,
		((long long) vol.size) * DEV_BSIZE / MEGABYTE,
		volume_state(vol.state),
		vol.flags & VF_OPEN ? "open " : "",
		(vol.flags & VF_WRITETHROUGH ? "writethrough " : ""),
		(vol.flags & VF_RAW ? "raw" : ""));
	    printf("\t\t%d plexes\n\t\tRead policy: ", vol.plexes);
	    if (vol.preferred_plex < 0)			    /* round robin */
		printf("round robin\n");
	    else {
		get_plex_info(&plex, vol.plex[vol.preferred_plex]);
		printf("plex %d (%s)\n", vol.preferred_plex, plex.name);
	    }
	} else if (!sflag)				    /* brief */
	    printf("V %-21s State: %s\tPlexes: %7d\tSize: %s\n",
		vol.name,
		volume_state(vol.state),
		vol.plexes,
		roughlength(vol.size << DEV_BSHIFT, 0));
	if (sflag) {
	    if (vflag || Verbose) {
		printf("\t\tReads:  \t%16lld\n\t\tRecovered:\t%16lld\n\t\tBytes read:\t%16lld (%s)\n",
		    (long long) vol.reads,
		    (long long) vol.recovered_reads,
		    (long long) vol.bytes_read,
		    roughlength(vol.bytes_read, 1));
		if (vol.reads != 0)
		    printf("\t\tAverage read:\t%16lld bytes\n",
			(long long) (vol.bytes_read / vol.reads));
		printf("\t\tWrites: \t%16lld\n\t\tBytes written:\t%16lld (%s)\n",
		    (long long) vol.writes,
		    (long long) vol.bytes_written,
		    roughlength(vol.bytes_written, 1));
		if (vol.writes != 0)
		    printf("\t\tAverage write:\t%16lld bytes\n",
			(long long) (vol.bytes_written / vol.writes));
		printf("\t\tActive requests:\t%8d\n", vol.active);
	    } else {					    /* brief stats listing */
		printf("%-15s\t%7lld\t%15lld\t",
		    vol.name,
		    (long long) vol.reads,
		    (long long) vol.bytes_read);
		if (vol.reads != 0)
		    printf("%7lld\t",
			(long long) (vol.bytes_read / vol.reads));
		else
		    printf("\t");
		printf("%7lld\t", (long long) vol.recovered_reads);
		printf("%7lld\t%15lld\t",
		    (long long) vol.writes,
		    vol.bytes_written);
		if (vol.writes != 0)
		    printf("%7lld\n",
			(long long) (vol.bytes_written / vol.writes));
		else
		    printf("\n");
	    }
	}
	if (vol.plexes > 0) {
	    int plexno;
	    if (Verbose) {				    /* brief list */
		for (plexno = 0; plexno < vol.plexes; plexno++) {
		    get_plex_info(&plex, vol.plex[plexno]);
							    /* Just a brief summary here */
		    printf("\t\tPlex %2d:\t%s\t(%s), %s\n",
			plexno,
			plex.name,
			plex_org(plex.organization),
			roughlength(plex.length << DEV_BSHIFT, 0));
		}
	    }
	    if (recurse) {
		for (plexno = 0; plexno < vol.plexes; plexno++)
		    vinum_lpi(vol.plex[plexno], 0);	    /* first show the plexes */
		for (plexno = 0; plexno < vol.plexes; plexno++) { /* then the subdisks */
		    get_plex_info(&plex, vol.plex[plexno]);
		    if (plex.subdisks > 0) {
			int sdno;

			for (sdno = 0; sdno < plex.subdisks; sdno++) {
			    get_plex_sd_info(&sd, vol.plex[plexno], sdno);
			    vinum_lsi(sd.sdno, 0);
			}
		    }
		}
		printf("\n");
	    }
	}
    }
}

void
vinum_lv(int argc, char *argv[], char *argv0[])
{
    int i;
    int volno;
    enum objecttype type;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    if (argc == 0)
	for (volno = 0; volno < vinum_conf.volumes_allocated; volno++)
	    vinum_lvi(volno, recurse);
    else {
	for (i = 0; i < argc; i++) {
	    volno = find_object(argv[i], &type);
	    if (type == volume_object)
		vinum_lvi(volno, recurse);
	    else
		fprintf(stderr, "%s is not a volume\n", argv[i]);
	}
    }
}

void
vinum_lpi(int plexno, int recurse)
{
    get_plex_info(&plex, plexno);
    if (plex.state != plex_unallocated) {
	if (vflag) {
	    printf("Plex %s:\tSize:\t%9lld bytes (%lld MB)\n\t\tSubdisks: %8d\n",
		plex.name,
		(long long) plex.length * DEV_BSIZE,
		(long long) plex.length * DEV_BSIZE / MEGABYTE,
		plex.subdisks);
	    printf("\t\tState: %s\n\t\tOrganization: %s",
		plex_state(plex.state),
		plex_org(plex.organization));
	    if (isstriped((&plex)))
		printf("\tStripe size: %s\n", roughlength(plex.stripesize * DEV_BSIZE, 1));
	    else
		printf("\n");
	    if ((isparity((&plex)))
		&& (plex.checkblock != 0))
		printf("\t\tCheck block pointer:\t\t%s (%d%%)\n",
		    roughlength((plex.checkblock << DEV_BSHIFT) * (plex.subdisks - 1), 0),
		    (int) (((u_int64_t) (plex.checkblock * 100)) * (plex.subdisks - 1) / plex.length));
	    if (plex.volno >= 0) {
		get_volume_info(&vol, plex.volno);
		printf("\t\tPart of volume %s\n", vol.name);
	    }
	} else if (!sflag) {				    /* non-verbose list */
	    char *org = "";				    /* organization */

	    switch (plex.organization) {
	    case plex_disorg:				    /* disorganized */
		org = "??";
		break;
	    case plex_concat:				    /* concatenated plex */
		org = "C";
		break;
	    case plex_striped:				    /* striped plex */
		org = "S";
		break;
	    case plex_raid4:				    /* RAID4 plex */
		org = "R4";
		break;
	    case plex_raid5:				    /* RAID5 plex */
		org = "R5";
		break;
	    }
	    printf("P %-18s %2s State: %s\tSubdisks: %5d\tSize: %s",
		plex.name,
		org,
		plex_state(plex.state),
		plex.subdisks,
		roughlength(plex.length << DEV_BSHIFT, 0));
	}
	if (sflag) {
	    if (vflag || Verbose) {
		printf("\t\tReads:  \t%16lld\n\t\tBytes read:\t%16lld (%s)\n",
		    (long long) plex.reads,
		    (long long) plex.bytes_read,
		    roughlength(plex.bytes_read, 1));
		if (plex.reads != 0)
		    printf("\t\tAverage read:\t%16lld bytes\n",
			(long long) (plex.bytes_read / plex.reads));
		printf("\t\tWrites: \t%16lld\n\t\tBytes written:\t%16lld (%s)\n",
		    (long long) plex.writes,
		    (long long) plex.bytes_written,
		    roughlength(plex.bytes_written, 1));
		if (plex.writes != 0)
		    printf("\t\tAverage write:\t%16lld bytes\n",
			(long long) (plex.bytes_written / plex.writes));
		if (((plex.reads + plex.writes) > 0)
		    && isstriped((&plex)))
		    printf("\t\tMultiblock:\t%16lld (%d%%)\n"
			"\t\tMultistripe:\t%16lld (%d%%)\n",
			(long long) plex.multiblock,
			(int) (plex.multiblock * 100 / (plex.reads + plex.writes)),
			(long long) plex.multistripe,
			(int) (plex.multistripe * 100 / (plex.reads + plex.writes)));
		if (plex.recovered_reads)
		    printf("\t\tRecovered reads:%16lld\n",
			(long long) plex.recovered_reads);
		if (plex.degraded_writes)
		    printf("\t\tDegraded writes:%16lld\n",
			(long long) plex.degraded_writes);
		if (plex.parityless_writes)
		    printf("\t\tParityless writes:%14lld\n",
			(long long) plex.parityless_writes);
	    } else {
		printf("%-15s\t%7lld\t%15lld\t",
		    plex.name,
		    (long long) plex.reads,
		    (long long) plex.bytes_read);
		if (plex.reads != 0)
		    printf("%7lld\t",
			(long long) (plex.bytes_read / plex.reads));
		else
		    printf("\t");
		printf("%7lld\t", (long long) plex.recovered_reads);
		printf("%7lld\t%15lld\t",
		    (long long) plex.writes,
		    (long long) plex.bytes_written);
		if (plex.writes != 0)
		    printf("%7lld\t",
			(long long) (plex.bytes_written / plex.writes));
		else
		    printf("\t");
		printf("%7lld\t%7lld\n",
		    (long long) plex.multiblock,
		    (long long) plex.multistripe);
	    }
	}
	if (plex.subdisks > 0) {
	    int sdno;

	    if (Verbose) {
		printf("\n");
		for (sdno = 0; sdno < plex.subdisks; sdno++) {
		    get_plex_sd_info(&sd, plexno, sdno);
		    printf("\t\tSubdisk %d:\t%s\n\t\t  state: %s\tsize %11lld (%lld MB)\n",
			sdno,
			sd.name,
			sd_state(sd.state),
			(long long) sd.sectors * DEV_BSIZE,
			(long long) sd.sectors * DEV_BSIZE / MEGABYTE);
		    if (plex.organization == plex_concat)
			printf("\t\t\toffset %9ld (0x%lx)\n",
			    (long) sd.plexoffset,
			    (long) sd.plexoffset);
		}
	    }
	    if (recurse) {
		printf("\n");
		for (sdno = 0; sdno < plex.subdisks; sdno++) {
		    get_plex_sd_info(&sd, plexno, sdno);
		    vinum_lsi(sd.sdno, 0);
		}
	    }
	}
	printf("\n");
    }
}

void
vinum_lp(int argc, char *argv[], char *argv0[])
{
    int i;
    int plexno;
    enum objecttype type;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    if (argc == 0) {
	for (plexno = 0; plexno < vinum_conf.plexes_allocated; plexno++)
	    vinum_lpi(plexno, recurse);
    } else {
	for (i = 0; i < argc; i++) {
	    plexno = find_object(argv[i], &type);
	    if (type == plex_object)
		vinum_lpi(plexno, recurse);
	    else
		fprintf(stderr, "%s is not a plex\n", argv[i]);
	}
    }
}

void
vinum_lsi(int sdno, int recurse)
{
    long long revived;					    /* keep an eye on revive progress */
    int times;

    get_sd_info(&sd, sdno);
    if (sd.state != sd_unallocated) {
	get_drive_info(&drive, sd.driveno);

	if (vflag) {
	    printf("Subdisk %s:\n\t\tSize: %16lld bytes (%lld MB)\n\t\tState: %s\n",
		sd.name,
		(long long) sd.sectors * DEV_BSIZE,
		(long long) sd.sectors / (MEGABYTE / DEV_BSIZE),
		sd_state(sd.state));
	    if (sd.plexno >= 0) {
		get_plex_info(&plex, sd.plexno);
		printf("\t\tPlex %s", plex.name);
		printf(" at offset %lld (%s)\n",
		    (long long) sd.plexoffset * DEV_BSIZE,
		    roughlength((long long) sd.plexoffset * DEV_BSIZE, 1));
	    }
	    if (sd.state == sd_reviving) {
		if (sd.reviver == 0)
		    printf("\t\t*** Start subdisk with 'start' command ***\n");
		else {
		    printf("\t\tReviver PID:\t%d\n", sd.reviver);
		    if (kill(sd.reviver, 0) == -1) {
			if (errno == ESRCH)		    /* no process */
			    printf("\t\t*** Revive process has died ***\n");
							    /* Don't report a problem that "can't happen" */
		    } else {
			revived = sd.revived;		    /* note how far we were */

			/*
			 * Wait for up to a second until we
			 * see some progress with the revive.
			 * Do it like this so we don't have
			 * annoying delays in the listing.
			 */
			for (times = 0; times < STALLCOUNT; times++) {
			    get_sd_info(&sd, sdno);
			    if (sd.revived != revived)	    /* progress? */
				break;
			    usleep(50000);
			}
			if (times == STALLCOUNT)
			    printf("\t\t*** Revive has stalled ***\n");
		    }
		}
		printf("\t\tRevive pointer:\t\t%s (%d%%)\n",
		    roughlength(sd.revived << DEV_BSHIFT, 0),
		    (int) (((u_int64_t) (sd.revived * 100)) / sd.sectors));
		printf("\t\tRevive blocksize:\t%s\n"
		    "\t\tRevive interval:\t%10d seconds\n",
		    roughlength(sd.revive_blocksize, 0),
		    sd.revive_interval);
	    }
	    if (sd.state == sd_initializing) {
		printf("\t\tInitialize pointer:\t%s (%d%%)\n",
		    roughlength(sd.initialized << DEV_BSHIFT, 0),
		    (int) (((u_int64_t) (sd.initialized * 100)) / sd.sectors));
		printf("\t\tInitialize blocksize:\t%s\n"
		    "\t\tInitialize interval:\t%10d seconds\n",
		    roughlength(sd.init_blocksize, 0),
		    sd.init_interval);
	    }
	    if (sd.driveoffset < 0)
		printf("\t\tDrive %s (%s), no offset\n",
		    drive.label.name,
		    drive.devicename);
	    else if (drive.devicename[0] != '\0')	    /* has a name */
		printf("\t\tDrive %s (%s) at offset %lld (%s)\n",
		    drive.label.name,
		    drive.devicename,
		    (long long) (sd.driveoffset * DEV_BSIZE),
		    roughlength(sd.driveoffset * DEV_BSIZE, 1));
	    else
		printf("\t\tDrive %s (*missing*) at offset %lld (%s)\n",
		    drive.label.name,
		    (long long) (sd.driveoffset * DEV_BSIZE),
		    roughlength(sd.driveoffset * DEV_BSIZE, 1));
	} else if (!sflag) {				    /* brief listing, no stats */
	    if (sd.state == sd_reviving)
		printf("S %-21s State: R %d%%\t",
		    sd.name,
		    (int) (((u_int64_t) (sd.revived * 100)) / sd.sectors));
	    else if (sd.state == sd_initializing)
		printf("S %-21s State: I %d%%\t",
		    sd.name,
		    (int) (((u_int64_t) (sd.initialized * 100)) / sd.sectors));
	    else
		printf("S %-21s State: %s\t",
		    sd.name,
		    sd_state(sd.state));
	    printf("D: %-12s Size: %s\n",
		drive.label.name,
		roughlength(sd.sectors << DEV_BSHIFT, 0));
	    if (sd.state == sd_reviving) {
		if (sd.reviver == 0)
		    printf("\t\t\t*** Start %s with 'start' command ***\n",
			sd.name);
		else if (kill(sd.reviver, 0) == -1) {
		    if (errno == ESRCH)			    /* no process */
			printf("\t\t\t*** Revive process for %s has died ***\n",
			    sd.name);
							    /* Don't report a problem that "can't happen" */
		} else {
		    revived = sd.revived;		    /* note how far we were */

		    /*
		     * Wait for up to a second until we
		     * see some progress with the revive.
		     * Do it like this so we don't have
		     * annoying delays in the listing.
		     */
		    for (times = 0; times < STALLCOUNT; times++) {
			get_sd_info(&sd, sdno);
			if (sd.revived != revived)	    /* progress? */
			    break;
			usleep(50000);
		    }
		    if (times == STALLCOUNT)
			printf("\t\t\t*** Revive of %s has stalled ***\n",
			    sd.name);
		}
	    }
	}
	if (sflag) {
	    if (vflag || Verbose) {
		printf("\t\tReads:  \t%16lld\n\t\tBytes read:\t%16lld (%s)\n",
		    (long long) sd.reads,
		    (long long) sd.bytes_read,
		    roughlength(sd.bytes_read, 1));
		if (sd.reads != 0)
		    printf("\t\tAverage read:\t%16lld bytes\n",
			(long long) (sd.bytes_read / sd.reads));
		printf("\t\tWrites: \t%16lld\n\t\tBytes written:\t%16lld (%s)\n",
		    (long long) sd.writes,
		    (long long) sd.bytes_written,
		    roughlength(sd.bytes_written, 1));
		if (sd.writes != 0)
		    printf("\t\tAverage write:\t%16lld bytes\n",
			(long long) (sd.bytes_written / sd.writes));
	    } else {
		printf("%-15s\t%7lld\t%15lld\t",
		    sd.name,
		    (long long) sd.reads,
		    (long long) sd.bytes_read);
		if (sd.reads != 0)
		    printf("%7lld\t\t",
			(long long) (sd.bytes_read / sd.reads));
		else
		    printf("\t\t");
		printf("%7lld\t%15lld\t",
		    (long long) sd.writes,
		    (long long) sd.bytes_written);
		if (sd.writes != 0)
		    printf("%7lld\n",
			(long long) (sd.bytes_written / sd.writes));
		else
		    printf("\n");
	    }
	}
	if (recurse)
	    vinum_ldi(sd.driveno, 0);
	if (vflag)
	    printf("\n");				    /* make it more readable */
    }
}

void
vinum_ls(int argc, char *argv[], char *argv0[])
{
    int i;
    int sdno;

    /* Structures to read kernel data into */
    struct __vinum_conf vinum_conf;
    enum objecttype type;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    if (argc == 0) {
	for (sdno = 0; sdno < vinum_conf.subdisks_allocated; sdno++)
	    vinum_lsi(sdno, recurse);
    } else {						    /* specific subdisks */
	for (i = 0; i < argc; i++) {
	    sdno = find_object(argv[i], &type);
	    if (type == sd_object)
		vinum_lsi(sdno, recurse);
	    else
		fprintf(stderr, "%s is not a subdisk\n", argv[i]);
	}
    }
}


/* List the complete configuration.

 * XXX Change this to specific lists */
void
listconfig()
{
    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    printf("%d drives:\n", vinum_conf.drives_used);
    if (vinum_conf.drives_used > 0) {
	vinum_ld(0, NULL, NULL);
	printf("\n");
    }
    printf("%d volumes:\n", vinum_conf.volumes_used);
    if (vinum_conf.volumes_used > 0) {
	vinum_lv(0, NULL, NULL);
	printf("\n");
    }
    printf("%d plexes:\n", vinum_conf.plexes_used);
    if (vinum_conf.plexes_used > 0) {
	vinum_lp(0, NULL, NULL);
	printf("\n");
    }
    printf("%d subdisks:\n", vinum_conf.subdisks_used);
    if (vinum_conf.subdisks_used > 0)
	vinum_ls(0, NULL, NULL);
}

/* Convert a timeval to Tue Oct 13 13:54:14.0434324
 * Return pointer to text */
char *
timetext(struct timeval *time)
{
    static char text[30];
    time_t t;						    /* to keep Bruce happy */

    t = time->tv_sec;
    strcpy(text, ctime(&t));				    /* to the second */
    sprintf(&text[19], ".%06ld", time->tv_usec);	    /* and the microseconds */
    return &text[11];
}

void
vinum_info(int argc, char *argv[], char *argv0[])
{
    struct meminfo meminfo;
    struct mc malloced;
    int i;
#if VINUMDEBUG
    struct rqinfo rq;
#endif

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    printf("Flags: 0x%x\n", vinum_conf.flags);
    if (ioctl(superdev, VINUM_MEMINFO, &meminfo) < 0) {
	perror("Can't get information");
	return;
    }
    printf("Total of %d blocks malloced, total memory: %d\nMaximum allocs: %8d, malloc table at 0x%08x\n",
	meminfo.mallocs,
	meminfo.total_malloced,
	meminfo.highwater,
	(int) meminfo.malloced);

    printf("%d requests active, maximum %d active\n",
	vinum_conf.active,
	vinum_conf.maxactive);
    if (vflag && (!Verbose))
	for (i = 0; i < meminfo.mallocs; i++) {
	    malloced.seq = i;
	    if (ioctl(superdev, VINUM_MALLOCINFO, &malloced) < 0) {
		perror("Can't get information");
		return;
	    }
	    if (!(i & 63))
		printf("Block\tSequence\t  size\t  address\t  line\t\tfile\n\n");
	    printf("%6d\t%6d\t\t%6d\t0x%08x\t%6d\t\t%s\n",
		i,
		malloced.seq,
		malloced.size,
		(int) malloced.address,
		malloced.line,
		(char *) &malloced.file);
	}
#if VINUMDEBUG
    if (Verbose) {
	printf("\nTime\t\t Event\t     Buf\tDev\t  Offset\tBytes\tSD\tSDoff\tDoffset\tGoffset\n\n");
	for (i = RQINFO_SIZE - 1; i >= 0; i--) {	    /* go through the request list in order */
	    *((int *) &rq) = i;
	    if (ioctl(superdev, VINUM_RQINFO, &rq) < 0) {
		perror("Can't get information");
		return;
	    }
	    /* Compress devminor into something printable. */
	    rq.devminor = (rq.devminor & 0xff)
		| ((rq.devminor & 0xfff0000) >> 8);
	    switch (rq.type) {
	    case loginfo_unused:			    /* never been used */
		break;

	    case loginfo_user_bp:			    /* this is the bp when strategy is called */
		printf("%s %dVS %s %p\t%d.%-6d 0x%-9x\t%ld\n",
		    timetext(&rq.timestamp),
		    rq.type,
		    rq.info.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount);
		break;

	    case loginfo_sdiol:				    /* subdisk I/O launch */
	    case loginfo_user_bpl:			    /* and this is the bp at launch time */
		printf("%s %dLR %s %p\t%d.%-6d 0x%-9x\t%ld\n",
		    timetext(&rq.timestamp),
		    rq.type,
		    rq.info.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount);
		break;

	    case loginfo_rqe:				    /* user RQE */
		printf("%s 3RQ %s %p\t%d.%-6d 0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_iodone:			    /* iodone called */
		printf("%s 4DN %s %p\t%d.%-6d 0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_raid5_data:			    /* RAID-5 write data block */
		printf("%s 5RD %s %p\t%d.%-6d 0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_raid5_parity:			    /* RAID-5 write parity block */
		printf("%s 6RP %s %p\t%d.%-6d 0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.devmajor,
		    rq.devminor,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_sdio:				    /* subdisk I/O */
		printf("%s %dVS %s %p\t\t  0x%-9x\t%ld\t%d\n",
		    timetext(&rq.timestamp),
		    rq.type,
		    rq.info.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount,
		    rq.devminor);
		break;

	    case loginfo_sdiodone:			    /* subdisk I/O done */
		printf("%s %dSD %s %p\t\t  0x%-9x\t%ld\t%d\n",
		    timetext(&rq.timestamp),
		    rq.type,
		    rq.info.b.b_iocmd == BIO_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount,
		    rq.devminor);
		break;

	    case loginfo_lockwait:
		printf("%s Lockwait  %p\t  0x%x\n",
		    timetext(&rq.timestamp),
		    rq.bp,
		    rq.info.lockinfo.stripe);
		break;

	    case loginfo_lock:
		printf("%s Lock      %p\t  0x%x\n",
		    timetext(&rq.timestamp),
		    rq.bp,
		    rq.info.lockinfo.stripe);
		break;

	    case loginfo_unlock:
		printf("%s Unlock\t  %p\t  0x%x\n",
		    timetext(&rq.timestamp),
		    rq.bp,
		    rq.info.lockinfo.stripe);
		break;
	    }
	}
    }
#endif
}

/*
 * Print config file to a file.  This is a userland version
 * of kernel format_config
 */
void
vinum_printconfig(int argc, char *argv[], char *argv0[])
{
    FILE *of;

    if (argc > 1) {
	fprintf(stderr, "Usage: \tprintconfig [<outfile>]\n");
	return;
    } else if (argc == 1)
	of = fopen(argv[0], "w");
    else
	of = stdout;
    if (of == NULL) {
	fprintf(stderr, "Can't open %s: %s\n", argv[0], strerror(errno));
	return;
    }
    printconfig(of, "");
    if (argc == 1)
	fclose(of);
}

/*
 * The guts of printconfig.  This is called from
 * vinum_printconfig and from vinum_create when
 * called without an argument, in order to give
 * the user something to edit.
 */
void
printconfig(FILE * of, char *comment)
{
    struct utsname uname_s;
    time_t now;
    int i;
    struct _volume vol;
    struct _plex plex;
    struct _sd sd;
    struct _drive drive;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    uname(&uname_s);					    /* get our system name */
    time(&now);						    /* and the current time */
    fprintf(of,
	"# Vinum configuration of %s, saved at %s",
	uname_s.nodename,
	ctime(&now));					    /* say who did it */

    if (comment[0] != 0)				    /* abuse this for commented version */
	fprintf(of, "# Current configuration:\n");
    for (i = 0; i < vinum_conf.drives_allocated; i++) {
	get_drive_info(&drive, i);
	if (drive.state != drive_unallocated) {
	    fprintf(of,
		"%sdrive %s device %s\n",
		comment,
		drive.label.name,
		drive.devicename);
	}
    }

    for (i = 0; i < vinum_conf.volumes_allocated; i++) {
	get_volume_info(&vol, i);
	if (vol.state != volume_unallocated) {
	    if (vol.preferred_plex >= 0)		    /* preferences, */
		fprintf(of,
		    "%svolume %s readpol prefer %s\n",
		    comment,
		    vol.name,
		    vinum_conf.plex[vol.preferred_plex].name);
	    else					    /* default round-robin */
		fprintf(of, "%svolume %s\n", comment, vol.name);
	}
    }

    /* Then the plex configuration */
    for (i = 0; i < vinum_conf.plexes_allocated; i++) {
	get_plex_info(&plex, i);
	if (plex.state != plex_unallocated) {
	    fprintf(of, "%splex name %s org %s ",
		comment,
		plex.name,
		plex_org(plex.organization));
	    if (isstriped((&plex)))
		fprintf(of, "%ds ", (int) plex.stripesize);
	    if (plex.volno >= 0) {			    /* we have a volume */
		get_volume_info(&vol, plex.volno);
		fprintf(of, "vol %s ", vol.name);
	    } else
		fprintf(of, "detached ");
	    fprintf(of, "\n");
	}
    }

    /* And finally the subdisk configuration */
    for (i = 0; i < vinum_conf.subdisks_allocated; i++) {
	get_sd_info(&sd, i);
	if (sd.state != sd_unallocated) {
	    get_drive_info(&drive, sd.driveno);
	    if (sd.plexno >= 0) {
		get_plex_info(&plex, sd.plexno);
		fprintf(of,
		    "%ssd name %s drive %s plex %s len %llds driveoffset %llds plexoffset %llds\n",
		    comment,
		    sd.name,
		    drive.label.name,
		    plex.name,
		    (long long) sd.sectors,
		    (long long) sd.driveoffset,
		    (long long) sd.plexoffset);
	    } else
		fprintf(of,
		    "%ssd name %s drive %s detached len %llds driveoffset %llds\n",
		    comment,
		    sd.name,
		    drive.label.name,
		    (long long) sd.sectors,
		    (long long) sd.driveoffset);
	}
    }
}

void
list_defective_objects()
{
    int o;						    /* object */
    int heading_needed = 1;

    if (ioctl(superdev, VINUM_GETCONFIG, &vinum_conf) < 0) {
	perror("Can't get vinum config");
	return;
    }
    for (o = 0; o < vinum_conf.drives_allocated; o++) {
	get_drive_info(&drive, o);
	if ((drive.state != drive_unallocated)		    /* drive exists */
	&&(drive.state != drive_up)) {			    /* but it's not up */
	    if (heading_needed) {
		printf("Warning: defective objects\n\n");
		heading_needed = 0;
	    }
	    vinum_ldi(o, 0);				    /* print info */
	}
    }

    for (o = 0; o < vinum_conf.volumes_allocated; o++) {
	get_volume_info(&vol, o);
	if ((vol.state != volume_unallocated)		    /* volume exists */
	&&(vol.state != volume_up)) {			    /* but it's not up */
	    if (heading_needed) {
		printf("Warning: defective objects\n\n");
		heading_needed = 0;
	    }
	    vinum_lvi(o, 0);				    /* print info */
	}
    }

    for (o = 0; o < vinum_conf.plexes_allocated; o++) {
	get_plex_info(&plex, o);
	if ((plex.state != plex_unallocated)		    /* plex exists */
	&&(plex.state != plex_up)) {			    /* but it's not up */
	    if (heading_needed) {
		printf("Warning: defective objects\n\n");
		heading_needed = 0;
	    }
	    vinum_lpi(o, 0);				    /* print info */
	}
    }

    for (o = 0; o < vinum_conf.subdisks_allocated; o++) {
	get_sd_info(&sd, o);
	if ((sd.state != sd_unallocated)		    /* sd exists */
	&&(sd.state != sd_up)) {			    /* but it's not up */
	    if (heading_needed) {
		printf("Warning: defective objects\n\n");
		heading_needed = 0;
	    }
	    vinum_lsi(o, 0);				    /* print info */
	}
    }
}

/* Dump config from specified disk drives */
void
vinum_dumpconfig(int argc, char *argv[], char *argv0[])
{
    int i;

    if (argc == 0) {					    /* start everything */
	int devs = getnumdevs();
	struct statinfo statinfo;
	char *namelist;
	char *enamelist;				    /* end of name list */
	int i;
	char **token;					    /* list of tokens */
	int tokens;					    /* and their number */

	bzero(&statinfo, sizeof(struct statinfo));
	statinfo.dinfo = malloc(devs * sizeof(struct statinfo));
	namelist = malloc(devs * (DEVSTAT_NAME_LEN + 8));
	token = malloc((devs + 1) * sizeof(char *));
	if ((statinfo.dinfo == NULL) || (namelist == NULL) || (token == NULL)) {
	    fprintf(stderr, "Can't allocate memory for drive list\n");
	    return;
	}
	bzero(statinfo.dinfo, sizeof(struct devinfo));

	tokens = 0;					    /* no tokens yet */
	if (getdevs(&statinfo) < 0) {			    /* find out what devices we have */
	    perror("Can't get device list");
	    return;
	}
	namelist[0] = '\0';				    /* start with empty namelist */
	enamelist = namelist;				    /* point to the end of the list */

	for (i = 0; i < devs; i++) {
	    struct devstat *stat = &statinfo.dinfo->devices[i];

	    if (((stat->device_type & DEVSTAT_TYPE_MASK) == DEVSTAT_TYPE_DIRECT) /* disk device */
	    &&((stat->device_type & DEVSTAT_TYPE_PASS) == 0) /* and not passthrough */
	    &&((stat->device_name[0] != '\0'))) {	    /* and it has a name */
		sprintf(enamelist, "/dev/%s%d", stat->device_name, stat->unit_number);
		token[tokens] = enamelist;		    /* point to it */
		tokens++;				    /* one more token */
		enamelist = &enamelist[strlen(enamelist) + 1]; /* and start beyond the end */
	    }
	}
	free(statinfo.dinfo);				    /* don't need the list any more */
	for (i = 0; i < tokens; i++)
	    dumpconfig(token[i]);
	free(namelist);
	free(token);
    } else {						    /* list specified drives */
	for (i = 0; i < argc; i++)
	    dumpconfig(argv[i]);
    }
}

#define DEVLEN 5
void
dumpconfig(char *part)
{
    char partname[MAXPATHLEN];
    char *partid;
    char partition;					    /* UNIX partition */
    int slice;
    int founddrive;					    /* flag when we find a vinum drive */
    struct disklabel label;				    /* label of this drive */
    int driveno;					    /* fd of drive */
    int found;
    u_int64_t drivelength;

    if (memcmp(part, "/dev/", DEVLEN) == 0)		    /* starts with /dev */
	memcpy(partname, part, MAXPATHLEN);
    else {						    /* prepend */
	strcpy(partname, "/dev/");
	strncat(&partname[DEVLEN], part, MAXPATHLEN - DEVLEN);
    }
    partid = &partname[strlen(partname)];
    founddrive = 0;					    /* no vinum drive found yet on this spindle */
    /* first try the partition table */
    for (slice = 1; slice < 5; slice++) {
	sprintf(partid, "s%dc", slice);			    /* c partition */
	driveno = open(partname, O_RDONLY);
	if (driveno < 0) {
	    if (errno != ENOENT)
		fprintf(stderr, "Can't open %s: %s (%d)\n", partname, strerror(errno), errno);
	    continue;
	}
	if (ioctl(driveno, DIOCGDINFO, &label) < 0) {
	    if ((errno != EINVAL) || vflag)
		fprintf(stderr, "Can't get label from %s: %s (%d)\n", partname, strerror(errno), errno);
	    continue;
	}
	for (partition = 'a'; partition < 'i'; partition++) {
	    if ((partition != 'c')			    /* it's not the c partition */
	    &&((label.d_partitions[partition - 'a'].p_fstype == FS_VINUM) /* and it's a Vinum partition */
	    ||Verbose)) {				    /* or we're just plain curious */
		sprintf(partid, "s%d%c", slice, partition);
		found = check_drive(partname);		    /* try to open it */
		founddrive |= found;			    /* and note if we were successful at all */
		if (label.d_partitions[partition - 'a'].p_fstype == FS_VINUM) {	/* it's a Vinum partition */
		    drivelength = ((u_int64_t) label.d_partitions[partition - 'a'].p_size) * DEV_BSIZE;
		    printf("Drive %s: %s (%lld bytes)\n",
			partname,
			roughlength(drivelength, 1),
			drivelength);
		    if ((!found) && vflag)		    /* we're talkative */
			printf("*** no configuration found ***\n");
		}
	    }
	}
    }
    if (founddrive == 0) {				    /* didn't find anything, */
	sprintf(partid, "c");				    /* c partition */
	driveno = open(partname, O_RDONLY);
	if (driveno < 0) {
	    if (errno != ENOENT)
		fprintf(stderr, "Can't open %s: %s (%d)\n", partname, strerror(errno), errno);
	    return;
	}
	if (ioctl(driveno, DIOCGDINFO, &label) < 0) {
	    fprintf(stderr, "Can't get label from %s: %s (%d)\n", partname, strerror(errno), errno);
	    return;
	}
	for (partition = 'a'; partition < 'i'; partition++) { /* try the compatibility partition */
	    if ((partition != 'c')			    /* it's not the c partition */
	    &&((label.d_partitions[partition - 'a'].p_fstype == FS_VINUM) /* and it's a Vinum partition */
	    ||Verbose)) {				    /* or we're just plain curious */
		sprintf(partid, "%c", partition);
		found = check_drive(partname);		    /* try to open it */
		founddrive |= found;			    /* and note if we were successful at all */
		if (label.d_partitions[partition - 'a'].p_fstype == FS_VINUM) {	/* it's a Vinum partition */
		    drivelength = ((u_int64_t) label.d_partitions[partition - 'a'].p_size) * DEV_BSIZE;
		    printf("Drive %s: %s (%lld bytes)\n",
			partname,
			roughlength(drivelength, 1),
			drivelength);
		    if ((!found) && vflag)		    /* we're talkative */
			printf("*** no configuration found ***\n");
		}
	    }
	}
    }
}

/*
 * Check a drive for a Vinum header.  If found,
 * print configuration information from the drive.
 *
 * Return 1 if Vinum config found.
 */
int
check_drive(char *devicename)
{
    int fd;
    char vinumlabel[DEV_BSIZE];				    /* one sector for label */
    struct vinum_hdr *hdr = (struct vinum_hdr *) vinumlabel; /* with this structure */
    char *config_text;					    /* read the config info from disk into here */
    time_t t;

    fd = open(devicename, O_RDONLY);
    if (fd >= 0) {
	if (lseek(fd, VINUM_LABEL_OFFSET, SEEK_SET) < 0) {
	    fprintf(stderr,
		"Can't seek label for %s: %s (%d)\n",
		devicename,
		strerror(errno),
		errno);
	    close(fd);
	    return 0;
	}
	if (read(fd, vinumlabel, DEV_BSIZE) != DEV_BSIZE) {
	    if (errno != EINVAL)
		fprintf(stderr,
		    "Can't read label from %s: %s (%d)\n",
		    devicename,
		    strerror(errno),
		    errno);
	    close(fd);
	    return 0;
	}
	if ((hdr->magic == VINUM_MAGIC)
	    || (vflag && (hdr->magic == VINUM_NOMAGIC))) {
	    printf("Drive %s:\tDevice %s\n",
		hdr->label.name,
		devicename);
	    if (hdr->magic == VINUM_NOMAGIC)
		printf("*** Drive has been obliterated ***\n");
	    t = hdr->label.date_of_birth.tv_sec;
	    printf("\t\tCreated on %s at %s",
		hdr->label.sysname,
		ctime(&t));
	    t = hdr->label.last_update.tv_sec;
	    printf("\t\tConfig last updated %s",	    /* care: \n at end */
		ctime(&t));
	    printf("\t\tSize: %16lld bytes (%lld MB)\n",
		(long long) hdr->label.drive_size,	    /* bytes used */
		(long long) (hdr->label.drive_size / MEGABYTE));
	    config_text = (char *) malloc(MAXCONFIG);
	    if (config_text == NULL)
		fprintf(stderr, "Can't allocate memory\n");
	    else {
		if (read(fd, config_text, MAXCONFIG) != MAXCONFIG)
		    fprintf(stderr,
			"Can't read config from %s: %s (%d)\n",
			devicename,
			strerror(errno),
			errno);
		else
		    puts(config_text);
		free(config_text);
	    }
	}
	close(fd);
	return 1;
    }
    return 0;
}

int
checkupdates()
{
    int options;

    if (ioctl(superdev, VINUM_GETDAEMON, &options) < 0)
	fprintf(stderr, "Can't get daemon options: %s (%d)\n", strerror(errno), errno);
    if (options & daemon_noupdate) {
	fprintf(stderr, "*** Warning: configuration updates are disabled. ***\n");
	return 1;
    } else
	return 0;
}

/* Local Variables: */
/* fill-column: 50 */
/* End: */
