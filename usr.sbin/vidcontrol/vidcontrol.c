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
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/fbio.h>
#include <sys/consio.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "path.h"
#include "decode.h"

#define _VESA_800x600_DFL_COLS 80
#define _VESA_800x600_DFL_ROWS 25
#define _VESA_800x600_DFL_FNSZ 16

#define DUMP_RAW	0
#define DUMP_TXT	1

#define DUMP_FMT_REV	1

char 	legal_colors[16][16] = {
	"black", "blue", "green", "cyan",
	"red", "magenta", "brown", "white",
	"grey", "lightblue", "lightgreen", "lightcyan",
	"lightred", "lightmagenta", "yellow", "lightwhite"
};
int 	hex = 0;
int 	number;
int	vesa_cols = _VESA_800x600_DFL_COLS;
int	vesa_rows = _VESA_800x600_DFL_ROWS;
char 	letter;
struct 	vid_info info;


static void
usage()
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n",
"usage: vidcontrol [-CdLPpx] [-b color] [-c appearance] [-f [size] file]",
"                  [-g geometry] [-h size] [-i adapter | mode] [-l screen_map]",
"                  [-m on | off] [-M char] [-r foreground background] [-s num]",
"                  [-t N | off] [mode] [foreground [background]] [show]");
	exit(1);
}

char *
nextarg(int ac, char **av, int *indp, int oc, int strict)
{
	if (*indp < ac)
		return(av[(*indp)++]);
	if (strict != 0)
		errx(1, "option requires two arguments -- %c", oc);
	return(NULL);
}

FILE *
openguess(char *a[], char *b[], char *c[], char *d[], char **name)
{
	FILE *f;
	int i, j, k, l;

	for (i = 0; a[i] != NULL; i++) {
		for (j = 0; b[j] != NULL; j++) {
			for (k = 0; c[k] != NULL; k++) {
				for (l = 0; d[l] != NULL; l++) {
					asprintf(name, "%s%s%s%s", a[i], b[j],
					    c[k], d[l]);
					f = fopen(*name, "r");
					if (f != NULL)
						return (f);
					free(*name);
				}
			}
		}
	}
	return (NULL);
}

void
load_scrnmap(char *filename)
{
	FILE *fd;
	int size;
	char *name;
	scrmap_t scrnmap;
	char *a[] = {"", SCRNMAP_PATH, NULL};
	char *b[] = {filename, NULL};
	char *c[] = {"", ".scm", NULL};
	char *d[] = {"", NULL};

	fd = openguess(a, b, c, d, &name);
	if (fd == NULL) {
		warn("screenmap file not found");
		return;
	}
	size = sizeof(scrnmap);
	if (decode(fd, (char *)&scrnmap, size) != size) {
		rewind(fd);
		if (fread(&scrnmap, 1, size, fd) != size) {
			warnx("bad screenmap file");
			fclose(fd);
			return;
		}
	}
	if (ioctl(0, PIO_SCRNMAP, &scrnmap) < 0)
		warn("can't load screenmap");
	fclose(fd);
}

void
load_default_scrnmap()
{
	scrmap_t scrnmap;
	int i;

	for (i=0; i<256; i++)
		*((char*)&scrnmap + i) = i;
	if (ioctl(0, PIO_SCRNMAP, &scrnmap) < 0)
		warn("can't load default screenmap");
}

void
print_scrnmap()
{
	unsigned char map[256];
	int i;

	if (ioctl(0, GIO_SCRNMAP, &map) < 0) {
		warn("getting screenmap");
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

int
fsize(FILE *file)
{
	struct stat sb;

	if (fstat(fileno(file), &sb) == 0)
		return sb.st_size;
	else
		return -1;
}

#define DATASIZE(x) ((x).w * (x).h * 256 / 8)

void
load_font(char *type, char *filename)
{
	FILE	*fd;
	int	h, i, size, w;
	unsigned long io = 0;	/* silence stupid gcc(1) in the Wall mode */
	char	*name, *fontmap, size_sufx[6];
	char	*a[] = {"", FONT_PATH, NULL};
	char	*b[] = {filename, NULL};
	char	*c[] = {"", size_sufx, NULL};
	char	*d[] = {"", ".fnt", NULL};
	vid_info_t info;

	struct sizeinfo {
		int w;
		int h;
		unsigned long io;
	} sizes[] = {{8, 16, PIO_FONT8x16},
		     {8, 14, PIO_FONT8x14},
		     {8,  8,  PIO_FONT8x8},
		     {0,  0,            0}};

	info.size = sizeof(info);
	if (ioctl(0, CONS_GETINFO, &info) == -1) {
		warn("failed to obtain current video mode parameters");
		return;
	}
	snprintf(size_sufx, sizeof(size_sufx), "-8x%d", info.font_size);
	fd = openguess(a, b, c, d, &name);
	if (fd == NULL) {
		warn("%s: can't load font file", filename);
		return;
	}
	if (type != NULL) {
		size = 0;
		if (sscanf(type, "%dx%d", &w, &h) == 2)
			for (i = 0; sizes[i].w != 0; i++)
				if (sizes[i].w == w && sizes[i].h == h) {
					size = DATASIZE(sizes[i]);
					io = sizes[i].io;
				}

		if (size == 0) {
			warnx("%s: bad font size specification", type);
			fclose(fd);
			return;
		}
	} else {
		/* Apply heuristics */
		int j;
		int dsize[2];

		size = DATASIZE(sizes[0]);
		fontmap = (char*) malloc(size);
		dsize[0] = decode(fd, fontmap, size);
		dsize[1] = fsize(fd);
		free(fontmap);

		size = 0;
		for (j = 0; j < 2; j++)
			for (i = 0; sizes[i].w != 0; i++)
				if (DATASIZE(sizes[i]) == dsize[j]) {
					size = dsize[j];
					io = sizes[i].io;
					j = 2;	/* XXX */
					break;
				}

		if (size == 0) {
			warnx("%s: can't guess font size", filename);
			fclose(fd);
			return;
		}
		rewind(fd);
	}

	fontmap = (char*) malloc(size);
	if (decode(fd, fontmap, size) != size) {
		rewind(fd);
		if (fsize(fd) != size || fread(fontmap, 1, size, fd) != size) {
			warnx("%s: bad font file", filename);
			fclose(fd);
			free(fontmap);
			return;
		}
	}
	if (ioctl(0, io, fontmap) < 0)
		warn("can't load font");
	fclose(fd);
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
			warnx("argument must be a positive number");
			return;
		}
	}
	if (ioctl(0, CONS_BLANKTIME, &nsec) == -1)
		warn("setting screensaver period");
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
		warnx("argument to -c must be normal, blink or destructive");
		return;
	}
	ioctl(0, CONS_CURSORTYPE, &type);
}

void
video_mode(int argc, char **argv, int *index)
{
	static struct {
		char *name;
		unsigned long mode;
	} modes[] = {
		{ "80x25",		SW_TEXT_80x25 },
		{ "80x30",		SW_TEXT_80x30 },
		{ "80x43",		SW_TEXT_80x43 },
		{ "80x50",		SW_TEXT_80x50 },
		{ "80x60",		SW_TEXT_80x60 },
		{ "132x25",		SW_TEXT_132x25 },
		{ "132x30",		SW_TEXT_132x30 },
		{ "132x43",		SW_TEXT_132x43 },
		{ "132x50",		SW_TEXT_132x50 },
		{ "132x60",		SW_TEXT_132x60 },
		{ "VGA_40x25",		SW_VGA_C40x25 },
		{ "VGA_80x25",		SW_VGA_C80x25 },
		{ "VGA_80x30",		SW_VGA_C80x30 },
		{ "VGA_80x50",		SW_VGA_C80x50 },
		{ "VGA_80x60",		SW_VGA_C80x60 },
#ifdef SW_VGA_C90x25
		{ "VGA_90x25",		SW_VGA_C90x25 },
		{ "VGA_90x30",		SW_VGA_C90x30 },
		{ "VGA_90x43",		SW_VGA_C90x43 },
		{ "VGA_90x50",		SW_VGA_C90x50 },
		{ "VGA_90x60",		SW_VGA_C90x60 },
#endif
		{ "VGA_320x200",	SW_VGA_CG320 },
		{ "EGA_80x25",		SW_ENH_C80x25 },
		{ "EGA_80x43",		SW_ENH_C80x43 },
		{ "VESA_132x25",	SW_VESA_C132x25 },
		{ "VESA_132x43",	SW_VESA_C132x43 },
		{ "VESA_132x50",	SW_VESA_C132x50 },
		{ "VESA_132x60",	SW_VESA_C132x60 },
		{ "VESA_800x600",	SW_VESA_800x600 },
		{ NULL },
	};
	unsigned long mode = 0;
	int cur_mode; 
	int ioerr;
	int size[3];
	int i;

	if (ioctl(0, CONS_GET, &cur_mode) < 0)
		err(1, "cannot get the current video mode");
	if (*index < argc) {
		for (i = 0; modes[i].name != NULL; ++i) {
			if (!strcmp(argv[*index], modes[i].name)) {
				mode = modes[i].mode;
				break;
			}
		}
		if (modes[i].name == NULL)
			return;
		if (ioctl(0, mode, NULL) < 0)
			warn("cannot set videomode");
		if (mode == SW_VESA_800x600) {
			/* columns */
			if ((vesa_cols * 8 > 800) || (vesa_cols <= 0)) {
				warnx("incorrect number of columns: %d",
				      vesa_cols);
				size[0] = _VESA_800x600_DFL_COLS;
			} else {
				size[0] = vesa_cols;
			}
			/* rows */
			if ((vesa_rows * _VESA_800x600_DFL_FNSZ > 600) ||
			    (vesa_rows <=0)) {
				warnx("incorrect number of rows: %d",
				      vesa_rows);
				size[1] = _VESA_800x600_DFL_ROWS;
			} else {
				size[1] = vesa_rows;
			}
			/* font size */
			size[2] = _VESA_800x600_DFL_FNSZ;
			if (ioctl(0, KDRASTER, size)) {
				ioerr = errno;
				if (cur_mode >= M_VESA_BASE)
					ioctl(0,
					    _IO('V', cur_mode - M_VESA_BASE),
					    NULL);
				else
					ioctl(0, _IO('S', cur_mode), NULL);
				warnc(ioerr, "cannot activate raster display");
			}
		}
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

void
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

void
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

void
set_console(char *arg)
{
	int n;

	if( !arg || strspn(arg,"0123456789") != strlen(arg)) {
		warnx("bad console number");
		return;
	}

	n = atoi(arg);
	if (n < 1 || n > 16) {
		warnx("console number out of range");
	} else if (ioctl(0, VT_ACTIVATE, (caddr_t) (long) n) == -1)
		warn("ioctl(VT_ACTIVATE)");
}

void
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
set_mouse_char(char *arg)
{
	struct mouse_info mouse;
	long l;

	l = strtol(arg, NULL, 0);
	if ((l < 0) || (l > UCHAR_MAX - 3)) {
		warnx("argument to -M must be 0 through %d", UCHAR_MAX - 3);
		return;
	}
	mouse.operation = MOUSE_MOUSECHAR;
	mouse.u.mouse_char = (int)l;
	ioctl(0, CONS_MOUSECTL, &mouse);
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
		warnx("argument to -m must either on or off");
		return;
	}
	ioctl(0, CONS_MOUSECTL, &mouse);
}

static char
*adapter_name(int type)
{
    static struct {
	int type;
	char *name;
    } names[] = {
	{ KD_MONO,	"MDA" },
	{ KD_HERCULES,	"Hercules" },
	{ KD_CGA,	"CGA" },
	{ KD_EGA,	"EGA" },
	{ KD_VGA,	"VGA" },
	{ KD_PC98,	"PC-98xx" },
	{ KD_TGA,	"TGA" },
	{ -1,		"Unknown" },
    };
    int i;

    for (i = 0; names[i].type != -1; ++i)
	if (names[i].type == type)
	    break;
    return names[i].name;
}

void
show_adapter_info(void)
{
	struct video_adapter_info ad;

	ad.va_index = 0;
	if (ioctl(0, CONS_ADPINFO, &ad)) {
		warn("failed to obtain adapter information");
		return;
	}

	printf("fb%d:\n", ad.va_index);
	printf("    %.*s%d, type:%s%s (%d), flags:0x%x\n",
	       (int)sizeof(ad.va_name), ad.va_name, ad.va_unit,
	       (ad.va_flags & V_ADP_VESA) ? "VESA " : "",
	       adapter_name(ad.va_type), ad.va_type, ad.va_flags);
	printf("    initial mode:%d, current mode:%d, BIOS mode:%d\n",
	       ad.va_initial_mode, ad.va_mode, ad.va_initial_bios_mode);
	printf("    frame buffer window:0x%x, buffer size:0x%x\n",
	       ad.va_window, ad.va_buffer_size);
	printf("    window size:0x%x, origin:0x%x\n",
	       ad.va_window_size, ad.va_window_orig);
	printf("    display start address (%d, %d), scan line width:%d\n",
	       ad.va_disp_start.x, ad.va_disp_start.y, ad.va_line_width);
	printf("    reserved:0x%x\n", ad.va_unused0);
}

void
show_mode_info(void)
{
	struct video_info info;
	char buf[80];
	int mode;
	int c;

	printf("    mode#     flags   type    size       "
	       "font      window      linear buffer\n");
	printf("---------------------------------------"
	       "---------------------------------------\n");
	for (mode = 0; mode < M_VESA_MODE_MAX; ++mode) {
		info.vi_mode = mode;
		if (ioctl(0, CONS_MODEINFO, &info))
			continue;
		if (info.vi_mode != mode)
			continue;

		printf("%3d (0x%03x)", mode, mode);
    		printf(" 0x%08x", info.vi_flags);
		if (info.vi_flags & V_INFO_GRAPHICS) {
			c = 'G';
			snprintf(buf, sizeof(buf), "%dx%dx%d %d",
				 info.vi_width, info.vi_height, 
				 info.vi_depth, info.vi_planes);
		} else {
			c = 'T';
			snprintf(buf, sizeof(buf), "%dx%d",
				 info.vi_width, info.vi_height);
		}
		printf(" %c %-15s", c, buf);
		snprintf(buf, sizeof(buf), "%dx%d", 
			 info.vi_cwidth, info.vi_cheight); 
		printf(" %-5s", buf);
    		printf(" 0x%05x %2dk %2dk", 
		       info.vi_window, (int)info.vi_window_size/1024, 
		       (int)info.vi_window_gran/1024);
    		printf(" 0x%08x %dk\n",
		       info.vi_buffer, (int)info.vi_buffer_size/1024);
	}
}

void
show_info(char *arg)
{
	if (!strcmp(arg, "adapter"))
		show_adapter_info();
	else if (!strcmp(arg, "mode"))
		show_mode_info();
	else {
		warnx("argument to -i must either adapter or mode");
		return;
	}
}

void
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

/*
 * Snapshot the video memory of that terminal, using the CONS_SCRSHOT
 * ioctl, and writes the results to stdout either in the special
 * binary format (see manual page for details), or in the plain
 * text format.
 */
void
dump_screen(int mode)
{
	scrshot_t shot;
	vid_info_t info;

	info.size = sizeof(info);
	if (ioctl(0, CONS_GETINFO, &info) == -1) {
		warn("failed to obtain current video mode parameters");
		return;
	}

	shot.buf = alloca(info.mv_csz * info.mv_rsz * sizeof(u_int16_t));
	if (shot.buf == NULL) {
		warn("failed to allocate memory for dump");
		return;
	}

	shot.xsize = info.mv_csz;
	shot.ysize = info.mv_rsz;
	if (ioctl(0, CONS_SCRSHOT, &shot) == -1) {
		warn("failed to get dump of the screen");
		return;
	}

	if (mode == DUMP_RAW) {
		printf("SCRSHOT_%c%c%c%c", DUMP_FMT_REV, 2,
		       shot.xsize, shot.ysize);
		fflush(stdout);

		(void)write(STDOUT_FILENO, shot.buf,
			    shot.xsize * shot.ysize * sizeof(u_int16_t));
	} else {
		char *line;
		int x, y;
		u_int16_t ch;

		line = alloca(shot.xsize + 1);
		if (line == NULL) {
			warn("failed to allocate memory for line buffer");
			return;
		}

		for (y = 0; y < shot.ysize; y++) {
			for (x = 0; x < shot.xsize; x++) {
				ch = shot.buf[x + (y * shot.xsize)];
				ch &= 0xff;
				if (isprint(ch) == 0)
					ch = ' ';
				line[x] = (char)ch;
			}

			/* Trim trailing spaces */
			do {
				line[x--] = '\0';
			} while (line[x] == ' ' && x != 0);

			puts(line);
		}
		fflush(stdout);
	}

	return;
}

void
set_history(char *opt)
{
	int size;

	size = atoi(opt);
	if ((*opt == '\0') || size < 0) {
		warnx("argument must be a positive number");
		return;
	}
	if (ioctl(0, CONS_HISTORY, &size) == -1)
		warn("setting history buffer size");
}

void
clear_history()
{

	if (ioctl(0, CONS_CLRHIST) == -1)
		warn("clear history buffer");
}

int
main(int argc, char **argv)
{
	char	*font, *type;
	int	opt;


	info.size = sizeof(info);
	if (argc == 1)
		usage();
		/* Not reached */
	if (ioctl(0, CONS_GETINFO, &info) < 0)
		err(1, "must be on a virtual console");
	while((opt = getopt(argc, argv, "b:Cc:df:g:h:i:l:LM:m:pPr:s:t:x")) != -1)
		switch(opt) {
		case 'b':
			set_border_color(optarg);
			break;
		case 'C':
			clear_history();
			break;
		case 'c':
			set_cursor_type(optarg);
			break;
		case 'd':
			print_scrnmap();
			break;
		case 'f':
			type = optarg;
			font = nextarg(argc, argv, &optind, 'f', 0);
			if (font == NULL) {
				type = NULL;
				font = optarg;
			}
			load_font(type, font);
			break;
		case 'g':
			if (sscanf(optarg, "%dx%d", &vesa_cols,
			    &vesa_rows) != 2) {
				warnx("incorrect geometry: %s", optarg);
				usage();
			}
			break;
		case 'h':
			set_history(optarg);
			break;
		case 'i':
			show_info(optarg);
			break;
		case 'l':
			load_scrnmap(optarg);
			break;
		case 'L':
			load_default_scrnmap();
			break;
		case 'M':
			set_mouse_char(optarg);
			break;
		case 'm':
			set_mouse(optarg);
			break;
		case 'p':
			dump_screen(DUMP_RAW);
			break;
		case 'P':
			dump_screen(DUMP_TXT);
			break;
		case 'r':
			set_reverse_colors(argc, argv, &optind);
			break;
		case 's':
			set_console(optarg);
			break;
		case 't':
			set_screensaver_timeout(optarg);
			break;
		case 'x':
			hex = 1;
			break;
		default:
			usage();
		}
	video_mode(argc, argv, &optind);
	set_normal_colors(argc, argv, &optind);
	if (optind < argc && !strcmp(argv[optind], "show")) {
		test_frame();
		optind++;
	}
	if ((optind != argc) || (argc == 1))
		usage();
	return 0;
}

