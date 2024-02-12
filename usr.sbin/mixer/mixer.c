/*-
 * Copyright (c) 2021 Christos Margiolis <christos@FreeBSD.org>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <mixer.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum {
	C_VOL = 0,
	C_MUT,
	C_SRC,
};

static void usage(void) __dead2;
static void initctls(struct mixer *);
static void printall(struct mixer *, int);
static void printminfo(struct mixer *, int);
static void printdev(struct mixer *, int);
static void printrecsrc(struct mixer *, int); /* XXX: change name */
static int set_dunit(struct mixer *, int);
/* Control handlers */
static int mod_volume(struct mix_dev *, void *);
static int mod_mute(struct mix_dev *, void *);
static int mod_recsrc(struct mix_dev *, void *);
static int print_volume(struct mix_dev *, void *);
static int print_mute(struct mix_dev *, void *);
static int print_recsrc(struct mix_dev *, void *);

int
main(int argc, char *argv[])
{
	struct mixer *m;
	mix_ctl_t *cp;
	char *name = NULL, buf[NAME_MAX];
	char *p, *q, *devstr, *ctlstr, *valstr = NULL;
	int dunit, i, n, pall = 1, shorthand;
	int aflag = 0, dflag = 0, oflag = 0, sflag = 0;
	int ch;

	while ((ch = getopt(argc, argv, "ad:f:hos")) != -1) {
		switch (ch) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			if (strncmp(optarg, "pcm", 3) == 0)
				optarg += 3;
			errno = 0;
			dunit = strtol(optarg, NULL, 10);
			if (errno == EINVAL || errno == ERANGE)
				err(1, "strtol(%s)", optarg);
			dflag = 1;
			break;
		case 'f':
			name = optarg;
			break;
		case 'o':
			oflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'h': /* FALLTHROUGH */
		case '?':
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	/* Print all mixers and exit. */
	if (aflag) {
		if ((n = mixer_get_nmixers()) < 0)
			errx(1, "no mixers present in the system");
		for (i = 0; i < n; i++) {
			(void)snprintf(buf, sizeof(buf), "/dev/mixer%d", i);
			if ((m = mixer_open(buf)) == NULL)
				errx(1, "%s: no such mixer", buf);
			initctls(m);
			if (sflag)
				printrecsrc(m, oflag);
			else {
				printall(m, oflag);
				if (oflag)
					printf("\n");
			}
			(void)mixer_close(m);
		}
		return (0);
	}

	if ((m = mixer_open(name)) == NULL)
		errx(1, "%s: no such mixer", name);

	initctls(m);

	if (dflag && set_dunit(m, dunit) < 0)
		goto parse;
	if (sflag) {
		printrecsrc(m, oflag);
		(void)mixer_close(m);
		return (0);
	}

parse:
	while (argc > 0) {
		if ((p = strdup(*argv)) == NULL)
			err(1, "strdup(%s)", *argv);

		/* Check if we're using the shorthand syntax for volume setting. */
		shorthand = 0;
		for (q = p; *q != '\0'; q++) {
			if (*q == '=') {
				q++;
				shorthand = ((*q >= '0' && *q <= '9') ||
				    *q == '+' || *q == '-' || *q == '.');
				break;
			} else if (*q == '.')
				break;
		}

		/* Split the string into device, control and value. */
		devstr = strsep(&p, ".=");
		if ((m->dev = mixer_get_dev_byname(m, devstr)) == NULL) {
			warnx("%s: no such device", devstr);
			goto next;
		}
		/* Input: `dev`. */
		if (p == NULL) {
			printdev(m, 1);
			pall = 0;
			goto next;
		} else if (shorthand) {
			/*
			 * Input: `dev=N` -> shorthand for `dev.volume=N`.
			 *
			 * We don't care what the rest of the string contains as
			 * long as we're sure the very beginning is right,
			 * mod_volume() will take care of parsing it properly.
			 */
			cp = mixer_get_ctl(m->dev, C_VOL);
			cp->mod(cp->parent_dev, p);
			goto next;
		}
		ctlstr = strsep(&p, "=");
		if ((cp = mixer_get_ctl_byname(m->dev, ctlstr)) == NULL) {
			warnx("%s.%s: no such control", devstr, ctlstr);
			goto next;
		}
		/* Input: `dev.control`. */
		if (p == NULL) {
			(void)cp->print(cp->parent_dev, cp->name);
			pall = 0;
			goto next;
		}
		valstr = p;
		/* Input: `dev.control=val`. */
		cp->mod(cp->parent_dev, valstr);
next:
		free(p);
		argc--;
		argv++;
	}

	if (pall)
		printall(m, oflag);
	(void)mixer_close(m);

	return (0);
}

static void __dead2
usage(void)
{
	fprintf(stderr, "usage: %1$s [-f device] [-d pcmN | N] [-os] [dev[.control[=value]]] ...\n"
	    "       %1$s [-os] -a\n"
	    "       %1$s -h\n", getprogname());
	exit(1);
}

static void
initctls(struct mixer *m)
{
	struct mix_dev *dp;
	int rc = 0;

	TAILQ_FOREACH(dp, &m->devs, devs) {
		rc += mixer_add_ctl(dp, C_VOL, "volume", mod_volume, print_volume);
		rc += mixer_add_ctl(dp, C_MUT, "mute", mod_mute, print_mute);
		rc += mixer_add_ctl(dp, C_SRC, "recsrc", mod_recsrc, print_recsrc);
	}
	if (rc) {
		(void)mixer_close(m);
		errx(1, "cannot make mixer controls");
	}
}

static void
printall(struct mixer *m, int oflag)
{
	struct mix_dev *dp;

	printminfo(m, oflag);
	TAILQ_FOREACH(dp, &m->devs, devs) {
		m->dev = dp;
		printdev(m, oflag);
	}
}

static void
printminfo(struct mixer *m, int oflag)
{
	int playrec = MIX_MODE_PLAY | MIX_MODE_REC;

	if (oflag)
		return;
	printf("%s:", m->mi.name);
	if (*m->ci.longname != '\0')
		printf(" <%s>", m->ci.longname);
	if (*m->ci.hw_info != '\0')
		printf(" %s", m->ci.hw_info);

	if (m->mode != 0)
		printf(" (");
	if (m->mode & MIX_MODE_PLAY)
		printf("play");
	if ((m->mode & playrec) == playrec)
		printf("/");
	if (m->mode & MIX_MODE_REC)
		printf("rec");
	if (m->mode != 0)
		printf(")");

	if (m->f_default)
		printf(" (default)");
	printf("\n");
}

static void
printdev(struct mixer *m, int oflag)
{
	struct mix_dev *d = m->dev;
	mix_ctl_t *cp;

	if (!oflag) {
		printf("    %-10s= %.2f:%.2f    ",
		    d->name, d->vol.left, d->vol.right);
		if (!MIX_ISREC(m, d->devno))
			printf(" pbk");
		if (MIX_ISREC(m, d->devno))
			printf(" rec");
		if (MIX_ISRECSRC(m, d->devno))
			printf(" src");
		if (MIX_ISMUTE(m, d->devno))
			printf(" mute");
		printf("\n");
	} else {
		TAILQ_FOREACH(cp, &d->ctls, ctls) {
			(void)cp->print(cp->parent_dev, cp->name);
		}
	}
}

static void
printrecsrc(struct mixer *m, int oflag)
{
	struct mix_dev *dp;
	int n = 0;

	if (!m->recmask)
		return;
	if (!oflag)
		printf("%s: ", m->mi.name);
	TAILQ_FOREACH(dp, &m->devs, devs) {
		if (MIX_ISRECSRC(m, dp->devno)) {
			if (n++ && !oflag)
				printf(", ");
			printf("%s", dp->name);
			if (oflag)
				printf(".%s=+%s",
				    mixer_get_ctl(dp, C_SRC)->name, n ? " " : "");
		}
	}
	printf("\n");
}

static int
set_dunit(struct mixer *m, int dunit)
{
	int n;

	if ((n = mixer_get_dunit()) < 0) {
		warn("cannot get default unit");
		return (-1);
	}
	if (mixer_set_dunit(m, dunit) < 0) {
		warn("cannot set default unit to %d", dunit);
		return (-1);
	}
	printf("default_unit: %d -> %d\n", n, dunit);

	return (0);
}

static int
mod_volume(struct mix_dev *d, void *p)
{
	struct mixer *m;
	mix_ctl_t *cp;
	mix_volume_t v;
	const char *val;
	char *endp, lstr[8], rstr[8];
	float lprev, rprev, lrel, rrel;
	int n;

	m = d->parent_mixer;
	cp = mixer_get_ctl(m->dev, C_VOL);
	val = p;
	n = sscanf(val, "%7[^:]:%7s", lstr, rstr);
	if (n == EOF) {
		warnx("invalid volume value: %s", val);
		return (-1);
	}
	lrel = rrel = 0;
	if (n > 0) {
		if (*lstr == '+' || *lstr == '-')
			lrel = 1;
		v.left = strtof(lstr, &endp);
		if (*endp != '\0' && (*endp != '%' || *(endp + 1) != '\0')) {
			warnx("invalid volume value: %s", lstr);
			return (-1);
		}

		if (*endp == '%')
			v.left /= 100.0f;
	}
	if (n > 1) {
		if (*rstr == '+' || *rstr == '-')
			rrel = 1;
		v.right = strtof(rstr, &endp);
		if (*endp != '\0' && (*endp != '%' || *(endp + 1) != '\0')) {
			warnx("invalid volume value: %s", rstr);
			return (-1);
		}

		if (*endp == '%')
			v.right /= 100.0f;
	}
	switch (n) {
	case 1:
		v.right = v.left; /* FALLTHROUGH */
		rrel = lrel;
	case 2:
		if (lrel)
			v.left += m->dev->vol.left;
		if (rrel)
			v.right += m->dev->vol.right;

		if (v.left < MIX_VOLMIN)
			v.left = MIX_VOLMIN;
		else if (v.left > MIX_VOLMAX)
			v.left = MIX_VOLMAX;
		if (v.right < MIX_VOLMIN)
			v.right = MIX_VOLMIN;
		else if (v.right > MIX_VOLMAX)
			v.right = MIX_VOLMAX;

		lprev = m->dev->vol.left;
		rprev = m->dev->vol.right;
		if (mixer_set_vol(m, v) < 0)
			warn("%s.%s=%.2f:%.2f",
			    m->dev->name, cp->name, v.left, v.right);
		else
			printf("%s.%s: %.2f:%.2f -> %.2f:%.2f\n",
			   m->dev->name, cp->name, lprev, rprev, v.left, v.right);
	}

	return (0);
}

static int
mod_mute(struct mix_dev *d, void *p)
{
	struct mixer *m;
	mix_ctl_t *cp;
	const char *val;
	int n, opt = -1;

	m = d->parent_mixer;
	cp = mixer_get_ctl(m->dev, C_MUT);
	val = p;
	if (strncmp(val, "off", strlen(val)) == 0 || *val == '0')
		opt = MIX_UNMUTE;
	else if (strncmp(val, "on", strlen(val)) == 0 || *val == '1')
		opt = MIX_MUTE;
	else if (strncmp(val, "toggle", strlen(val)) == 0 || *val == '^')
		opt = MIX_TOGGLEMUTE;
	else {
		warnx("%s: no such modifier", val);
		return (-1);
	}
	n = MIX_ISMUTE(m, m->dev->devno);
	if (mixer_set_mute(m, opt) < 0)
		warn("%s.%s=%s", m->dev->name, cp->name, val);
	else
		printf("%s.%s: %s -> %s\n",
		    m->dev->name, cp->name,
		    n ? "on" : "off",
		    MIX_ISMUTE(m, m->dev->devno) ? "on" : "off");

	return (0);
}

static int
mod_recsrc(struct mix_dev *d, void *p)
{
	struct mixer *m;
	mix_ctl_t *cp;
	const char *val;
	int n, opt = -1;

	m = d->parent_mixer;
	cp = mixer_get_ctl(m->dev, C_SRC);
	val = p;
	if (strncmp(val, "add", strlen(val)) == 0 || *val == '+')
		opt = MIX_ADDRECSRC;
	else if (strncmp(val, "remove", strlen(val)) == 0 || *val == '-')
		opt = MIX_REMOVERECSRC;
	else if (strncmp(val, "set", strlen(val)) == 0 || *val == '=')
		opt = MIX_SETRECSRC;
	else if (strncmp(val, "toggle", strlen(val)) == 0 || *val == '^')
		opt = MIX_TOGGLERECSRC;
	else {
		warnx("%s: no such modifier", val);
		return (-1);
	}
	n = MIX_ISRECSRC(m, m->dev->devno);
	if (mixer_mod_recsrc(m, opt) < 0)
		warn("%s.%s=%s", m->dev->name, cp->name, val);
	else
		printf("%s.%s: %s -> %s\n",
		    m->dev->name, cp->name,
		    n ? "add" : "remove",
		    MIX_ISRECSRC(m, m->dev->devno) ? "add" : "remove");

	return (0);
}

static int
print_volume(struct mix_dev *d, void *p)
{
	struct mixer *m = d->parent_mixer;
	const char *ctl_name = p;

	printf("%s.%s=%.2f:%.2f\n",
	    m->dev->name, ctl_name, m->dev->vol.left, m->dev->vol.right);

	return (0);
}

static int
print_mute(struct mix_dev *d, void *p)
{
	struct mixer *m = d->parent_mixer;
	const char *ctl_name = p;

	printf("%s.%s=%s\n", m->dev->name, ctl_name,
	    MIX_ISMUTE(m, m->dev->devno) ? "on" : "off");

	return (0);
}

static int
print_recsrc(struct mix_dev *d, void *p)
{
	struct mixer *m = d->parent_mixer;
	const char *ctl_name = p;

	if (!MIX_ISRECSRC(m, m->dev->devno))
		return (-1);
	printf("%s.%s=add\n", m->dev->name, ctl_name);

	return (0);
}
