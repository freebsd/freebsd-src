/*-
 * Copyright (c) 1994-1996 Søren Schmidt
 * All rights reserved.
 *
 * Portions of this software are based in part on the work of
 * Sascha Wildner <saw@online.de> contributed to The DragonFly Project
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
 *    derived from this software without specific prior written permission
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
 * $DragonFly: src/usr.sbin/vidcontrol/vidcontrol.c,v 1.10 2005/03/02 06:08:29 joerg Exp $
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


#define	DATASIZE(x)	((x).w * (x).h * 256 / 8)

/* Screen dump modes */
#define DUMP_FMT_RAW	1
#define DUMP_FMT_TXT	2
/* Screen dump options */
#define DUMP_FBF	0
#define DUMP_ALL	1
/* Screen dump file format revision */
#define DUMP_FMT_REV	1

char 	legal_colors[16][16] = {
	"black", "blue", "green", "cyan",
	"red", "magenta", "brown", "white",
	"grey", "lightblue", "lightgreen", "lightcyan",
	"lightred", "lightmagenta", "yellow", "lightwhite"
};

struct {
	int			active_vty;
	vid_info_t		console_info;
	unsigned char		screen_map[256];
	int			video_mode_number;
	struct video_info	video_mode_info;
} cur_info;

int	hex = 0;
int	number;
int	vesa_cols;
int	vesa_rows;
int	font_height;
int	colors_changed;
int	video_mode_changed;
int	normal_fore_color, normal_back_color;
int	revers_fore_color, revers_back_color;
char	letter;
struct	vid_info info;
struct	video_info new_mode_info;


/*
 * Initialize revert data.
 *
 * NOTE: the following parameters are not yet saved/restored:
 *
 *   screen saver timeout
 *   cursor type
 *   mouse character and mouse show/hide state
 *   vty switching on/off state
 *   history buffer size
 *   history contents
 *   font maps
 */

static void
init(void)
{
	if (ioctl(0, VT_GETACTIVE, &cur_info.active_vty) == -1)
		errc(1, errno, "getting active vty");

	cur_info.console_info.size = sizeof(cur_info.console_info);

	if (ioctl(0, CONS_GETINFO, &cur_info.console_info) == -1)
		errc(1, errno, "getting console information");

	if (ioctl(0, GIO_SCRNMAP, &cur_info.screen_map) == -1)
		errc(1, errno, "getting screen map");

	if (ioctl(0, CONS_GET, &cur_info.video_mode_number) == -1)
		errc(1, errno, "getting video mode number");

	cur_info.video_mode_info.vi_mode = cur_info.video_mode_number;

	if (ioctl(0, CONS_MODEINFO, &cur_info.video_mode_info) == -1)
		errc(1, errno, "getting video mode parameters");

	normal_fore_color = cur_info.console_info.mv_norm.fore;
	normal_back_color = cur_info.console_info.mv_norm.back;
	revers_fore_color = cur_info.console_info.mv_rev.fore;
	revers_back_color = cur_info.console_info.mv_rev.back;
}


/*
 * If something goes wrong along the way we call revert() to go back to the
 * console state we came from (which is assumed to be working).
 *
 * NOTE: please also read the comments of init().
 */

static void
revert(void)
{
	int size[3];

	ioctl(0, VT_ACTIVATE, cur_info.active_vty);

	fprintf(stderr, "\033[=%dA", cur_info.console_info.mv_ovscan);
	fprintf(stderr, "\033[=%dF", cur_info.console_info.mv_norm.fore);
	fprintf(stderr, "\033[=%dG", cur_info.console_info.mv_norm.back);
	fprintf(stderr, "\033[=%dH", cur_info.console_info.mv_rev.fore);
	fprintf(stderr, "\033[=%dI", cur_info.console_info.mv_rev.back);

	ioctl(0, PIO_SCRNMAP, &cur_info.screen_map);

	if (cur_info.video_mode_number >= M_VESA_BASE)
		ioctl(0, _IO('V', cur_info.video_mode_number - M_VESA_BASE),
		      NULL);
	else
		ioctl(0, _IO('S', cur_info.video_mode_number), NULL);

	if (cur_info.video_mode_info.vi_flags & V_INFO_GRAPHICS) {
		size[0] = cur_info.video_mode_info.vi_width / 8;
		size[1] = cur_info.video_mode_info.vi_height /
			  cur_info.console_info.font_size;
		size[2] = cur_info.console_info.font_size;

		ioctl(0, KDRASTER, size);
	}
}


/*
 * Print a short usage string describing all options, then exit.
 */

static void
usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n",
"usage: vidcontrol [-CdHLPpx] [-b color] [-c appearance] [-f [size] file]",
"                  [-g geometry] [-h size] [-i adapter | mode] [-l screen_map]",
"                  [-M char] [-m on | off] [-r foreground background]",
"                  [-S on | off] [-s number] [-T xterm | cons25] [-t N | off]",
"                  [mode] [foreground [background]] [show]");
	exit(1);
}


/*
 * Retrieve the next argument from the command line (for options that require
 * more than one argument).
 */

static char *
nextarg(int ac, char **av, int *indp, int oc, int strict)
{
	if (*indp < ac)
		return(av[(*indp)++]);

	if (strict != 0) {
		revert();
		errx(1, "option requires two arguments -- %c", oc);
	}

	return(NULL);
}


/*
 * Guess which file to open. Try to open each combination of a specified set
 * of file name components.
 */

static FILE *
openguess(const char *a[], const char *b[], const char *c[], const char *d[], char **name)
{
	FILE *f;
	int i, j, k, l;

	for (i = 0; a[i] != NULL; i++) {
		for (j = 0; b[j] != NULL; j++) {
			for (k = 0; c[k] != NULL; k++) {
				for (l = 0; d[l] != NULL; l++) {
					asprintf(name, "%s%s%s%s",
						 a[i], b[j], c[k], d[l]);

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


/*
 * Load a screenmap from a file and set it.
 */

static void
load_scrnmap(const char *filename)
{
	FILE *fd;
	int size;
	char *name;
	scrmap_t scrnmap;
	const char *a[] = {"", SCRNMAP_PATH, NULL};
	const char *b[] = {filename, NULL};
	const char *c[] = {"", ".scm", NULL};
	const char *d[] = {"", NULL};

	fd = openguess(a, b, c, d, &name);

	if (fd == NULL) {
		revert();
		errx(1, "screenmap file not found");
	}

	size = sizeof(scrnmap);

	if (decode(fd, (char *)&scrnmap, size) != size) {
		rewind(fd);

		if (fread(&scrnmap, 1, size, fd) != (size_t)size) {
			warnx("bad screenmap file");
			fclose(fd);
			revert();
			errx(1, "bad screenmap file");
		}
	}

	if (ioctl(0, PIO_SCRNMAP, &scrnmap) == -1) {
		revert();
		errc(1, errno, "loading screenmap");
	}

	fclose(fd);
}


/*
 * Set the default screenmap.
 */

static void
load_default_scrnmap(void)
{
	scrmap_t scrnmap;
	int i;

	for (i=0; i<256; i++)
		*((char*)&scrnmap + i) = i;

	if (ioctl(0, PIO_SCRNMAP, &scrnmap) == -1) {
		revert();
		errc(1, errno, "loading default screenmap");
	}
}


/*
 * Print the current screenmap to stdout.
 */

static void
print_scrnmap(void)
{
	unsigned char map[256];
	size_t i;

	if (ioctl(0, GIO_SCRNMAP, &map) == -1) {
		revert();
		errc(1, errno, "getting screenmap");
	}
	for (i=0; i<sizeof(map); i++) {
		if (i != 0 && i % 16 == 0)
			fprintf(stdout, "\n");

		if (hex != 0)
			fprintf(stdout, " %02x", map[i]);
		else
			fprintf(stdout, " %03d", map[i]);
	}
	fprintf(stdout, "\n");

}


/*
 * Determine a file's size.
 */

static int
fsize(FILE *file)
{
	struct stat sb;

	if (fstat(fileno(file), &sb) == 0)
		return sb.st_size;
	else
		return -1;
}


/*
 * Load a font from file and set it.
 */

static void
load_font(const char *type, const char *filename)
{
	FILE	*fd;
	int	h, i, size, w;
	unsigned long io = 0;	/* silence stupid gcc(1) in the Wall mode */
	char	*name, *fontmap, size_sufx[6];
	const char	*a[] = {"", FONT_PATH, NULL};
	const char	*b[] = {filename, NULL};
	const char	*c[] = {"", size_sufx, NULL};
	const char	*d[] = {"", ".fnt", NULL};
	vid_info_t _info;

	struct sizeinfo {
		int w;
		int h;
		unsigned long io;
	} sizes[] = {{8, 16, PIO_FONT8x16},
		     {8, 14, PIO_FONT8x14},
		     {8,  8,  PIO_FONT8x8},
		     {0,  0,            0}};

	_info.size = sizeof(_info);
	if (ioctl(0, CONS_GETINFO, &_info) == -1) {
		revert();
		warn("failed to obtain current video mode parameters");
		return;
	}

	snprintf(size_sufx, sizeof(size_sufx), "-8x%d", _info.font_size);
	fd = openguess(a, b, c, d, &name);

	if (fd == NULL) {
		revert();
		errx(1, "%s: can't load font file", filename);
	}

	if (type != NULL) {
		size = 0;
		if (sscanf(type, "%dx%d", &w, &h) == 2) {
			for (i = 0; sizes[i].w != 0; i++) {
				if (sizes[i].w == w && sizes[i].h == h) {
					size = DATASIZE(sizes[i]);
					io = sizes[i].io;
					font_height = sizes[i].h;
				}
			}
		}
		if (size == 0) {
			fclose(fd);
			revert();
			errx(1, "%s: bad font size specification", type);
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
		for (j = 0; j < 2; j++) {
			for (i = 0; sizes[i].w != 0; i++) {
				if (DATASIZE(sizes[i]) == dsize[j]) {
					size = dsize[j];
					io = sizes[i].io;
					font_height = sizes[i].h;
					j = 2;	/* XXX */
					break;
				}
			}
		}

		if (size == 0) {
			fclose(fd);
			revert();
			errx(1, "%s: can't guess font size", filename);
		}

		rewind(fd);
	}

	fontmap = (char*) malloc(size);

	if (decode(fd, fontmap, size) != size) {
		rewind(fd);
		if (fsize(fd) != size ||
		    fread(fontmap, 1, size, fd) != (size_t)size) {
			warnx("%s: bad font file", filename);
			fclose(fd);
			free(fontmap);
			revert();
			errx(1, "%s: bad font file", filename);
		}
	}

	if (ioctl(0, io, fontmap) == -1) {
		revert();
		errc(1, errno, "loading font");
	}

	fclose(fd);
	free(fontmap);
}


/*
 * Set the timeout for the screensaver.
 */

static void
set_screensaver_timeout(char *arg)
{
	int nsec;

	if (!strcmp(arg, "off")) {
		nsec = 0;
	} else {
		nsec = atoi(arg);

		if ((*arg == '\0') || (nsec < 1)) {
			revert();
			errx(1, "argument must be a positive number");
		}
	}

	if (ioctl(0, CONS_BLANKTIME, &nsec) == -1) {
		revert();
		errc(1, errno, "setting screensaver period");
	}
}


/*
 * Set the cursor's shape/type.
 */

static void
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
		revert();
		errx(1, "argument to -c must be normal, blink or destructive");
	}

	if (ioctl(0, CONS_CURSORTYPE, &type) == -1) {
		revert();
		errc(1, errno, "setting cursor type");
	}
}


/*
 * Set the video mode.
 */

static int
video_mode(int argc, char **argv, int *mode_index)
{
	static struct {
		const char *name;
		unsigned long mode;
		unsigned long mode_num;
	} modes[] = {
		{ "80x25",        SW_TEXT_80x25,   M_TEXT_80x25 },
		{ "80x30",        SW_TEXT_80x30,   M_TEXT_80x30 },
		{ "80x43",        SW_TEXT_80x43,   M_TEXT_80x43 },
		{ "80x50",        SW_TEXT_80x50,   M_TEXT_80x50 },
		{ "80x60",        SW_TEXT_80x60,   M_TEXT_80x60 },
		{ "132x25",       SW_TEXT_132x25,  M_TEXT_132x25 },
		{ "132x30",       SW_TEXT_132x30,  M_TEXT_132x30 },
		{ "132x43",       SW_TEXT_132x43,  M_TEXT_132x43 },
		{ "132x50",       SW_TEXT_132x50,  M_TEXT_132x50 },
		{ "132x60",       SW_TEXT_132x60,  M_TEXT_132x60 },
		{ "VGA_40x25",    SW_VGA_C40x25,   M_VGA_C40x25 },
		{ "VGA_80x25",    SW_VGA_C80x25,   M_VGA_C80x25 },
		{ "VGA_80x30",    SW_VGA_C80x30,   M_VGA_C80x30 },
		{ "VGA_80x50",    SW_VGA_C80x50,   M_VGA_C80x50 },
		{ "VGA_80x60",    SW_VGA_C80x60,   M_VGA_C80x60 },
#ifdef SW_VGA_C90x25
		{ "VGA_90x25",    SW_VGA_C90x25,   M_VGA_C90x25 },
		{ "VGA_90x30",    SW_VGA_C90x30,   M_VGA_C90x30 },
		{ "VGA_90x43",    SW_VGA_C90x43,   M_VGA_C90x43 },
		{ "VGA_90x50",    SW_VGA_C90x50,   M_VGA_C90x50 },
		{ "VGA_90x60",    SW_VGA_C90x60,   M_VGA_C90x60 },
#endif
		{ "VGA_320x200",	SW_VGA_CG320,	M_CG320 },
		{ "EGA_80x25",		SW_ENH_C80x25,	M_ENH_C80x25 },
		{ "EGA_80x43",		SW_ENH_C80x43,	M_ENH_C80x43 },
		{ "VESA_132x25",	SW_VESA_C132x25,M_VESA_C132x25 },
		{ "VESA_132x43",	SW_VESA_C132x43,M_VESA_C132x43 },
		{ "VESA_132x50",	SW_VESA_C132x50,M_VESA_C132x50 },
		{ "VESA_132x60",	SW_VESA_C132x60,M_VESA_C132x60 },
		{ "VESA_800x600",	SW_VESA_800x600,M_VESA_800x600 },
		{ NULL, 0, 0 },
	};

	int new_mode_num = 0;
	unsigned long mode = 0;
	int cur_mode; 
	int ioerr;
	int size[3];
	int i;

	if (ioctl(0, CONS_GET, &cur_mode) < 0)
		err(1, "cannot get the current video mode");

	/*
	 * Parse the video mode argument...
	 */

	if (*mode_index < argc) {
		if (!strncmp(argv[*mode_index], "MODE_", 5)) {
			if (!isdigit(argv[*mode_index][5]))
				errx(1, "invalid video mode number");

			new_mode_num = atoi(&argv[*mode_index][5]);
		} else {
			for (i = 0; modes[i].name != NULL; ++i) {
				if (!strcmp(argv[*mode_index], modes[i].name)) {
					mode = modes[i].mode;
					new_mode_num = modes[i].mode_num;
					break;
				}
			}

			if (modes[i].name == NULL)
				return EXIT_FAILURE;
			if (ioctl(0, mode, NULL) < 0) {
				warn("cannot set videomode");
				return EXIT_FAILURE;
			}
		}

		/*
		 * Collect enough information about the new video mode...
		 */

		new_mode_info.vi_mode = new_mode_num;

		if (ioctl(0, CONS_MODEINFO, &new_mode_info) == -1) {
			revert();
			errc(1, errno, "obtaining new video mode parameters");
		}

		if (mode == 0) {
			if (new_mode_num >= M_VESA_BASE)
				mode = _IO('V', new_mode_num - M_VESA_BASE);
			else
				mode = _IO('S', new_mode_num);
		}

		/*
		 * Try setting the new mode.
		 */

		if (ioctl(0, mode, NULL) == -1) {
			revert();
			errc(1, errno, "setting video mode");
		}

		/*
		 * For raster modes it's not enough to just set the mode.
		 * We also need to explicitly set the raster mode.
		 */

		if (new_mode_info.vi_flags & V_INFO_GRAPHICS) {
			/* font size */

			if (font_height == 0)
				font_height = cur_info.console_info.font_size;

			size[2] = font_height;

			/* adjust columns */

			if ((vesa_cols * 8 > new_mode_info.vi_width) ||
			    (vesa_cols <= 0)) {
				size[0] = new_mode_info.vi_width / 8;
			} else {
				size[0] = vesa_cols;
			}

			/* adjust rows */

			if ((vesa_rows * font_height > new_mode_info.vi_height) ||
			    (vesa_rows <= 0)) {
				size[1] = new_mode_info.vi_height /
					  font_height;
			} else {
				size[1] = vesa_rows;
			}

			/* set raster mode */

			if (ioctl(0, KDRASTER, size)) {
				ioerr = errno;
				if (cur_mode >= M_VESA_BASE)
					ioctl(0,
					    _IO('V', cur_mode - M_VESA_BASE),
					    NULL);
				else
					ioctl(0, _IO('S', cur_mode), NULL);
				revert();
				warnc(ioerr, "cannot activate raster display");
				return EXIT_FAILURE;
			}
		}

		video_mode_changed = 1;

		(*mode_index)++;
	}
	return EXIT_SUCCESS;
}


/*
 * Return the number for a specified color name.
 */

static int
get_color_number(char *color)
{
	int i;

	for (i=0; i<16; i++) {
		if (!strcmp(color, legal_colors[i]))
			return i;
	}
	return -1;
}


/*
 * Get normal text and background colors.
 */

static void
get_normal_colors(int argc, char **argv, int *_index)
{
	int color;

	if (*_index < argc && (color = get_color_number(argv[*_index])) != -1) {
		(*_index)++;
		fprintf(stderr, "\033[=%dF", color);
		normal_fore_color=color;
		colors_changed = 1;
		if (*_index < argc
		    && (color = get_color_number(argv[*_index])) != -1
		    && color < 8) {
			(*_index)++;
			fprintf(stderr, "\033[=%dG", color);
			normal_back_color=color;
		}
	}
}


/*
 * Get reverse text and background colors.
 */

static void
get_reverse_colors(int argc, char **argv, int *_index)
{
	int color;

	if ((color = get_color_number(argv[*(_index)-1])) != -1) {
		fprintf(stderr, "\033[=%dH", color);
		revers_fore_color=color;
		colors_changed = 1;
		if (*_index < argc
		    && (color = get_color_number(argv[*_index])) != -1
		    && color < 8) {
			(*_index)++;
			fprintf(stderr, "\033[=%dI", color);
			revers_back_color=color;
		}
	}
}


/*
 * Set normal and reverse foreground and background colors.
 */

static void
set_colors(void)
{
	fprintf(stderr, "\033[=%dF", normal_fore_color);
	fprintf(stderr, "\033[=%dG", normal_back_color);
	fprintf(stderr, "\033[=%dH", revers_fore_color);
	fprintf(stderr, "\033[=%dI", revers_back_color);
}


/*
 * Switch to virtual terminal #arg.
 */

static void
set_console(char *arg)
{
	int n;

	if(!arg || strspn(arg,"0123456789") != strlen(arg)) {
		revert();
		errx(1, "bad console number");
	}

	n = atoi(arg);

	if (n < 1 || n > 16) {
		revert();
		errx(1, "console number out of range");
	} else if (ioctl(0, VT_ACTIVATE, n) == -1) {
		revert();
		errc(1, errno, "switching vty");
	}
}


/*
 * Sets the border color.
 */

static void
set_border_color(char *arg)
{
	int color;

	if ((color = get_color_number(arg)) != -1) {
		fprintf(stderr, "\033[=%dA", color);
	}
	else
		usage();
}

static void
set_mouse_char(char *arg)
{
	struct mouse_info mouse;
	long l;

	l = strtol(arg, NULL, 0);

	if ((l < 0) || (l > UCHAR_MAX - 3)) {
		revert();
		warnx("argument to -M must be 0 through %d", UCHAR_MAX - 3);
		return;
	}

	mouse.operation = MOUSE_MOUSECHAR;
	mouse.u.mouse_char = (int)l;

	if (ioctl(0, CONS_MOUSECTL, &mouse) == -1) {
		revert();
		errc(1, errno, "setting mouse character");
	}
}


/*
 * Show/hide the mouse.
 */

static void
set_mouse(char *arg)
{
	struct mouse_info mouse;

	if (!strcmp(arg, "on")) {
		mouse.operation = MOUSE_SHOW;
	} else if (!strcmp(arg, "off")) {
		mouse.operation = MOUSE_HIDE;
	} else {
		revert();
		errx(1, "argument to -m must be either on or off");
	}

	if (ioctl(0, CONS_MOUSECTL, &mouse) == -1) {
		revert();
		errc(1, errno, "%sing the mouse",
		     mouse.operation == MOUSE_SHOW ? "show" : "hid");
	}
}


static void
set_lockswitch(char *arg)
{
	int data;

	if (!strcmp(arg, "off")) {
		data = 0x01;
	} else if (!strcmp(arg, "on")) {
		data = 0x02;
	} else {
		revert();
		errx(1, "argument to -S must be either on or off");
	}

	if (ioctl(0, VT_LOCKSWITCH, &data) == -1) {
		revert();
		errc(1, errno, "turning %s vty switching",
		     data == 0x01 ? "off" : "on");
	}
}


/*
 * Return the adapter name for a specified type.
 */

static const char
*adapter_name(int type)
{
    static struct {
	int type;
	const char *name;
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


/*
 * Show graphics adapter information.
 */

static void
show_adapter_info(void)
{
	struct video_adapter_info ad;

	ad.va_index = 0;

	if (ioctl(0, CONS_ADPINFO, &ad) == -1) {
		revert();
		errc(1, errno, "obtaining adapter information");
	}

	printf("fb%d:\n", ad.va_index);
	printf("    %.*s%d, type:%s%s (%d), flags:0x%x\n",
	       (int)sizeof(ad.va_name), ad.va_name, ad.va_unit,
	       (ad.va_flags & V_ADP_VESA) ? "VESA " : "",
	       adapter_name(ad.va_type), ad.va_type, ad.va_flags);
	printf("    initial mode:%d, current mode:%d, BIOS mode:%d\n",
	       ad.va_initial_mode, ad.va_mode, ad.va_initial_bios_mode);
	printf("    frame buffer window:0x%zx, buffer size:0x%zx\n",
	       ad.va_window, ad.va_buffer_size);
	printf("    window size:0x%zx, origin:0x%x\n",
	       ad.va_window_size, ad.va_window_orig);
	printf("    display start address (%d, %d), scan line width:%d\n",
	       ad.va_disp_start.x, ad.va_disp_start.y, ad.va_line_width);
	printf("    reserved:0x%zx\n", ad.va_unused0);
}


/*
 * Show video mode information.
 */

static void
show_mode_info(void)
{
	char buf[80];
	struct video_info _info;
	int c;
	int mm;
	int mode;

	printf("    mode#     flags   type    size       "
	       "font      window      linear buffer\n");
	printf("---------------------------------------"
	       "---------------------------------------\n");

	for (mode = 0; mode < M_VESA_MODE_MAX; ++mode) {
		_info.vi_mode = mode;
		if (ioctl(0, CONS_MODEINFO, &_info))
			continue;
		if (_info.vi_mode != mode)
			continue;

		printf("%3d (0x%03x)", mode, mode);
    		printf(" 0x%08x", _info.vi_flags);
		if (_info.vi_flags & V_INFO_GRAPHICS) {
			c = 'G';

			if (_info.vi_mem_model == V_INFO_MM_PLANAR)
				snprintf(buf, sizeof(buf), "%dx%dx%d %d",
				    _info.vi_width, _info.vi_height, 
				    _info.vi_depth, _info.vi_planes);
			else {
				switch (_info.vi_mem_model) {
				case V_INFO_MM_PACKED:
					mm = 'P';
					break;
				case V_INFO_MM_DIRECT:
					mm = 'D';
					break;
				case V_INFO_MM_CGA:
					mm = 'C';
					break;
				case V_INFO_MM_HGC:
					mm = 'H';
					break;
				case V_INFO_MM_VGAX:
					mm = 'V';
					break;
				default:
					mm = ' ';
					break;
				}
				snprintf(buf, sizeof(buf), "%dx%dx%d %c",
				    _info.vi_width, _info.vi_height, 
				    _info.vi_depth, mm);
			}
		} else {
			c = 'T';

			snprintf(buf, sizeof(buf), "%dx%d",
				 _info.vi_width, _info.vi_height);
		}

		printf(" %c %-15s", c, buf);
		snprintf(buf, sizeof(buf), "%dx%d", 
			 _info.vi_cwidth, _info.vi_cheight); 
		printf(" %-5s", buf);
    		printf(" 0x%05zx %2dk %2dk", 
		       _info.vi_window, (int)_info.vi_window_size/1024, 
		       (int)_info.vi_window_gran/1024);
    		printf(" 0x%08zx %dk\n",
		       _info.vi_buffer, (int)_info.vi_buffer_size/1024);
	}
}


static void
show_info(char *arg)
{
	if (!strcmp(arg, "adapter")) {
		show_adapter_info();
	} else if (!strcmp(arg, "mode")) {
		show_mode_info();
	} else {
		revert();
		errx(1, "argument to -i must be either adapter or mode");
	}
}


static void
test_frame(void)
{
	int i, cur_mode, fore;

	fore = 15;

	if (ioctl(0, CONS_GET, &cur_mode) < 0)
		err(1, "must be on a virtual console");
	switch (cur_mode) {
	case M_PC98_80x25:
	case M_PC98_80x30:
		fore = 7;
		break;
	}

	fprintf(stdout, "\033[=0G\n\n");
	for (i=0; i<8; i++) {
		fprintf(stdout, "\033[=%dF\033[=0G        %2d \033[=%dF%-16s"
				"\033[=%dF\033[=0G        %2d \033[=%dF%-16s        "
				"\033[=%dF %2d \033[=%dGBACKGROUND\033[=0G\n",
			fore, i, i, legal_colors[i],
			fore, i+8, i+8, legal_colors[i+8],
			fore, i, i);
	}
	fprintf(stdout, "\033[=%dF\033[=%dG\033[=%dH\033[=%dI\n",
		info.mv_norm.fore, info.mv_norm.back,
		info.mv_rev.fore, info.mv_rev.back);
}


/*
 * Snapshot the video memory of that terminal, using the CONS_SCRSHOT
 * ioctl, and writes the results to stdout either in the special
 * binary format (see manual page for details), or in the plain
 * text format.
 */

static void
dump_screen(int mode, int opt)
{
	scrshot_t shot;
	vid_info_t _info;

	_info.size = sizeof(_info);

	if (ioctl(0, CONS_GETINFO, &_info) == -1) {
		revert();
		errc(1, errno, "obtaining current video mode parameters");
		return;
	}

	shot.x = shot.y = 0;
	shot.xsize = _info.mv_csz;
	shot.ysize = _info.mv_rsz;
	if (opt == DUMP_ALL)
		shot.ysize += _info.mv_hsz;

	shot.buf = alloca(shot.xsize * shot.ysize * sizeof(u_int16_t));
	if (shot.buf == NULL) {
		revert();
		errx(1, "failed to allocate memory for dump");
	}

	if (ioctl(0, CONS_SCRSHOT, &shot) == -1) {
		revert();
		errc(1, errno, "dumping screen");
	}

	if (mode == DUMP_FMT_RAW) {
		printf("SCRSHOT_%c%c%c%c", DUMP_FMT_REV, 2,
		       shot.xsize, shot.ysize);

		fflush(stdout);

		write(STDOUT_FILENO, shot.buf,
		      shot.xsize * shot.ysize * sizeof(u_int16_t));
	} else {
		char *line;
		int x, y;
		u_int16_t ch;

		line = alloca(shot.xsize + 1);

		if (line == NULL) {
			revert();
			errx(1, "failed to allocate memory for line buffer");
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
}


/*
 * Set the console history buffer size.
 */

static void
set_history(char *opt)
{
	int size;

	size = atoi(opt);

	if ((*opt == '\0') || size < 0) {
		revert();
		errx(1, "argument must be a positive number");
	}

	if (ioctl(0, CONS_HISTORY, &size) == -1) {
		revert();
		errc(1, errno, "setting history buffer size");
	}
}


/*
 * Clear the console history buffer.
 */

static void
clear_history(void)
{
	if (ioctl(0, CONS_CLRHIST) == -1) {
		revert();
		errc(1, errno, "clearing history buffer");
	}
}

static void
set_terminal_mode(char *arg)
{

	if (strcmp(arg, "xterm") == 0)
		fprintf(stderr, "\033[=T");
	else if (strcmp(arg, "cons25") == 0)
		fprintf(stderr, "\033[=1T");
	else
		usage();
}


int
main(int argc, char **argv)
{
	char	*font, *type;
	int	dumpmod, dumpopt, opt;
	int	reterr;

	init();

	info.size = sizeof(info);

	if (ioctl(0, CONS_GETINFO, &info) == -1)
		err(1, "must be on a virtual console");
	dumpmod = 0;
	dumpopt = DUMP_FBF;
	while ((opt = getopt(argc, argv,
	    "b:Cc:df:g:h:Hi:l:LM:m:pPr:S:s:T:t:x")) != -1)
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
			if (sscanf(optarg, "%dx%d",
			    &vesa_cols, &vesa_rows) != 2) {
				revert();
				warnx("incorrect geometry: %s", optarg);
				usage();
			}
                	break;
		case 'h':
			set_history(optarg);
			break;
		case 'H':
			dumpopt = DUMP_ALL;
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
			dumpmod = DUMP_FMT_RAW;
			break;
		case 'P':
			dumpmod = DUMP_FMT_TXT;
			break;
		case 'r':
			get_reverse_colors(argc, argv, &optind);
			break;
		case 'S':
			set_lockswitch(optarg);
			break;
		case 's':
			set_console(optarg);
			break;
		case 'T':
			set_terminal_mode(optarg);
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

	if (dumpmod != 0)
		dump_screen(dumpmod, dumpopt);
	reterr = video_mode(argc, argv, &optind);
	get_normal_colors(argc, argv, &optind);

	if (optind < argc && !strcmp(argv[optind], "show")) {
		test_frame();
		optind++;
	}

	video_mode(argc, argv, &optind);

	get_normal_colors(argc, argv, &optind);

	if (colors_changed || video_mode_changed) {
		if (!(new_mode_info.vi_flags & V_INFO_GRAPHICS)) {
			if ((normal_back_color < 8) && (revers_back_color < 8)) {
				set_colors();
			} else {
				revert();
				errx(1, "bg color for text modes must be < 8");
			}
		} else {
			set_colors();
		}
	}

	if ((optind != argc) || (argc == 1))
		usage();
	return reterr;
}

