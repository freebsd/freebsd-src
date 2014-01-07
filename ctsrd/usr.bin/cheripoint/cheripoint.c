/*-
 * Copyright (c) 2012 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <sys/wait.h>

#include <machine/cheri.h>

#include <cheri/sandbox.h>

#include <terasic_mtl.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <imagebox.h>
#include <libutil.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#define vwhite(v)       fb_colour((v), (v), (v))
#define black           vwhite(0)
#define white           vwhite(0xFF)
#define vred(v)		fb_colour((v), 0, 0)
#define red		vred(0xFF)

#define ICON_WH         32

enum mtl_display_mode {
	MTL_DM_800x480,		/* Full touch screen */
	MTL_DM_720x480,		/* Full 480p HDMI out */
	MTL_DM_640x480,		/* 640x480 VGA from 480p, left pixels */
	MTL_DM_640x480_CENTER	/* 640x480 VGA from 480p, center pixels */
};

int verbose = 0;
int sb_vis = 0;
uint32_t header_height;
uint32_t *busyarea, *hourglass;
enum sbtype sb = SB_CHERI;
enum mtl_display_mode res = MTL_DM_800x480;
static int zombies_waiting = 0;

int *slidep;

static uint32_t slide_fcol;
static uint32_t slide_width;
static uint32_t slide_height;

static void __dead2
usage(void)
{
	
	fprintf(stderr, "cheripoint [-f] <slidedir>\n");
	exit(1);
}

static void
init_busy(void)
{
	int pfd;
	struct iboxstate *is;

	if ((busyarea = malloc(sizeof(uint32_t) * 32 * 32)) == NULL)
		err(1, "malloc of busyarea failed");

	if ((pfd = open("/usr/share/images/icons.png", O_RDONLY)) == -1)
		err(1, "Failed to open icons.png");
	if ((is = png_read_start(pfd, 32, 2048, SB_NONE)) == NULL)
		errx(1, "Failed to start PNG decode for icons.png");
	if (png_read_finish(is) != 0)
		errx(1, "png_read_finish() failed for icons.png");
	
	if ((hourglass = malloc(sizeof(uint32_t) * 32 * 32)) == NULL)
		err(1, "malloc of hourglass failed");
	memcpy(hourglass, __DEVOLATILE(uint32_t *,
	    is->buffer + (32 * 32 * 20)), sizeof(uint32_t) * 32 * 32);
	iboxstate_free(is);
}

static void
busy(int init)
{
	int c, r;

	if (init)
		memcpy(busyarea, hourglass,
		    sizeof(uint32_t) * ICON_WH * ICON_WH);
	else
		/*
		 * Save all parts of the busy area that don't match the
		 * hourglass.
		 */
		for (r = 0; r < ICON_WH; r++)
			for(c = 0; c < ICON_WH; c++)
				if (pfbp[slide_fcol + r * fb_width + c] !=
				    hourglass[r * ICON_WH + c])
					busyarea[r * ICON_WH + c] =
					    pfbp[slide_fcol + r * fb_width + c];
	/* Draw the hourglass */
	fb_post_region(hourglass, slide_fcol, 0, ICON_WH, ICON_WH);
}

static void
unbusy(void)
{
	int c, r;
	
	busy(0);

	/* Restore the parts that don't match the hourglass */
	for (r = 0; r < ICON_WH; r++)
		for(c = 0; c < ICON_WH; c++)
			if (busyarea[r * ICON_WH + c] !=
			    hourglass[r * ICON_WH + c])
				pfbp[slide_fcol + r * fb_width + c] =
				    busyarea[r * ICON_WH + c];
}

static void
set_display_mode(enum mtl_display_mode dm)
{

	slide_height = 410;
	slide_fcol = 0;

	switch (dm) {
	case (MTL_DM_800x480):
		slide_width = 800;
		break;
	case (MTL_DM_720x480):
		slide_width = 720;
		break;
	case (MTL_DM_640x480):
		slide_width = 640;
		break;
	case (MTL_DM_640x480_CENTER):
		slide_width = 640;
		slide_fcol = 40;
		break;
	}
}

/*
 * Draw simple configuration dialog, let the user toggle things.  When
 * done (user pinches) return 0 if nothing changed, >0 if something did.
 *
 * The screen looks like:
 *
 * Sandbox        Resolution
 * (*) None       ( ) 800x480
 * ( ) Capsicum   (*) 720x480
 * ( ) CHERI      ( ) 640x480 (Left)
 *                ( ) 640x480 (Centered)
 * [ ] Visible
 *
 */
#define	CD_BORDER_WIDTH	8
#define	CD_TEXT_SCALE	2
#define	CD_TEXT_ROWS	6
#define	CD_SB_COLS	12
#define CD_GAP		16
#define	CD_RES_COLS	22
static int
config_dialog(void)
{
	int changed = 0;
	int f_height, f_width, tbufcols;
	char *text;
	uint32_t d_width, d_height, d_x, d_y;
	uint32_t cfg_startr, row;
	uint32_t sb_startc, sb_endc;
	uint32_t res_startc, res_endc;
	uint32_t *dbuf, *savebuf, *textbuf;
	struct tsstate *ts;

	dbuf = savebuf = textbuf = NULL;
	text = NULL;

	if ((savebuf = malloc(sizeof(uint32_t) * fb_width * fb_height)) == NULL)
		return (-1);

	f_width = fb_get_font_width() * 2;
	f_height = fb_get_font_height() * 2;
	tbufcols = CD_RES_COLS; /* XXX: max chars */
	textbuf = malloc(sizeof(uint32_t) * f_height * f_width * tbufcols);
	if (textbuf == NULL) {
		changed = -1;
		goto error;
	}
	if ((text = malloc(tbufcols + 1)) == NULL) {
		changed = -1;
		goto error;
	}

	d_width = (CD_BORDER_WIDTH * 2) + CD_GAP +
	   ((CD_SB_COLS + CD_RES_COLS) * f_width);
	d_height = (CD_BORDER_WIDTH * 2) + (CD_TEXT_ROWS * f_height);
	d_x = (fb_width - d_width) / 2;
	d_y = (fb_height - d_height) / 2;
	if ((dbuf = malloc(sizeof(uint32_t) * d_width * d_height)) == NULL) {
		changed = -1;
		goto error;
	}

	fb_save(savebuf);

repaint:
	fb_fill_buf(dbuf, white, d_width, d_height);

	cfg_startr = CD_BORDER_WIDTH;
	sb_startc = CD_BORDER_WIDTH;
	sb_endc = sb_startc + (CD_SB_COLS * f_width);
	res_startc = sb_endc + CD_GAP;
	res_endc = res_startc + (CD_RES_COLS * f_width);

	fb_fill_buf(textbuf, white, f_width * CD_SB_COLS, f_height);
	fb_render_text("Sandbox",
	    2, black, white, textbuf, f_width * CD_SB_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    sb_startc, cfg_startr + (0 * f_height), 
	    textbuf, f_width * CD_SB_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_SB_COLS, f_height);
	sprintf(text, "(%c) None", sb == SB_NONE ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_SB_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    sb_startc, cfg_startr + (1 * f_height), 
	    textbuf, f_width * CD_SB_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_SB_COLS, f_height);
	sprintf(text, "(%c) Capsicum", sb == SB_CAPSICUM ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_SB_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    sb_startc, cfg_startr + (2 * f_height), 
	    textbuf, f_width * CD_SB_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_SB_COLS, f_height);
	sprintf(text, "(%c) CHERI", sb == SB_CHERI ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_SB_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    sb_startc, cfg_startr + (3 * f_height), 
	    textbuf, f_width * CD_SB_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_SB_COLS, f_height);
	sprintf(text, "[%c] Visible", sb_vis ? 'X' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_SB_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    sb_startc, cfg_startr + (5 * f_height), 
	    textbuf, f_width * CD_SB_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_RES_COLS, f_height);
	fb_render_text("Resolution",
	    2, black, white, textbuf, f_width * CD_RES_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    res_startc, cfg_startr + (0 * f_height), 
	    textbuf, f_width * CD_RES_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_RES_COLS, f_height);
	sprintf(text, "(%c) 800x480", res == MTL_DM_800x480 ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_RES_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    res_startc, cfg_startr + (1 * f_height), 
	    textbuf, f_width * CD_RES_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_RES_COLS, f_height);
	sprintf(text, "(%c) 720x480", res == MTL_DM_720x480 ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_RES_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    res_startc, cfg_startr + (2 * f_height), 
	    textbuf, f_width * CD_RES_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_RES_COLS, f_height);
	sprintf(text, "(%c) 640x480 (Left)", res == MTL_DM_640x480 ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_RES_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    res_startc, cfg_startr + (3 * f_height), 
	    textbuf, f_width * CD_RES_COLS, f_height);

	fb_fill_buf(textbuf, white, f_width * CD_RES_COLS, f_height);
	sprintf(text, "(%c) 640x480 (Centered)",
	    res == MTL_DM_640x480_CENTER ? '*' : ' ');
	fb_render_text(text, 2, black, white, textbuf,
	    f_width * CD_RES_COLS, f_height);
	fb_composite(dbuf, d_width, d_height,
	    res_startc, cfg_startr + (4 * f_height), 
	    textbuf, f_width * CD_RES_COLS, f_height);

	fb_post_region(dbuf, d_x, d_y, d_width, d_height);
	/* XXX: should composite into a somewhat larger buffer */
	fb_rectangle(black, CD_BORDER_WIDTH,
	    d_x - CD_BORDER_WIDTH, d_y - CD_BORDER_WIDTH,
	    d_width + (CD_BORDER_WIDTH * 2), d_height + (CD_BORDER_WIDTH * 2));

	for(;;) {
		ts = ts_poll();
#if DEBUG
		printf("gesture 0x%x\n", ts->ts_gesture);
#endif
		switch (ts->ts_gesture) {
		case TSG2_ZOOM_OUT:
		case TSG_SOUTH:
			goto done;
		case TSG_CLICK:
			row = (ts->ts_y1 - (d_y + cfg_startr)) / f_height;
			if ((uint)ts->ts_x1 > d_x + sb_startc &&
			    (uint)ts->ts_x1 < d_x + sb_endc) {
				switch (row) {
				case 1:
					if (sb != SB_NONE) {
						changed = 1;
						sb = SB_NONE;
						goto repaint;
					}
					break;
				case 2:
					if (sb != SB_CAPSICUM) {
						changed = 1;
						sb = SB_CAPSICUM;
						goto repaint;
					}
					break;
				case 3:
					if (sb != SB_CHERI) {
						changed = 1;
						sb = SB_CHERI;
						goto repaint;
					}
					break;
				case 5:
					changed = 1;
					sb_vis = sb_vis ? 0 : 1;
					goto repaint;
					break;
				}
			} else if ((uint)ts->ts_x1 > d_y + res_startc &&
			    (uint)ts->ts_x1 < d_y + res_endc) {
				switch (row) {
				case 1:
					if (res != MTL_DM_800x480) {
						changed = 1;
						res = MTL_DM_800x480;
						goto repaint;
					}
					break;
				case 2:
					if (res != MTL_DM_720x480) {
						changed = 1;
						res = MTL_DM_720x480;
						goto repaint;
					}
					break;
				case 3:
					if (res != MTL_DM_640x480) {
						changed = 1;
						res = MTL_DM_640x480;
						goto repaint;
					}
					break;
				case 4:
					if (res != MTL_DM_640x480_CENTER) {
						changed = 1;
						res = MTL_DM_640x480_CENTER;
						goto repaint;
					}
					break;
				}
			}
		}
	}

done:
	fb_post(savebuf);
error:
	free(savebuf);
	free(dbuf);
	free(textbuf);
	free(text);

	return (changed);
}

static int
strpcmp(const void *v1, const void *v2)
{
	const char * const *sp1;
	const char * const *sp2;
	const char *s1, *s2;

	sp1 = v1;
	sp2 = v2;

	s1 = *sp1;
	s2 = *sp2;

	return (strcmp(s1, s2));
}

static int
render_slide(int dfd, int slidenum, const char *slide)
{
	int error, pfd;
	int f_width, f_height;
	int x, y, w, h;
	uint sv1, sv2;
	size_t olen;
	char sntext[8];
	uint32_t *snimage;
	uint32_t r;
	uint64_t decode, total;
	struct iboxstate *is;

	error = 0;
	decode = total = 0;

	busy(1);

	if ((pfd = openat(dfd, slide, O_RDONLY)) == -1) {
		warn("Failed to open %s", slide);
		return (-1);
	}
	if (sb == SB_CHERI) {
		olen = sizeof(sv1);
		sysctlbyname("security.cheri.syscall_violations",
		    &sv1, &olen, NULL, 0);
	}
	if ((is = png_read_start(pfd, fb_width, fb_height, sb)) == NULL) {
		warn("Failed to start PNG decode for %s", slide);
		return (-1);
	}
	if (png_read_finish(is) != 0) {
		warnx("png_read_finish() failed for %s", slide);
		return (-1);
	}
	decode += iboxstate_get_dtime(is);
	total += iboxstate_get_ttime(is);
	fb_fill_region(white, 0, 0, fb_width, fb_height);
	busy(0);
	/*
	 * If the image is the full display height, assume it's meant to be
	 * displayed as a simple slide without compositing.  Make a decent
	 * effort to display it in a nice place horizontaly.
	 */
	if (is->height == (u_int)fb_height) {
		y = 0;
		h = is->height;
	} else {
		h = is->height < slide_height ? is->height : slide_height;
		y = header_height;
	}
	if (is->width > slide_width) {
		if (is->width < fb_width - slide_fcol)
			x = slide_fcol;
		else
			x = 0;
	} else
		x = slide_fcol + ((slide_width - is->width) / 2);
	w = is->width;
	fb_post_region(__DEVOLATILE(uint32_t *, is->buffer), x, y, w, h);
	if (sb_vis && sb != SB_NONE)
		fb_rectangle(red, 2,
		    x, y, is->width,
		    is->height < (u_int)fb_height - y ?
		    (u_int)fb_height - y : is->height);
	switch (sb) {
	case SB_CAPSICUM:
		if (is->error == 99)
			error = 99;
		break;
	case SB_CHERI:
		olen = sizeof(sv2);
		sysctlbyname("security.cheri.syscall_violations",
		    &sv2, &olen, NULL, 0);
		if (sv1 != sv2)
			error = 99;
		break;
	default:
		break;
	}

	/* 
	 * If the image is full height, then left and right extend the
	 * edges and skip further compositing
	 */
	if (y == 0) {
		if (x > 0) {
			/* Left extend the image if needed */
			for (r = 0; r < is->height; r++)
				fb_fill_region(is->buffer[r * is->width],
				    0, r, x, 1);
		}
		if (x + is->width < (uint)fb_width) {
			/* Right extend the image if needed */
			for (r = 0; r < is->height; r++)
				fb_fill_region(is->buffer[((r + 1) *
				    is->width) - 1],
				    x + is->width, r,
				    fb_width - (x + is->width), 1);
		}
		iboxstate_free(is);
		unbusy();
		return (0);
	}

	iboxstate_free(is);

	busy(0);

	/* put an SRI logo in the lower left corner */
	if ((pfd = open("/usr/share/images/sri.png", O_RDONLY)) == -1) {
		warn("Failed to open sri.png");
		return (-1);
	}
	if ((is = png_read_start(pfd, slide_width, fb_height, sb)) == NULL) {
		warn("Failed to start PNG decode for sri.png");
		return (-1);
	}
	if (png_read_finish(is) != 0) {
		warnx("png_read_finish() failed for sri.png");
		return (-1);
	}
	decode += iboxstate_get_dtime(is);
	total += iboxstate_get_ttime(is);
	fb_post_region(__DEVOLATILE(uint32_t *, is->buffer),
	    slide_fcol, fb_height - is->height, is->width, is->height);
	if (sb_vis && sb != SB_NONE)
		fb_rectangle(red, 2, slide_fcol, fb_height - is->height,
		    is->width, is->height);
	iboxstate_free(is);

	/* put a cambridge logo in the lower right corner */
	if ((pfd = open("/usr/share/images/ucam.png", O_RDONLY)) == -1) {
		warn("Failed to open ucam.png");
		return (-1);
	}
	if ((is = png_read_start(pfd, slide_width, fb_height, sb)) == NULL) {
		warn("Failed to start PNG decode for ucam.png");
		return (-1);
	}
	if (png_read_finish(is) != 0) {
		warnx("png_read_finish() failed for ucam.png");
		return (-1);
	}
	decode += iboxstate_get_dtime(is);
	total += iboxstate_get_ttime(is);
	fb_post_region(__DEVOLATILE(uint32_t *, is->buffer),
	    slide_fcol + slide_width - is->width, fb_height - is->height,
	    is->width, is->height);
	if (sb_vis && sb != SB_NONE)
		fb_rectangle(red, 2, slide_fcol + slide_width - is->width,
		    fb_height - is->height, is->width, is->height);
	iboxstate_free(is);

	f_width = fb_get_font_width();
	f_height = fb_get_font_height();
	if ((uint)slidenum < (sizeof(sntext) - 1) * 10) {
		snprintf(sntext, sizeof(sntext), "%d", slidenum);
		if ((snimage = malloc(sizeof(uint32_t) * strlen(sntext) * 
		    f_width * f_height)) == NULL)
			warn("failed to malloc space for slide number");
		else
			fb_render_text(sntext, 1, black, white, snimage,
			    f_width * strlen(sntext), f_height);
			fb_post_region(snimage,
			    slide_fcol + (slide_width / 2) -
			    (f_width * strlen(sntext) / 2),
			    fb_height - f_height,
			    f_width * strlen(sntext), f_height);
			free(snimage);
	}

	/*
	 * Draw the header with image at the upper right.  Assume
	 * the background color is the same on each row and that the
	 * left most pixel of the image is that color.
	 */
	if ((pfd = open("/usr/share/images/header.png", O_RDONLY)) == -1) {
		warn("Failed to open header.png");
		return (-1);
	}
	if ((is = png_read_start(pfd, slide_width, fb_height, sb)) == NULL) {
		warn("Failed to start PNG decode for header.png");
		return (-1);
	}
	if (png_read_finish(is) != 0) {
		warnx("png_read_finish() failed for header.png");
		return (-1);
	}
	decode += iboxstate_get_dtime(is);
	total += iboxstate_get_ttime(is);
	/* Fill in the header's background. */
	for (r = 0; r < is->height; r++)
		fb_fill_region(is->buffer[r * is->width], 0, r,
		fb_width, 1);
	fb_post_region(__DEVOLATILE(uint32_t *, is->buffer),
	    slide_fcol + slide_width - is->width, 0, is->width, is->height);
	if (sb_vis && sb != SB_NONE)
		fb_rectangle(red, 2, slide_fcol + slide_width - is->width,
		    0, is->width, is->height);
	iboxstate_free(is);

	unbusy();

	if (verbose)
		printf("total: %ju  decode: %ju  overhead: %.1f%%\n", total,
		    decode, 100.0 * (((float)total - decode) / total));

	return (error);
}

static void
addslide(int *np, int *maxp, char ***arrayp, const char *name)
{

	if (*maxp == 0) {
		*maxp = 8;
		if ((*arrayp = malloc(sizeof(**arrayp) * (*maxp))) == NULL)
			err(1, "malloc slide array\n");
	}
	if (*np == *maxp) {
		if (*maxp == 0)
			*maxp = 512;
		else
			*maxp *= 2;
		if ((*arrayp = realloc(*arrayp,
		    sizeof(**arrayp) * (*maxp))) == NULL)
			err(1, "realloc slide array");
	}

	if (((*arrayp)[*np] = strdup(name)) == NULL)
		err(1, "strdup slide name");
	(*np)++;
}

static void
handle_sigchld(int sig __unused)
{
	
	zombies_waiting = 1;
}

static void
writeall(int fd, const char *buf, ssize_t len)
{
	ssize_t wlen = 0, n;
	
	while (wlen != len) {
		n = write(fd, buf + wlen, len - wlen);
		if (n < 0) {
			syslog(LOG_ALERT, "write failed: %s", strerror(errno));
			err(1, "write");
		}
		wlen += n;
	}
}

static void
fork_child(void)
{
	int pmaster, pslave, status;
	ssize_t rlen;
	pid_t pid;
	struct sigaction act;
	struct pollfd pfd[1];
	char buf[1024];
	u_int32_t *image;

restart:
	if (openpty(&pmaster, &pslave, NULL, NULL, NULL) == -1)
		err(1, "openpty");
	pid = fork();
	if (pid < 0)
		err(1, "fork()");
	else if (pid == 0) {
		close(pmaster);
		if (login_tty(pslave) < 0) {
			syslog(LOG_ALERT, "login_tty failed in child: %s",
			    strerror(errno));
			err(1, "tty_login");
		}
		/* return to begin normal processing */
		return;
	}

	memset (&act, 0, sizeof(act));
	act.sa_handler = handle_sigchld;

	if (sigaction(SIGCHLD, &act, 0))
		err(1, "sigacation");

	close(pslave);
	/*
	 * We poll for data from the child's pty.  Don't bother looking for
	 * tty input since the child couldn't do anything with it.
	 */
	pfd[0].fd = pmaster;
	pfd[0].events = POLLIN;
	for (;;) {
		if (poll(pfd, 2, INFTIM) < 0) {
			if (errno == EINTR)
				continue;
			syslog(LOG_ALERT, "poll failed with %s",
			    strerror(errno));
			err(1, "poll");
		}
		if (zombies_waiting) {
			wait4(pid, &status, 0, NULL);
			if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
				warnx("child exited with %d",
				    WEXITSTATUS(status));
				if (WEXITSTATUS(status) == 99) {
					warnx("child was exploited");
					image = malloc(sizeof(u_int32_t) *
					    fb_width * fb_height);
					if (image == NULL)
						err(1, "malloc");
					fb_save(image);
					fb_dialog(FBDT_PINCH2CLOSE, black,
					    white, black,
					    "CheriPoint Exited", 
"CheriPoint vulnerability exploited\n"
"\n"
"Pinch to close dialog and restart"
					    );
					fb_post(image);
					free(image);
				}
			} else if(WIFSIGNALED(status)) {
				warn("child killed by signal %d",
				    WTERMSIG(status));
			} else {
				exit(0);
			}
			zombies_waiting = 0;
			close(pmaster); /* XXX: should we drain it first? */
			fb_fill_region(vwhite(128), 0, 0, fb_width, fb_height);
			goto restart;
		}
		
		if (pfd[0].revents & POLLIN) {
			rlen = read(pmaster, buf, sizeof(buf));
			if (rlen < 0) {
				err(1, "read");
			} else if (rlen > 0)
				writeall(1, buf, rlen);
		}
	}
}

static void
init_header_height(void)
{
	int pfd;
	struct iboxstate *is;

	if ((pfd = open("/usr/share/images/header.png", O_RDONLY)) == -1)
		err(1, "Failed to open header.png");
	if ((is = png_read_start(pfd, slide_width, fb_height, sb)) == NULL)
		errx(1, "Failed to start PNG decode for header.png");
	if (png_read_finish(is) != 0)
		errx(1, "png_read_finish() failed for header.png");
	header_height = is->height;
	iboxstate_free(is);
}

int
main(int argc, char **argv)
{
	DIR *dirp;
	struct dirent *entry;
	char *coverpat;
	char **covers, **slides;
	uint32_t *save;
	int error, gesture;
	int ch, forkflag = 0;
	int cover, ncovers, maxcovers;
	int slide, nslides, maxslides;
	struct tsstate *ts, tshack = {0, 0, 0, 0, 0, 0,};

	while ((ch = getopt(argc, argv, "fv")) != -1) {
		switch (ch) {
		case 'f':
			forkflag = 1;
			break;
		case 'v':
			verbose++;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (verbose)
		ibox_verbose = verbose;

	if (argc != 1)
		usage();
	
	fb_init();
        ts_drain();
	busy_indicator();
	fb_fill_region(white, 0, 0, fb_width, fb_height);
        fb_fade2on(); 
        fb_load_syscons_font(NULL, "/usr/share/syscons/fonts/iso-8x16.fnt");
	init_busy();

	if (forkflag) {
		if ((slidep = mmap(NULL, sizeof(int), PROT_READ|PROT_WRITE,
		    MAP_ANON | MAP_SHARED, -1, 0)) == NULL)
			err(1, "failed to allocate slide pointer");
		*slidep = 0;
		fork_child();
	} else {
		if ((slidep = malloc(sizeof(int))) == NULL)
			err(1, "failed to allocate slide pointer");
		*slidep = 0;
	}
        busy_indicator();

	set_display_mode(res);
	init_header_height();

	maxcovers = ncovers = 0;
	maxslides = nslides = 0;
	if ((dirp = opendir(argv[0])) == NULL)
		err(1, "opendir(%s)", argv[0]);
	while ((entry = readdir(dirp)) != NULL) {
		/* XXX: doesn't support symlinks */
		if (entry->d_type != DT_REG)
			continue;
		/* Skip obvious non-PNGs */
		if (fnmatch("*.png", entry->d_name, FNM_CASEFOLD) != 0)
			continue;

		if (fnmatch("*-cover-*.png", entry->d_name, FNM_CASEFOLD) == 0)
			addslide(&ncovers, &maxcovers, &covers, entry->d_name);
		else
			addslide(&nslides, &maxslides, &slides, entry->d_name);
	}
	if (verbose)
		printf("read %d covers and %d slides\n", ncovers, nslides);
	qsort(slides, nslides, sizeof(*slides), &strpcmp);
	qsort(covers, ncovers, sizeof(*covers), &strpcmp);
	
	slide = *slidep;
	for (;;) {
		gesture = 0;
		/* If there isn't a cover, skip over it */
		if (slide == 0 && ncovers == 0)
			slide = 1;

		if (slide == 0) {
			asprintf(&coverpat, "*-cover-%d.png", slide_width);
			for (cover = 0; cover < ncovers; cover++)
				if (fnmatch(coverpat, covers[cover],
				    FNM_CASEFOLD) == 0)
					break;
			free(coverpat);
			if (cover == ncovers)
				cover = 0;	/* Smallest cover due to sort */
			render_slide(dirfd(dirp), 1, covers[cover]);
			*slidep = slide; /* Update post success */
		} else {
			error = render_slide(dirfd(dirp),
			    slide + (ncovers > 0 ? 1 : 0),
			    slides[slide - 1]);
			if (error == 0)
				*slidep = slide; /* Update post success */
			else if (error == 99) {
				save = malloc(sizeof(uint32_t) * fb_width *
				    fb_height);
				if (save != NULL)
					fb_save(save);
				switch (sb) {
				case SB_CAPSICUM:
					gesture = fb_dialog_gestures(
					    TSGF_ZOOM_OUT |
					    TSGF_EAST | TSGF_WEST,
					    black, white, black,
					    "Exploit Mitigated",
					    "Capsicum prevented an exploit\n"
					    "from gaining control!");
					break;
				case SB_CHERI:
					gesture = fb_dialog_gestures(
					    TSGF_ZOOM_OUT |
					    TSGF_EAST | TSGF_WEST,
					    black, white, black,
					    "Exploit Mitigated",
					    "CHERI prevented an exploit\n"
					    "from gaining control!");
					break;
				default:
					break;
				}
				if (gesture == TSGF_ZOOM_OUT)
					gesture = 0;
				if (save != NULL) {
					fb_post(save);
					free(save);
				}
			}
		}
		ts_drain();
nop:
		if (gesture != 0) {
			tshack.ts_gesture = tsgf2tsg(gesture);
			ts = &tshack;
			gesture = 0;
		} else
			ts = ts_poll();
#ifdef DEBUG
		printf("gesture 0x%x\n", ts->ts_gesture);
#endif
		switch (ts->ts_gesture) {
		case TSG2_ZOOM_OUT:
			exit(0);
		case TSG_NORTH:
			error = config_dialog();
			ts_drain();
			if (error == -1)
				err(1, "internal error in config dialog");
			else if (error == 0)
				goto nop;
			set_display_mode(res);
			break;
		case TSG_EAST:
			if (slide == 0 || (slide == 1 && ncovers == 0))
				slide = nslides;
			else
				slide--;
			break;
		case TSG_WEST:
			if (slide == nslides)
				slide = 0;
			else
				slide++;
			break;
		default:
			goto nop;
		}

	}
}
