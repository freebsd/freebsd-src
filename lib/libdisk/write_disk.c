/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: write_disk.c,v 1.26 1999/01/29 11:39:24 jkh Exp $
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <err.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/disklabel.h>
#include <sys/diskslice.h>
#include "libdisk.h"

#define DOSPTYP_EXTENDED        5
#define BBSIZE			8192
#define SBSIZE			8192
#define DEF_RPM			3600
#define DEF_INTERLEAVE	1

#define WHERE(offset,disk) (disk->flags & DISK_ON_TRACK ? offset + 63 : offset)
int
Write_FreeBSD(int fd, struct disk *new, struct disk *old, struct chunk *c1)
{
	struct disklabel *dl;
	struct chunk *c2;
	int i,j;
	void *p;
	u_char buf[BBSIZE];
#ifdef __alpha__
	u_long *lp, sum;
#endif

	for(i=0;i<BBSIZE/512;i++) {
		p = read_block(fd,WHERE(i + c1->offset,new));
		memcpy(buf+512*i,p,512);
		free(p);
	}
#if defined(__i386__)
	if(new->boot1)
		memcpy(buf,new->boot1,512);

	if(new->boot2)
		memcpy(buf+512,new->boot2,BBSIZE-512);
#elif defined(__alpha__)
	if(new->boot1)
		memcpy(buf+512,new->boot1,BBSIZE-512);
#endif

	dl = (struct disklabel *) (buf+512*LABELSECTOR+LABELOFFSET);
	memset(dl,0,sizeof *dl);

	for(c2=c1->part;c2;c2=c2->next) {
		if (c2->type == unused) continue;
		if (!strcmp(c2->name,"X")) continue;
#ifdef __alpha__
		j = c2->name[strlen(c2->name) - 1] - 'a';
#else
		j = c2->name[strlen(new->name) + 2] - 'a';
#endif
		if (j < 0 || j >= MAXPARTITIONS || j == RAW_PART) {
#ifdef DEBUG
			warn("Weird parititon letter %c",c2->name[strlen(new->name) + 2]);
#endif
			continue;
		}
		dl->d_partitions[j].p_size = c2->size;
		dl->d_partitions[j].p_offset = c2->offset;
		dl->d_partitions[j].p_fstype = c2->subtype;
	}

	dl->d_bbsize = BBSIZE;
	/*
	 * Add in defaults for superblock size, interleave, and rpms
	 */
	dl->d_sbsize = SBSIZE;
	dl->d_interleave = DEF_INTERLEAVE;
	dl->d_rpm = DEF_RPM;

	strcpy(dl->d_typename,c1->name);

	dl->d_secsize = 512;
	dl->d_secperunit = new->chunks->size;
	dl->d_ncylinders =  new->bios_cyl;
	dl->d_ntracks =  new->bios_hd;
	dl->d_nsectors =  new->bios_sect;
	dl->d_secpercyl = dl->d_ntracks * dl->d_nsectors;

	dl->d_npartitions = MAXPARTITIONS;

	dl->d_type = new->name[0] == 's' || new->name[0] == 'd' ||
	    new->name[0] == 'o' ? DTYPE_SCSI : DTYPE_ESDI;
	dl->d_partitions[RAW_PART].p_size = c1->size;
	dl->d_partitions[RAW_PART].p_offset = c1->offset;

	if(new->flags & DISK_ON_TRACK)
		for(i=0;i<MAXPARTITIONS;i++)
			if (dl->d_partitions[i].p_size)
				dl->d_partitions[i].p_offset += 63;
	dl->d_magic = DISKMAGIC;
	dl->d_magic2 = DISKMAGIC;
	dl->d_checksum = dkcksum(dl);

#ifdef __alpha__
	/*
	 * Tell SRM where the bootstrap is.
	 */
	lp = (u_long *)buf;
	lp[60] = 15;
	lp[61] = 1;
	lp[62] = 0;

	/*
	 * Generate the bootblock checksum for the SRM console.
	 */
	for (lp = (u_long *)buf, i = 0, sum = 0; i < 63; i++)
	    sum += lp[i];
	lp[63] = sum;
#endif

	for(i=0;i<BBSIZE/512;i++) {
		write_block(fd,WHERE(i + c1->offset,new),buf+512*i);
	}

	return 0;
}

int
Write_Extended(int fd, struct disk *new, struct disk *old, struct chunk *c1)
{
	return 0;
}

static void
Write_Int32(u_int32_t *p, u_int32_t v)
{
    u_int8_t *bp = (u_int8_t *)p;
    bp[0] = (v >> 0) & 0xff;
    bp[1] = (v >> 8) & 0xff;
    bp[2] = (v >> 16) & 0xff;
    bp[3] = (v >> 24) & 0xff;
}

int
Write_Disk(struct disk *d1)
{
	int fd,i,j;
	struct disk *old = 0;
	struct chunk *c1;
	int ret = 0;
	char device[64];
	u_char *mbr;
	struct dos_partition *dp,work[NDOSPART];
	int s[4];
	int one = 1;
	int zero = 0;

	strcpy(device,"/dev/r");
        strcat(device,d1->name);

        fd = open(device,O_RDWR);
        if (fd < 0) {
#ifdef DEBUG
                warn("open(%s) failed",device);
#endif
                return 1;
        }
	ioctl(fd, DIOCWLABEL, &one);

	memset(s,0,sizeof s);
	mbr = read_block(fd,WHERE(0,d1));
	dp = (struct dos_partition*) (mbr + DOSPARTOFF);
	memcpy(work,dp,sizeof work);
	dp = work;
	free(mbr);
	for (c1=d1->chunks->part; c1 ; c1 = c1->next) {
		if (c1->type == unused) continue;
		if (!strcmp(c1->name,"X")) continue;
#ifndef __alpha__
		j = c1->name[4] - '1';
		j = c1->name[strlen(d1->name) + 1] - '1';
		if (j < 0 || j > 3)
			continue;
		s[j]++;
#endif
		if (c1->type == extended)
			ret += Write_Extended(fd, d1,old,c1);
		if (c1->type == freebsd)
			ret += Write_FreeBSD(fd, d1,old,c1);

#ifndef __alpha__
		Write_Int32(&dp[j].dp_start, c1->offset);
		Write_Int32(&dp[j].dp_size, c1->size);

		i = c1->offset;
		if (i >= 1024*d1->bios_sect*d1->bios_hd) {
			dp[j].dp_ssect = 0xff;
			dp[j].dp_shd = 0xff;
			dp[j].dp_scyl = 0xff;
		} else {
			dp[j].dp_ssect = i % d1->bios_sect;
			i -= dp[j].dp_ssect++;
			i /= d1->bios_sect;
			dp[j].dp_shd =  i % d1->bios_hd;
			i -= dp[j].dp_shd;
			i /= d1->bios_hd;
			dp[j].dp_scyl = i;
			i -= dp[j].dp_scyl;
			dp[j].dp_ssect |= i >> 2;
		}

#ifdef DEBUG
		printf("S:%lu = (%x/%x/%x)",
			c1->offset,dp[j].dp_scyl,dp[j].dp_shd,dp[j].dp_ssect);
#endif

		i = c1->end;
		dp[j].dp_esect = i % d1->bios_sect;
		i -= dp[j].dp_esect++;
		i /= d1->bios_sect;
		dp[j].dp_ehd =  i % d1->bios_hd;
		i -= dp[j].dp_ehd;
		i /= d1->bios_hd;
		if (i>1023) i = 1023;
		dp[j].dp_ecyl = i;
		i -= dp[j].dp_ecyl;
		dp[j].dp_esect |= i >> 2;

#ifdef DEBUG
		printf("  E:%lu = (%x/%x/%x)\n",
			c1->end,dp[j].dp_ecyl,dp[j].dp_ehd,dp[j].dp_esect);
#endif

		dp[j].dp_typ = c1->subtype;
		if (c1->flags & CHUNK_ACTIVE)
			dp[j].dp_flag = 0x80;
		else
			dp[j].dp_flag = 0;
#endif
	}
#ifndef __alpha__
	j = 0;
	for(i=0;i<NDOSPART;i++) {
		if (!s[i])
			memset(dp+i,0,sizeof *dp);
		if (dp[i].dp_flag)
			j++;
	}
	if (!j)
		for(i=0;i<NDOSPART;i++)
			if (dp[i].dp_typ == 0xa5)
				dp[i].dp_flag = 0x80;

	mbr = read_block(fd,WHERE(0,d1));
	if (d1->bootmgr)
		memcpy(mbr,d1->bootmgr,DOSPARTOFF);
	memcpy(mbr+DOSPARTOFF,dp,sizeof *dp * NDOSPART);
	mbr[512-2] = 0x55;
	mbr[512-1] = 0xaa;
	write_block(fd,WHERE(0,d1),mbr);
#endif

	i = 1;
	i = ioctl(fd,DIOCSYNCSLICEINFO,&i);
#ifdef DEBUG
	if (i != 0)
		warn("ioctl(DIOCSYNCSLICEINFO)");
#endif
	ioctl(fd, DIOCWLABEL, &zero);
	close(fd);
	return 0;
}

