/* hostname.c -- use uname() to get the name of the host
   Copyright (C) 1992 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.  */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(STDC_HEADERS) || defined(USG)
#include <string.h>
#ifndef index
#define	index strchr
#endif
#else
#include <strings.h>
#endif

#include <sys/utsname.h>

/* Put this host's name into NAME, using at most NAMELEN characters */

int
gethostname(name, namelen)
     char *name;
     int namelen;
{
  struct utsname ugnm;

  if (uname(&ugnm) < 0)
    return (-1);

  (void) strncpy(name, ugnm.nodename, namelen-1);
  name[namelen-1] = '\0';

  return (0);
}
