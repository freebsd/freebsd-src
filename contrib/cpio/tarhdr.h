/* Extended tar header from POSIX.1.
   Copyright (C) 1992 Free Software Foundation, Inc.

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

#ifndef _TARHDR_H

#define _TARHDR_H 1

#include <tar.h>

/* Size of `name' field.  */
#define TARNAMESIZE 100

/* Size of `linkname' field.  */
#define TARLINKNAMESIZE 100

/* Size of `prefix' field.  */
#define TARPREFIXSIZE 155

/* Size of entire tar header.  */
#define TARRECORDSIZE 512

struct tar_header
{
  char name[TARNAMESIZE];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[TARLINKNAMESIZE];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[TARPREFIXSIZE];
};

union tar_record
{
  struct tar_header header;
  char buffer[TARRECORDSIZE];
};

#endif /* tarhdr.h */
