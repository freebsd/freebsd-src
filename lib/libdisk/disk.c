/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: disk.c,v 1.2 1995/04/29 01:55:21 phk Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include "libdisk.h"

#define DOSPTYP_EXTENDED        5
#define DOSPTYP_ONTRACK         84      

struct disk *
Open_Disk(char *name)
{
	return Int_Open_Disk(name,0);
}

struct disk *
Int_Open_Disk(char *name, u_long size)
{
	int i,fd;
	struct diskslices ds;
	char device[64];
	struct disk *d;

	strcpy(device,"/dev/r");
	strcat(device,name);

	fd = open(device,O_RDONLY);
	if (fd < 0) {
		warn("open(%s) failed",device);
		return 0;
	}
	i = ioctl(fd,DIOCGSLICEINFO,&ds);
	if (i < 0) {
		warn("DIOCSLICEINFO(%s) failed",device);
		close(fd);
		return 0;
	}

	d = (struct disk *)malloc(sizeof *d);
	if(!d) err(1,"malloc failed");

	memset(d,0,sizeof *d);

	d->name = strdup(name);

	if (!size)
		size = ds.dss_slices[WHOLE_DISK_SLICE].ds_size;

	Add_Chunk(d, 0, size, name,whole,0,0);
	if (ds.dss_slices[COMPATIBILITY_SLICE].ds_offset)
		Add_Chunk(d, 0, 1, "-",reserved,0,0);
	
	for(i=BASE_SLICE;i<ds.dss_nslices;i++) {
		char sname[20];
		chunk_e ce;
		u_long flags=0;
		int subtype=0;
		if (! ds.dss_slices[i].ds_size)
			continue;
		sprintf(sname,"%ss%d",name,i-1);
		switch (ds.dss_slices[i].ds_type) {
			case 0xa5:
				ce = freebsd;
				break;
			case 0x1:
			case 0x6:
				ce = fat;
				break;
			case DOSPTYP_EXTENDED:
				ce = extended;
				break;
			default:
				ce = foo;
				subtype = -ds.dss_slices[i].ds_type;
				break;
		}	
		flags |= CHUNK_ALIGN;
		Add_Chunk(d,ds.dss_slices[i].ds_offset,
			ds.dss_slices[i].ds_size, sname,ce,subtype,flags);
		if (ce == extended)
			Add_Chunk(d,ds.dss_slices[i].ds_offset,
				1, "-",reserved, subtype, flags);
		if (ds.dss_slices[i].ds_type == 0xa5) {
			struct disklabel *dl;
			int j;

			dl = read_disklabel(fd,
				ds.dss_slices[i].ds_offset + LABELSECTOR);
			if(dl) {
				for(j=0; j < dl->d_npartitions; j++) {
					char pname[20];
					sprintf(pname,"%s%c",sname,j+'a');
					if (j == 2)
						continue;
					if (!dl->d_partitions[j].p_size)
						continue;
					Add_Chunk(d,
						dl->d_partitions[j].p_offset,
						dl->d_partitions[j].p_size,
						pname,part,0,0);
				}
			}
			free(dl);
		}
	}
	close(fd);
	return d;
}

void
Debug_Disk(struct disk *d)
{
	printf("Debug_Disk(%s)",d->name);
	printf("  flags=%lx",d->flags);
	printf("  real_geom=%lu/%lu/%lu",d->real_cyl,d->real_hd,d->real_sect);
	printf("  bios_geom=%lu/%lu/%lu\n",d->bios_cyl,d->bios_hd,d->bios_sect);
	Debug_Chunk(d->chunks);
}

void
Free_Disk(struct disk *d)
{
	if(d->chunks)
		Free_Chunk(d->chunks);
	if(d->name)
		free(d->name);
	free(d);
}

struct disk *
Clone_Disk(struct disk *d)
{
	struct disk *d2;

	d2 = (struct disk*) malloc(sizeof *d2);
	if(!d2) err(1,"malloc failed");
	*d2 = *d;
	d2->name = strdup(d2->name);
	d2->chunks = Clone_Chunk(d2->chunks);
	return d2;
}

void
Collapse_Disk(struct disk *d)
{

	while(Collapse_Chunk(d,d->chunks))
		;
}
