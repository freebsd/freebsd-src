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
 * Create an MS-DOS (FAT) file system.
 *
 * $Id$
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <memory.h>
#include <err.h>
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
  /*
   * kilobytes
   * sec cls res fat  rot   tot   med fsz spt hds hid
   * tot phs ebt    label          fsysid
   */
  { 720,
     512,  2,  1,  2, 112, 1440, 0xf9,  3,  9,  2,  0,
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  {1440,
     512,  1,  1,  2, 224, 2880, 0xf0,  9, 18,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  { 360,
     512,  2,  1,  2, 112,  720, 0xfd,  2,  9,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
  {1200,
     512,  1,  1,  2, 224, 2400, 0xf9,  7, 15,  2,  0,    
       0,  0,  0,  "4.4BSD     ", "FAT12   "},
};

void
usage(void)
{
  fprintf(stderr, "usage: ");
  errx(2, "[-f kbytes] [-L label] device");
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
  
  while((c = getopt(argc, argv, "f:L:")) != EOF)
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

  for(i = 0, dp = table; i < sizeof table / sizeof(struct descrip); i++, dp++)
    if(dp->kilobytes == format)
      break;
  if(i == sizeof table / sizeof(struct descrip))
    errx(1, "cannot find format description for %d KB", format);
  
  if((fd = open(argv[0], O_RDWR|O_EXCL, 0)) == -1)
    err(1, "open(%s)", argv[0]);

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

