/* sasc(1) - utility for the `asc' scanner device driver
 *
 *
 * Copyright (c) 1995 Gunther Schadow.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Gunther Schadow.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * $FreeBSD$
 */

#include <stdlib.h>
#include <stdio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/asc_ioctl.h>

#ifndef DEFAULT_FILE
#define DEFAULT_FILE "/dev/asc0"
#endif
#ifdef FAIL
#undef FAIL
#endif
#define FAIL -1

usage(char *progn)
{
  fprintf(stderr, "usage: %s [-sq] [-f file] [-r dpi] "
	  "[-w width] [-h height] "
	  "[-b len] [-t time]\n", progn);
  exit(1);
}

int
main(int argc, char **argv)
{
  char c;
  int fd;

  char *file = DEFAULT_FILE;
  
  int show_dpi     = 0;
  int show_width   = 0;
  int show_height  = 0;
  int show_blen    = 0;
  int show_btime   = 0;
  int show_all     = 1;

  int set_blen     = 0;
  int set_dpi      = 0;
  int set_width    = 0;
  int set_height   = 0;
  int set_btime    = 0;
  int set_switch   = 0;

  char *progn = *argv;
  
  if (argc == 0) usage(progn);

  while( (c = getopt(argc, argv, "sqf:b:r:w:h:t:")) != FAIL)
    {
      switch(c) {
      case 'f': file = optarg; break;
      case 'r': set_dpi = atoi(optarg); break;
      case 'w': set_width = atoi(optarg); break;
      case 'h': set_height = atoi(optarg); break;
      case 'b': set_blen = atoi(optarg); break;
      case 't': set_btime = atoi(optarg); break;
      case 's': set_switch = 1; break;
      case 'q': show_all = 0; break;
      default: usage(progn);
      }
    }

  fd = open(file, O_RDONLY);
  if ( fd == FAIL )
  {
    perror(file);
    exit(1);
  }

  if (set_switch != 0)
    {
      if(ioctl(fd, ASC_SRESSW) == FAIL)
	{
	  perror("ASC_SRESSW");
	  exit(1);
	}
    }

  if (set_dpi != 0)
    {
      if(ioctl(fd, ASC_SRES, &set_dpi) == FAIL)
	{
	  perror("ASC_SRES");
	  exit(1);
	}
    }

  if (set_width != 0)
    {
      if(ioctl(fd, ASC_SWIDTH, &set_width) == FAIL)
	{
	  perror("ASC_SWIDTH");
	  exit(1);
	}
    }

  if (set_height != 0)
    {
      if(ioctl(fd, ASC_SHEIGHT, &set_height) == FAIL)
	{
	  perror("ASC_SHEIGHT");
	  exit(1);
	}
    }

  if (set_blen != 0)
    {
      if(ioctl(fd, ASC_SBLEN, &set_blen) == FAIL)
	{
	  perror("ASC_SBLEN");
	  exit(1);
	}
    }

  if (set_btime != 0)
    {
      if(ioctl(fd, ASC_SBTIME, &set_btime) == FAIL)
	{
	  perror("ASC_SBTIME");
	  exit(1);
	}
    }

  if (show_all != 0)
    {
      if(ioctl(fd, ASC_GRES,  &show_dpi) == FAIL)
	{
	  perror("ASC_GRES");
	  exit(1);
	}
      if(ioctl(fd, ASC_GWIDTH,  &show_width) == FAIL)
	{
	  perror("ASC_GWIDTH");
	  exit(1);
	}
      if(ioctl(fd, ASC_GHEIGHT,  &show_height) == FAIL)
	{
	  perror("ASC_GHEIGHT");
	  exit(1);
	}
      if(ioctl(fd, ASC_GBLEN, &show_blen) == FAIL)
	{
	  perror("ASC_GBLEN");
	  exit(1);
	}
      if(ioctl(fd, ASC_GBTIME, &show_btime) == FAIL)
	{
	  perror("ASC_GBTIME");
	  exit(1);
	}

      printf("%s:\n", file);
      printf("resolution\t %d dpi\n", show_dpi);
      printf("width\t\t %d\n", show_width);
      printf("height\t\t %d\n",show_height);
      printf("buffer length\t %d\n", show_blen);
      printf("buffer timeout\t %d\n", show_btime);
    }

  return 0;
}
