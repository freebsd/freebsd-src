/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 *
 *	$Id: vidcontrol.c,v 1.11 1995/05/30 03:52:53 rgrimes Exp $
 */

#include <ctype.h>
#include <stdio.h>
#include <machine/console.h>
#include <sys/errno.h>
#include "path.h"

char 	legal_colors[16][16] = {
	"black", "blue", "green", "cyan",
	"red", "magenta", "brown", "white",
	"grey", "lightblue", "lightgreen", "lightcyan",
	"lightred", "lightmagenta", "yellow", "lightwhite"
};
int 	hex = 0;
int 	number;
char 	letter;
struct 	vid_info info;


void
usage()
{
	fprintf(stderr,
"Usage: vidcontrol mode             (available modes: VGA_40x25, VGA_80x25,\n"
"                                                     VGA_80x50, VGA_320x200,\n"
"                                                     EGA_80x25, EGA_80x43)\n"
"                                   (experimental)    VGA_80x30, VGA_80x60)\n"
"\n"
"                  show             (show available colors)\n"
"                  fgcol bgcol      (set fore- & background colors)\n"
"                  -r fgcol bgcol   (set reverse fore- & background colors)\n"
"                  -b color         (set border color)\n"
"                  -c normal        (set cursor to inverting block)\n"
"                  -c blink         (set cursor to blinking inverted block)\n"
"                  -c destructive   (set cursor to blinking destructive char)\n"
"                  -d               (dump screenmap to stdout)\n"
"                  -l filename      (load srceenmap file filename)\n"
"                  -m on|off        (switch mousepointer support on or off)\n"
"                  -L               (load default screenmap)\n"
"                  -f DxL filename  (load font, D dots wide & L lines high)\n"
"                  -t N             (set screensaver timeout in seconds)\n"
"                  -x               (use hex numbers for output)\n"
	);
}

char *
nextarg(int ac, char **av, int *indp, int oc)
{
	if (*indp < ac)
		return(av[(*indp)++]);
	fprintf(stderr, "%s: option requires two arguments -- %c\n", av[0], oc);
	usage();
	exit(1);
	return("");
}

char *
mkfullname(const char *s1, const char *s2, const char *s3)
{
	static char *buf = NULL;
	static int bufl = 0;
	int f;

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
load_scrnmap(char *filename)
{
	FILE *fd;
	int i, size;
	char *name;
	scrmap_t scrnmap;
	char *prefix[]  = {"", "", SCRNMAP_PATH, SCRNMAP_PATH, NULL};
	char *postfix[] = {"", ".scm", "", ".scm"};

	for (i=0; prefix[i]; i++) {
		name = mkfullname(prefix[i], filename, postfix[i]);
		if (fd = fopen(name, "r"))
			break;
	}
	if (fd == NULL) {
		perror("screenmap file not found");
		return;
	}
	size = sizeof(scrnmap);
	if (decode(fd, &scrnmap) != size) {
		rewind(fd);
		if (fread(&scrnmap, 1, size, fd) != size) {
			fprintf(stderr, "bad scrnmap file\n");
			close(fd);
			return;
		}
	}
	if (ioctl(0, PIO_SCRNMAP, &scrnmap) < 0)
		perror("can't load screenmap");
	close(fd);
}

void
load_default_scrnmap()
{
	scrmap_t scrnmap;
	int i;

	for (i=0; i<256; i++)
		*((char*)&scrnmap + i) = i;
	if (ioctl(0, PIO_SCRNMAP, &scrnmap) < 0)
		perror("can't load default screenmap");
}

void
print_scrnmap()
{
	unsigned char map[256];
	int i;

	if (ioctl(0, GIO_SCRNMAP, &map) < 0) {
		perror("getting scrnmap");
		return;
	}
	for (i=0; i<sizeof(map); i++) {
		if (i > 0 && i % 16 == 0)
			fprintf(stdout, "\n");
		if (hex)
			fprintf(stdout, " %02x", map[i]);
		else
			fprintf(stdout, " %03d", map[i]);
	}
	fprintf(stdout, "\n");

}

void
load_font(char *type, char *filename)
{
	FILE	*fd;
	int	i, io, size;
	char	*name, *fontmap;
	char	*prefix[]  = {"", "", FONT_PATH, FONT_PATH, NULL};
	char	*postfix[] = {"", ".fnt", "", ".fnt"};

	for (i=0; prefix[i]; i++) {
		name = mkfullname(prefix[i], filename, postfix[i]);
		if (fd = fopen(name, "r"))
			break;
	}
	if (fd == NULL) {
		perror("font file not found");
		return;
	}
	if (!strcmp(type, "8x8")) {
		size = 8*256;
		io = PIO_FONT8x8;
	}
	else if (!strcmp(type, "8x14")) {
		size = 14*256;
		io = PIO_FONT8x14;
	}
	else if (!strcmp(type, "8x16")) {
		size = 16*256;
		io = PIO_FONT8x16;
	}
	else {
		perror("bad font size specification");
		close(fd);
		return;
	}
	fontmap = (char*) malloc(size);
	if (decode(fd, fontmap) != size) {
		rewind(fd);
		if (fread(fontmap, 1, size, fd) != size) {
			fprintf(stderr, "bad font file\n");
			close(fd);
			free(fontmap);
			return;
		}
	}
	if (ioctl(0, io, fontmap) < 0)
		perror("can't load font");
	close(fd);
	free(fontmap);
}

void
set_screensaver_timeout(char *arg)
{
	int nsec;

	if (!strcmp(arg, "off"))
		nsec = 0;
	else {
		nsec = atoi(arg);
		if ((*arg == '\0') || (nsec < 1)) {
			fprintf(stderr, "argument must be a positive number\n");
			return;
		}
	}
	if (ioctl(0, CONS_BLANKTIME, &nsec) == -1)
		perror("setting screensaver period");
}

void
set_cursor_type(char *appearence)
{
	int type;

	if (!strcmp(appearence, "normal"))
		type = 0;
	else if (!strcmp(appearence, "blink"))
		type = 1;
	else if (!strcmp(appearence, "destructive"))
		type = 3;
	else {
		fprintf(stderr,
		    "argument to -c must be normal, blink or destructive\n");
		return;
	}
	ioctl(0, CONS_CURSORTYPE, &type);
}

int
video_mode(int argc, char **argv, int *index)
{
	int mode;

	if (*index < argc) {
		if (!strcmp(argv[*index], "VGA_40x25"))
			mode = SW_VGA_C40x25;
		else if (!strcmp(argv[*index], "VGA_80x25"))
			mode = SW_VGA_C80x25;
		else if (!strcmp(argv[*index], "VGA_80x30"))
			mode = SW_VGA_C80x30;
		else if (!strcmp(argv[*index], "VGA_80x50"))
			mode = SW_VGA_C80x50;
		else if (!strcmp(argv[*index], "VGA_80x60"))
			mode = SW_VGA_C80x60;
		else if (!strcmp(argv[*index], "VGA_320x200"))
			mode = SW_VGA_CG320;
		else if (!strcmp(argv[*index], "EGA_80x25"))
			mode = SW_ENH_C80x25;
		else if (!strcmp(argv[*index], "EGA_80x43"))
			mode = SW_ENH_C80x43;
		else
			return;
		if (ioctl(0, mode, NULL) < 0)
			perror("Cannot set videomode");
		(*index)++;
	}
	return;
}

int
get_color_number(char *color)
{
	int i;

	for (i=0; i<16; i++)
		if (!strcmp(color, legal_colors[i]))
			return i;
	return -1;
}

int
set_normal_colors(int argc, char **argv, int *index)
{
	int color;

	if (*index < argc && (color = get_color_number(argv[*index])) != -1) {
		(*index)++;
		fprintf(stderr, "[=%dF", color);
		if (*index < argc
		    && (color = get_color_number(argv[*index])) != -1
		    && color < 8) {
			(*index)++;
			fprintf(stderr, "[=%dG", color);
		}
	}
}

set_reverse_colors(int argc, char **argv, int *index)
{
	int color;

	if ((color = get_color_number(argv[*(index)-1])) != -1) {
		fprintf(stderr, "[=%dH", color);
		if (*index < argc
		    && (color = get_color_number(argv[*index])) != -1
		    && color < 8) {
			(*index)++;
			fprintf(stderr, "[=%dI", color);
		}
	}
}

set_border_color(char *arg)
{
	int color;

	if ((color = get_color_number(arg)) != -1) {
		fprintf(stderr, "[=%dA", color);
	}
	else
		usage();
}

void
set_mouse(char *arg)
{
	struct mouse_info mouse;

	if (!strcmp(arg, "on"))
		mouse.operation = MOUSE_SHOW;
	else if (!strcmp(arg, "off"))
		mouse.operation = MOUSE_HIDE;
	else {
		fprintf(stderr,
		    "argument to -m must either on or off\n");
		return;
	}
	ioctl(0, CONS_MOUSECTL, &mouse);
}

test_frame()
{
	int i;

	fprintf(stdout, "[=0G\n\n");
	for (i=0; i<8; i++) {
		fprintf(stdout, "[=15F[=0G        %2d [=%dF%-16s"
				"[=15F[=0G        %2d [=%dF%-16s        "
				"[=15F %2d [=%dGBACKGROUND[=0G\n",
			i, i, legal_colors[i], i+8, i+8,
			legal_colors[i+8], i, i);
	}
	fprintf(stdout, "[=%dF[=%dG[=%dH[=%dI\n",
		info.mv_norm.fore, info.mv_norm.back,
		info.mv_rev.fore, info.mv_rev.back);
}

void
main(int argc, char **argv)
{
	extern char	*optarg;
	extern int	optind;
	int		opt;


	info.size = sizeof(info);
	if (ioctl(0, CONS_GETINFO, &info) < 0) {
		perror("Must be on a virtual console");
		exit(1);
	}
	while((opt = getopt(argc, argv, "b:c:df:l:Lm:r:t:x")) != -1)
		switch(opt) {
			case 'b':
				set_border_color(optarg);
				break;
			case 'c':
				set_cursor_type(optarg);
				break;
			case 'd':
				print_scrnmap();
				break;
			case 'f':
				load_font(optarg,
					nextarg(argc, argv, &optind, 'f'));
				break;
			case 'l':
				load_scrnmap(optarg);
				break;
			case 'L':
				load_default_scrnmap();
				break;
			case 'm':
				set_mouse(optarg);
				break;
			case 'r':
				set_reverse_colors(argc, argv, &optind);
				break;
			case 't':
				set_screensaver_timeout(optarg);
				break;
			case 'x':
				hex = 1;
				break;
			default:
				usage();
				exit(1);
		}
	if (video_mode(argc, argv, &optind)) ;
	if (set_normal_colors(argc, argv, &optind)) ;
	if (optind < argc && !strcmp(argv[optind], "show")) {
		test_frame();
		optind++;
	}
	if ((optind != argc) || (argc == 1)) {
		usage();
		exit(1);
	}
	exit(0);
}

