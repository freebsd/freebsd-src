/*
 * Compact Disc Control Utility by Serge V. Vakulenko, <vak@cronyx.ru>.
 * Based on the non-X based CD player by Jean-Marc Zucconi and
 * Andrew A. Chernov.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/file.h>
#include <sys/cdio.h>
#include <sys/ioctl.h>

#define VERSION "1.0"

/*
 * Audio Status Codes
 */
#define ASTS_INVALID    0x00    /* Audio status byte not valid */
#define ASTS_PLAYING    0x11    /* Audio play operation in progress */
#define ASTS_PAUSED     0x12    /* Audio play operation paused */
#define ASTS_COMPLETED  0x13    /* Audio play operation successfully completed */
#define ASTS_ERROR      0x14    /* Audio play operation stopped due to error */
#define ASTS_VOID       0x15    /* No current audio status to return */

struct cmdtab {
	int command;
	char *name;
	char *args;
} cmdtab[] = {
#define CMD_DEBUG       1
	{ CMD_DEBUG,    "Debug",        "[ on | off | reset ]", },
#define CMD_EJECT       2
	{ CMD_EJECT,    "Eject",        "", },
#define CMD_HELP        3
	{ CMD_HELP,     "?",            0, },
	{ CMD_HELP,     "Help",         "", },
#define CMD_INFO        4
	{ CMD_INFO,     "Info",         "", },
#define CMD_PAUSE       5
	{ CMD_PAUSE,    "PAuse",        "", },
#define CMD_PLAY        6
	{ CMD_PLAY,     "P",            0, },
	{ CMD_PLAY,     "Play",         "min1:sec1.fr1 [ min2:sec2.fr2 ]", },
	{ CMD_PLAY,     "Play",         "track1.index1 [ track2.index2 ]", },
	{ CMD_PLAY,     "Play",         "#block [ len ]", },
#define CMD_QUIT        7
	{ CMD_QUIT,     "Quit",         "", },
#define CMD_RESUME      8
	{ CMD_RESUME,   "Resume",       "", },
#define CMD_STOP        9
	{ CMD_STOP,     "Stop",         "", },
#define CMD_VOLUME      10
	{ CMD_VOLUME,   "Volume",       "<l> <r> | left | right | mute | mono | stereo", },
	{ 0,            0, },
};

struct cd_toc_entry toc_buffer[100];

char *cdname;
int fd = -1;
int verbose = 1;

extern char *optarg;
extern int optind;

int setvol (int, int);
int read_toc_entrys (int);
int play_msf (int, int, int, int, int, int);
int play_track (int, int, int, int);
int get_vol (int *, int *);
int status (int *, int *, int *, int *);
int open_cd (void);
int play (char *arg);
int info (char *arg);
char *input (int*);
void prtrack (struct cd_toc_entry *e, int lastflag);
void lba2msf (int lba, u_char *m, u_char *s, u_char *f);
int msf2lba (u_char m, u_char s, u_char f);
int play_blocks (int blk, int len);
int run (int cmd, char *arg);
char *parse (char *buf, int *cmd);

extern int errno;

void help ()
{
	struct cmdtab *c;

	for (c=cmdtab; c->name; ++c) {
		if (! c->args)
			continue;
		printf ("\t%s", c->name);
		if (*c->args)
			printf (" %s", c->args);
		printf ("\n");
	}
}

void usage ()
{
	printf ("Usage:\n\tcdcontrol [ -vs ] [ -f disc ] [ command args... ]\n");
	printf ("Options:\n");
	printf ("\t-v       - verbose mode\n");
	printf ("\t-s       - silent mode\n");
	printf ("\t-f disc  - device name such as /dev/cd0c\n");
	printf ("\tDISC     - shell variable with device name\n");
	printf ("Commands:\n");
	help ();
	exit (1);
}

int main (int argc, char **argv)
{
	int cmd;
	char *arg;

	cdname = getenv ("DISC");
	if (! cdname)
		cdname = getenv ("CDPLAY");

	for (;;) {
		switch (getopt (argc, argv, "svhf:")) {
		case EOF:
			break;
		case 's':
			verbose = 0;
			continue;
		case 'v':
			verbose = 2;
			continue;
		case 'f':
			cdname = optarg;
			continue;
		case 'h':
		default:
			usage ();
		}
		break;
	}
	argc -= optind;
	argv += optind;

	if (argc > 0 && strcasecmp (*argv, "help") == 0)
		usage ();

	if (! cdname) {
		fprintf (stderr, "No CD device name specified.\n");
		usage ();
	}

	if (argc > 0) {
		char buf[80], *p;
		int len;

		for (p=buf; argc-- > 0; ++argv) {
			len = strlen (*argv);
			if (p + len >= buf + sizeof (buf) - 1)
				usage ();
			if (p > buf)
				*p++ = ' ';
			strcpy (p, *argv);
			p += len;
		}
		*p = 0;
		arg = parse (buf, &cmd);
		return run (cmd, arg);
	}

	if (verbose == 1)
		verbose = isatty (0);
	if (verbose) {
		printf ("Compact Disc Control Utility, Version %s\n", VERSION);
		printf ("Type `?' for command list\n\n");
	}

	for (;;) {
		arg = input (&cmd);
		if (run (cmd, arg) < 0) {
			if (verbose)
				perror ("cdplay");
			close (fd);
			fd = -1;
		}
		fflush (stdout);
	}
}

int run (int cmd, char *arg)
{
	int l, r, rc;

	switch (cmd) {
	case CMD_QUIT:
		exit (0);

	default:
	case CMD_HELP:
		help ();
		return (0);

	case CMD_INFO:
		if (fd<0 && ! open_cd ()) return (0);
		return info (arg);

	case CMD_PAUSE:
		if (fd<0 && ! open_cd ()) return (0);
		return ioctl (fd, CDIOCPAUSE);

	case CMD_RESUME:
		if (fd<0 && ! open_cd ()) return (0);
		return ioctl (fd, CDIOCRESUME);

	case CMD_STOP:
		if (fd<0 && ! open_cd ()) return (0);
		return ioctl (fd, CDIOCSTOP);

	case CMD_DEBUG:
		if (fd<0 && ! open_cd ()) return (0);
		if (strcasecmp (arg, "on") == 0)
			return ioctl (fd, CDIOCSETDEBUG);
		if (strcasecmp (arg, "off") == 0)
			return ioctl (fd, CDIOCCLRDEBUG);
		if (strcasecmp (arg, "reset") == 0)
			return ioctl (fd, CDIOCRESET);
		printf ("Invalid command arguments\n");
		return (0);

	case CMD_EJECT:
		if (fd<0 && ! open_cd ()) return (0);
		(void) ioctl (fd, CDIOCALLOW);
		rc = ioctl (fd, CDIOCEJECT);
		if (rc < 0)
			return (rc);
		close (fd);
		fd = -1;
		return (0);

	case CMD_PLAY:
		if (fd<0 && ! open_cd ()) return (0);
		return play (arg);

	case CMD_VOLUME:
		if (fd<0 && ! open_cd ()) return (0);

		if (strcasecmp (arg, "left") == 0)
			return ioctl (fd, CDIOCSETLEFT);
		else if (strcasecmp (arg, "right") == 0)
			return ioctl (fd, CDIOCSETRIGHT);
		else if (strcasecmp (arg, "mute") == 0)
			return ioctl (fd, CDIOCSETMUTE);
		else if (strcasecmp (arg, "mono") == 0)
			return ioctl (fd, CDIOCSETMONO);
		else if (strcasecmp (arg, "stereo") == 0)
			return ioctl (fd, CDIOCSETSTERIO);

		if (2 != sscanf (arg, "%d %d", &l, &r)) {
			printf ("Invalid command arguments\n");
			return (0);
		}
		return setvol (l, r);
	}
}

int play (char *arg)
{
	struct ioc_toc_header h;
	int rc, n, start, end = 0, istart = 1, iend = 1;

	rc = ioctl (fd, CDIOREADTOCHEADER, &h);
	if (rc < 0)
		return (rc);

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys ((n + 1) * sizeof (struct cd_toc_entry));
	if (rc < 0)
		return (rc);

	if (! *arg)
		/*
		 * Play the whole disc
		 */
		return play_blocks (0, msf2lba (toc_buffer[n].addr.msf.minute,
			toc_buffer[n].addr.msf.second,
			toc_buffer[n].addr.msf.frame));

	if (strchr (arg, '#')) {
		/*
		 * Play block #blk [ len ]
		 */
		int blk, len = 0;

		if (2 != sscanf (arg, "#%d%d", &blk, &len) &&
		    1 != sscanf (arg, "#%d", &blk)) {
err:                    printf ("Invalid command arguments\n");
			return (0);
		}
		if (len == 0)
			len = msf2lba (toc_buffer[n].addr.msf.minute,
				toc_buffer[n].addr.msf.second,
				toc_buffer[n].addr.msf.frame) - blk;
		return play_blocks (blk, len);
	}

	if (strchr (arg, ':')) {
		/*
		 * Play MSF m1:s1 [ .f1 ] [ m2:s2 [ .f2 ] ]
		 */
		int m1, m2 = 0, s1, s2 = 0, f1 = 0, f2 = 0;

		if (6 != sscanf (arg, "%d:%d.%d%d:%d.%d", &m1, &s1, &f1, &m2, &s2, &f2) &&
		    5 != sscanf (arg, "%d:%d.%d%d:%d", &m1, &s1, &f1, &m2, &s2) &&
		    5 != sscanf (arg, "%d:%d%d:%d.%d", &m1, &s1, &m2, &s2, &f2) &&
		    3 != sscanf (arg, "%d:%d.%d", &m1, &s1, &f1) &&
		    4 != sscanf (arg, "%d:%d%d:%d", &m1, &s1, &m2, &s2) &&
		    2 != sscanf (arg, "%d:%d", &m1, &s1))
			goto err;
		if (m2 == 0) {
			m2 = toc_buffer[n].addr.msf.minute;
			s2 = toc_buffer[n].addr.msf.second;
			f2 = toc_buffer[n].addr.msf.frame;
		}
		return play_msf (m1, s1, f1, m2, s2, f2);
	}

	/*
	 * Play track trk1 [ .idx1 ] [ trk2 [ .idx2 ] ]
	 */
	if (4 != sscanf (arg, "%d.%d%d.%d", &start, &istart, &end, &iend) &&
	    3 != sscanf (arg, "%d.%d%d", &start, &istart, &end) &&
	    3 != sscanf (arg, "%d%d.%d", &start, &end, &iend) &&
	    2 != sscanf (arg, "%d.%d", &start, &istart) &&
	    2 != sscanf (arg, "%d%d", &start, &end) &&
	    1 != sscanf (arg, "%d", &start))
		goto err;
	if (end == 0)
		end = n;
	return play_track (start, istart, end, iend);
}

char *strstatus (int sts)
{
	switch (sts) {
	case ASTS_INVALID:   return ("invalid");
	case ASTS_PLAYING:   return ("playing");
	case ASTS_PAUSED:    return ("paused");
	case ASTS_COMPLETED: return ("completed");
	case ASTS_ERROR:     return ("error");
	case ASTS_VOID:      return ("void");
	default:             return ("??");
	}
}

int info (char *arg)
{
	struct ioc_toc_header h;
	struct ioc_vol v;
	int rc, i, n, trk, m, s, f;

	rc = status (&trk, &m, &s, &f);
	if (rc >= 0)
		if (verbose)
			printf ("Audio status = %d<%s>, current track = %d, current position = %d:%02d.%02d\n",
				rc, strstatus (rc), trk, m, s, f);
		else
			printf ("%d %d %d:%02d.%02d\n", rc, trk, m, s, f);
	else
		printf ("No current status info\n");

	rc = ioctl (fd, CDIOCGETVOL, &v);
	if (rc >= 0)
		if (verbose)
			printf ("Left volume = %d, right volume = %d\n",
				v.vol[0], v.vol[1]);
		else
			printf ("%d %d\n", v.vol[0], v.vol[1]);
	else
		printf ("No volume info\n");

	rc = ioctl (fd, CDIOREADTOCHEADER, &h);
	if (rc >= 0)
		if (verbose)
			printf ("Starting track = %d, ending track = %d, TOC size = %d bytes\n",
				h.starting_track, h.ending_track, h.len);
		else
			printf ("%d %d %d\n", h.starting_track,
				h.ending_track, h.len);
	else {
		perror ("getting toc header");
		return (rc);
	}

	n = h.ending_track - h.starting_track + 1;
	rc = read_toc_entrys ((n + 1) * sizeof (struct cd_toc_entry));
	if (rc < 0)
		return (rc);
	if (verbose) {
		printf ("track     start  duration   block  length   type\n");
		printf ("-------------------------------------------------\n");
	}
	for (i = 0; i < n; i++) {
		printf ("%5d  ", toc_buffer[i].track);
		prtrack (toc_buffer + i, 0);
	}
	printf ("  end  ");
	prtrack (toc_buffer + n, 1);
	return (0);
}

void lba2msf (int lba, u_char *m, u_char *s, u_char *f)
{
	lba += 150;             /* block start offset */
	lba &= 0xffffff;        /* negative lbas use only 24 bits */
	*m = lba / (60 * 75);
	lba %= (60 * 75);
	*s = lba / 75;
	*f = lba % 75;
}

int msf2lba (u_char m, u_char s, u_char f)
{
	return (((m * 60) + s) * 75 + f) - 150;
}

void prtrack (struct cd_toc_entry *e, int lastflag)
{
	int block, next, len;
	u_char m, s, f;

	/* Print track start */
	printf ("%2d:%02d.%02d  ", e->addr.msf.minute,
		e->addr.msf.second, e->addr.msf.frame);

	block = msf2lba (e->addr.msf.minute, e->addr.msf.second,
		e->addr.msf.frame);
	if (lastflag) {
		/* Last track -- print block */
		printf ("       -  %6d       -      -\n", block);
		return;
	}

	next = msf2lba (e[1].addr.msf.minute, e[1].addr.msf.second,
		e[1].addr.msf.frame);
	len = next - block;
	lba2msf (len, &m, &s, &f);

	/* Print duration, block, length, type */
	printf ("%2d:%02d.%02d  %6d  %6d  %5s\n", m, s, f, block, len,
		e->addr_type & 4 ? "data" : "audio");
}

int play_track (int tstart, int istart, int tend, int iend)
{
	struct ioc_play_track t;

	t.start_track = tstart;
	t.start_index = istart;
	t.end_track = tend;
	t.end_index = iend;
	return ioctl (fd, CDIOCPLAYTRACKS, &t);
}

int play_blocks (int blk, int len)
{
	struct ioc_play_blocks t;

	t.blk = blk;
	t.len = len;
	return ioctl (fd, CDIOCPLAYBLOCKS, &t);
}

int setvol (int l, int r)
{
	struct ioc_vol v;

	v.vol[0] = l;
	v.vol[1] = r;
	v.vol[2] = 0;
	v.vol[3] = 0;
	return ioctl (fd, CDIOCSETVOL, &v);
}

int read_toc_entrys (int len)
{
	struct ioc_read_toc_entry t;

	t.address_format = CD_MSF_FORMAT;
	t.starting_track = 0;
	t.data_len = len;
	t.data = toc_buffer;
	return ioctl (fd, CDIOREADTOCENTRYS, (char *) &t);
}

int play_msf (int start_m, int start_s, int start_f,
     int end_m, int end_s, int end_f)
{
	struct ioc_play_msf a;

	a.start_m = start_m;
	a.start_s = start_s;
	a.start_f = start_f;
	a.end_m = end_m;
	a.end_s = end_s;
	a.end_f = end_f;
	return ioctl (fd, CDIOCPLAYMSF, (char *) &a);
}

int status (int *trk, int *min, int *sec, int *frame)
{
	struct ioc_read_subchannel s;
	struct cd_sub_channel_info data;

	bzero (&s, sizeof (s));
	s.data = &data;
	s.data_len = sizeof (data);
	s.address_format = CD_MSF_FORMAT;
	s.data_format = CD_CURRENT_POSITION;
	if (ioctl (fd, CDIOCREADSUBCHANNEL, (char *) &s) < 0)
		return -1;
	*trk = s.data->what.position.track_number;
	*min = s.data->what.position.reladdr.msf.minute;
	*sec = s.data->what.position.reladdr.msf.second;
	*frame = s.data->what.position.reladdr.msf.frame;
	return s.data->header.audio_status;
}

char *input (int *cmd)
{
	static char buf[80];
	char *p;

	do {
		if (verbose)
			fprintf (stderr, "cd> ");
		if (! fgets (buf, sizeof (buf), stdin)) {
			*cmd = CMD_QUIT;
			return 0;
		}
		p = parse (buf, cmd);
	} while (! p);
	return (p);
}

char *parse (char *buf, int *cmd)
{
	struct cmdtab *c;
	char *p;
	int len;

	for (p=buf; *p; ++p)
		if (*p == '\t')
			*p = ' ';
		else if (*p == '\n')
			*p = 0;

	for (p=buf; *p; ++p)
		if (*p == ' ') {
			*p++ = 0;
			break;
		}
	while (*p == ' ')
		++p;

	len = strlen (buf);
	if (! len)
		return (0);
	*cmd = -1;
	for (c=cmdtab; c->name; ++c) {
		/* Try short command form. */
		if (! c->args && len == strlen (c->name) &&
		    strncasecmp (buf, c->name, len) == 0) {
			*cmd = c->command;
			break;
		}

		/* Try long form. */
		if (strncasecmp (buf, c->name, len) != 0)
			continue;

		/* Check inambiguity. */
		if (*cmd != -1) {
			fprintf (stderr, "Ambiguous command\n");
			return (0);
		}
		*cmd = c->command;
	}
	if (*cmd == -1) {
		fprintf (stderr, "Invalid command, enter ``help'' for command list\n");
		return (0);
	}
	return p;
}

int open_cd ()
{
	char devbuf[80];

	if (fd > -1)
		return (1);
	if (*cdname == '/')
		strcpy (devbuf, cdname);
	else if (*cdname == 'r')
		sprintf (devbuf, "/dev/%s", cdname);
	else
		sprintf (devbuf, "/dev/r%s", cdname);
	fd = open (devbuf, O_RDONLY);
	if (fd < 0 && errno == ENOENT) {
		strcat (devbuf, "c");
		fd = open (devbuf, O_RDONLY);
	}
	if (fd < 0) {
		if (errno != ENXIO) {
			perror (devbuf);
			exit (1);
		}
		/* open says 'Device not configured' if no cd in */
		fprintf (stderr, "open: No CD in\n");
		return (0);
	}
	return (1);
}
