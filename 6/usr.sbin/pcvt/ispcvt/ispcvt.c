/*
 * Copyright (c) 1992, 2000 Hellmuth Michaelis
 *
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
 *
 */

/*---------------------------------------------------------------------------*
 *
 *	ispcvt - check for pcvt driver running and its options
 *	------------------------------------------------------
 *
 *	Last Edit-Date: [Fri Mar 31 10:24:43 2000]
 *
 * $FreeBSD$
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <err.h>
#include <fcntl.h>
#include <unistd.h>
#include <machine/pcvt_ioctl.h>

#define DEFAULTFD 0

main(argc,argv)
int argc;
char *argv[];
{
	struct pcvtid pcvtid;
	struct pcvtinfo pcvtinfo;
	int c;
	char *p;
	int verbose = 0;
	int config = 0;
	int dflag = 0;
	int n_screens = 0;
	int fd;
	char *device;

	while( (c = getopt(argc, argv, "cd:nv")) != -1)
	{
		switch(c)
		{
			case 'c':
				config = 1;
				break;

			case 'd':
				device = optarg;
				dflag = 1;
				break;

			case 'n':
				n_screens = 1;
				break;

			case 'v':
				verbose = 1;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if(dflag)
	{
		if((fd = open(device, O_RDWR)) == -1)
		{
			if(verbose)
				warn("ERROR opening %s", device);
			exit(1);
		}
	}
	else
	{
		fd = DEFAULTFD;
	}

	if(ioctl(fd, VGAPCVTID, &pcvtid) == -1)
	{
		if(verbose)
			warn("ioctl VGAPCVTID failed, error");
		exit(1);
	}

	if(!strcmp(pcvtid.name, PCVTIDNAME))
	{
		if(pcvtid.rmajor == PCVTIDMAJOR)
		{
			if(pcvtid.rminor != PCVTIDMINOR)
			{
				if(verbose)
					warnx("minor revision: expected %d, got %d", PCVTIDMINOR, pcvtid.rminor);
				exit(4);	/* minor revision mismatch */
			}
		}
		else
		{
			if(verbose)
				warnx("major revision: expected %d, got %d", PCVTIDMAJOR, pcvtid.rmajor);
			exit(3);	/* major revision mismatch */
		}
	}
	else
	{
		if(verbose)
			warnx("name check: expected %s, got %s", PCVTIDNAME, pcvtid.name);
		exit(2);	/* name mismatch */
	}

	if(verbose)
	{
		warnx("\nkernel and utils match, driver name [%s], release [%1.1d.%02.2d]\n",
			  pcvtid.name, pcvtid.rmajor, pcvtid.rminor);
	}

	if(config == 0 && n_screens == 0)
		exit(0);

	if(ioctl(fd, VGAPCVTINFO, &pcvtinfo) == -1)
	{
		if(verbose)
			warn("ioctl VGAPCVTINFO failed, error");
		exit(1);
	}

	if(n_screens)
	{
		printf("%d", pcvtinfo.nscreens);
		exit(0);
	}

	if(verbose)
	{
		fprintf(stderr,"PCVT_NSCREENS        = %u\t\t", pcvtinfo.nscreens);
		fprintf(stderr,"PCVT_UPDATEFAST      = %u\n", pcvtinfo.updatefast);
		fprintf(stderr,"PCVT_UPDATESLOW      = %u\t\t", pcvtinfo.updateslow);
		fprintf(stderr,"PCVT_SYSBEEPF        = %u\n", pcvtinfo.sysbeepf);
		fprintf(stderr,"PCVT_PCBURST         = %u\t\t", pcvtinfo.pcburst);
		fprintf(stderr,"PCVT_KBD_FIFO_SZ     = %u\n\n", pcvtinfo.kbd_fifo_sz);

		/* config booleans */

		fprintf(stderr,"PCVT_132GENERIC      = %s",
			(pcvtinfo.compile_opts & CONF_132GENERIC) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_24LINESDEF      = %s",
			(pcvtinfo.compile_opts & CONF_24LINESDEF) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_CTRL_ALT_DEL    = %s",
			(pcvtinfo.compile_opts & CONF_CTRL_ALT_DEL) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_INHIBIT_NUMLOCK = %s",
			(pcvtinfo.compile_opts & CONF_INHIBIT_NUMLOCK) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_META_ESC        = %s",
			(pcvtinfo.compile_opts & CONF_META_ESC) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_NO_LED_UPDATE   = %s",
			(pcvtinfo.compile_opts & CONF_NO_LED_UPDATE) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_NULLCHARS       = %s",
			(pcvtinfo.compile_opts & CONF_NULLCHARS) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_PRETTYSCRNS     = %s",
			(pcvtinfo.compile_opts & CONF_PRETTYSCRNS) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SCREENSAVER     = %s",
			(pcvtinfo.compile_opts & CONF_SCREENSAVER) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SETCOLOR        = %s",
			(pcvtinfo.compile_opts & CONF_SETCOLOR) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SHOWKEYS        = %s",
			(pcvtinfo.compile_opts & CONF_SHOWKEYS) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SLOW_INTERRUPT  = %s",
			(pcvtinfo.compile_opts & CONF_SLOW_INTERRUPT) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_USEKBDSEC       = %s",
			(pcvtinfo.compile_opts & CONF_USEKBDSEC) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_VT220KEYB       = %s",
			((u_int)pcvtinfo.compile_opts & (u_int)CONF_VT220KEYB) ? "ON" : "OFF");
		next();
		fprintf(stderr,"XSERVER              = %s",
			(pcvtinfo.compile_opts & CONF_XSERVER) ? "ON" : "OFF");

		next();
		fprintf(stderr,"PCVT_GREENSAVER      = %s",
			(pcvtinfo.compile_opts & CONF_GREENSAVER) ? "ON" : "OFF");

		fprintf(stderr,"\n\n");
	}
	else /* !verbose */
	{
		fprintf(stderr,"PCVT_NSCREENS    = %u\n", pcvtinfo.nscreens);
		fprintf(stderr,"PCVT_UPDATEFAST  = %u\n", pcvtinfo.updatefast);
		fprintf(stderr,"PCVT_UPDATESLOW  = %u\n", pcvtinfo.updateslow);
		fprintf(stderr,"PCVT_SYSBEEPF    = %u\n", pcvtinfo.sysbeepf);
		fprintf(stderr,"Compile options  = 0x%08X\n", pcvtinfo.compile_opts);
	}
}

usage()
{
	fprintf(stderr,"\nispcvt - verify current video driver is the pcvt-driver\n");
	fprintf(stderr,"  usage: ispcvt [-c] [-d device] [-n] [-v]\n");
	fprintf(stderr,"options: -c         print compile time configuration\n");
	fprintf(stderr,"         -d <name>  use devicefile <name>\n");
	fprintf(stderr,"         -n         print number of virtual screens (to stdout)\n");
	fprintf(stderr,"         -v         be verbose\n\n");
	exit(5);
}

next()
{
	static int i = 0;

	fprintf(stderr, "%s", (i == 0) ? "\t\t" : "\n");

	i = ~i;
}

/* EOF */
