/*
 * Copyright (c) 1995, 1996 Joerg Wunsch
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
 * Create an MS-DOS (FAT) file system.
 *
 * $Id: mkdosfs.c,v 1.2 1996/01/30 02:35:08 joerg Exp $
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <memory.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>

#include "bootcode.h"
#include "dosfs.h"

struct descrip
{
  /* our database key */
  unsigned kilobytes;
  /* MSDOS 3.3 BPB fields */
  u_short sectsiz;
  u_char clustsiz;
  u_short ressecs;
  u_char fatcnt;
  u_short rootsiz;
  u_short totsecs;
  u_char media;
  u_short fatsize;
  u_short trksecs;
  u_short headcnt;
  u_short hidnsec;
  /* MSDOS 4 BPB extensions */
  u_long ext_totsecs;
  u_short ext_physdrv;
  u_char ext_extboot;
  char ext_label[11];
  char ext_fsysid[8];
};

static struct descrip
table[] = 
{
  /* NB: must be sorted, starting with the largest format! */
  /*
   * kilobytes
   * sec cls res fat  rot   tot   med fsz spt hds hid
   * tot phs ebt    label          fsysid
   */
  {1440,
     512,  1,  1,  2, 224, 2880, 0xf0,  9, 18,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  {1200,
     512,  1,  1,  2, 224, 2400, 0xf9,  7, 15,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  { 720,
     512,  2,  1,  2, 112, 1440, 0xf9,  3,  9,  2,  0,
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  { 360,
     512,  2,  1,  2, 112,  720, 0xfd,  2,  9,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
};

void
usage(void)
{
  fprintf(stderr, "usage: ");
  errx(2, "[-f kbytes] [-L label] device");
}

unsigned
findformat(int fd)
{
  struct stat sb;

  /*
   * This is a bit tricky.  If the argument is a regular file, we can
   * lseek() to its end and get the size reported.  If it's a device
   * however, lseeking doesn't report us any useful number.  Instead,
   * we try to seek just to the end of the device and try reading a
   * block there.  In the case where we've hit exactly the device
   * boundary, we get a zero read, and thus have found the size.
   * Since our knowledge of distinct formats is limited anyway, this
   * is not a big deal at all.
   */

  if(fstat(fd, &sb) == -1)
    err(1, "Huh? Cannot fstat()"); /* Cannot happen */
  if(S_ISREG(sb.st_mode))
    {
      off_t o;
      if(lseek(fd, (off_t)0, SEEK_END) == -1 ||
	 (o = lseek(fd, (off_t)0, SEEK_CUR)) == -1)
	/* Hmm, hmm.  Hard luck. */
	return 0;
      return (int)(o / 1024);
    }
  else if(S_ISCHR(sb.st_mode) || S_ISBLK(sb.st_mode))
    {
      char b[512];
      int i, rv;
      struct descrip *dp;

      for(i = 0, dp = table;
	  i < sizeof table / sizeof(struct descrip);
	  i++, dp++)
	{
	  if(lseek(fd, (off_t)(dp->kilobytes * 1024), SEEK_SET) == 1)
	    /* Uh-oh, lseek() is not supposed to fail. */
	    return 0;
	  if((rv = read(fd, b, 512)) == 0)
	    break;
	  /* XXX The ENOSPC is for the bogus fd(4) driver return value. */
	  if(rv == -1 && errno != EINVAL && errno != ENOSPC)
	    return 0;
	  /* else: continue */
	}
      if(i == sizeof table / sizeof(struct descrip))
	return 0;
      (void)lseek(fd, (off_t)0, SEEK_SET);
      return dp->kilobytes;
    }
  else
    /* Outta luck. */
    return 0;
}
  

void
setup_boot_sector_from_template(union bootsector *bs, struct descrip *dp)
{
  memcpy((void *)bs->raw, (void *)bootcode, 512);
  
  /* historical part of BPB */
  s_to_little_s(bs->bsec.sectsiz, dp->sectsiz);
  bs->bsec.clustsiz = dp->clustsiz;
  s_to_little_s(bs->bsec.ressecs, dp->ressecs);
  bs->bsec.fatcnt = dp->fatcnt;
  s_to_little_s(bs->bsec.rootsiz, dp->rootsiz);
  s_to_little_s(bs->bsec.totsecs, dp->totsecs);
  bs->bsec.media = dp->media;
  s_to_little_s(bs->bsec.fatsize, dp->fatsize);
  s_to_little_s(bs->bsec.trksecs, dp->trksecs);
  s_to_little_s(bs->bsec.headcnt, dp->headcnt);
  s_to_little_s(bs->bsec.hidnsec, dp->hidnsec);

  /* MSDOS 4 extensions */
  l_to_little_l(bs->bsec.variable_part.extended.totsecs, dp->ext_totsecs);
  s_to_little_s(bs->bsec.variable_part.extended.physdrv, dp->ext_physdrv);
  bs->bsec.variable_part.extended.extboot = dp->ext_extboot;

  /* assign a "serial number" :) */
  srandom((unsigned)time((time_t)0));
  l_to_little_l(bs->bsec.variable_part.extended.serial, random());

  memcpy((void *)bs->bsec.variable_part.extended.label,
	 (void *)dp->ext_label, 11);
  memcpy((void *)bs->bsec.variable_part.extended.fsysid,
	 (void *)dp->ext_fsysid, 8);
}

#define roundup(dst, limit) dst = (((dst) | ((limit) - 1)) & ~(limit)) + 1

int
main(int argc, char **argv)
{
  union bootsector bs;
  struct descrip *dp;
  struct fat *fat;
  struct direntry *rootdir;
  struct tm *tp;
  time_t now;
  
  int c, i, fd, format = 0, rootdirsize;
  const char *label = 0;
  
  while((c = getopt(argc, argv, "f:L:")) !=  -1)
    switch(c)
      {
      case 'f':
	format = atoi(optarg);
	break;

      case 'L':
	label = optarg;
	break;

      case '?':
      default:
	usage();
      }
  argc -= optind;
  argv += optind;

  if(argc != 1)
    usage();

  if((fd = open(argv[0], O_RDWR|O_EXCL, 0)) == -1)
    err(1, "open(%s)", argv[0]);

  if(format == 0)
    {
      /*
       * No format specified, try to figure it out.
       */
      if((format = findformat(fd)) == 0)
	errx(1, "cannot determine size, must use -f format");
    }

  for(i = 0, dp = table; i < sizeof table / sizeof(struct descrip); i++, dp++)
    if(dp->kilobytes == format)
      break;
  if(i == sizeof table / sizeof(struct descrip))
    errx(1, "cannot find format description for %d KB", format);

  /* prepare and write the boot sector */
  setup_boot_sector_from_template(&bs, dp);

  /* if we've got an explicit label, use it */
  if(label)
    strncpy(bs.bsec.variable_part.extended.label, label, 11);
  
  if(write(fd, (char *)bs.raw, sizeof bs) != sizeof bs)
    err(1, "boot sector write()");

  /* now, go on with the FATs */
  if((fat = (struct fat *)malloc(dp->sectsiz * dp->fatsize)) == 0)
    abort();
  memset((void *)fat, 0, dp->sectsiz * dp->fatsize);
  
  fat->media = dp->media;
  fat->padded = 0xff;
  fat->contents[0] = 0xff;
  if(dp->totsecs > 20740 || (dp->totsecs == 0 && dp->ext_totsecs > 20740))
    /* 16-bit FAT */
    fat->contents[1] = 0xff;

  for(i = 0; i < dp->fatcnt; i++)
    if(write(fd, (char *)fat, dp->sectsiz * dp->fatsize)
       != dp->sectsiz * dp->fatsize)
      err(1, "FAT write()");

  free((void *)fat);
  
  /* finally, build the root dir */
  rootdirsize = dp->rootsiz * sizeof(struct direntry);
  roundup(rootdirsize, dp->clustsiz * dp->sectsiz);

  if((rootdir = (struct direntry *)malloc(rootdirsize)) == 0)
    abort();
  memset((void *)fat, 0, rootdirsize);

  /* set up a volume label inside the root dir :) */
  if(label)
    strncpy(rootdir[0].name, label, 11);
  else
    memcpy(rootdir[0].name, dp->ext_label, 11);
  rootdir[0].attr = FA_VOLLABEL;
  now = time((time_t)0);
  tp = localtime(&now);
  rootdir[0].fdate.time[0] = tp->tm_sec / 2;
  rootdir[0].fdate.time[0] |= (tp->tm_min & 7) << 5;
  rootdir[0].fdate.time[1] = ((tp->tm_min >> 3) & 7);
  rootdir[0].fdate.time[1] |= tp->tm_hour << 3;
  rootdir[0].fdate.date[0] = tp->tm_mday;
  rootdir[0].fdate.date[0] |= ((tp->tm_mon + 1) & 7) << 5;
  rootdir[0].fdate.date[1] = ((tp->tm_mon + 1) >> 3) & 1;
  rootdir[0].fdate.date[1] |= (tp->tm_year - 80) << 1;

  if(write(fd, (char *)rootdir, rootdirsize) != rootdirsize)
    err(1, "root dir write()");
  
  (void)close(fd);
  
  return 0;
}

