/* sgsc(1) - utility for the `gsc' scanner device driver
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

#ifndef lint
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/gsc.h>

#ifndef DEFAULT_FILE
#define DEFAULT_FILE "/dev/gsc0"
#endif
#ifdef FAIL
#undef FAIL
#endif
#define FAIL -1

static void
usage()
{
  fprintf(stderr, "%s\n%s\n",
	"usage: sgsc [-sq] [-f file] [-r dpi] [-w width] [-h height]",
	"            [-b len] [-t time]");
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

  if (argc == 0) usage();

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
      default: usage();
      }
    }

  fd = open(file, O_RDONLY);
  if ( fd == FAIL )
    err(1, "%s", file);

  if (set_switch != 0)
    {
      if(ioctl(fd, GSC_SRESSW) == FAIL)
	  err(1, "GSC_SRESSW");
    }

  if (set_dpi != 0)
    {
      if(ioctl(fd, GSC_SRES, &set_dpi) == FAIL)
	  err(1, "GSC_SRES");
    }

  if (set_width != 0)
    {
      if(ioctl(fd, GSC_SWIDTH, &set_width) == FAIL)
	  err(1, "GSC_SWIDTH");
    }

  if (set_height != 0)
    {
      if(ioctl(fd, GSC_SHEIGHT, &set_height) == FAIL)
	  err(1, "GSC_SHEIGHT");
    }

  if (set_blen != 0)
    {
      if(ioctl(fd, GSC_SBLEN, &set_blen) == FAIL)
	  err(1, "GSC_SBLEN");
    }

  if (set_btime != 0)
    {
      if(ioctl(fd, GSC_SBTIME, &set_btime) == FAIL)
	  err(1, "GSC_SBTIME");
    }

  if (show_all != 0)
    {
      if(ioctl(fd, GSC_GRES,  &show_dpi) == FAIL)
	  err(1, "GSC_GRES");
      if(ioctl(fd, GSC_GWIDTH,  &show_width) == FAIL)
	  err(1, "GSC_GWIDTH");
      if(ioctl(fd, GSC_GHEIGHT,  &show_height) == FAIL)
	  err(1, "GSC_GHEIGHT");
      if(ioctl(fd, GSC_GBLEN, &show_blen) == FAIL)
	  err(1, "GSC_GBLEN");
      if(ioctl(fd, GSC_GBTIME, &show_btime) == FAIL)
	  err(1, "GSC_GBTIME");

      printf("%s:\n", file);
      printf("resolution\t %d dpi\n", show_dpi);
      printf("width\t\t %d\n", show_width);
      printf("height\t\t %d\n",show_height);
      printf("buffer length\t %d\n", show_blen);
      printf("buffer timeout\t %d\n", show_btime);
    }

  return 0;
}
