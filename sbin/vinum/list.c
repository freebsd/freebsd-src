/*      list.c: vinum interface program, list routines
 */
/*-
 * Copyright (c) 1997, 1998
 *	Nan Yang Computer Services Limited.  All rights reserved.
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
 * $Id: list.c,v 1.18 1999/03/10 09:26:46 grog Exp grog $
 */

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <netdb.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/utsname.h>
#include <dev/vinum/vinumhdr.h>
#include "vext.h"
#include <dev/vinum/request.h>
/* Take a size in sectors and return a pointer to a
 * string which represents the size best.
 * If lj is != 0, return left justified, otherwise
 * in a fixed 10 character field suitable for
 * columnar printing.
 *
 * Note this uses a static string: it's only intended to
 * be used immediately for printing */
char *
roughlength(long long bytes, int lj)
{
    static char description[16];

    if (bytes > (long long) MEGABYTE * 10000)		    /* gigabytes */
	sprintf(description, lj ? "%d GB" : "%10d GB", bytes / GIGABYTE);
    else if (bytes > KILOBYTE * 10000)			    /* megabytes */
	sprintf(description, lj ? "%d MB" : "%10d MB", bytes / MEGABYTE);
    else if (bytes > 10000)				    /* kilobytes */
	sprintf(description, lj ? "%d kB" : "%10d kB", bytes / KILOBYTE);
    else						    /* bytes */
	sprintf(description, lj ? "%d  B" : "%10d  B", bytes);
    return description;
}

void 
vinum_list(int argc, char *argv[], char *argv0[])
{
    int object;
    int i;
    enum objecttype type;

    if (stats & (!verbose))				    /* just summary stats, */
	printf("Object\t\t  Reads\t\tBytes\tAverage\tRecover\t Writes\t\tBytes\tAverage\t  Mblock  Mstripe\n\n");
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

    get_drive_info(&drive, driveno);
    if (drive.state != drive_unallocated) {
	if (verbose) {
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
	    printf("\t\tSize: %16qd bytes (%qd MB)\n\t\tUsed: %16qd bytes (%qd MB)\n"
		"\t\tAvailable: %11qd bytes (%d MB)\n",
		drive.label.drive_size,			    /* bytes used */
		(drive.label.drive_size / MEGABYTE),
		drive.label.drive_size - drive.sectors_available * DEV_BSIZE,
		(drive.label.drive_size - drive.sectors_available * DEV_BSIZE) / MEGABYTE,
		drive.sectors_available * DEV_BSIZE,
		(int) (drive.sectors_available * DEV_BSIZE / MEGABYTE));
	    printf("\t\tState: %s\n", drive_state(drive.state));
	    if (drive.lasterror != 0)
		printf("\t\tLast error: %s\n", strerror(drive.lasterror));
	    else
		printf("\t\tLast error: none\n");
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
		    printf("\t\t%9qd\t%9ld\n", freelist.offset, freelist.sectors);
		}
	    }
	} else if (!stats) {
	    printf("D %-21s State: %s\tDevice %s\tAvail: %qd/%qd MB",
		drive.label.name,
		drive_state(drive.state),
		drive.devicename,
		drive.sectors_available * DEV_BSIZE / MEGABYTE,
		(drive.label.drive_size / MEGABYTE));
	    if (drive.label.drive_size == 0)
		printf("\n");				    /* can't print percentages */
	    else
		printf(" (%d%%)\n",
		    (int) ((drive.sectors_available * 100 * DEV_BSIZE) / drive.label.drive_size));
	}
	if (stats) {
	    if (verbose || Verbose) {
		printf("\t\tReads:  \t%16qd\n\t\tBytes read:\t%16qd (%s)\n",
		    drive.reads,
		    drive.bytes_read,
		    roughlength(drive.bytes_read, 1));
		if (drive.reads != 0)
		    printf("\t\tAverage read:\t%16qd bytes\n", drive.bytes_read / drive.reads);
		printf("\t\tWrites: \t%16qd\n\t\tBytes written:\t%16qd (%s)\n",
		    drive.writes,
		    drive.bytes_written,
		    roughlength(drive.bytes_written, 1));
		if (drive.writes != 0)
		    printf("\t\tAverage write:\t%16qd bytes\n",
			drive.bytes_written / drive.writes);
	    } else {					    /* non-verbose stats */
		printf("%-15s\t%7qd\t%15qd\t", drive.label.name, drive.reads, drive.bytes_read);
		if (drive.reads != 0)
		    printf("%7qd\t\t", drive.bytes_read / drive.reads);
		else
		    printf("\t\t");
		printf("%7qd\t%15qd\t", drive.writes, drive.bytes_written);
		if (drive.writes != 0)
		    printf("%7qd\n", drive.bytes_written / drive.writes);
		else
		    printf("\n");
	    }
	}
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
	if (verbose) {
	    printf("Volume %s:\tSize: %qd bytes (%qd MB)\n"
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
	} else if (!stats)				    /* brief */
	    printf("V %-21s State: %s\tPlexes: %7d\tSize: %s\n",
		vol.name,
		volume_state(vol.state),
		vol.plexes,
		roughlength(vol.size << DEV_BSHIFT, 0));
	if (stats) {
	    if (verbose || Verbose) {
		printf("\t\tReads:  \t%16qd\n\t\tRecovered:\t%16qd\n\t\tBytes read:\t%16qd (%s)\n",
		    vol.reads,
		    vol.recovered_reads,
		    vol.bytes_read,
		    roughlength(vol.bytes_read, 1));
		if (vol.reads != 0)
		    printf("\t\tAverage read:\t%16qd bytes\n", vol.bytes_read / vol.reads);
		printf("\t\tWrites: \t%16qd\n\t\tBytes written:\t%16qd (%s)\n",
		    vol.writes,
		    vol.bytes_written,
		    roughlength(vol.bytes_written, 1));
		if (vol.writes != 0)
		    printf("\t\tAverage write:\t%16qd bytes\n",
			vol.bytes_written / vol.writes);
		printf("\t\tActive requests:\t%8d\n", vol.active);
	    } else {					    /* brief stats listing */
		printf("%-15s\t%7qd\t%15qd\t", vol.name, vol.reads, vol.bytes_read);
		if (vol.reads != 0)
		    printf("%7qd\t", vol.bytes_read / vol.reads);
		else
		    printf("\t");
		printf("%7qd\t", vol.recovered_reads);
		printf("%7qd\t%15qd\t", vol.writes, vol.bytes_written);
		if (vol.writes != 0)
		    printf("%7qd\n", vol.bytes_written / vol.writes);
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
		if (verbose == 0)			    /* not verbose, but recursive */
		    printf("\n");			    /* leave a line at the end of each hierarchy */
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
	if (verbose) {
	    printf("Plex %s:\tSize:\t%9qd bytes (%qd MB)\n\t\tSubdisks: %8d\n",
		plex.name,
		(long long) plex.length * DEV_BSIZE,
		(long long) plex.length * DEV_BSIZE / MEGABYTE,
		plex.subdisks);
	    printf("\t\tState: %s\n\t\tOrganization: %s",
		plex_state(plex.state),
		plex_org(plex.organization));
	    if ((plex.organization == plex_striped)
		|| (plex.organization == plex_raid5))
		printf("\tStripe size: %s\n", roughlength(plex.stripesize * DEV_BSIZE, 1));
	    else
		printf("\n");
	    if (plex.volno >= 0) {
		get_volume_info(&vol, plex.volno);
		printf("\t\tPart of volume %s\n", vol.name);
	    }
	} else if (!stats) {
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
	    case plex_raid5:				    /* RAID5 plex */
		org = "R5";
		break;
	    }
	    printf("P %-18s %2s State: %s\tSubdisks: %5d\tSize: %s\n",
		plex.name,
		org,
		plex_state(plex.state),
		plex.subdisks,
		roughlength(plex.length << DEV_BSHIFT, 0));
	}
	if (stats) {
	    if (verbose || Verbose) {
		printf("\t\tReads:  \t%16qd\n\t\tBytes read:\t%16qd (%s)\n",
		    plex.reads,
		    plex.bytes_read,
		    roughlength(plex.bytes_read, 1));
		if (plex.reads != 0)
		    printf("\t\tAverage read:\t%16qd bytes\n", plex.bytes_read / plex.reads);
		printf("\t\tWrites: \t%16qd\n\t\tBytes written:\t%16qd (%s)\n",
		    plex.writes,
		    plex.bytes_written,
		    roughlength(plex.bytes_written, 1));
		if (plex.writes != 0)
		    printf("\t\tAverage write:\t%16qd bytes\n",
			plex.bytes_written / plex.writes);
		if (((plex.reads + plex.writes) > 0)
		    && ((plex.organization == plex_striped)
			|| (plex.organization == plex_raid5)))
		    printf("\t\tMultiblock:\t%16qd (%d%%)\n"
			"\t\tMultistripe:\t%16qd (%d%%)\n",
			plex.multiblock,
			(int) (plex.multiblock * 100 / (plex.reads + plex.writes)),
			plex.multistripe,
			(int) (plex.multistripe * 100 / (plex.reads + plex.writes)));
	    } else {
		printf("%-15s\t%7qd\t%15qd\t", plex.name, plex.reads, plex.bytes_read);
		if (plex.reads != 0)
		    printf("%7qd\t\t", plex.bytes_read / plex.reads);
		else
		    printf("\t\t");
		printf("%7qd\t%15qd\t", plex.writes, plex.bytes_written);
		if (plex.writes != 0)
		    printf("%7qd\t", plex.bytes_written / plex.writes);
		else
		    printf("\t");
		printf("%7qd\t%7qd\n", plex.multiblock, plex.multistripe);
	    }
	}
	if (plex.subdisks > 0) {
	    int sdno;

	    if (Verbose) {
		printf("\n");
		for (sdno = 0; sdno < plex.subdisks; sdno++) {
		    get_plex_sd_info(&sd, plexno, sdno);
		    printf("\t\tSubdisk %d:\t%s\n\t\t  state: %s\tsize %11qd (%qd MB)\n",
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
	    if (recurse)
		for (sdno = 0; sdno < plex.subdisks; sdno++) {
		    get_plex_sd_info(&sd, plexno, sdno);
		    vinum_lsi(sd.sdno, 0);
		}
	}
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
    get_sd_info(&sd, sdno);
    if (sd.state != sd_unallocated) {
	if (verbose) {
	    printf("Subdisk %s:\n\t\tSize: %16qd bytes (%qd MB)\n\t\tState: %s\n",
		sd.name,
		(long long) sd.sectors * DEV_BSIZE,
		(long long) sd.sectors / (MEGABYTE / DEV_BSIZE),
		sd_state(sd.state));
	    if (sd.plexno >= 0) {
		get_plex_info(&plex, sd.plexno);
		printf("\t\tPlex %s", plex.name);
		printf(" at offset %qd (%s)\n",
		    (long long) sd.plexoffset * DEV_BSIZE,
		    roughlength((long long) sd.plexoffset * DEV_BSIZE, 1));
	    }
	    if (sd.state == sd_reviving) {
		printf("\t\tRevive pointer:\t\t%s (%d%%)\n",
		    roughlength(sd.revived << DEV_BSHIFT, 0),
		    (int) (((u_int64_t) (sd.revived * 100)) / sd.sectors));
		printf("\t\tRevive blocksize:\t%s\n"
		    "\t\tRevive interval:\t%10d seconds\n",
		    roughlength(sd.revive_blocksize, 0),
		    sd.revive_interval);
	    }
	    get_drive_info(&drive, sd.driveno);
	    if (sd.driveoffset < 0)
		printf("\t\tDrive %s (%s), no offset\n",
		    drive.label.name,
		    drive.devicename);
	    else
		printf("\t\tDrive %s (%s) at offset %qd (%s)\n",
		    drive.label.name,
		    drive.devicename,
		    sd.driveoffset * DEV_BSIZE,
		    roughlength(sd.driveoffset * DEV_BSIZE, 1));
	} else if (!stats) {				    /* brief listing, no stats */
	    printf("S %-21s State: %s\tPO: %s ",
		sd.name,
		sd_state(sd.state),
		&(roughlength(sd.plexoffset << DEV_BSHIFT, 0))[2]); /* what a kludge! */
	    printf("Size: %s\n",
		roughlength(sd.sectors << DEV_BSHIFT, 0));
	}
	if (stats) {
	    if (verbose || Verbose) {
		printf("\t\tReads:  \t%16qd\n\t\tBytes read:\t%16qd (%s)\n",
		    sd.reads,
		    sd.bytes_read,
		    roughlength(sd.bytes_read, 1));
		if (sd.reads != 0)
		    printf("\t\tAverage read:\t%16qd bytes\n", sd.bytes_read / sd.reads);
		printf("\t\tWrites: \t%16qd\n\t\tBytes written:\t%16qd (%s)\n",
		    sd.writes,
		    sd.bytes_written,
		    roughlength(sd.bytes_written, 1));
		if (sd.writes != 0)
		    printf("\t\tAverage write:\t%16qd bytes\n",
			sd.bytes_written / sd.writes);
	    } else {
		printf("%-15s\t%7qd\t%15qd\t", sd.name, sd.reads, sd.bytes_read);
		if (sd.reads != 0)
		    printf("%7qd\t\t", sd.bytes_read / sd.reads);
		else
		    printf("\t\t");
		printf("%7qd\t%15qd\t", sd.writes, sd.bytes_written);
		if (sd.writes != 0)
		    printf("%7qd\n", sd.bytes_written / sd.writes);
		else
		    printf("\n");
	    }
	}
	if (recurse)
	    vinum_ldi(sd.driveno, recurse);
	if (verbose)
	    printf("\n");				    /* make it more readable */
    }
}

void 
vinum_ls(int argc, char *argv[], char *argv0[])
{
    int i;
    int sdno;

    /* Structures to read kernel data into */
    struct _vinum_conf vinum_conf;
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
    if (verbose || (!stats)) {
	printf("Configuration summary\n\n");
	printf("Drives:\t\t%d (%d configured)\n", vinum_conf.drives_used, vinum_conf.drives_allocated);
	printf("Volumes:\t%d (%d configured)\n", vinum_conf.volumes_used, vinum_conf.volumes_allocated);
	printf("Plexes:\t\t%d (%d configured)\n", vinum_conf.plexes_used, vinum_conf.plexes_allocated);
	printf("Subdisks:\t%d (%d configured)\n\n", vinum_conf.subdisks_used, vinum_conf.subdisks_allocated);
    }
    vinum_ld(0, NULL, NULL);
    printf("\n");
    vinum_lv(0, NULL, NULL);
    printf("\n");
    vinum_lp(0, NULL, NULL);
    printf("\n");
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

    if (verbose && (!Verbose))
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
	printf("\nTime\t\t Event\t     Buf\tDev\tOffset\t\tBytes\tSD\tSDoff\tDoffset\tGoffset\n\n");
	for (i = RQINFO_SIZE - 1; i >= 0; i--) {	    /* go through the request list in order */
	    *((int *) &rq) = i;
	    if (ioctl(superdev, VINUM_RQINFO, &rq) < 0) {
		perror("Can't get information");
		return;
	    }
	    switch (rq.type) {
	    case loginfo_unused:			    /* never been used */
		break;

	    case loginfo_user_bp:			    /* this is the bp when strategy is called */
		printf("%s 1VS %s %p\t0x%x\t0x%-9x\t%ld\n",
		    timetext(&rq.timestamp),
		    rq.info.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.b.b_dev,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount);
		break;

	    case loginfo_user_bpl:			    /* and this is the bp at launch time */
		printf("%s 2LR %s %p\t0x%x\t0x%-9x\t%ld\n",
		    timetext(&rq.timestamp),
		    rq.info.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.b.b_dev,
		    rq.info.b.b_blkno,
		    rq.info.b.b_bcount);
		break;

	    case loginfo_rqe:				    /* user RQE */
		printf("%s 3RQ %s %p\t0x%x\t0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.rqe.b.b_dev,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_iodone:			    /* iodone called */
		printf("%s 4DN %s %p\t0x%x\t0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.rqe.b.b_dev,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_raid5_data:			    /* RAID-5 write data block */
		printf("%s 5RD %s %p\t0x%x\t0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.rqe.b.b_dev,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
		break;

	    case loginfo_raid5_parity:			    /* RAID-5 write parity block */
		printf("%s 6RP %s %p\t0x%x\t0x%-9x\t%ld\t%d\t%x\t%x\t%x\n",
		    timetext(&rq.timestamp),
		    rq.info.rqe.b.b_flags & B_READ ? "Read " : "Write",
		    rq.bp,
		    rq.info.rqe.b.b_dev,
		    rq.info.rqe.b.b_blkno,
		    rq.info.rqe.b.b_bcount,
		    rq.info.rqe.sdno,
		    rq.info.rqe.sdoffset,
		    rq.info.rqe.dataoffset,
		    rq.info.rqe.groupoffset);
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
    struct volume vol;
    struct plex plex;
    struct sd sd;
    struct drive drive;

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
	    if ((plex.organization == plex_striped)
		|| (plex.organization == plex_raid5)) {
		fprintf(of, "%db ", (int) plex.stripesize);
	    }
	    if (plex.volno >= 0) {			    /* we have a volume */
		get_volume_info(&vol, plex.volno);
		fprintf(of, "vol %s ", vol.name);
	    }
	    fprintf(of, "\n");
	}
    }

    /* And finally the subdisk configuration */
    for (i = 0; i < vinum_conf.subdisks_allocated; i++) {
	get_sd_info(&sd, i);
	if (sd.state != sd_unallocated) {
	    get_drive_info(&drive, sd.driveno);
	    get_plex_info(&plex, sd.plexno);
	    fprintf(of,
		"%ssd name %s drive %s plex %s len %qdb driveoffset %qdb plexoffset %qdb\n",
		comment,
		sd.name,
		drive.label.name,
		plex.name,
		sd.sectors,
		sd.driveoffset,
		sd.plexoffset);
	}
    }
}
