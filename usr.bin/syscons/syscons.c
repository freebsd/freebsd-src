/*
 * Copyright (c) 1993 Christoph M. Robitschko
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
 *      This product includes software developed by Christoph M. Robitschko
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software withough specific prior written permission
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

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <machine/console.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/errno.h>

#include "syscons.h"

int		verbose = 0;
char		*prgname;


__BEGIN_DECLS
int	is_syscons	__P((int));
void	screensaver	__P((char *));
void	screensavertype	__P((char *));
void	linemode	__P((char *));
void	keyrate		__P((char *));
void	keymap		__P((char *));
void    mapscr          __P((char *));
void	loadfont	__P((char *, char *));
void	switchto	__P((char *));
void	setfkey		__P((char *, char *));
void	displayit	__P((void));
void	usage		__P((void));
char	*mkfullname	__P((const char *, const char *, const char *));
char	*nextarg	__P((int, char **, int *, int));
__END_DECLS



void
main(argc, argv)
int		argc;
char		**argv;
{
extern char	*optarg;
extern int	optind;
int		opt;


	prgname = argv[0];
	if (!is_syscons(0))
		exit(1);
	while((opt = getopt(argc, argv, "s:S:m:r:k:f:t:F:V:vd")) != -1)
		switch(opt) {
			case 's':
				screensaver(optarg);
				break;
			case 'V':
				screensavertype(optarg);
				break;
			case 'm':
				linemode(optarg);
				break;
			case 'r':
				keyrate(optarg);
				break;
			case 'k':
				keymap(optarg);
				break;
			case 'S':
				mapscr(optarg);
				break;
			case 'f':
				loadfont(optarg, nextarg(argc, argv, &optind, 'f'));
				break;
			case 't':
				switchto(optarg);
				break;
			case 'F':
				setfkey(optarg, nextarg(argc, argv, &optind, 'F'));
				break;
			case 'v':
				verbose ++;
				break;
			case 'd':
				displayit();
				break;
			default:
				usage();
				exit(1);
		}
	if ((optind != argc) || (argc == 1)) {
		usage();
		exit(1);
	}
	exit(0);
}


int
is_syscons(fd)
int		fd;
{
vtmode_t	mode;

	/* Try to find out if we are running on a syscons VT */
	if (ioctl(fd, VT_GETMODE, &mode) == 0)
		return(1);
	if (errno == ENOTTY) {
		printf("You must be running this from a syscons vty for it to work.\n\n");
		printf("If you are currently using pccons (default) and wish to have virtual\n");
		printf("consoles, then rebuild your kernel after replacing the line:\n\n");
		printf("\tdevice pc0 at isa? port \042IO_KBD\042 tty irq 1 vector pcrint\n");
		printf("\nwith:\n\n");
		printf("\tdevice sc0 at isa? port \042IO_KBD\042 tty irq 1 vector scintr\n");
		printf("\n(Then install the new kernel and reboot your system)\n");
	}
	else
		perror("getting console state");
	return(0);
}



void
usage(void)
{
const char	usagestr[] = {"\
Usage: syscons  -v               (be verbose)\n\
                -s {TIME|off}    (set screensaver timeout to TIME seconds)\n\
		-V {NAME|list}   (set screensaver type or list available types\n\
                -m {80x25|80x50} (set screen to 25 or 50 lines)\n\
                -r DELAY.REPEAT  (set keyboard delay & repeat rate)\n\
                -r fast		 (set keyboard delay & repeat to fast)\n\
                -r slow		 (set keyboard delay & repeat to slow)\n\
                -r normal	 (set keyboard delay & repeat to normal)\n\
                -k MAPFILE       (load keyboard map file)\n\
                -f SIZE FILE     (load font file of size 8, 14 or 16)\n\
                -t SCRNUM        (switch to specified VT)\n\
                -F NUM STRING    (set function key NUM to send STRING)\n\
		-S SCRNMAP       (load screen map file)\n\
"};
	fprintf(stderr, usagestr);
}


char *
nextarg(ac, av, indp, oc)
int		ac;
char		**av;
int		*indp;
int		oc;
{
	if (*indp < ac)
		return(av[(*indp)++]);
	fprintf(stderr, "%s: option requires two arguments -- %c\n", av[0], oc);
	usage();
	exit(1);
	return("");
}



char *
mkfullname(s1, s2, s3)
const char	*s1, *s2, *s3;
{
static char	*buf = NULL;
static int	bufl = 0;
int		f;


	f = strlen(s1) + strlen(s2) + strlen(s3) + 1;
	if (f > bufl)
		if (buf)
			buf = (char *)realloc(buf, f);
		else
			buf = (char *)malloc(f);
	if (!buf) {
		bufl = 0;
		return(NULL);
	}

	bufl = f;
	strcpy(buf, s1);
	strcat(buf, s2);
	strcat(buf, s3);
	return(buf);
}


void
screensaver(opt)
char		*opt;
{
int		nsec;
char		*s1;


	if (!strcmp(opt, "off"))
		nsec = 0;
	else {
		nsec = strtol(opt, &s1, 0);
		if ((nsec < 0) || (*opt == '\0') || (*s1 != '\0')) {
			fprintf(stderr, "argument to -s must be a positive integer.\n");
			return;
		}
	}

	if (ioctl(0, CONS_BLANKTIME, &nsec) == -1)
		perror("setting screensaver period");
}


void
screensavertype(opt)
char		*opt;
{
ssaver_t	shaver;
int		i, e;


	if (!strcmp(opt, "list")) {
		i = 0;
		printf("available screen saver types:\n");
		do {
			shaver.num = i;
			e = ioctl(0, CONS_GSAVER, &shaver);
			i ++;
			if (e == 0)
				printf("\t%d\t%s\n", shaver.num, shaver.name);
		} while (e == 0);
		if (e == -1 && errno != EIO)
			perror("getting screensaver info");
	} else {
		i = 0;
		do {
			shaver.num = i;
			e = ioctl(0, CONS_GSAVER, &shaver);
			i ++;
			if (e == 0 && !strcmp(opt, shaver.name)) {
				if (ioctl(0, CONS_SSAVER, &shaver) == -1)
					perror("setting screensaver type");
				return;
			}
		} while (e == 0);
		if (e == -1 && errno != EIO)
			perror("getting screensaver info");
		else
			fprintf(stderr, "%s: No such screensaver\n", opt);
	}
}


void
linemode(opt)
char		*opt;
{
unsigned long	req;

	if (!strcmp(opt, "80x25"))
		req = CONS_80x25TEXT;
	else if (!strcmp(opt, "80x50"))
		req = CONS_80x50TEXT;
	else {
		fprintf(stderr, "Unknown mode to -m: %s\n", opt);
		return;
	}

	if (ioctl(0, req, NULL) == -1)
		perror("Setting line mode");
}


void
keyrate(opt)
char		*opt;
{
const int	delays[]  = {250, 500, 750, 1000};
const int	repeats[] = { 34,  38,  42,  46,  50,  55,  59,  63,
			      68,  76,  84,  92, 100, 110, 118, 126,
			     136, 152, 168, 184, 200, 220, 236, 252,
			     272, 304, 336, 368, 400, 440, 472, 504};
const int	ndelays = (sizeof(delays) / sizeof(int));
const int	nrepeats = (sizeof(repeats) / sizeof(int));
struct	{
	int	rep:5;
	int	del:2;
	int	pad:1;
	}	rate;

	if (!strcmp(opt, "slow"))
		rate.del = 3, rate.rep = 31;
	else if (!strcmp(opt, "normal"))
		rate.del = 1, rate.rep = 15;
	else if (!strcmp(opt, "fast"))
		rate.del = rate.rep = 0;
	else {
		int		n;
		int		delay, repeat;
		char		*v1;


		delay = strtol(opt, &v1, 0);
		if ((delay < 0) || (*v1 != '.'))
			goto badopt;
		opt = ++v1;
		repeat = strtol(opt, &v1, 0);
		if ((repeat < 0) || (*opt == '\0') || (*v1 != '\0')) {
badopt:
			fprintf(stderr, "argument to -r must be DELAY.REPEAT\n");
			return;
		}
		for (n = 0; n < ndelays - 1; n++)
			if (delay <= delays[n])
				break;
		rate.del = n;
		for (n = 0; n < nrepeats - 1; n++)
			if (repeat <= repeats[n])
				break;
		rate.rep = n;
	}

	if (verbose)
		fprintf(stderr, "setting keyboard rate to %d.%d\n", delays[rate.del], repeats[rate.rep]);
	if (ioctl(0, KDSETRAD, rate) == -1)
		perror("setting keyboard rate");
}


void
keymap(opt)
char		*opt;
{
char		*mapfn;
int		mapfd;
keymap_t	map;
int		f;
const char	*prefix[]  = {"", "",     KEYMAP_PATH, NULL};
const char	*postfix[] = {"", ".map", ".map"};


	mapfd = -1;
	for (f = 0; prefix[f]; f++) {
		mapfn = mkfullname(prefix[f], opt, postfix[f]);
		mapfd = open(mapfn, O_RDONLY, 0);
		if (verbose)
			fprintf(stderr, "trying to open keymap file %s ... %s\n", mapfn, (mapfd==-1?"failed":"OK"));
		if (mapfd >= 0)
			break;
	}
	if (mapfd == -1) {
		perror("Keymap file not found");
		return;
	}
	if ((read(mapfd, &map, sizeof(map)) != sizeof(map)) ||
	    (read(mapfd, &map, 1) != 0)) {
		fprintf(stderr, "\"%s\" is not in keymap format.\n", opt);
		(void) close(mapfd);
		return;
	}
	(void) close(mapfd);
	if (ioctl(0, PIO_KEYMAP, &map) == -1)
		perror("setting keyboard map");
}

void
mapscr(opt)
char		*opt;
{
char		*mapfn;
int		mapfd;
scrmap_t        map;
int		f;
const char      *prefix[]  = {"", "",     SCRNMAP_PATH, NULL};
const char      *postfix[] = {"", ".scr", ".scr"};


	mapfd = -1;
	for (f = 0; prefix[f]; f++) {
		mapfn = mkfullname(prefix[f], opt, postfix[f]);
		mapfd = open(mapfn, O_RDONLY, 0);
		if (verbose)
			fprintf(stderr, "trying to open scrnmap file %s ... %s\n", mapfn, (mapfd==-1?"failed":"OK"));
		if (mapfd >= 0)
			break;
	}
	if (mapfd == -1) {
		perror("Scrnmap file not found");
		return;
	}
	if ((read(mapfd, &map, sizeof(map)) != sizeof(map)) ||
	    (read(mapfd, &map, 1) != 0)) {
		fprintf(stderr, "\"%s\" is not in scrnmap format.\n", opt);
		(void) close(mapfd);
		return;
	}
	(void) close(mapfd);
	if (ioctl(0, PIO_SCRNMAP, &map) == -1)
		perror("setting screen map");
}

void
loadfont(sizec, fname)
char		*sizec;
char		*fname;
{
char		*fontfn;
int		fontfd;
void		*font;
char		*v1;
int		ind, f;
const struct	{
		int		fsize;
		int		msize;
		unsigned long	req;
		}	fontinfo[] = {
				{ 8,  sizeof(fnt8_t),	PIO_FONT8x8},
				{ 14, sizeof(fnt14_t),	PIO_FONT8x14},
				{ 16, sizeof(fnt16_t),	PIO_FONT8x16},
				{ 0 }};
const char	*prefix[]  = {"", "",     FONT_PATH, FONT_PATH, NULL};
const char	*postfix[] = {"", ".fnt", "",        ".fnt"};



	f = strtol(sizec, &v1, 0);
	for (ind = 0; fontinfo[ind].fsize; ind++)
		if (fontinfo[ind].fsize == f)
			break;
	if ((fontinfo[ind].fsize == 0) || (*v1 != '\0')) {
		fprintf(stderr, "%s is an unsupported font size.\n", sizec);
		return;
	}
	font = (void *)malloc(fontinfo[ind].msize);
	if (!font) {
		fprintf(stderr, "loading font: Out of memory.\n");
		return;
	}

	fontfd = -1; fontfn = "";
	for (f = 0; prefix[f]; f++) {
		fontfn = mkfullname(prefix[f], fname, postfix[f]);
		fontfd = open(fontfn, O_RDONLY, 0);
		if (verbose)
			fprintf(stderr, "trying to open font file %s ... %s\n", fontfn, (fontfd==-1?"failed":"OK"));
		if (fontfd >= 0)
			break;
	}
	if (fontfd == -1) {
		perror("Font file not found");
		return;
	}
	if ((read(fontfd, font, fontinfo[ind].msize) != fontinfo[ind].msize) ||
	    (read(fontfd, font, 1) != 0)) {
		fprintf(stderr, "\"%s\" is not a font with size %s.\n", fontfn, sizec);
		(void) close(fontfd);
		return;
	}

	if (ioctl(0, fontinfo[ind].req, font) == -1)
		perror("Setting font");
}


void
switchto(opt)
char		*opt;
{
int		scrno;
char		*v1;


	scrno = strtol(opt, &v1, 0);
	if ((scrno < 1) || (scrno > 12) || (*v1 != '\0')) {
		fprintf(stderr, "argument to -t must be between 1 and 12.\n");
		return;
	}
	if (ioctl(0, VT_ACTIVATE, scrno) == -1)
		perror("switching to new VT");
}


void
setfkey(knumc, str)
char		*knumc;
char		*str;
{
fkeyarg_t	fkey;
char		*v1;
long            keynum;

	keynum = strtol(knumc, &v1, 0);
	if (keynum < 0 || keynum > 59 || *v1 != '\0') {
		fprintf(stderr, "function key number must be between 0 and 59.\n");
		return;
	}
	fkey.keynum = keynum;
	if ((fkey.flen = strlen(str)) > MAXFK) {
		fprintf(stderr, "function key string too long (%d > %d)\n", fkey.flen, MAXFK);
		return;
	}
	strcpy(fkey.keydef, str);
	if (verbose)
		fprintf(stderr, "setting F%d to \"%s\"\n", fkey.keynum, str);
	fkey.keynum--;
	if (ioctl(0, SETFKEY, &fkey) == -1)
		perror("setting function key");
}



void
displayit(void)
{
	if (verbose)
		printf("absolutely ");
	printf("nothing to display.\n");
}
