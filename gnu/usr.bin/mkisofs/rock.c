/*
 * File rock.c - generate RRIP  records for iso9660 filesystems.

   Written by Eric Youngdale (1993).

   Copyright 1993 Yggdrasil Computing, Incorporated

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdlib.h>

#ifndef VMS
#if defined(HASSYSMACROS) && !defined(HASMKDEV)
#include <sys/sysmacros.h>
#endif
#include <unistd.h>
#endif
#ifdef HASMKDEV
#include <sys/types.h>
#include <sys/mkdev.h>
#endif

#include "mkisofs.h"
#include "iso9660.h"
#include <string.h>

#ifdef NON_UNIXFS
#define S_ISLNK(m)	(0)
#else
#ifndef S_ISLNK
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#endif
#endif

#define SU_VERSION 1

#define SL_ROOT    8
#define SL_PARENT  4
#define SL_CURRENT 2
#define SL_CONTINUE 1

#define CE_SIZE 28
#define CL_SIZE 12
#define ER_SIZE 8
#define NM_SIZE 5
#define PL_SIZE 12
#define PN_SIZE 20
#define PX_SIZE 36
#define RE_SIZE 4
#define SL_SIZE 20
#define ZZ_SIZE 15
#ifdef __QNX__
#define TF_SIZE (5 + 4 * 7)
#else
#define TF_SIZE (5 + 3 * 7)
#endif

/* If we need to store this number of bytes, make sure we
   do not box ourselves in so that we do not have room for
   a CE entry for the continuation record */

#define MAYBE_ADD_CE_ENTRY(BYTES) \
    (BYTES + CE_SIZE + currlen + (ipnt - recstart) > reclimit ? 1 : 0)

/*
 * Buffer to build RR attributes
 */

static unsigned char Rock[16384];
static unsigned char symlink_buff[256];
static int ipnt = 0;
static int recstart = 0;
static int currlen = 0;
static int mainrec = 0;
static int reclimit;

static add_CE_entry(){
          if(recstart)
	    set_733((char*)Rock + recstart - 8, ipnt + 28 - recstart);
	  Rock[ipnt++] ='C';
	  Rock[ipnt++] ='E';
	  Rock[ipnt++] = CE_SIZE;
	  Rock[ipnt++] = SU_VERSION;
	  set_733((char*)Rock + ipnt, 0);
	  ipnt += 8;
	  set_733((char*)Rock + ipnt, 0);
	  ipnt += 8;
	  set_733((char*)Rock + ipnt, 0);
	  ipnt += 8;
	  recstart = ipnt;
	  currlen = 0;
	  if(!mainrec) mainrec = ipnt;
	  reclimit = SECTOR_SIZE - 8; /* Limit to one sector */
}

#ifdef __STDC__
int generate_rock_ridge_attributes (char * whole_name, char * name,
				    struct directory_entry * s_entry,
				    struct stat * statbuf,
				    struct stat * lstatbuf,
				    int deep_opt)
#else
int generate_rock_ridge_attributes (whole_name, name,
				    s_entry,
				    statbuf,
				    lstatbuf,
				    deep_opt)
char * whole_name; char * name; struct directory_entry * s_entry;
struct stat * statbuf, *lstatbuf;
int deep_opt;
#endif
{
  int flagpos, flagval;
  int need_ce;

  statbuf = statbuf;        /* this shuts up unreferenced compiler warnings */
  mainrec = recstart = ipnt = 0;
  reclimit = 0xf8;

  /* Obtain the amount of space that is currently used for the directory
     record.  Assume max for name, since name conflicts may cause us
     to rename the file later on */
  currlen = sizeof(s_entry->isorec);

  /* Identify that we are using the SUSP protocol */
  if(deep_opt & NEED_SP){
	  Rock[ipnt++] ='S';
	  Rock[ipnt++] ='P';
	  Rock[ipnt++] = 7;
	  Rock[ipnt++] = SU_VERSION;
	  Rock[ipnt++] = 0xbe;
	  Rock[ipnt++] = 0xef;
	  Rock[ipnt++] = 0;
  };

  /* First build the posix name field */
  Rock[ipnt++] ='R';
  Rock[ipnt++] ='R';
  Rock[ipnt++] = 5;
  Rock[ipnt++] = SU_VERSION;
  flagpos = ipnt;
  flagval = 0;
  Rock[ipnt++] = 0;   /* We go back and fix this later */

  if(strcmp(name,".")  && strcmp(name,"..")){
    char * npnt;
    int remain, use;

    remain = strlen(name);
    npnt = name;

    while(remain){
          use = remain;
	  need_ce = 0;
	  /* Can we fit this SUSP and a CE entry? */
	  if(use + currlen + CE_SIZE + (ipnt - recstart) > reclimit) {
	    use = reclimit - currlen - CE_SIZE - (ipnt - recstart);
	    need_ce++;
	  }

	  /* Only room for 256 per SUSP field */
	  if(use > 0xf8) use = 0xf8;

	  /* First build the posix name field */
	  Rock[ipnt++] ='N';
	  Rock[ipnt++] ='M';
	  Rock[ipnt++] = NM_SIZE + use;
	  Rock[ipnt++] = SU_VERSION;
	  Rock[ipnt++] = (remain != use ? 1 : 0);
	  flagval |= (1<<3);
	  strncpy((char *)&Rock[ipnt], npnt, use);
	  npnt += use;
	  ipnt += use;
	  remain -= use;
	  if(remain && need_ce) add_CE_entry();
	};
  };

  /*
   * Add the posix modes
   */
  if(MAYBE_ADD_CE_ENTRY(PX_SIZE)) add_CE_entry();
  Rock[ipnt++] ='P';
  Rock[ipnt++] ='X';
  Rock[ipnt++] = PX_SIZE;
  Rock[ipnt++] = SU_VERSION;
  flagval |= (1<<0);
  set_733((char*)Rock + ipnt, lstatbuf->st_mode);
  ipnt += 8;
  set_733((char*)Rock + ipnt, lstatbuf->st_nlink);
  ipnt += 8;
  set_733((char*)Rock + ipnt, lstatbuf->st_uid);
  ipnt += 8;
  set_733((char*)Rock + ipnt, lstatbuf->st_gid);
  ipnt += 8;

  /*
   * Check for special devices
   */
#ifndef NON_UNIXFS
  if (S_ISCHR(lstatbuf->st_mode) || S_ISBLK(lstatbuf->st_mode)) {
    if(MAYBE_ADD_CE_ENTRY(PN_SIZE)) add_CE_entry();
    Rock[ipnt++] ='P';
    Rock[ipnt++] ='N';
    Rock[ipnt++] = PN_SIZE;
    Rock[ipnt++] = SU_VERSION;
    flagval |= (1<<1);
    if(sizeof(dev_t) <= 4) {
        set_733((char*)Rock + ipnt, 0);
        ipnt += 8;
        set_733((char*)Rock + ipnt, lstatbuf->st_rdev);
        ipnt += 8;
    }
    else {
#if defined(__BSD__)
        set_733((char*)Rock + ipnt, (lstatbuf->st_rdev >> 16) >> 16);
#else
        set_733((char*)Rock + ipnt, lstatbuf->st_rdev >> 32);
#endif
        ipnt += 8;
        set_733((char*)Rock + ipnt, lstatbuf->st_rdev);
        ipnt += 8;
    }
  };
#endif
  /*
   * Check for and symbolic links.  VMS does not have these.
   */
  if (S_ISLNK(lstatbuf->st_mode)){
    int lenpos, lenval, j0, j1;
    int cflag, nchar;
    unsigned char * cpnt, *cpnt1;
    nchar = readlink(whole_name, symlink_buff, sizeof(symlink_buff));
    symlink_buff[nchar] = 0;
    set_733(s_entry->isorec.size, 0);
    cpnt = &symlink_buff[0];
    flagval |= (1<<2);

    while(nchar){
      if(MAYBE_ADD_CE_ENTRY(SL_SIZE)) add_CE_entry();
      Rock[ipnt++] ='S';
      Rock[ipnt++] ='L';
      lenpos = ipnt;
      Rock[ipnt++] = SL_SIZE;
      Rock[ipnt++] = SU_VERSION;
      Rock[ipnt++] = 0; /* Flags */
      lenval = 5;
      while(*cpnt){
	cpnt1 = (unsigned char *) strchr((char *) cpnt, '/');
	if(cpnt1) {
	  nchar--;
	  *cpnt1 = 0;
	};

	/* We treat certain components in a special way.  */
	if(cpnt[0] == '.' && cpnt[1] == '.' && cpnt[2] == 0){
	  if(MAYBE_ADD_CE_ENTRY(2)) add_CE_entry();
	  Rock[ipnt++] = SL_PARENT;
	  Rock[ipnt++] = 0;  /* length is zero */
	  lenval += 2;
	  nchar -= 2;
	} else if(cpnt[0] == '.' && cpnt[1] == 0){
	  if(MAYBE_ADD_CE_ENTRY(2)) add_CE_entry();
	  Rock[ipnt++] = SL_CURRENT;
	  Rock[ipnt++] = 0;  /* length is zero */
	  lenval += 2;
	  nchar -= 1;
	} else if(cpnt[0] == 0){
	  if(MAYBE_ADD_CE_ENTRY(2)) add_CE_entry();
	  Rock[ipnt++] = (cpnt == &symlink_buff[0] ? SL_ROOT : 0);
	  Rock[ipnt++] = 0;  /* length is zero */
	  lenval += 2;
	} else {
	  /* If we do not have enough room for a component, start
	     a new continuations segment now */
	  if(MAYBE_ADD_CE_ENTRY(6)) {
	    add_CE_entry();
	    if(cpnt1){
	      *cpnt1 = '/';
	      cpnt1 = NULL; /* A kluge so that we can restart properly */
	    }
	    break;
	  }
	  j0 = strlen((char *) cpnt);
	  while(j0) {
	    j1 = j0;
	    if(j1 > 0xf8) j1 = 0xf8;
	    need_ce = 0;
	    if(j1 + currlen + CE_SIZE + (ipnt - recstart) > reclimit) {
	      j1 = reclimit - currlen - CE_SIZE - (ipnt - recstart);
	      need_ce++;
	    }
	    Rock[ipnt++] = (j1 != j0 ? SL_CONTINUE : 0);
	    Rock[ipnt++] = j1;
	    strncpy((char *) Rock + ipnt, (char *) cpnt, j1);
	    ipnt += j1;
	    lenval += j1 + 2;
	    cpnt += j1;
	    nchar -= j1;  /* Number we processed this time */
	    j0 -= j1;
	    if(need_ce) {
	      add_CE_entry();
	      if(cpnt1) {
		*cpnt1 = '/';
		cpnt1 = NULL; /* A kluge so that we can restart properly */
	      }
	      break;
	    }
	  }
	};
	if(cpnt1) {
	  cpnt = cpnt1 + 1;
	} else
	  break;
      };
      Rock[lenpos] = lenval;
      if(nchar) Rock[lenpos + 2] = SL_CONTINUE; /* We need another SL entry */
    } /* while nchar */
  } /* Is a symbolic link */
  /*
   * Add in the Rock Ridge TF time field
   */
  if(MAYBE_ADD_CE_ENTRY(TF_SIZE)) add_CE_entry();
  Rock[ipnt++] ='T';
  Rock[ipnt++] ='F';
  Rock[ipnt++] = TF_SIZE;
  Rock[ipnt++] = SU_VERSION;
#ifdef __QNX__
  Rock[ipnt++] = 0x0f;
#else
  Rock[ipnt++] = 0x0e;
#endif
  flagval |= (1<<7);
#ifdef __QNX__
  iso9660_date((char *) &Rock[ipnt], lstatbuf->st_ftime);
  ipnt += 7;
#endif
  iso9660_date((char *) &Rock[ipnt], lstatbuf->st_mtime);
  ipnt += 7;
  iso9660_date((char *) &Rock[ipnt], lstatbuf->st_atime);
  ipnt += 7;
  iso9660_date((char *) &Rock[ipnt], lstatbuf->st_ctime);
  ipnt += 7;

  /*
   * Add in the Rock Ridge RE time field
   */
  if(deep_opt & NEED_RE){
          if(MAYBE_ADD_CE_ENTRY(RE_SIZE)) add_CE_entry();
	  Rock[ipnt++] ='R';
	  Rock[ipnt++] ='E';
	  Rock[ipnt++] = RE_SIZE;
	  Rock[ipnt++] = SU_VERSION;
	  flagval |= (1<<6);
  };
  /*
   * Add in the Rock Ridge PL record, if required.
   */
  if(deep_opt & NEED_PL){
          if(MAYBE_ADD_CE_ENTRY(PL_SIZE)) add_CE_entry();
	  Rock[ipnt++] ='P';
	  Rock[ipnt++] ='L';
	  Rock[ipnt++] = PL_SIZE;
	  Rock[ipnt++] = SU_VERSION;
	  set_733((char*)Rock + ipnt, 0);
	  ipnt += 8;
	  flagval |= (1<<5);
  };

  /*
   * Add in the Rock Ridge CL field, if required.
   */
  if(deep_opt & NEED_CL){
          if(MAYBE_ADD_CE_ENTRY(CL_SIZE)) add_CE_entry();
	  Rock[ipnt++] ='C';
	  Rock[ipnt++] ='L';
	  Rock[ipnt++] = CL_SIZE;
	  Rock[ipnt++] = SU_VERSION;
	  set_733((char*)Rock + ipnt, 0);
	  ipnt += 8;
	  flagval |= (1<<4);
  };

#ifndef VMS
  /* If transparent compression was requested, fill in the correct
     field for this file */
  if(transparent_compression &&
     S_ISREG(lstatbuf->st_mode) &&
     strlen(name) > 3 &&
     strcmp(name + strlen(name) - 3,".gZ") == 0){
    FILE * zipfile;
    char * checkname;
    unsigned int file_size;
    unsigned char header[8];
    int OK_flag;

    /* First open file and verify that the correct algorithm was used */
    file_size = 0;
    OK_flag = 1;

    zipfile = fopen(whole_name, "r");
    fread(header, 1, sizeof(header), zipfile);

    /* Check some magic numbers from gzip. */
    if(header[0] != 0x1f || header[1] != 0x8b || header[2] != 8) OK_flag = 0;
    /* Make sure file was blocksized. */
    if((header[3] & 0x40 == 0)) OK_flag = 0;
    /* OK, now go to the end of the file and get some more info */
    if(OK_flag){
      int status;
      status = (long)lseek(fileno(zipfile), (off_t)(-8), SEEK_END);
      if(status == -1) OK_flag = 0;
    }
    if(OK_flag){
      if(read(fileno(zipfile), (char*)header, sizeof(header)) != sizeof(header))
	OK_flag = 0;
      else {
	int blocksize;
	blocksize = (header[3] << 8) | header[2];
	file_size = ((unsigned int)header[7] << 24) |
		    ((unsigned int)header[6] << 16) |
		    ((unsigned int)header[5] << 8)  | header[4];
#if 0
	fprintf(stderr,"Blocksize = %d %d\n", blocksize, file_size);
#endif
	if(blocksize != SECTOR_SIZE) OK_flag = 0;
      }
    }
    fclose(zipfile);

    checkname = strdup(whole_name);
    checkname[strlen(whole_name)-3] = 0;
    zipfile = fopen(checkname, "r");
    if(zipfile) {
      OK_flag = 0;
      fprintf(stderr,"Unable to insert transparent compressed file - name conflict\n");
      fclose(zipfile);
    }

    free(checkname);

    if(OK_flag){
      if(MAYBE_ADD_CE_ENTRY(ZZ_SIZE)) add_CE_entry();
      Rock[ipnt++] ='Z';
      Rock[ipnt++] ='Z';
      Rock[ipnt++] = ZZ_SIZE;
      Rock[ipnt++] = SU_VERSION;
      Rock[ipnt++] = 'g'; /* Identify compression technique used */
      Rock[ipnt++] = 'z';
      Rock[ipnt++] = 3;
      set_733((char*)Rock + ipnt, file_size); /* Real file size */
      ipnt += 8;
    };
  }
#endif
  /*
   * Add in the Rock Ridge CE field, if required.  We use  this for the
   * extension record that is stored in the root directory.
   */
  if(deep_opt & NEED_CE) add_CE_entry();
  /*
   * Done filling in all of the fields.  Now copy it back to a buffer for the
   * file in question.
   */

  /* Now copy this back to the buffer for the file */
  Rock[flagpos] = flagval;

  /* If there was a CE, fill in the size field */
  if(recstart)
    set_733((char*)Rock + recstart - 8, ipnt - recstart);

  s_entry->rr_attributes = (unsigned char *) e_malloc(ipnt);
  s_entry->total_rr_attr_size = ipnt;
  s_entry->rr_attr_size = (mainrec ? mainrec : ipnt);
  memcpy(s_entry->rr_attributes, Rock, ipnt);
  return ipnt;
}

/* Guaranteed to  return a single sector with the relevant info */

char * FDECL4(generate_rr_extension_record, char *, id,  char  *, descriptor,
				    char *, source, int  *, size){
  int ipnt = 0;
  char * pnt;
  int len_id, len_des, len_src;

  len_id = strlen(id);
  len_des =  strlen(descriptor);
  len_src = strlen(source);
  Rock[ipnt++] ='E';
  Rock[ipnt++] ='R';
  Rock[ipnt++] = ER_SIZE + len_id + len_des + len_src;
  Rock[ipnt++] = 1;
  Rock[ipnt++] = len_id;
  Rock[ipnt++] = len_des;
  Rock[ipnt++] = len_src;
  Rock[ipnt++] = 1;

  memcpy(Rock  + ipnt, id, len_id);
  ipnt += len_id;

  memcpy(Rock  + ipnt, descriptor, len_des);
  ipnt += len_des;

  memcpy(Rock  + ipnt, source, len_src);
  ipnt += len_src;

  if(ipnt  > SECTOR_SIZE) {
	  fprintf(stderr,"Extension record too  long\n");
	  exit(1);
  };
  pnt = (char *) e_malloc(SECTOR_SIZE);
  memset(pnt, 0,  SECTOR_SIZE);
  memcpy(pnt, Rock, ipnt);
  *size = ipnt;
  return pnt;
}
