/*
 * Copyright (c) 1995 Joerg Wunsch
 *
 * All rights reserved.
 *
 * This program is free software.
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * MS-DOS (FAT) file system structure definitions.
 *
 * $FreeBSD$
 */

#ifndef DOSFS_H
#define DOSFS_H 1

typedef u_char Long[4];
typedef u_char Short[2];

union bootsector
{
  unsigned char raw[512];
  struct bsec
  {
    u_char jump_boot[3];	/* jump code to boot-up partition */
    char oem_name[8];		/* OEM company name & version */
    Short sectsiz;		/* bytes per sector */
    u_char clustsiz;		/* sectors per cluster */
    Short ressecs;		/* reserved sectors [before 1st FAT] */
    u_char fatcnt;		/* # of FAT's */
    Short rootsiz;		/* number of root dir entries */
    Short totsecs;		/* total # of sectors */
    u_char media;		/* media descriptor */
    Short fatsize;		/* # of sectors per FAT */
    Short trksecs;		/* sectors per track (cylinder) */
    Short headcnt;		/* # of r/w heads */
    Short hidnsec;		/* hidden sectors */
    union
    {
      /* case totsecs != 0: */
      /* This is a partition of MS-DOS 3.3 format (< 32 MB) */
      u_char bootprogram[480];
      
      /* case totsecs == 0: */
      /* partition of MS-DOS 4.0+ format, or > 32 MB */
      struct
      {
	Short unused;
	Long totsecs;		/* total # of sectors, as a 32-bit */
	Short physdrv;		/* physical drive # [0x80...] */
	u_char extboot;		/* extended boot signature??? */
	Long serial;		/* volume serial number */
	char label[11];		/* same as volume label in root dir */
	char fsysid[8];		/* some like `FAT16' */
	u_char bootprogram[448];
      } extended;
    } variable_part;
    u_char signature[2];	/* always {0x55, 0xaa} */
  } bsec;
};

struct fat
{
  u_char media;			/* the media descriptor again */
  u_char padded;		/* alway 0xff */
  u_char contents[1];		/* the `1' is a placeholder only */
};

/* DOS file attributes */
#define	FA_RONLY	1	/* read/only */
#define	FA_HIDDEN	2	/* hidden */
#define	FA_SYSTEM	4	/* system */
#define	FA_VOLLABEL	8	/* this is the volume label */
#define	FA_SUBDIR	0x10	/* sub-directory */
#define	FA_ARCH		0x20	/* archive - file hasn't been backed up */

struct dosftime
{
  u_char  time[2];	/* [0] & 0x1f - seconds div 2
			 * ([1] & 7) * 8 + ([0] >> 5) - minutes
			 * [1] >> 3   - hours
			 */
  u_char  date[2];	/* [0] & 0x1f - day
			 * ([1] & 1) * 8 + ([0] >> 5) - month
			 * [1] >> 1   - year - 1980
			 */
};

#define dosft_hour(dft)    ((dft).time[1] >> 3)
#define dosft_minute(dft)  (((dft).time[1] & 7) * 8 + ((dft).time[0] >> 5))
#define dosft_second(dft)  (((dft).time[0] & 0x1f) * 2)
#define dosft_year(dft)    (((dft).date[1] >> 1) + 1980)
#define dosft_month(dft)   (((dft).date[1] & 1) * 8 + ((dft).date[0] >> 5))
#define dosft_day(dft)     ((dft).date[0] & 0x1f)


struct direntry
{
  char name[8];			/* file name portion */
  char ext[3];			/* file extension */
  u_char attr;			/* file attribute as above */
  char reserved[10];
  struct dosftime fdate;	/* time created/last modified */
  Short startclstr;		/* starting cluster number */
  Long filesiz;			/* file size in bytes */
};

/* handle endiannes: */
#define s_to_little_s(dst, src) dst[0]=(src)&0xff; dst[1]=((src)&0xff00)>>8
#define l_to_little_l(dst, src) \
dst[0]=(src)&0xff; dst[1]=((src)&0xff00)>>8; \
dst[2]=((src)&0xff0000)>>16; dst[3]=((src)&0xff000000)>>24

#endif /* DOSFS_H */
