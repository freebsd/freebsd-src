/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@login.dknet.dk> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/lib/libdisk/write_disk.c,v 1.28.2.6 2000/09/14 12:10:46 nyan Exp $
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
#include <paths.h>
#include "libdisk.h"

#define DOSPTYP_EXTENDED        5
#define BBSIZE			8192
#define SBSIZE			8192
#define DEF_RPM			3600
#define DEF_INTERLEAVE	1

#ifdef PC98
#define WHERE(offset,disk) (offset)
#else
#define WHERE(offset,disk) (disk->flags & DISK_ON_TRACK ? offset + 63 : offset)
#endif
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
			warn("weird partition letter %c",c2->name[strlen(new->name) + 2]);
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
#ifdef PC98
	dl->d_rpm = 3600;
	dl->d_interleave = 1;
#endif

#ifndef PC98
	if(new->flags & DISK_ON_TRACK)
		for(i=0;i<MAXPARTITIONS;i++)
			if (dl->d_partitions[i].p_size)
				dl->d_partitions[i].p_offset += 63;
#endif
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

#ifndef PC98
static void
Write_Int32(u_int32_t *p, u_int32_t v)
{
    u_int8_t *bp = (u_int8_t *)p;
    bp[0] = (v >> 0) & 0xff;
    bp[1] = (v >> 8) & 0xff;
    bp[2] = (v >> 16) & 0xff;
    bp[3] = (v >> 24) & 0xff;
}
#endif

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
#ifdef PC98
	int s[7];
	int PC98_EntireDisk = 0;
#else
	int s[4];
#endif
	int one = 1;
	int zero = 0;

	strcpy(device,_PATH_DEV);
        strcat(device,d1->name);

#ifdef PC98
	/* XXX - for entire FreeBSD(98) */
	for (c1 = d1->chunks->part; c1; c1 = c1->next) {
	    if ((c1->type == freebsd) || (c1->offset == 0))
		device[9] = 0;
	}
#endif

        fd = open(device,O_RDWR);
        if (fd < 0) {
#ifdef DEBUG
                warn("open(%s) failed",device);
#endif
                return 1;
        }
	ioctl(fd, DIOCWLABEL, &one);

	memset(s,0,sizeof s);
#ifdef PC98
	mbr = read_block(fd,WHERE(1,d1));
#else
	mbr = read_block(fd,WHERE(0,d1));
#endif
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
#ifdef PC98
		if (j < 0 || j > 7)
#else
		if (j < 0 || j > 3)
#endif
			continue;
		s[j]++;
#endif
#ifndef PC98
		if (c1->type == extended)
			ret += Write_Extended(fd, d1,old,c1);
#endif
		if (c1->type == freebsd)
			ret += Write_FreeBSD(fd, d1,old,c1);

#ifndef __alpha__
#ifndef PC98
		Write_Int32(&dp[j].dp_start, c1->offset);
		Write_Int32(&dp[j].dp_size, c1->size);
#endif

		i = c1->offset;
#ifdef PC98
		dp[j].dp_ssect = dp[j].dp_ipl_sct = i % d1->bios_sect;
		i -= dp[j].dp_ssect;
		i /= d1->bios_sect;
		dp[j].dp_shd = dp[j].dp_ipl_head = i % d1->bios_hd;
		i -= dp[j].dp_shd;
		i /= d1->bios_hd;
		dp[j].dp_scyl = dp[j].dp_ipl_cyl = i;
#else
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
#endif

#ifdef DEBUG
		printf("S:%lu = (%x/%x/%x)",
			c1->offset,dp[j].dp_scyl,dp[j].dp_shd,dp[j].dp_ssect);
#endif

		i = c1->end;
#ifdef PC98
#if 1
		dp[j].dp_esect = dp[j].dp_ehd = 0;
		dp[j].dp_ecyl = i / (d1->bios_sect * d1->bios_hd);
#else
		dp[j].dp_esect = i % d1->bios_sect;
		i -= dp[j].dp_esect;
		i /= d1->bios_sect;
		dp[j].dp_ehd =  i % d1->bios_hd;
		i -= dp[j].dp_ehd;
		i /= d1->bios_hd;
		dp[j].dp_ecyl = i;
#endif
#else
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
#endif

#ifdef DEBUG
		printf("  E:%lu = (%x/%x/%x)\n",
			c1->end,dp[j].dp_ecyl,dp[j].dp_ehd,dp[j].dp_esect);
#endif

#ifdef PC98
		dp[j].dp_mid = c1->subtype & 0xff;
		dp[j].dp_sid = c1->subtype >> 8;
		if (c1->flags & CHUNK_ACTIVE)
			dp[j].dp_mid |= 0x80;

		strncpy(dp[j].dp_name, c1->sname, 16);
#else
		dp[j].dp_typ = c1->subtype;
		if (c1->flags & CHUNK_ACTIVE)
			dp[j].dp_flag = 0x80;
		else
			dp[j].dp_flag = 0;
#endif
#endif
	}
#ifndef __alpha__
	j = 0;
	for(i=0;i<NDOSPART;i++) {
		if (!s[i])
			memset(dp+i,0,sizeof *dp);
#ifndef PC98
		if (dp[i].dp_flag)
			j++;
#endif
	}
#ifndef PC98
	if (!j)
		for(i=0;i<NDOSPART;i++)
			if (dp[i].dp_typ == 0xa5)
				dp[i].dp_flag = 0x80;
#endif

#ifdef PC98
	if (d1->bootipl)
		write_block(fd,WHERE(0,d1),d1->bootipl);

	mbr = read_block(fd,WHERE(1,d1));
	memcpy(mbr+DOSPARTOFF,dp,sizeof *dp * NDOSPART);
	/* XXX - for entire FreeBSD(98) */
	for (c1 = d1->chunks->part; c1; c1 = c1->next)
		if (((c1->type == freebsd) || (c1->type == fat))
			 && (c1->offset == 0))
			PC98_EntireDisk = 1;
	if (PC98_EntireDisk == 0)
		write_block(fd,WHERE(1,d1),mbr);

	if (d1->bootmenu)
		for (i = 0; i * 512 < d1->bootmenu_size; i++)
			write_block(fd,WHERE(2+i,d1),&d1->bootmenu[i * 512]);
#else
	mbr = read_block(fd,WHERE(0,d1));
	if (d1->bootmgr)
		memcpy(mbr,d1->bootmgr,DOSPARTOFF);
	memcpy(mbr+DOSPARTOFF,dp,sizeof *dp * NDOSPART);
	mbr[512-2] = 0x55;
	mbr[512-1] = 0xaa;
	write_block(fd,WHERE(0,d1),mbr);
	if (d1->bootmgr && d1->bootmgr_size > 512)
	  for(i = 1; i * 512 <= d1->bootmgr_size; i++)
	    write_block(fd,WHERE(i,d1),&d1->bootmgr[i * 512]);
#endif
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

