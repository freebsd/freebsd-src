/*
 * Copyright (c) 1992, 1995 Hellmuth Michaelis
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Hellmuth Michaelis
 * 4. The name authors may not be used to endorse or promote products
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
 *
 */

static char *id =
	"@(#)ispcvt.c, 3.20, Last Edit-Date: [Wed Apr  5 17:53:28 1995]";

/*---------------------------------------------------------------------------*
 *
 *	history:
 *
 *	-hm	upgraded to report pcvt compile time configuration
 *	-hm	PCVT_INHIBIT_NUMLOCK patch from Joerg
 *	-hm	PCVT_META_ESC patch from Joerg
 *	-hm	PCVT_PCBURST
 *	-hm	new ioctl VGAPCVTINFO
 *	-hm	new CONF_ values for 3.10
 *	-hm	new CONF_ values for 3.20
 *	-hm	removed PCVT_FAKE_SYSCONS10
 *	-hm	added PCVT_PORTIO_DELAY
 *	-hm	removed PCVT_386BSD
 *	-hm	add -d option to specify a device
 *	-hm	PCVT_XSERVER -> XSERVER
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <fcntl.h>
#include <machine/pcvt_ioctl.h>

#define DEFAULTFD 0

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern int opterr;
	extern char *optarg;

	struct pcvtid pcvtid;
	struct pcvtinfo pcvtinfo;
	int c;
	char *p;
	int verbose = 0;
	int config = 0;
	int dflag = 0;
	int fd;
	char *device;

	while( (c = getopt(argc, argv, "vcd:")) !=  -1)
	{
		switch(c)
		{
			case 'v':
				verbose = 1;
				break;

			case 'c':
				config = 1;
				break;

			case 'd':
				device = optarg;
				dflag = 1;
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
			{
				char buffer[80];
				strcpy(buffer,"ERROR opening ");
				strcat(buffer,device);
				perror(buffer);
			}
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
			perror("ispcvt - ioctl VGAPCVTID failed, error");
		exit(1);
	}

	if(!strcmp(pcvtid.name, PCVTIDNAME))
	{
		if(pcvtid.rmajor == PCVTIDMAJOR)
		{
			if(pcvtid.rminor != PCVTIDMINOR)
			{
				if(verbose)
					fprintf(stderr,"ispcvt - minor revision: expected %d, got %d\n", PCVTIDMINOR, pcvtid.rminor);
				exit(4);	/* minor revision mismatch */
			}
		}
		else
		{
			if(verbose)
				fprintf(stderr,"ispcvt - major revision: expected %d, got %d\n", PCVTIDMAJOR, pcvtid.rmajor);
			exit(3);	/* major revision mismatch */
		}
	}
	else
	{
		if(verbose)
			fprintf(stderr,"ispcvt - name check: expected %s, got %s\n", PCVTIDNAME, pcvtid.name);
		exit(2);	/* name mismatch */
	}

	if(verbose)
	{
		fprintf(stderr,"\nispcvt: kernel and utils match, driver name [%s], release [%1.1d.%02.2d]\n\n",pcvtid.name,pcvtid.rmajor,pcvtid.rminor);
	}

	if(config == 0)
		exit(0);

	if(ioctl(fd, VGAPCVTINFO, &pcvtinfo) == -1)
	{
		if(verbose)
			perror("ispcvt - ioctl VGAPCVTINFO failed, error");
		exit(1);
	}

	if(verbose)
	{
		switch(pcvtinfo.opsys)
		{
			case CONF_NETBSD:
				p = "PCVT_NETBSD";
				break;

			case CONF_FREEBSD:
				p = "PCVT_FREEBSD";
				break;

			default:
			case CONF_UNKNOWNOPSYS:
				p = "UNKNOWN";
				break;

		}
		fprintf(stderr,"Operating System     = %s\t", p);
		fprintf(stderr,"OS Release Id        = %u\n", pcvtinfo.opsysrel);
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
		fprintf(stderr,"PCVT_BACKUP_FONTS    = %s",
			(pcvtinfo.compile_opts & CONF_BACKUP_FONTS) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_CTRL_ALT_DEL    = %s",
			(pcvtinfo.compile_opts & CONF_CTRL_ALT_DEL) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_EMU_MOUSE       = %s",
			(pcvtinfo.compile_opts & CONF_EMU_MOUSE) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_INHIBIT_NUMLOCK = %s",
			(pcvtinfo.compile_opts & CONF_INHIBIT_NUMLOCK) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_KEYBDID         = %s",
			(pcvtinfo.compile_opts & CONF_KEYBDID) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_KBD_FIFO        = %s",
			(pcvtinfo.compile_opts & CONF_KBD_FIFO) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_META_ESC        = %s",
			(pcvtinfo.compile_opts & CONF_META_ESC) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_NOFASTSCROLL    = %s",
			(pcvtinfo.compile_opts & CONF_NOFASTSCROLL) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_NO_LED_UPDATE   = %s",
			(pcvtinfo.compile_opts & CONF_NO_LED_UPDATE) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_NULLCHARS       = %s",
			(pcvtinfo.compile_opts & CONF_NULLCHARS) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_PALFLICKER      = %s",
			(pcvtinfo.compile_opts & CONF_PALFLICKER) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_PORTIO_DELAY    = %s",
			(pcvtinfo.compile_opts & CONF_PORTIO_DELAY) ? "ON" : "OFF");
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
		fprintf(stderr,"PCVT_SIGWINCH        = %s",
			(pcvtinfo.compile_opts & CONF_SIGWINCH) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SLOW_INTERRUPT  = %s",
			(pcvtinfo.compile_opts & CONF_SLOW_INTERRUPT) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_SW0CNOUTP       = %s",
			(pcvtinfo.compile_opts & CONF_SW0CNOUTP) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_USEKBDSEC       = %s",
			(pcvtinfo.compile_opts & CONF_USEKBDSEC) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_USL_VT_COMPAT   = %s",
			(pcvtinfo.compile_opts & CONF_USL_VT_COMPAT) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_VT220KEYB       = %s",
			((u_int)pcvtinfo.compile_opts & (u_int)CONF_VT220KEYB) ? "ON" : "OFF");
		next();
		fprintf(stderr,"PCVT_WAITRETRACE     = %s",
			(pcvtinfo.compile_opts & CONF_WAITRETRACE) ? "ON" : "OFF");
		next();
		fprintf(stderr,"XSERVER              = %s",
			(pcvtinfo.compile_opts & CONF_XSERVER) ? "ON" : "OFF");

		fprintf(stderr,"\n\n");
	}
	else /* !verbose */
	{
		fprintf(stderr,"BSD Version      = %u\n", pcvtinfo.opsys);
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
	fprintf(stderr,"  usage: ispcvt [-v] [-c] [-d device]\n");
	fprintf(stderr,"options: -v         be verbose\n");
	fprintf(stderr,"         -c         print compile time configuration\n");
	fprintf(stderr,"         -d <name>  use devicefile <name> for verification\n\n");
	exit(5);
}

next()
{
	static int i = 0;

	fprintf(stderr, "%s", (i == 0) ? "\t\t" : "\n");

	i = ~i;
}

/* EOF */
