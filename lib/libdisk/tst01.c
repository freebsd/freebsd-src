/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id$
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include <sys/queue.h>
#include "libdisk.h"

void
fprint_diskslices(FILE *fi, struct diskslices *ds)
{
	int i;

	printf("@%p: struct diskslices\n",ds);
	printf("\tdss_first_bsd_slice = %d\n",ds->dss_first_bsd_slice);
	printf("\tdss_nslices = %d\n",ds->dss_nslices);
	for(i=0;i<ds->dss_nslices;i++) {
		printf("\tdss_slices[%d] = struct diskslice",i);
		if (i == 0)
			printf(" /* FreeBSD compatibility slice */\n");
		else if (i == 1)
			printf(" /* Whole disk slice */\n");
		else if (i < 6)
			printf(" /* Primary MBR slice %d */\n",i-1);
		else
			printf("\n");
		printf("\t\tds_offset = %lu\n",ds->dss_slices[i].ds_offset);
		printf("\t\tds_size = %lu\n",ds->dss_slices[i].ds_size);
		printf("\t\tds_type = %u\n",ds->dss_slices[i].ds_type);
		printf("\t\tds_openmask = %u\n",ds->dss_slices[i].ds_openmask);
	}
}

int
main(int argc, char **argv)
{
	int i;
	struct disk *d;

	for(i=1;i<argc;i++) {
		d = Open_Disk(argv[i]);
		if (!d) continue;
		Debug_Disk(d);
		Delete_Chunk(d,0,4108599,freebsd);
		Debug_Disk(d);
		printf("Create=%d\n",Create_Chunk(d,0,32768,fat,0,0));
		printf("Create=%d\n",Create_Chunk(d,192512,409600,freebsd,0,0));
		printf("Create=%d\n",Create_Chunk(d,192512,409600,part,0,0));
		Debug_Disk(d);
	}
	exit (0);
}
