/*-
 * Copyright (c) 1998 Michael Smith <msmith@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MBR/GPT partitioned disk device handling.
 *
 * Ideas and algorithms from:
 *
 * - NetBSD libi386/biosdisk.c
 * - FreeBSD biosboot/disk.c
 *
 */

#include <stand.h>

#include <sys/diskmbr.h>
#include <sys/disklabel.h>
#include <sys/gpt.h>

#include <stdarg.h>
#include <uuid.h>

#include <bootstrap.h>

#include "disk.h"

#ifdef DISK_DEBUG
# define DEBUG(fmt, args...)	printf("%s: " fmt "\n" , __func__ , ## args)
#else
# define DEBUG(fmt, args...)
#endif

/*
 * Search for a slice with the following preferences:
 *
 * 1: Active FreeBSD slice
 * 2: Non-active FreeBSD slice
 * 3: Active Linux slice
 * 4: non-active Linux slice
 * 5: Active FAT/FAT32 slice
 * 6: non-active FAT/FAT32 slice
 */
#define PREF_RAWDISK	0
#define PREF_FBSD_ACT	1
#define PREF_FBSD	2
#define PREF_LINUX_ACT	3
#define PREF_LINUX	4
#define PREF_DOS_ACT	5
#define PREF_DOS	6
#define PREF_NONE	7

#ifdef LOADER_GPT_SUPPORT

struct gpt_part {
	int		gp_index;
	uuid_t		gp_type;
	uint64_t	gp_start;
	uint64_t	gp_end;
};

static uuid_t efi = GPT_ENT_TYPE_EFI;
static uuid_t freebsd_boot = GPT_ENT_TYPE_FREEBSD_BOOT;
static uuid_t freebsd_ufs = GPT_ENT_TYPE_FREEBSD_UFS;
static uuid_t freebsd_swap = GPT_ENT_TYPE_FREEBSD_SWAP;
static uuid_t freebsd_zfs = GPT_ENT_TYPE_FREEBSD_ZFS;
static uuid_t ms_basic_data = GPT_ENT_TYPE_MS_BASIC_DATA;

#endif

#if defined(LOADER_GPT_SUPPORT) || defined(LOADER_MBR_SUPPORT)

/* Given a size in 512 byte sectors, convert it to a human-readable number. */
static char *
display_size(uint64_t size)
{
	static char buf[80];
	char unit;

	size /= 2;
	unit = 'K';
	if (size >= 10485760000LL) {
		size /= 1073741824;
		unit = 'T';
	} else if (size >= 10240000) {
		size /= 1048576;
		unit = 'G';
	} else if (size >= 10000) {
		size /= 1024;
		unit = 'M';
	}
	sprintf(buf, "%.6ld%cB", (long)size, unit);
	return (buf);
}

#endif

#ifdef LOADER_MBR_SUPPORT

static void
disk_checkextended(struct disk_devdesc *dev,
    struct dos_partition *slicetab, int slicenum, int *nslicesp)
{
	uint8_t			buf[DISK_SECSIZE];
	struct dos_partition	*dp;
	uint32_t		base;
	int			rc, i, start, end;

	dp = &slicetab[slicenum];
	start = *nslicesp;

	if (dp->dp_size == 0)
		goto done;
	if (dp->dp_typ != DOSPTYP_EXT)
		goto done;
	rc = dev->d_dev->dv_strategy(dev, F_READ, dp->dp_start, DISK_SECSIZE,
		(char *) buf, NULL);
	if (rc)
		goto done;
	if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xaa) {
		DEBUG("no magic in extended table");
		goto done;
	}
	base = dp->dp_start;
	dp = (struct dos_partition *) &buf[DOSPARTOFF];
	for (i = 0; i < NDOSPART; i++, dp++) {
		if (dp->dp_size == 0)
			continue;
		if (*nslicesp == NEXTDOSPART)
			goto done;
		dp->dp_start += base;
		bcopy(dp, &slicetab[*nslicesp], sizeof(*dp));
		(*nslicesp)++;
	}
	end = *nslicesp;

	/*
	 * now, recursively check the slices we just added
	 */
	for (i = start; i < end; i++)
		disk_checkextended(dev, slicetab, i, nslicesp);
done:
	return;
}

static int
disk_readslicetab(struct disk_devdesc *dev,
    struct dos_partition **slicetabp, int *nslicesp)
{
	struct dos_partition	*slicetab = NULL;
	int			nslices, i;
	int			rc;
	uint8_t			buf[DISK_SECSIZE];

	/*
	 * Find the slice in the DOS slice table.
	 */
	rc = dev->d_dev->dv_strategy(dev, F_READ, 0, DISK_SECSIZE,
		(char *) buf, NULL);
	if (rc) {
		DEBUG("error reading MBR");
		return (rc);
	}

	/*
	 * Check the slice table magic.
	 */
	if (buf[0x1fe] != 0x55 || buf[0x1ff] != 0xaa) {
		DEBUG("no slice table/MBR (no magic)");
		return (rc);
	}

	/*
	 * copy the partition table, then pick up any extended partitions.
	 */
	slicetab = malloc(NEXTDOSPART * sizeof(struct dos_partition));
	bcopy(buf + DOSPARTOFF, slicetab,
		sizeof(struct dos_partition) * NDOSPART);
	nslices = NDOSPART;		/* extended slices start here */
	for (i = 0; i < NDOSPART; i++)
		disk_checkextended(dev, slicetab, i, &nslices);

	*slicetabp = slicetab;
	*nslicesp = nslices;
	return (0);
}

/*
 * Search for the best MBR slice (typically the first FreeBSD slice).
 */
static int
disk_bestslice(struct dos_partition *slicetab, int nslices)
{
	struct dos_partition *dp;
	int pref, preflevel;
	int i, prefslice;

	prefslice = 0;
	preflevel = PREF_NONE;

	dp = &slicetab[0];
	for (i = 0; i < nslices; i++, dp++) {
		switch (dp->dp_typ) {
		case DOSPTYP_386BSD:		/* FreeBSD */
			pref = dp->dp_flag & 0x80 ? PREF_FBSD_ACT : PREF_FBSD;
			break;

		case DOSPTYP_LINUX:
			pref = dp->dp_flag & 0x80 ? PREF_LINUX_ACT : PREF_LINUX;
			break;

		case 0x01:		/* DOS/Windows */
		case 0x04:
		case 0x06:
		case 0x0b:
		case 0x0c:
		case 0x0e:
			pref = dp->dp_flag & 0x80 ? PREF_DOS_ACT : PREF_DOS;
			break;

		default:
		        pref = PREF_NONE;
		}
		if (pref < preflevel) {
			preflevel = pref;
			prefslice = i + 1;
		}
	}
	return (prefslice);
}

static int
disk_openmbr(struct disk_devdesc *dev)
{
	struct dos_partition	*slicetab = NULL, *dptr;
	int			nslices, sector, slice;
	int			rc;
	uint8_t			buf[DISK_SECSIZE];
	struct disklabel	*lp;

	/*
	 * Following calculations attempt to determine the correct value
	 * for dev->d_offset by looking for the slice and partition specified,
	 * or searching for reasonable defaults.
	 */
	rc = disk_readslicetab(dev, &slicetab, &nslices);
	if (rc)
		return (rc);

	/*
	 * if a slice number was supplied but not found, this is an error.
	 */
	if (dev->d_slice > 0) {
		slice = dev->d_slice - 1;
		if (slice >= nslices) {
			DEBUG("slice %d not found", slice);
			rc = EPART;
			goto out;
		}
	}

	/*
	 * Check for the historically bogus MBR found on true dedicated disks
	 */
	if (slicetab[3].dp_typ == DOSPTYP_386BSD &&
	    slicetab[3].dp_start == 0 && slicetab[3].dp_size == 50000) {
		sector = 0;
		goto unsliced;
	}

	/*
	 * Try to auto-detect the best slice; this should always give
	 * a slice number
	 */
	if (dev->d_slice == 0) {
		slice = disk_bestslice(slicetab, nslices);
		if (slice == -1) {
			rc = ENOENT;
			goto out;
		}
		dev->d_slice = slice;
	}

	/*
	 * Accept the supplied slice number unequivocally (we may be looking
	 * at a DOS partition).
	 * Note: we number 1-4, offsets are 0-3
	 */
	dptr = &slicetab[dev->d_slice - 1];
	sector = dptr->dp_start;
	DEBUG("slice entry %d at %d, %d sectors",
		dev->d_slice - 1, sector, dptr->dp_size);

unsliced:
	/*
	 * Now we have the slice offset, look for the partition in the
	 * disklabel if we have a partition to start with.
	 *
	 * XXX we might want to check the label checksum.
	 */
	if (dev->d_partition < 0) {
		/* no partition, must be after the slice */
		DEBUG("opening raw slice");
		dev->d_offset = sector;
		rc = 0;
		goto out;
	}

	rc = dev->d_dev->dv_strategy(dev, F_READ, sector + LABELSECTOR,
            DISK_SECSIZE, (char *) buf, NULL);
	if (rc) {
		DEBUG("error reading disklabel");
		goto out;
	}

	lp = (struct disklabel *) buf;

	if (lp->d_magic != DISKMAGIC) {
		DEBUG("no disklabel");
		rc = ENOENT;
		goto out;
	}
	if (dev->d_partition >= lp->d_npartitions) {
		DEBUG("partition '%c' exceeds partitions in table (a-'%c')",
		  'a' + dev->d_partition,
		  'a' + lp->d_npartitions);
		rc = EPART;
		goto out;
	}

	dev->d_offset =
		lp->d_partitions[dev->d_partition].p_offset -
		lp->d_partitions[RAW_PART].p_offset +
		sector;
	rc = 0;

out:
	if (slicetab)
		free(slicetab);
	return (rc);
}

/*
 * Print out each valid partition in the disklabel of a FreeBSD slice.
 * For size calculations, we assume a 512 byte sector size.
 */
static void
disk_printbsdslice(struct disk_devdesc *dev, daddr_t offset,
    char *prefix, int verbose)
{
	char			line[80];
	char			buf[DISK_SECSIZE];
	struct disklabel	*lp;
	int			i, rc, fstype;

	/* read disklabel */
	rc = dev->d_dev->dv_strategy(dev, F_READ, offset + LABELSECTOR,
		DISK_SECSIZE, (char *) buf, NULL);
	if (rc)
		return;
	lp =(struct disklabel *)(&buf[0]);
	if (lp->d_magic != DISKMAGIC) {
		sprintf(line, "%s: FFS  bad disklabel\n", prefix);
		pager_output(line);
		return;
	}

	/* Print partitions */
	for (i = 0; i < lp->d_npartitions; i++) {
		/*
		 * For each partition, make sure we know what type of fs it
		 * is.  If not, then skip it.
		 */
		fstype = lp->d_partitions[i].p_fstype;
		if (fstype != FS_BSDFFS &&
		    fstype != FS_SWAP &&
		    fstype != FS_VINUM)
			continue;

		/* Only print out statistics in verbose mode */
		if (verbose)
			sprintf(line, "  %s%c: %s %s (%d - %d)\n",
				prefix, 'a' + i,
			    (fstype == FS_SWAP) ? "swap " :
			    (fstype == FS_VINUM) ? "vinum" :
			    "FFS  ",
			    display_size(lp->d_partitions[i].p_size),
			    lp->d_partitions[i].p_offset,
			    (lp->d_partitions[i].p_offset
			     + lp->d_partitions[i].p_size));
		else
			sprintf(line, "  %s%c: %s\n", prefix, 'a' + i,
			    (fstype == FS_SWAP) ? "swap" :
			    (fstype == FS_VINUM) ? "vinum" :
			    "FFS");
		pager_output(line);
	}
}

static void
disk_printslice(struct disk_devdesc *dev, int slice,
    struct dos_partition *dp, char *prefix, int verbose)
{
	char stats[80];
	char line[80];

	if (verbose)
		sprintf(stats, " %s (%d - %d)", display_size(dp->dp_size),
		    dp->dp_start, dp->dp_start + dp->dp_size);
	else
		stats[0] = '\0';

	switch (dp->dp_typ) {
	case DOSPTYP_386BSD:
		disk_printbsdslice(dev, (daddr_t)dp->dp_start,
		     prefix, verbose);
		return;
	case DOSPTYP_LINSWP:
		sprintf(line, "%s: Linux swap%s\n", prefix, stats);
		break;
	case DOSPTYP_LINUX:
		/*
		 * XXX
		 * read the superblock to confirm this is an ext2fs partition?
		 */
		sprintf(line, "%s: ext2fs%s\n", prefix, stats);
		break;
	case 0x00:				/* unused partition */
	case DOSPTYP_EXT:
		return;
	case 0x01:
		sprintf(line, "%s: FAT-12%s\n", prefix, stats);
		break;
	case 0x04:
	case 0x06:
	case 0x0e:
		sprintf(line, "%s: FAT-16%s\n", prefix, stats);
		break;
	case 0x07:
		sprintf(line, "%s: NTFS/HPFS%s\n", prefix, stats);
		break;
	case 0x0b:
	case 0x0c:
		sprintf(line, "%s: FAT-32%s\n", prefix, stats);
		break;
	default:
		sprintf(line, "%s: Unknown fs: 0x%x %s\n", prefix, dp->dp_typ,
		    stats);
	}
	pager_output(line);
}

static int
disk_printmbr(struct disk_devdesc *dev, char *prefix, int verbose)
{
	struct dos_partition	*slicetab;
	int			nslices, i;
	int			rc;
	char			line[80];

	rc = disk_readslicetab(dev, &slicetab, &nslices);
	if (rc)
		return (rc);
	for (i = 0; i < nslices; i++) {
		sprintf(line, "%ss%d", prefix, i + 1);
		disk_printslice(dev, i, &slicetab[i], line, verbose);
	}
	free(slicetab);
	return (0);
}

#endif

#ifdef LOADER_GPT_SUPPORT

static int
disk_readgpt(struct disk_devdesc *dev, struct gpt_part **gptp, int *ngptp)
{
	struct dos_partition	*dp;
	struct gpt_hdr		*hdr;
	struct gpt_ent		*ent;
	struct gpt_part		*gptab = NULL;
	int			entries_per_sec, rc, i, part;
	daddr_t			lba, elba;
	uint8_t			gpt[DISK_SECSIZE], tbl[DISK_SECSIZE];

	/*
	 * Following calculations attempt to determine the correct value
	 * for dev->d_offset by looking for the slice and partition specified,
	 * or searching for reasonable defaults.
	 */
	rc = 0;

	/* First, read the MBR and see if we have a PMBR. */
	rc = dev->d_dev->dv_strategy(dev, F_READ, 0, DISK_SECSIZE,
		(char *) tbl, NULL);
	if (rc) {
		DEBUG("error reading MBR");
		return (EIO);
	}

	/* Check the slice table magic. */
	if (tbl[0x1fe] != 0x55 || tbl[0x1ff] != 0xaa)
		return (ENXIO);

	/* Check for GPT slice. */
	part = 0;
	dp = (struct dos_partition *)(tbl + DOSPARTOFF);
	for (i = 0; i < NDOSPART; i++) {
		if (dp[i].dp_typ == 0xee)
			part++;
		else if ((part != 1) && (dp[i].dp_typ != 0x00))
			return (EINVAL);
	}
	if (part != 1)
		return (EINVAL);

	/* Read primary GPT table header. */
	rc = dev->d_dev->dv_strategy(dev, F_READ, 1, DISK_SECSIZE,
		(char *) gpt, NULL);
	if (rc) {
		DEBUG("error reading GPT header");
		return (EIO);
	}
	hdr = (struct gpt_hdr *)gpt;
	if (bcmp(hdr->hdr_sig, GPT_HDR_SIG, sizeof(hdr->hdr_sig)) != 0 ||
	    hdr->hdr_lba_self != 1 || hdr->hdr_revision < 0x00010000 ||
	    hdr->hdr_entsz < sizeof(*ent) ||
	    DISK_SECSIZE % hdr->hdr_entsz != 0) {
		DEBUG("Invalid GPT header\n");
		return (EINVAL);
	}

	/* Walk the partition table to count valid partitions. */
	part = 0;
	entries_per_sec = DISK_SECSIZE / hdr->hdr_entsz;
	elba = hdr->hdr_lba_table + hdr->hdr_entries / entries_per_sec;
	for (lba = hdr->hdr_lba_table; lba < elba; lba++) {
		rc = dev->d_dev->dv_strategy(dev, F_READ, lba, DISK_SECSIZE,
			(char *) tbl, NULL);
		if (rc) {
			DEBUG("error reading GPT table");
			return (EIO);
		}
		for (i = 0; i < entries_per_sec; i++) {
			ent = (struct gpt_ent *)(tbl + i * hdr->hdr_entsz);
			if (uuid_is_nil(&ent->ent_type, NULL) ||
			    ent->ent_lba_start == 0 ||
			    ent->ent_lba_end < ent->ent_lba_start)
				continue;
			part++;
		}
	}

	/* Save the important information about all the valid partitions. */
	if (part != 0) {
		gptab = malloc(part * sizeof(struct gpt_part));
		part = 0;
		for (lba = hdr->hdr_lba_table; lba < elba; lba++) {
			rc = dev->d_dev->dv_strategy(dev, F_READ, lba, DISK_SECSIZE,
				(char *) tbl, NULL);
			if (rc) {
				DEBUG("error reading GPT table");
				free(gptab);
				return (EIO);
			}
			for (i = 0; i < entries_per_sec; i++) {
				ent = (struct gpt_ent *)(tbl + i * hdr->hdr_entsz);
				if (uuid_is_nil(&ent->ent_type, NULL) ||
				    ent->ent_lba_start == 0 ||
				    ent->ent_lba_end < ent->ent_lba_start)
					continue;
				gptab[part].gp_index = (lba - hdr->hdr_lba_table) *
					entries_per_sec + i + 1;
				gptab[part].gp_type = ent->ent_type;
				gptab[part].gp_start = ent->ent_lba_start;
				gptab[part].gp_end = ent->ent_lba_end;
				part++;
			}
		}
	}

	*gptp = gptab;
	*ngptp = part;
	return (0);
}

static struct gpt_part *
disk_bestgpt(struct gpt_part *gpt, int ngpt)
{
	struct gpt_part *gp, *prefpart;
	int i, pref, preflevel;

	prefpart = NULL;
	preflevel = PREF_NONE;

	gp = gpt;
	for (i = 0; i < ngpt; i++, gp++) {
		/* Windows. XXX: Also Linux. */
		if (uuid_equal(&gp->gp_type, &ms_basic_data, NULL))
			pref = PREF_DOS;
		/* FreeBSD */
		else if (uuid_equal(&gp->gp_type, &freebsd_ufs, NULL) ||
		         uuid_equal(&gp->gp_type, &freebsd_zfs, NULL))
			pref = PREF_FBSD;
		else
			pref = PREF_NONE;
		if (pref < preflevel) {
			preflevel = pref;
			prefpart = gp;
		}
	}
	return (prefpart);
}

static int
disk_opengpt(struct disk_devdesc *dev)
{
	struct gpt_part		*gpt = NULL, *gp;
	int			rc, ngpt, i;

	rc = disk_readgpt(dev, &gpt, &ngpt);
	if (rc)
		return (rc);

	/* Is this a request for the whole disk? */
	if (dev->d_slice < 0) {
		dev->d_offset = 0;
		rc = 0;
		goto out;
	}

	/*
	 * If a partition number was supplied, then the user is trying to use
	 * an MBR address rather than a GPT address, so fail.
	 */
	if (dev->d_partition != 0xff) {
		rc = ENOENT;
		goto out;
	}

	/* If a slice number was supplied but not found, this is an error. */
	gp = NULL;
	if (dev->d_slice > 0) {
		for (i = 0; i < ngpt; i++) {
			if (gpt[i].gp_index == dev->d_slice) {
				gp = &gpt[i];
				break;
			}
		}
		if (gp == NULL) {
			DEBUG("partition %d not found", dev->d_slice);
			rc = ENOENT;
			goto out;
		}
	}

	/* Try to auto-detect the best partition. */
	if (dev->d_slice == 0) {
		gp = disk_bestgpt(gpt, ngpt);
		if (gp == NULL) {
			rc = ENOENT;
			goto out;
		}
		dev->d_slice = gp->gp_index;
	}

	dev->d_offset = gp->gp_start;
	rc = 0;

out:
	if (gpt)
		free(gpt);
	return (rc);
}

static void
disk_printgptpart(struct disk_devdesc *dev, struct gpt_part *gp,
    char *prefix, int verbose)
{
	char stats[80];
	char line[96];

	if (verbose)
		sprintf(stats, " %s",
			display_size(gp->gp_end + 1 - gp->gp_start));
	else
		stats[0] = '\0';

	if (uuid_equal(&gp->gp_type, &efi, NULL))
		sprintf(line, "%s: EFI         %s\n", prefix, stats);
	else if (uuid_equal(&gp->gp_type, &ms_basic_data, NULL))
		sprintf(line, "%s: FAT/NTFS    %s\n", prefix, stats);
	else if (uuid_equal(&gp->gp_type, &freebsd_boot, NULL))
		sprintf(line, "%s: FreeBSD boot%s\n", prefix, stats);
	else if (uuid_equal(&gp->gp_type, &freebsd_ufs, NULL))
		sprintf(line, "%s: FreeBSD UFS %s\n", prefix, stats);
	else if (uuid_equal(&gp->gp_type, &freebsd_zfs, NULL))
		sprintf(line, "%s: FreeBSD ZFS %s\n", prefix, stats);
	else if (uuid_equal(&gp->gp_type, &freebsd_swap, NULL))
		sprintf(line, "%s: FreeBSD swap%s\n", prefix, stats);
	else
		sprintf(line,
		    "%s: %08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x%s\n",
		    prefix,
		    gp->gp_type.time_low, gp->gp_type.time_mid,
		    gp->gp_type.time_hi_and_version,
		    gp->gp_type.clock_seq_hi_and_reserved,
		    gp->gp_type.clock_seq_low,
		    gp->gp_type.node[0],
		    gp->gp_type.node[1],
		    gp->gp_type.node[2],
		    gp->gp_type.node[3],
		    gp->gp_type.node[4],
		    gp->gp_type.node[5],
		    stats);
	pager_output(line);
}

static int
disk_printgpt(struct disk_devdesc *dev, char *prefix, int verbose)
{
	struct gpt_part		*gpt = NULL;
	int			rc, ngpt, i;
	char			line[80];

	rc = disk_readgpt(dev, &gpt, &ngpt);
	if (rc)
		return (rc);
	for (i = 0; i < ngpt; i++) {
		sprintf(line, "%sp%d", prefix, i + 1);
		disk_printgptpart(dev, &gpt[i], line, verbose);
	}
	free(gpt);
	return (0);
}

#endif

int
disk_open(struct disk_devdesc *dev)
{
	int rc;

	rc = 0;
	/*
	 * While we are reading disk metadata, make sure we do it relative
	 * to the start of the disk
	 */
	dev->d_offset = 0;

#ifdef LOADER_GPT_SUPPORT
	rc = disk_opengpt(dev);
#endif
#ifdef LOADER_MBR_SUPPORT
	if (rc)
		rc = disk_openmbr(dev);
#endif

	return (rc);
}

void
disk_print(struct disk_devdesc *dev, char *prefix, int verbose)
{

#ifdef LOADER_GPT_SUPPORT
	if (disk_printgpt(dev, prefix, verbose) == 0)
		return;
#endif
#ifdef LOADER_MBR_SUPPORT
	disk_printmbr(dev, prefix, verbose);
#endif
}
