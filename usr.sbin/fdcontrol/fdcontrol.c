/*
 * Copyright (C) 1994 by Joerg Wunsch, Dresden
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef lint
static const char rcsid[] =
	"$Id: fdcontrol.c,v 1.2.6.2 1997/09/18 06:29:16 charnier Exp $";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <machine/ioctl_fd.h>
#include <sys/file.h>

int
getnumber(void)
{
  int i;
  char b[80];

  fgets(b, 80, stdin);
  if(b[0] == '\n') return -1;

  sscanf(b, " %i", &i);
  return i;
}

void
usage(void)
{
  fprintf(stderr, "usage: fdcontrol [-d 0|1] | [-s] device-node\n");
  exit(2);
}


#define ask(name, fmt) \
printf(#name "? [" fmt "]: ", ft.name); fflush(stdout);   \
if((i = getnumber()) != -1) ft.name = i

int
main(int argc, char **argv)
{
  struct fd_type ft;
  int fd, i;
  int debug = -1, settype = 1;

  while((i = getopt(argc, argv, "d:s")) != -1)
    switch(i)
      {
      case 'd':
	debug = atoi(optarg);
	settype = 0;
	break;

      case 's':
	debug = -1;
	settype = 1;
	break;

      case '?':
      default:
	usage();
      }

  argc -= optind;
  argv += optind;

  if(argc != 1)
    usage();

  if((fd = open(argv[0], 0)) < 0)
    {
      warn("open(floppy)");
      return 1;
    }

  if(debug != -1)
    {
      if(ioctl(fd, FD_DEBUG, &debug) < 0)
	{
	  warn("ioctl(FD_DEBUG)");
	  return 1;
	}
      return 0;
    }

  if(settype)
    {
      if(ioctl(fd, FD_GTYPE, &ft) < 0)
	{
	  warn("ioctl(FD_GTYPE)");
	  return 1;
	}

      ask(sectrac, "%d");
      ask(secsize, "%d");
      ask(datalen, "0x%x");
      ask(gap, "0x%x");
      ask(tracks, "%d");
      ask(size, "%d");
      ask(steptrac, "%d");
      ask(trans, "%d");
      ask(heads, "%d");
      ask(f_gap, "0x%x");
      ask(f_inter, "%d");

      if(ioctl(fd, FD_STYPE, &ft) < 0)
	{
	  warn("ioctl(FD_STYPE)");
	  return 1;
	}
      return 0;
    }

  return 0;
}
