/*
 * Copyright (c) 1992,1993,1994 Hellmuth Michaelis and Joerg Wunsch
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
 *	This product includes software developed by
 *	Hellmuth Michaelis and Joerg Wunsch
 * 4. The name authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

static char *id =
	"@(#)scon.c, 3.20, Last Edit-Date: [Sun Sep 25 12:33:21 1994]";

/*---------------------------------------------------------------------------*
 *
 *	history:
 *
 *	-hm	moving fd for default device from 1 -> 0 for such things
 *		as "scon -p list | more" to be possible
 *		(reported by Gordon L. Burditt, gordon@sneaky.lonestar.org)
 *	-hm	adding option "a" for just returning the type of video adaptor
 *	-hm	removing explicit HGC support, same as MDA ...
 *	-hm	vga type/family/132col support info on -l
 *	-hm	force 24 lines in DEC 25 lines mode and HP 28 lines mode
 *	-hm	fixed bug with 132 column mode display status display
 *	-jw	added 132/80 col mode switching
 *	-hm	removed -h flag, use -? now ... ;-)
 *	-hm	S3 chipsets ..
 *	-hm	Cirrus chipsets support from Onno van der Linden
 *	-hm	-m option, display monitor type
 *	-hm	bugfix, scon -c <screen-num> cleared dest screen, fixed
 *
 *---------------------------------------------------------------------------*/

#include <stdio.h>
#include <fcntl.h>
#include <machine/pcvt_ioctl.h>

#define DEFAULTFD 0

int aflag = -1;
int lflag = -1;
int mflag = -1;
int current = -1;
int pflag = -1;
int hflag = -1;
int res = -1;
char *device;
int dflag = -1;
int vflag = 0;
int Pflag = 0;
int tflag = 0;
int fflag = -1;
int colms = 0;
char *onoff;

unsigned timeout;
struct screeninfo screeninfo;

#define NVGAPEL 256

struct rgb {
	unsigned r, g, b;
	int dothis;
};

static struct rgb palette[NVGAPEL] = {
	{ 0x00,  0x00,  0x00, 0},		/*   0 - black		*/
	{ 0x00,  0x00,  0x2a, 0},		/*   1 - blue		*/
	{ 0x00,  0x2a,  0x00, 0},		/*   2 - green		*/
	{ 0x00,  0x2a,  0x2a, 0},		/*   3 - cyan		*/
	{ 0x2a,  0x00,  0x00, 0},		/*   4 - red		*/
	{ 0x2a,  0x00,  0x2a, 0},		/*   5 - magenta	*/
	{ 0x2a,  0x2a,  0x00, 0},		/*   6 			*/
	{ 0x2a,  0x2a,  0x2a, 0},		/*   7 - lightgray	*/
	{ 0x00,  0x00,  0x15, 0},		/*   8 			*/
	{ 0x00,  0x00,  0x3f, 0},		/*   9 			*/
	{ 0x00,  0x2a,  0x15, 0},		/*  10 			*/
	{ 0x00,  0x2a,  0x3f, 0},		/*  11 			*/
	{ 0x2a,  0x00,  0x15, 0},		/*  12 			*/
	{ 0x2a,  0x00,  0x3f, 0},		/*  13 			*/
	{ 0x2a,  0x2a,  0x15, 0},		/*  14 			*/
	{ 0x2a,  0x2a,  0x3f, 0},		/*  15 			*/
	{ 0x00,  0x15,  0x00, 0},		/*  16 			*/
	{ 0x00,  0x15,  0x2a, 0},		/*  17 			*/
	{ 0x00,  0x3f,  0x00, 0},		/*  18 			*/
	{ 0x00,  0x3f,  0x2a, 0},		/*  19 			*/
	{ 0x2a,  0x15,  0x00, 0},		/*  20 - brown		*/
	{ 0x2a,  0x15,  0x2a, 0},		/*  21 			*/
	{ 0x2a,  0x3f,  0x00, 0},		/*  22 			*/
	{ 0x2a,  0x3f,  0x2a, 0},		/*  23 			*/
	{ 0x00,  0x15,  0x15, 0},		/*  24 			*/
	{ 0x00,  0x15,  0x3f, 0},		/*  25 			*/
	{ 0x00,  0x3f,  0x15, 0},		/*  26 			*/
	{ 0x00,  0x3f,  0x3f, 0},		/*  27 			*/
	{ 0x2a,  0x15,  0x15, 0},		/*  28 			*/
	{ 0x2a,  0x15,  0x3f, 0},		/*  29 			*/
	{ 0x2a,  0x3f,  0x15, 0},		/*  30 			*/
	{ 0x2a,  0x3f,  0x3f, 0},		/*  31 			*/
	{ 0x15,  0x00,  0x00, 0},		/*  32 			*/
	{ 0x15,  0x00,  0x2a, 0},		/*  33 			*/
	{ 0x15,  0x2a,  0x00, 0},		/*  34 			*/
	{ 0x15,  0x2a,  0x2a, 0},		/*  35 			*/
	{ 0x3f,  0x00,  0x00, 0},		/*  36 			*/
	{ 0x3f,  0x00,  0x2a, 0},		/*  37 			*/
	{ 0x3f,  0x2a,  0x00, 0},		/*  38 			*/
	{ 0x3f,  0x2a,  0x2a, 0},		/*  39 			*/
	{ 0x15,  0x00,  0x15, 0},		/*  40 			*/
	{ 0x15,  0x00,  0x3f, 0},		/*  41 			*/
	{ 0x15,  0x2a,  0x15, 0},		/*  42 			*/
	{ 0x15,  0x2a,  0x3f, 0},		/*  43 			*/
	{ 0x3f,  0x00,  0x15, 0},		/*  44 			*/
	{ 0x3f,  0x00,  0x3f, 0},		/*  45 			*/
	{ 0x3f,  0x2a,  0x15, 0},		/*  46 			*/
	{ 0x3f,  0x2a,  0x3f, 0},		/*  47 			*/
	{ 0x15,  0x15,  0x00, 0},		/*  48 			*/
	{ 0x15,  0x15,  0x2a, 0},		/*  49 			*/
	{ 0x15,  0x3f,  0x00, 0},		/*  50 			*/
	{ 0x15,  0x3f,  0x2a, 0},		/*  51 			*/
	{ 0x3f,  0x15,  0x00, 0},		/*  52 			*/
	{ 0x3f,  0x15,  0x2a, 0},		/*  53 			*/
	{ 0x3f,  0x3f,  0x00, 0},		/*  54 			*/
	{ 0x3f,  0x3f,  0x2a, 0},		/*  55 			*/
	{ 0x15,  0x15,  0x15, 0},		/*  56 - darkgray	*/
	{ 0x15,  0x15,  0x3f, 0},		/*  57 - lightblue	*/
	{ 0x15,  0x3f,  0x15, 0},		/*  58 - lightgreen	*/
	{ 0x15,  0x3f,  0x3f, 0},		/*  59 - lightcyan	*/
	{ 0x3f,  0x15,  0x15, 0},		/*  60 - lightred	*/
	{ 0x3f,  0x15,  0x3f, 0},		/*  61 - lightmagenta	*/
	{ 0x3f,  0x3f,  0x15, 0},		/*  62 - yellow		*/
	{ 0x3f,  0x3f,  0x3f, 0},		/*  63 - white		*/
	{ 0x00,  0x00,  0x00, 0}		/*  64 ... - empty	*/
};

static struct colname {
	const char *name;
	unsigned idx;
} colnames[] = {
	{"black", 0},
	{"blue", 1},
	{"green", 2},
	{"cyan", 3},
	{"red", 4},
	{"magenta", 5},
	{"brown", 20},
	{"lightgray", 7},
	{"lightgrey", 7},
	{"darkgray", 56},
	{"darkgrey", 56},
	{"lightblue", 57},
	{"lightgreen", 58},
	{"lightcyan", 59},
	{"lightred", 60},
	{"lightmagenta", 61},
	{"yellow", 62},
	{"white", 63},
	/* must be terminator: */ {(const char *)NULL, 0}
};


static void parsepopt(char *arg, unsigned *idx,
		      unsigned *r, unsigned *g, unsigned *b);
static void printpalette(int fd);

main(argc,argv)
int argc;
char *argv[];
{
	extern int optind;
	extern int opterr;
	extern char *optarg;

	int c;
	int fd;

	while( (c = getopt(argc, argv, "ac:d:f:HVlms:t:vp:18")) != -1)
	{
		switch(c)
		{
			case 'a':
				aflag = 1;
				break;

			case 'l':
				lflag = 1;
				break;

			case 'm':
				mflag = 1;
				break;

			case 'c':
				current = atoi(optarg);
				break;

			case 'd':
				device = optarg;
				dflag = 1;
				break;

			case 'f':
				onoff = optarg;
				fflag = 1;
				break;

			case 'V':
				pflag = 1;
				break;

			case 'H':
				hflag = 1;
				break;

			case 's':
				if     (!strncmp(optarg, "25", 2))
					res = SIZ_25ROWS;
				else if(!strncmp(optarg, "28", 2))
					res = SIZ_28ROWS;
				else if(!strncmp(optarg, "35", 2))
					res = SIZ_35ROWS;
				else if(!strncmp(optarg, "40", 2))
					res = SIZ_40ROWS;
				else if(!strncmp(optarg, "43", 2))
					res = SIZ_43ROWS;
				else if(!strncmp(optarg, "50", 2))
					res = SIZ_50ROWS;
				break;

			case 'v':
				vflag++;
				break;

			case 'p':
				if(!strcmp(optarg, "list"))
				{
					if(Pflag)
					{
						fprintf(stderr,
						"-p list is mutual exclusive "
						"with other -p options\n");
						return 2;
					}
					Pflag = 3;
				}
				else if(!strcmp(optarg, "default"))
				{
					if(Pflag)
					{
						fprintf(stderr,
						"multiple -p default not "
						"allowed\n");
						return 2;
					}
					Pflag = 2;
				} else {
					unsigned idx, r, g, b;

					if(Pflag > 1)
					{
						fprintf(stderr,
						"-p default and -p i,r,g,b "
						"ambiguous\n");
						return 2;
					}
					Pflag = 1;
					parsepopt(optarg, &idx, &r, &g, &b);
					if(idx >= NVGAPEL)
					{
						fprintf(stderr,
						"index %u in -p option "
						"out of range\n", idx);
						return 2;
					}
					palette[idx].r = r;
					palette[idx].g = g;
					palette[idx].b = b;
					palette[idx].dothis = 1;
				}
				break;

			case 't':
				tflag++;
				timeout = atoi(optarg);
				break;

			case '1':
				colms = 132;
				break;

			case '8':
				colms = 80;
				break;

			case '?':
			default:
				usage();
				break;
		}
	}

	if((pflag == 1) && (hflag == 1))
		usage();

	if(dflag == -1 && lflag == -1 && current == -1 && pflag == -1 &&
	   hflag == -1 && res == -1 && Pflag == 0 && tflag == 0 && fflag == -1
	   && colms == 0 && mflag == -1)
	{
		lflag = 1;
	}

	if(dflag == -1)
	{
		if(vflag)
			printf("using current device\n");
		fd = DEFAULTFD;		/* -hm, Feb 12 1993 */
	}
	else
	{
		if((fd = open(device, O_RDWR)) == -1)
		{
			char buffer[80];
			strcpy(buffer,"ERROR opening ");
			strcat(buffer,device);
			perror(buffer);
			exit(1);
		}
		if(vflag)
			printf("using device %s\n",device);
	}

	if(aflag == 1)	/* return adaptor type */
	{
		printadaptor(fd);
		exit(0);
	}

	if(mflag == 1)	/* return monitor type */
	{
		printmonitor(fd);
		exit(0);
	}

	if(lflag == 1)	/* list information */
	{
		if(vflag)
			printf("processing option -l, listing screen info\n");
		printinfo(fd);
		exit(0);
	}

	if(tflag)	/* set screen saver timeout */
	{
		if(vflag)
		{
			printf(
			"processing option -t, setting screen saver timeout: "
			);
			if(timeout)
				printf("new timeout = %d s\n", timeout);
			else
				printf("turned off\n");
		}

		if(ioctl(fd, VGASCREENSAVER, &timeout) < 0)
		{
			perror("ioctl(VGASCREENSAVER)");
			fprintf(stderr, "Check the driver, the screensaver is probably not compiled in!\n");
			exit(2);
		}
		goto success;
	}

	if(colms)
	{
		if(vflag)
			printf("Setting number of columns to %d\n", colms);
		if(ioctl(fd, VGASETCOLMS, &colms) < 0)
		{
			perror("ioctl(VGASETCOLMS)");
			exit(2);
		}
		goto success;
	}

	if(Pflag == 3)
	{
		/* listing VGA palette */
		if(vflag)
			printf("processing option -p list, "
			       "listing VGA palette\n");

		printpalette(fd);
		goto success;
	}

	if(Pflag)
	{
		unsigned int idx;

		/* setting VGA palette */
		if(vflag)
			printf("processing option -p, setting VGA palette%s\n",
			       Pflag == 2? " to default": "");

		for(idx = 0; idx < NVGAPEL; idx++)
			if(Pflag == 2 || palette[idx].dothis)
			{
				struct vgapel p;
				p.idx = idx;
				p.r = palette[idx].r;
				p.g = palette[idx].g;
				p.b = palette[idx].b;
				if(ioctl(fd, VGAWRITEPEL, (caddr_t)&p) < 0)
				{
					perror("ioctl(fd, VGAWRITEPEL)");
					return 2;
				}
			}
		goto success;
	}

	screeninfo.screen_no = -1; /* We are using fd */
	screeninfo.current_screen = current;
	screeninfo.pure_vt_mode = -1;
	screeninfo.screen_size = res;
	screeninfo.force_24lines = -1;

	if(current != -1)	/* set current screen */
	{
		if(vflag)
			printf("processing option -c, setting current screen to %d\n",current);

		if(ioctl(1, VGASETSCREEN, &screeninfo) == -1)
		{
			perror("ioctl VGASETSCREEN failed");
			exit(1);
		}
		exit(0);
	}

	if(pflag == 1)
	{
		if(vflag)
			printf("processing option -V, setting emulation to pure VT220\n");
		screeninfo.pure_vt_mode = M_PUREVT;
	}
	else if(hflag == 1)
	{
		if(vflag)
			printf("processing option -H, setting emulation to VT220 + HP Labels\n");
		screeninfo.pure_vt_mode = M_HPVT;
	}
	else
	{
		if(vflag)
			printf("no change in terminal emulation\n");
	}

	if(vflag)
	{
		if(res == -1)
			printf("no change in screen resolution\n");
		else if(res == SIZ_25ROWS)
			printf("change screen resolution to 25 lines\n");
		else if(res == SIZ_28ROWS)
			printf("change screen resolution to 28 lines\n");
		else if(res == SIZ_35ROWS)
			printf("change screen resolution to 35 lines\n");
		else if(res == SIZ_40ROWS)
			printf("change screen resolution to 40 lines\n");
		else if(res == SIZ_43ROWS)
			printf("change screen resolution to 43 lines\n");
		else if(res == SIZ_50ROWS)
			printf("change screen resolution to 50 lines\n");
	}

	if(fflag == 1)	/* force 24 lines on/off */
	{
		if(!strcmp(onoff, "on"))
		{
			fflag = 1;
		}
		else if(!strcmp(onoff, "off"))
		{
			fflag = 0;
		}
		else
		{
			fprintf(stderr,"you must specify 'on' or 'off' with -f option!\n");
			exit(1);
		}
	}
	screeninfo.force_24lines = fflag;

	if(ioctl(fd, VGASETSCREEN, &screeninfo) == -1)
	{
		perror("ioctl VGASETSCREEN failed");
		exit(1);
	}
success:
	if(vflag)
		printf("successful execution of ioctl VGASETSCREEN!\n");
	exit(0);
}

usage()
{
	fprintf(stderr,"\nscon - screen control utility for the pcvt video driver\n");
	fprintf(stderr,"usage: scon -a -l -m -v -c [n] -d [dev] -f [on|off] -V -H -s [n]\n");
	fprintf(stderr,"usage: scon -p [default | list | i,r,g,b] | -t [sec] | -1 | -8\n");
	fprintf(stderr,"       -a              list video adaptor type (MDA,CGA,EGA or VGA)\n");
	fprintf(stderr,"       -c <screen no>  switch current virtual screen to <screen no>\n");
	fprintf(stderr,"       -d <device>     set parameters(-V|-H|-s) for virtual device\n");
	fprintf(stderr,"       -f <on|off>     force 24 lines in VT 25 lines and HP 28 lines mode\n");
	fprintf(stderr,"       -H              set VT220/HP emulation mode for a virtual screen\n");
	fprintf(stderr,"       -l              list current parameters for a virtual screen\n");
	fprintf(stderr,"       -m              report monitor type (MONO/COLOR)\n");
	fprintf(stderr,"       -p default      set default VGA palette\n");
	fprintf(stderr,"       -p list         list current VGA palette\n");
	fprintf(stderr,"       -p <i,r,g,b>    set VGA palette entry i to r/g/b\n");
	fprintf(stderr,"       -p <name,r,g,b> set VGA palette entry for color name to r/g/b\n");
	fprintf(stderr,"       -s <lines>      set 25, 28, 35, 40, 43 or 50 lines for a virtual screen\n");
	fprintf(stderr,"       -t <timeout>    set screen saver timeout [seconds]\n");
	fprintf(stderr,"       -1              set 132 columns mode\n");
	fprintf(stderr,"       -8              set 80 columns mode\n");
	fprintf(stderr,"       -v              verbose mode\n");
	fprintf(stderr,"       -V              set pure VT220 emulation for a virtual screen\n");
	fprintf(stderr,"       -?              display help (this message)\n\n");
	exit(1);
}

printadaptor(fd)
int fd;
{
	if(ioctl(fd, VGAGETSCREEN, &screeninfo) == -1)
	{
		perror("ioctl VGAGETSCREEN failed");
		exit(1);
	}
	switch(screeninfo.adaptor_type)
	{
		default:
		case UNKNOWN_ADAPTOR:
			printf("UNKNOWN\n");
			break;

		case MDA_ADAPTOR:
			printf("MDA\n");
			break;

		case CGA_ADAPTOR:
			printf("CGA\n");
			break;

		case EGA_ADAPTOR:
			printf("EGA\n");
			break;

		case VGA_ADAPTOR:
			printf("VGA\n");
			break;
	}
}

printmonitor(fd)
int fd;
{
	if(ioctl(fd, VGAGETSCREEN, &screeninfo) == -1)
	{
		perror("ioctl VGAGETSCREEN failed");
		exit(1);
	}
	switch(screeninfo.monitor_type)
	{
		default:
			printf("UNKNOWN\n");
			break;

		case MONITOR_MONO:
			printf("MONO\n");
			break;

		case MONITOR_COLOR:
			printf("COLOR\n");
			break;
	}
}

char *vga_type(int number)
{
	static char *vga_tab[] = {
		"Generic VGA",
		"ET4000",
		"ET3000",
		"PVGA1A",
		"WD90C00",
		"WD90C10",
		"WD90C11",
		"VIDEO 7 VEGA",
		"VIDEO 7 FAST",
		"VIDEO 7 VER5",
		"VIDEO 7 1024I",
		"Unknown VIDEO 7",
		"TVGA 8800BR",
		"TVGA 8800CS",
		"TVGA 8900B",
		"TVGA 8900C",
		"TVGA 8900CL",
		"TVGA 9000",
		"TVGA 9100",
		"TVGA 9200",
		"Unknown TRIDENT",
		"S3 80C911",
		"S3 80C924",
		"S3 80C801/80C805",
		"S3 80C928",
		"Unknown S3",
 		"CL-GD5402",
 		"CL-GD5402r1",
 		"CL-GD5420",
 		"CL-GD5420r1",
 		"CL-GD5422",
 		"CL-GD5424",
 		"CL-GD5426",
 		"CL-GD5428",

	};
	return(vga_tab[number]);
}

char *vga_family(int number)
{
	static char *vga_tab[] = {
		"Generic VGA",
		"Tseng Labs",
		"Western Digital",
		"Video Seven",
		"Trident",
		"S3 Incorporated",
		"Cirrus Logic",
	};
	return(vga_tab[number]);
}

printinfo(fd)
int fd;
{
	if(ioctl(fd, VGAGETSCREEN, &screeninfo) == -1)
	{
		perror("ioctl VGAGETSCREEN failed");
		exit(1);
	}

	printf( "\nVideo Adaptor Type           = ");

	switch(screeninfo.adaptor_type)
	{
		default:
		case UNKNOWN_ADAPTOR:
			printf("UNKNOWN Video Adaptor\n");
			break;

		case MDA_ADAPTOR:
			printf("MDA - Monochrome Display Adaptor\n");
			break;

		case CGA_ADAPTOR:
			printf("CGA - Color Graphics Adaptor\n");
			break;

		case EGA_ADAPTOR:
			printf("EGA - Enhanced Graphics Adaptor\n");
			break;

		case VGA_ADAPTOR:
			printf("VGA - Video Graphics Adaptor/Array\n");
			printf(" VGA Chipset Manufacturer    = %s\n",
					vga_family(screeninfo.vga_family));
			printf(" VGA Chipset Type            = %s\n",
					vga_type(screeninfo.vga_type));
			printf(" Support for 132 Column Mode = %s\n",
					screeninfo.vga_132 ? "Yes" : "No");
			break;
	}

	printf( "Display Monitor Type         = ");

	switch(screeninfo.monitor_type)
	{
		default:
			printf("UNKNOWN Monitor Type\n");
			break;

		case MONITOR_MONO:
			printf("Monochrome Monitor\n");
			break;

		case MONITOR_COLOR:
			printf("Color Monitor\n");
			break;
	}

	printf( "Number of Downloadable Fonts = %d\n",screeninfo.totalfonts);
	printf( "Number of Virtual Screens    = %d\n",screeninfo.totalscreens);
	printf( "Info Request Screen Number   = %d\n",screeninfo.screen_no);
	printf( "Current Displayed Screen     = %d\n",screeninfo.current_screen);

	if(screeninfo.pure_vt_mode == M_PUREVT)
		printf( "Terminal Emulation Mode      = VT220\n");
	else
		printf( "Terminal Emulation Mode      = VT220 with HP Features\n");

	printf( "Lines                        = ");

	switch(screeninfo.screen_size)
	{
		case SIZ_25ROWS:
			printf( "25\n");
			break;

		case SIZ_28ROWS:
			printf( "28\n");
			break;

		case SIZ_35ROWS:
			printf( "35\n");
			break;

		case SIZ_40ROWS:
			printf( "40\n");
			break;

		case SIZ_43ROWS:
			printf( "43\n");
			break;

		case SIZ_50ROWS:
			printf( "50\n");
			break;

		default:
			printf( "UNKNOWN\n");
			break;
	}
	printf( "Force 24 Lines               = %s",
			screeninfo.force_24lines ? "Yes" : "No");

	printf("\n\n");
}

static const char *findname(unsigned idx)
{
	/* try to find a name for palette entry idx */
	/* if multiple names exist, returns first matching */
	register struct colname *cnp;

	for(cnp = colnames; cnp->name; cnp++)
		if(cnp->idx == idx)
			return cnp->name;

	/* not found */
	return (const char *)NULL;
}

static void printpalette(int fd)
{
	register unsigned idx, last;

	for(idx = 0; idx < NVGAPEL; idx++)
	{
		struct vgapel p;
		p.idx = idx;
		if(ioctl(fd, VGAREADPEL, &p) < 0)
		{
			perror("ioctl(VGAREADPEL)");
			exit(2);
		}
		palette[idx].r = p.r;
		palette[idx].g = p.g;
		palette[idx].b = p.b;
	}

	/* find last non-empty entry */
	for(last = NVGAPEL - 1; last; last--)
		if(palette[last].r || palette[last].g || palette[last].b)
			break;

	if(last != NVGAPEL - 1)
		last++;

	/* now, everything's collected. print out table */
	printf("VGA palette status\n");
	printf("index    red  green   blue  name\n");
	for(idx = 0; idx < last; idx++)
	{
		const char *cp;
		printf("%5d  %5d  %5d  %5d",
		       idx, palette[idx].r, palette[idx].g, palette[idx].b);
		if(cp = findname(idx))
			printf("  %s\n", cp);
		else
			putchar('\n');
	}
	putchar('\n');
}


static void parsepopt(char *arg, unsigned *idx,
		      unsigned *r, unsigned *g, unsigned *b)
{
	char firstarg[21];
	register unsigned i;

	if(sscanf(arg, "%20[a-zA-Z0-9]%*[,:]%u,%u,%u", firstarg, r, g, b) < 4
	   || strlen(firstarg) == 0) {
		fprintf(stderr, "too few args in -p i,r,g,b\n");
		exit(2);
	}

	if(firstarg[0] >= '0' && firstarg[0] <= '9') {
		*idx = strtoul(firstarg, NULL, 10);
		return;
	}

	for(i = 0; colnames[i].name; i++)
		if(strcasecmp(colnames[i].name, firstarg) == 0) {
			*idx = colnames[i].idx;
			return;
		}
	fprintf(stderr, "arg ``%s'' in -p option not recognized\n",
		firstarg);
	exit(2);
}
