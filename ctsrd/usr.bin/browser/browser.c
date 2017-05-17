/*-
 * Copyright (c) 2012 Simon W. Moore
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/capsicum.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <assert.h>
#include <ctype.h>
#include <terasic_mtl.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <inttypes.h>
#include <libutil.h>
#include <magic.h>
#include <poll.h>
#include <signal.h>
#define _WITH_DPRINTF
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#define	BASEIMG		"/usr/share/images/browser.png"
#define	ICONS		"/usr/share/images/icons.png"

#define	vwhite(v)	fb_colour((v), (v), (v))
#define	vred(v)		fb_colour((v), 0, 0)
#define	vgreen(v)	fb_colour(0, (v), 0)
#define	vblue(v)	fb_colour(0, 0, (v))
#define	vyellow(v)	fb_colour((v), (v), 0)
#define	vcyan(v)	fb_colour(0, (v), (v))
#define	vmagenta(v)	fb_colour((v), 0, (v))
#define	black		vwhite(0)
#define	white		vwhite(0xFF)
#define	red		vred(0xFF)
#define	green		vgreen(0xFF)
#define	blue		vblue(0xFF)
#define yellow		vyellow(0xFF)
#define cyan		vcyan(0xFF)
#define magenta		vmagenta(0xFF)

/*
 * Each file is displayed in a 266x40 box:
 * +--------------------------------------------------------------------------+
 * |  4 pixel border                                                          |
 * |4 +------+ 4+----------------------------------------------------------+  |
 * |p |32x32 | p| Text in 16x32 characters                                 |  |
 * |x |icon  | x|                                                          |  |
 * |  +------+  +----------------------------------------------------------+  |
 * |  4 pixel border                                                          |
 * +--------------------------------------------------------------------------+
 * |----------------------------------(800/3 = 266) pixels--------------------|
 */
#define	FROW		41
#define	NCOL		3
#define	NROW		10
#define	NSLOTS		(NCOL * NROW)
#define	CWIDTH		266
#define	RHEIGHT		40
#define	ICON_WH		32
#define	BORDER		4
#define	CHAR_HEIGHT	32 
#define	CHAR_WIDTH	16
#define	TEXT_OFFSET	(BORDER + ICON_WH + BORDER)
#define	_TEXTSPACE	(CWIDTH - (TEXT_OFFSET + BORDER))
#define	TEXTSPACE	(_TEXTSPACE - _TEXTSPACE % CHAR_WIDTH)

/*
 * The get_action() function polls for input and returns a slot number
 * (either a line on the console or a space on the screen) or one of
 * these actions.  Internally it handles changing protection modes.
 */
#define ACT_NEXT	100
#define ACT_PREV	101
#define	ACT_QUIT	102
#define	ACT_REFRESH	103

/* Beginning and ending colums of each sandbox type's name */
#define	SB_IMG_SPACING		20
#define	SB_IMG_NONE_BCOL	145
#define	SB_IMG_CAPSICUM_BCOL	222
#define	SB_IMG_CHERI_BCOL	350
#define	SB_IMG_NONE_ECOL	(SB_IMG_CAPSICUM_BCOL - SB_IMG_SPACING)
#define	SB_IMG_CAPSICUM_ECOL	(SB_IMG_CHERI_BCOL - SB_IMG_SPACING)
#define	SB_IMG_CHERI_ECOL	445
#define	SB_MINCOL		SB_IMG_NONE_BCOL
#define	SB_MAXCOL		SB_IMG_CHERI_ECOL
#define	SB_MINROW		(fb_height - 31)
#define	SB_MAXROW		(fb_height - 4)

#define SB_SHOW_MINCOL		604
#define SB_SHOW_MAXCOL		793

/* Start offsets for browser columns */
static const int	colstart[] = {0, 267, 534};

struct dent {
	struct dirent	 entry;
	char		*desc;
	u_int32_t	*icon;
};

/*
 * List of mappings between icons in the icons.png file and values from
 * the get_desc() function.  Processing is first match so most specific
 * entries should come first.
 */
static struct _iconmap {
	int		 i_offset;
	const char	*i_type;
} iconmap[] = {
	{ 0, "prev" },
	{ 1, "next" },
	{ 2, "special/character" },
	{ 2, "special/block" },
	{ 3, "unopenable" },
	{ 4, "important" },
	{ 5, "devil" },
	{ 6, "application/x-executable" },
	{ 6, "application/x-sharedlib" },
	{ 9, "text/html" },
	{ 11, "text/x-shellscript" },
	{ 13, "badmagic" },
	{ 14, "directory" },
	{ 15, "application/x-dbm" },
	{ 19, "x-application/cheripoint" },

	{ 7, "audio/*" },
	{ 8, "image/*" },
	{ 10, "text/*" },
	{ 12, "video/*" },

	{ 16, "*" },
	{ 0, NULL }
};

static enum _sbtype {
	SB_NONE = 1,
	SB_CAPSICUM,
	SB_CHERI
} sbtype = SB_NONE;

static struct _sbdata {
	enum _sbtype	sbtype;
	int		enabled;
	int		bcol;
	int		ecol;
} sbdata[] =  {
	{ SB_NONE, 1, SB_IMG_NONE_BCOL, SB_IMG_NONE_ECOL },
	{ SB_CAPSICUM, 1, SB_IMG_CAPSICUM_BCOL, SB_IMG_CAPSICUM_ECOL },
	{ SB_CHERI, 0, SB_IMG_CHERI_BCOL, SB_IMG_CHERI_ECOL },
	{ 0, 0, 0, 0 }
};

static int sbshow = 0;

/*
 * baseimage is the black and white background image with any disabled
 * protection modes grayed out.  After init_bgimage() runs it will not
 * be modified.  bgimage starts equal to baseimage and is updated as
 * the sandbox mode changes and showing sandboxes is toggled.
 */
static u_int32_t	*baseimage;
static u_int32_t	*bgimage;
static u_int32_t	*icons;
static magic_t		 magic;
static int		 zombies_waiting = 0;
static int		 verbose = 0;

static void
usage(void)
{
	
	printf("usage:	browser [-fv] <dir> <tty>\n");
	printf("	browser [-fv] -T <dir>\n");
	printf("\n");
	printf("	-f	Fork and monitor a child instance\n");
	printf("	-T	Don't open a tty\n");
	printf("	-v	Verbose mode\n");
	exit(1);
}

static void
init_tty(char *tty_name) {
	int	 tty;
	char	*devpath;

	if (tty_name[0] != '/')
		asprintf(&devpath, "/dev/%s", tty_name);
	else
		devpath = tty_name;
	if ((tty = open(devpath, O_RDWR)) < 0) {
		syslog(LOG_ALERT, "open failed with %s", strerror(errno));
		err(1, "open(%s)", devpath);
	}
	if (login_tty(tty) < 0) {
		syslog(LOG_ALERT, "login_tty failed: %s", strerror(errno));
		err(1, "login_tty()");
	} 
	if (devpath != tty_name)
		free(devpath);
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
					    "Browser Exited", 
"Browser vulnerability exploited\n"
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
init_magic(void) {
	magic = magic_open(MAGIC_MIME_TYPE);
	if (magic == NULL)
		errx(1, "magic_open()");
	if (magic_load(magic, NULL) == -1) {
		warnx("magic_load() %s", magic_error(magic));
		magic_close(magic);
		exit(1);
	}
}

static const char *
get_magic(int fd)
{
	pid_t pid;
	ssize_t rlen;
	char buf[4096], *desc;
	const char *cdesc;
	int pfd[2], status;

	rlen = read(fd, buf, sizeof(buf));
	if (rlen == -1)
		return "read-error";
	if (rlen == 0)
		return "empty";

	switch (sbtype) {
	case SB_NONE:
		return magic_buffer(magic, buf, rlen);
	case SB_CAPSICUM:
		if (pipe(pfd) == -1)
			err(1, "pipe()");
		pid = fork();
		if (pid < 0)
			err(1, "fork()");
		else if (pid == 0) {
			close(fd);
			close(pfd[0]);
			fb_fini();
			/* XXX: do more cleanup here */
			cap_enter();

			cdesc = magic_buffer(magic, buf, rlen);
			if (cdesc == NULL)
				dprintf(pfd[1], "badmagic");
			else
				dprintf(pfd[1], "%s", cdesc);
			close(pfd[1]);
			exit(0);
		} else {
			close(pfd[1]);
			while (wait4(pid, &status, 0, NULL) == -1)
				if (errno != EINTR)
					err(1, "wait4()");
			if (WIFEXITED(status) &&
			    WEXITSTATUS(status) != 0) {
				warnx("child exited with %d",
				    WEXITSTATUS(status));
				close(pfd[0]);
				return "badmagic";
			}
			else if(WIFSIGNALED(status)) {
				warn("child killed by signal %d",
				    WTERMSIG(status));
				close(pfd[0]);
				return "badmagic";
			} else {
				rlen = read(pfd[0], buf, 128);
				close(pfd[0]);
				if (rlen == -1)
					return "read error";
				if (rlen == 0 || rlen == 1)
					return "unknown";
				/* Don't trust the result */
				desc = buf + rlen;
				strvisx(desc, buf, rlen, 0);
				return (desc);
			}
		}
		break;
	case SB_CHERI:
		return "devil";
	default:
		errx(1, "invalid sandbox type");
	}
}

static void
init_bgimage(void)
{
	int i, j, pixel, sb;

	bgimage = malloc(sizeof(u_int32_t) * fb_height * fb_width);
	if (bgimage == NULL)
		err(1, "malloc");
	baseimage = malloc(sizeof(u_int32_t) * fb_height * fb_width);
	if (baseimage == NULL)
		err(1, "malloc");
	read_png_file(BASEIMG, baseimage, fb_width, fb_height);

	for (sb = 0; sbdata[sb].sbtype != 0; sb++) {
		if (sbdata[sb].enabled)
			continue;
		for (j = SB_MINROW; j < SB_MAXROW; j++) {
			for (i = sbdata[sb].bcol; i <= sbdata[sb].ecol; i++) {
				pixel = (j * fb_width) + i;
				if (baseimage[pixel] == white)
					continue;
				baseimage[pixel] = vwhite(
				    (((baseimage[pixel] >> 24) & 0xFF) / 2) +
				    128);
			}
		}
	}

	memcpy(bgimage, baseimage, sizeof(u_int32_t) * fb_height * fb_width);
}

static void
update_sandbox(enum _sbtype type)
{
	int bcol, ecol, i, j, pixel, value;

	sbtype = type;

	for (i = 0; sbdata[i].sbtype != 0 && sbdata[i].sbtype != type; i++)
		/* do nothing */;
	if (sbdata[i].sbtype == 0)
		errx(1, "update_sandbox() Invalid type %d\n", type);
	bcol = sbdata[i].bcol;
	ecol = sbdata[i].ecol;

	for (j = SB_MINROW; j < SB_MAXROW; j++) {
		for (i = SB_MINCOL; i <= SB_MAXCOL; i++) {
			pixel = (j * fb_width) + i;
			if (baseimage[pixel] != black) {
				value = (baseimage[pixel] >> 24) & 0xFF;
				bgimage[pixel] = (i >= bcol && i <= ecol) ?
				    vblue(value) : vwhite(value);
			}
		}
		for (i = SB_SHOW_MINCOL; i <= SB_SHOW_MAXCOL; i++) {
			pixel = (j * fb_width) + i;
			if (bgimage[pixel] != black) {
				value = (baseimage[pixel] >> 16) & 0xFF;
				bgimage[pixel] = (sbtype == SB_NONE) ?
				    vwhite(value / 2) : (sbshow ?
				    vred(value) : vwhite(value));
			}
		}
	}
	fb_post(bgimage);
}

static void
toggle_sbshow(void)
{

	sbshow = (sbshow + 1) % 2;
	update_sandbox(sbtype);
}

static const char *
get_desc(int dfd, struct dirent *entry)
{
	int fd, type;
	const char *desc;
	struct stat sbuf;

	if (entry->d_type == DT_LNK) {
		if (fstatat(dfd, entry->d_name, &sbuf, 0) == -1)
			type = DT_UNKNOWN;
		else
			type = IFTODT(sbuf.st_mode);
	} else
		type = entry->d_type;

	switch (type) {
	case DT_UNKNOWN:
		desc = "unknown";
		break;
	case DT_REG:
		if ((fd = openat(dfd, entry->d_name, O_RDONLY)) == -1)
			desc = "unopenable";
		else {
			desc = get_magic(fd);
			close(fd);
		}
		break;
	case DT_FIFO:
		desc = "special/fifo";
		break;
	case DT_CHR:
		desc = "special/character";
		break;
	case DT_DIR:
		if (fnmatch("*.[cC][pP][tT]", entry->d_name, 0) == 0)
			desc = "x-application/cheripoint";
		else
			desc = "directory";
		break;
	case DT_BLK:
		desc = "special/block";
		break;
	case DT_SOCK:
		desc = "special/socket";
		break;
	case DT_WHT:
		desc = "special/whiteout";
		break;
	default:
		err(1, "Unhandled type %d", type);
	}

	return (desc);
}

static u_int32_t *
get_icon(const char *desc)
{
	struct _iconmap *icon;

	for (icon = iconmap; icon->i_type != NULL; icon++)
		if (fnmatch(icon->i_type, desc, 0) != FNM_NOMATCH)
			return (icons + (ICON_WH * ICON_WH * icon->i_offset));

	return (NULL);
}

static void
update_slot(int s, u_int32_t *icon, const char *text)
{
	u_int32_t textbuf[TEXTSPACE*CHAR_HEIGHT];

	fb_fill_buf(textbuf, white, TEXTSPACE, CHAR_HEIGHT);
	fb_render_text(text, 2, black, white, textbuf,
	    TEXTSPACE, CHAR_HEIGHT);
	fb_post_region(textbuf, colstart[(s/NROW)] + TEXT_OFFSET,
	    FROW + (RHEIGHT * (s % NROW)) + BORDER, TEXTSPACE,
	    CHAR_HEIGHT);
	if (sbshow && sbtype != SB_NONE)
		fb_fill_region(red, colstart[(s/NROW)] + BORDER - 2,
		    FROW + (RHEIGHT * (s % NROW)) + BORDER - 2,
		    ICON_WH + 4, ICON_WH + 4);
	fb_post_region(icon, colstart[(s/NROW)] + BORDER,
	    FROW + (RHEIGHT * (s % NROW)) + BORDER, ICON_WH, ICON_WH);
}

static int
get_action(void)
{
	struct timespec stime;
	struct tsstate *ts;
	int col, i, row;

	if (verbose)
		printf("entering get_action\n");

	/* Ignore all input for a quarter second to let the display settle. */
	stime.tv_sec = 0;
	stime.tv_nsec =  250 * 1000 * 1000;
	nanosleep(&stime, NULL);
	ts_drain();

	for (;;) {
		ts = ts_poll(0);
		if (verbose)
			printf("gesture = %x\n", ts->ts_gesture);
		if (ts->ts_gesture == TSG_CLICK) {
			if (ts->ts_y1 < FROW) {
				if (ts->ts_x1 > fb_width - 40)
					return (ACT_QUIT);
			} else if (ts->ts_y1 <= FROW + (NROW * RHEIGHT)) {
				row = (ts->ts_y1 - FROW) / RHEIGHT;
				for (col = NCOL - 1;
				    col > 0 && ts->ts_x1 < colstart[col]; col--)
					/* do nothing */;
				if (verbose)
					printf("row = %d, col = %d\n",
					    row, col);
				return (col * NROW + row);
			} else {
				if (ts->ts_x1 >= SB_MINCOL &&
				    ts->ts_x1 <= SB_MAXCOL) {
					for (i = 0;
					    ts->ts_x1 < sbdata[i].bcol ||
					    ts->ts_x1 > sbdata[i].ecol; i++)
						/* do nothing */;
					assert(sbdata[i].sbtype != 0);
					if (sbdata[i].sbtype == sbtype ||
						!sbdata[i].enabled)
					    continue;
					update_sandbox(sbdata[i].sbtype);
					return (ACT_REFRESH);
				}
				if (ts->ts_x1 >= SB_SHOW_MINCOL &&
				    ts->ts_x1 <= SB_SHOW_MAXCOL) {
					toggle_sbshow();
					return (ACT_REFRESH);
				}
			}
		} else if(ts->ts_gesture == TSG2_ZOOM_OUT) {
			if (ts->ts_count != 0)
				ts_drain();
			return (ACT_QUIT);
		} else if (ts->ts_gesture == TSG_EAST) {
			if (ts->ts_count != 0)
				ts_drain();
			return (ACT_PREV);
		} else if (ts->ts_gesture == TSG_WEST) {
			if (ts->ts_count != 0)
				ts_drain();
			return (ACT_NEXT);
		}
	}
	/* NOTREACHED */
	return (ACT_QUIT);
}

static int
show_png(int dfd, const char *name)
{
	int fd, ret = 0;
	u_int32_t *image, *previmage;
	struct tsstate *ts;

	image = malloc(sizeof(u_int32_t) * fb_width * fb_height);
	previmage = malloc(sizeof(u_int32_t) * fb_width * fb_height);
	if (image == NULL || previmage == NULL)
		err(1, "malloc");
	fb_save(previmage);
	busy_indicator();
	if ((fd = openat(dfd, name, O_RDONLY)) == -1) {
		ret = -1;
		goto end;
	}
	busy_indicator();
	if (read_png_fd(fd, image, fb_width, fb_height) != 0) {
		ret = -1;
		goto end;
	}
	/* read_png_fd() closes the descriptor */
	fb_post(image);
	for (;;) {
		ts = ts_poll(0);
		if(ts->ts_gesture == TSG2_ZOOM_OUT) {
			if (ts->ts_count != 0)
				ts_drain();
			break;
		}
	}
end:
	fb_post(previmage);
	free(previmage);
	free(image);

	return (0);
}

static int
show_text_file(int dfd, const char *name)
{
	int fd, i, nlines, topline;
	size_t fbuflen;
	ssize_t len;
	fb_dialog_action da;
	char dbuf[14 * 50], *dbufp, *fbuf, *lines[1024];
	const size_t maxbuflen = 32 * 1024 - 1;
	u_int32_t *image;

	if ((fd = openat(dfd, name, O_RDONLY)) < 0)
		return (-1);

	fbuf = malloc(maxbuflen + 1);
	if (fbuf == NULL)
		err(1, "malloc");
	for (fbuflen = 0; fbuflen < maxbuflen; fbuflen += len) {
		len = read(fd, fbuf + fbuflen,
		    (fbuflen - maxbuflen < 4096) ? fbuflen - maxbuflen : 4096);
		if (len < 0) {
			warn("read");
			if (fbuflen > 0)
				break; /* Use what we have if anything */
			free(fbuf);
			return (-1);
		}
		if (len == 0)
			break;
	}
	/* NUL terminate the buffer, replacing the last \n if there */
	if (fbuf[fbuflen - 1] == '\n')
		fbuf[fbuflen - 1] = '\0';
	else
		fbuf[fbuflen] = '\0';

	nlines = 0;
	while (nlines <= 1024 &&
	    (lines[nlines] = strsep(&fbuf, "\n")) != NULL) {
		if (strlen(lines[nlines]) > 50)
			lines[nlines][50] = '\0';
		nlines++;
	}
	if (nlines == 0) {
		free(fbuf);
		return (-1);
	}

	image = malloc(sizeof(u_int32_t) * fb_width * fb_height);
	if (image == NULL)
		err(1, "malloc");
	fb_save(image);

	topline = 0;
	for (;;) {
		dbufp = dbuf;
		for (i = topline; i < topline + 13; i++) {
			len = sprintf(dbufp, "%-49s%s",
			    (i < nlines) ? lines[i] : "",
			    (i < topline + 13 - 1) ? "\n" : "");
			dbufp += len;
		}

		da = fb_dialog(FBDT_PINCH_OR_VSCROLL, black, white, black,
		    name, dbuf);
		switch (da) {
		case FBDA_OK:
			for (i = 0; i < nlines; i++)
				free(lines[nlines]);
			free(fbuf);
			fb_post(image);
			free(image);
			return (0);
		case FBDA_UP:
			if (topline > 0)
				topline -= 13;
			break;
		case FBDA_DOWN:
			if (topline + 14 < nlines)
				topline += 13;
			break;
		default:
			err(1, "unhandled action");
		}
	}
}

static int
invoke_cheripoint(int dfd, const char *name)
{
	static int pmaster;
	int pslave, n, status;
	char buf[1024];  
	ssize_t rlen;
	pid_t child_pid;
	struct pollfd pfd[1];
  
	if (openpty(&pmaster, &pslave, NULL, NULL, NULL) == -1)
		err(1, "openpty");
	child_pid = fork();
	if (child_pid < 0)
		err(1, "fork()");
	else if (child_pid > 0)
		close(pslave);
	else {
		close(pmaster);
		if (fchdir(dfd) == -1) {
			syslog(LOG_ALERT, "fchdir failed in child: %s",
			   strerror(errno));
			err(1, "fchdir");
		}
		if (login_tty(pslave) < 0) {
			syslog(LOG_ALERT, "login_tty failed in child: %s",
			   strerror(errno));
			err(1, "tty_login");
		}
		execl("/usr/bin/cheripoint", "cheripoint", "-f", name,
		    NULL);
		syslog(LOG_ALERT, "exec of /usr/bin/browser failed: %s",
		strerror(errno));
		err(1, "execl()");
	}
 
	for(;;) {
		/*
		* If the child has exited, reset the state and return to the
		* main screen.
		*/
		if (zombies_waiting) {
			wait4(child_pid, &status, 0, NULL);
			/* XXX: ideally we'd check the status */
			close(pmaster);
			zombies_waiting = 0;
			break;
		}
 
		/* Check for output from the child and post it if needed */
		pfd[0].fd = pmaster;
		pfd[0].events = POLLIN;
		n = poll(pfd, 1, INFTIM);
		if (n == 0)
			continue;
		else if (n < 0) {
			if (errno == EINTR)
				continue;
			err(1, "poll");
		}
		if (n < 0) {
			syslog(LOG_ALERT, "poll failed with %s",
			   strerror(errno));
			err(1, "poll");
		}
		if (pfd[0].revents & POLLIN) {
			rlen = read(pfd[0].fd, buf, sizeof(buf));
			if (rlen < 0) {
				syslog(LOG_ALERT, "read failed: %s",
				    strerror(errno));
				err(1, "read");
			} else if (rlen > 0)
				writeall(0, buf, rlen);
		}
	}
	return (0);
}

static int
dentcmp(const void *v1, const void *v2)
{
        const struct dent *d1, *d2;

        d1 = v1;
        d2 = v2;
 
        /* Sort .. first */
        if (strcmp(d1->entry.d_name, "..") == 0)
                return (-1);
        if (strcmp(d2->entry.d_name, "..") == 0)
                return (1);
        return strcmp(d1->entry.d_name, d2->entry.d_name);
}

static int
browsedir(int dfd)
{
	int action, topslot, j, curslot, maxdents, nfd, ndents, retfd;
	DIR *dirp;
	struct dirent *entry;
	struct dent *dents, *dent;

	if ((dirp = fdopendir(dfd)) == NULL)
		err(1, "fdopendir()");

	ndents = 0;
	maxdents = 1024;
	dents = malloc(sizeof(struct dent) * maxdents);
	if (dents == NULL)
		err(1, "malloc dents");

	while ((entry = readdir(dirp)) != NULL) {
		if (ndents == maxdents) {
			maxdents *= 2;
			dents = realloc(dents, sizeof(struct dent) * maxdents);
			if (dents == NULL)
				err(1, "realloc dents");
		}
		if (strcmp(".", entry->d_name) == 0)
			continue;
		memcpy(&(dents[ndents].entry), entry, sizeof(*entry));
		dents[ndents].desc = NULL;
		dents[ndents].icon = NULL;
		ndents++;
	}
	qsort(dents, ndents, sizeof(struct dent), &dentcmp);
	
	topslot = 0;
render:
	fb_fill_region(white, colstart[0], FROW, fb_width, NROW * RHEIGHT);
	for(curslot = 0; curslot < NSLOTS && topslot + curslot < ndents;
	    curslot++) {
		dent = &dents[topslot + curslot];
		if (dent->desc == NULL)
			dent->desc = strdup(get_desc(dfd, &(dent->entry)));
		if (dent->icon == NULL)
			dent->icon = get_icon(dent->desc);

		if (verbose)
			printf("%2d %20s %s\n", curslot, dent->entry.d_name,
			    dent->desc);
		update_slot(curslot, dent->icon, dent->entry.d_name);
	}
	if (curslot == NSLOTS)
		curslot--;

	for (;;) {
		action = get_action();
		if (verbose)
			printf("action %d\n", action);
		switch (action) {
		case ACT_NEXT:
			if (topslot + curslot < ndents) {
				topslot += NSLOTS;
				goto render;
			}
			continue;
		case ACT_PREV:
			if (topslot != 0) {
				topslot -= NSLOTS;
				goto render;
			}
			continue;
		case ACT_QUIT:
			retfd = -1;
			goto cleanup;
		case ACT_REFRESH:
			/* Reset descriptions and icons */ 
			for (j = 0; j < ndents; j++) {
				free(dents[j].desc);
				dents[j].desc = NULL;
				dents[j].icon = NULL;
			}
			goto render;
		default:
			if (action < 0 || action >= NSLOTS - 1)
				errx(1, "invalid action");
			if (topslot + action >= ndents)
				continue;
			if (strcmp("image/png",
			    dents[topslot + action].desc) == 0) {
				show_png(dfd,
				    dents[topslot + action].entry.d_name);
				goto render;
			} else if (strcmp("text/plain",
			    dents[topslot + action].desc) == 0) {
				show_text_file(dfd,
				    dents[topslot + action].entry.d_name);
				goto render;
			} else if (strcmp("x-application/cheripoint",
			    dents[topslot + action].desc) == 0) {
				invoke_cheripoint(dfd, dents[topslot +
				    action].entry.d_name);
				fb_post(bgimage); /* Restore background */
				goto render;
			} else if (dents[topslot + action].entry.d_type ==
			    DT_DIR) {
				if ((nfd = openat(dfd,
				    dents[topslot + action].entry.d_name,
				    O_RDONLY|O_DIRECTORY)) == -1)
					goto render; /* XXX: display error */
				retfd = nfd;
				goto cleanup;
			} else {
				if (verbose)
					printf("opening non-directory not "
					    "supported\n");
				goto render;
			}
		}
	}

cleanup:
	for (j = 0; j < ndents; j++)
		free(dents[j].desc);
	free(dents);

	if (closedir(dirp) == -1)
		err(1, "closedir()");

	return (retfd);
}

int
main(int argc, char *argv[])
{
	int ch, dfd;
	int ttyflag = 1, forkflag = 0;
	struct sigaction act;

	while ((ch = getopt(argc, argv, "fTv")) != -1) {
		switch (ch) {
		case 'f':
			forkflag = 1;
			break;
		case 'T':
			ttyflag = 0;
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

	if (argc <= 0 || argc > 2)
		usage();

	if (argc == 2) {
		if (ttyflag)
			init_tty(argv[1]);
		else
			usage();
	}

	fb_init();
	ts_drain();
	busy_indicator();
	fb_fade2on();
	fb_load_syscons_font(NULL, "/usr/share/syscons/fonts/iso-8x16.fnt");
	busy_indicator();

	memset (&act, 0, sizeof(act));
	act.sa_handler = handle_sigchld;

	if (sigaction(SIGCHLD, &act, 0))
		err(1, "sigacation");

	if (forkflag)
		fork_child();
	busy_indicator();

	init_magic();
	busy_indicator();
	init_bgimage();
	busy_indicator();

	icons = malloc(sizeof(u_int32_t) * ICON_WH * 640);
	if (icons == NULL)
		err(1, "malloc");
	read_png_file(ICONS, icons, 32, 640);
	busy_indicator();

	fb_post(bgimage);
	//fb_fade2text(127);
	fb_text_cursor(255, 255);

	update_sandbox(SB_NONE);

        if ((dfd = open(argv[0], O_RDONLY|O_DIRECTORY)) == -1)
                err(1, "open(%s)", argv[1]);

        while (dfd != -1)
                dfd = browsedir(dfd);

	ts_drain();
	fb_fini();
	return (0);
}
