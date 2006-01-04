/*
 *	This is an example of a mixer program for Linux
 *
 *	updated 1/1/93 to add stereo, level query, broken
 *      	devmask kludge - cmetz@thor.tjhsst.edu
 *
 * (C) Craig Metz and Hannu Savolainen 1993.
 *
 * You may do anything you wish with this program.
 *
 * ditto for my modifications (John-Mark Gurney, 1997)
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/soundcard.h>

const char *names[SOUND_MIXER_NRDEVICES] = SOUND_DEVICE_NAMES;

void usage(int devmask, int recmask);
int res_name(const char *name, int mask);
void print_recsrc(int recsrc, int shortflag);

void
usage(int devmask, int recmask)
{
	int i, n;

	printf("usage: mixer [-f device] [-s] [dev [+|-][voll[:[+|-]volr]] ...\n"
	       "       mixer [-f device] [-s] recsrc ...\n"
	       "       mixer [-f device] [-s] {^|+|-|=}rec rdev ... \n");
	printf(" devices: ");
	for (i = 0, n = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((1 << i) & devmask)  {
			if (n)
				printf(", ");
			printf("%s", names[i]);
			n = 1;
		}
	printf("\n rec devices: ");
	for (i = 0, n = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((1 << i) & recmask)  {
			if (n)
				printf(", ");
			printf("%s", names[i]);
			n = 1;
		}
	printf("\n");
	exit(1);
}

int
res_name(const char *name, int mask)
{
	int foo;

	for (foo = 0; foo < SOUND_MIXER_NRDEVICES; foo++)
		if ((1 << foo) & mask && !strcmp(names[foo], name))
			break;

	return foo == SOUND_MIXER_NRDEVICES ? -1 : foo;
}

void
print_recsrc(int recsrc, int shortflag)
{
	int i, n = 0;

	if (!shortflag)
		printf("Recording source: ");

	for (i = 0; i < SOUND_MIXER_NRDEVICES; i++)
		if ((1 << i) & recsrc) {
			if (shortflag) {
				if (n)
					printf(" +rec ");
				else
					printf("=rec ");
			} else if (n)
				printf(", ");
			printf("%s", names[i]);
			n = 1;
		}
	if (!shortflag)
		printf("\n");
}

int
main(int argc, char *argv[])
{
	int foo, bar, baz, dev;
	int devmask = 0, recmask = 0, recsrc = 0, orecsrc;
	int dusage = 0, drecsrc = 0, shortflag = 0;
	int l = 0, r = 0, t = 0;
	char lstr[5], rstr[5];
	int n = 0, lrel = 0, rrel = 0;
	int ch;

	char *name;

	name = "/dev/mixer";

	if (!strcmp(argv[0], "mixer2"))
		name = "/dev/mixer1";
	else if (!strcmp(argv[0], "mixer3"))
		name = "/dev/mixer2";

	while ((ch = getopt(argc, argv, "f:s")) != -1)
		switch (ch) {
			case 'f':
				name = optarg;
				break;
			case 's':
				shortflag = 1;
				break;
			default:
				dusage = 1;
		}
	argc -= (optind - 1);
	argv += (optind - 1);

	if ((baz = open(name, O_RDWR)) < 0)
		err(1, "%s", name);
	if (ioctl(baz, SOUND_MIXER_READ_DEVMASK, &devmask) == -1)
		err(1, "SOUND_MIXER_READ_DEVMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECMASK, &recmask) == -1)
		err(1, "SOUND_MIXER_READ_RECMASK");
	if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
		err(1, "SOUND_MIXER_READ_RECSRC");
	orecsrc = recsrc;

	if ((argc == 1) && (dusage == 0)) {
		for (foo = 0; foo < SOUND_MIXER_NRDEVICES; foo++) {
			if (!((1 << foo) & devmask)) 
				continue;
			if (ioctl(baz, MIXER_READ(foo),&bar)== -1) {
			   	warn("MIXER_READ");
				continue;
			}
			if (shortflag)
				printf("%s %d:%d ", names[foo], bar & 0x7f,
				       (bar >> 8) & 0x7f);
			else
				printf("Mixer %-8s is currently set to %3d:%d\n",
				       names[foo], bar & 0x7f, (bar >> 8) & 0x7f);
		}
		if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_READ_RECSRC");
		print_recsrc(recsrc, shortflag);
		return(0);
	}

	argc--; argv++;

	while ((argc > 0) && (dusage == 0)) {
		if (!strcmp("recsrc", *argv)) {
			drecsrc = 1;
			argc--; argv++;
			continue;
		} else if (argc > 1 && !strcmp("rec", *argv + 1)) {
			if (**argv != '+' && **argv != '-' &&
			    **argv != '=' && **argv != '^') {
				warnx("unknown modifier: %c", **argv);
				dusage = 1;
				break;
			}
			if ((dev = res_name(argv[1], recmask)) == -1) {
				warnx("unknown recording device: %s", argv[1]);
				dusage = 1;
				break;
			}
			switch(**argv) {
			case '+':
				recsrc |= (1 << dev);
				break;
			case '-':
				recsrc &= ~(1 << dev);
				break;
			case '=':
				recsrc = (1 << dev);
				break;
			case '^':
				recsrc ^= (1 << dev);
				break;
			}
			drecsrc = 1;
			argc -= 2; argv += 2;
			continue;
		}

		if ((t = sscanf(*argv, "%d:%d", &l, &r)) > 0) {
			dev = 0;
		}
		else if((dev = res_name(*argv, devmask)) == -1) {
			warnx("unknown device: %s", *argv);
			dusage = 1;
			break;
		}

#define	issign(c)	(((c) == '+') || ((c) == '-'))

		if (argc > 1) {
			n = sscanf(argv[1], "%7[^:]:%7s", lstr, rstr);
			if (n > 0) {
				if (issign(lstr[0]))
					lrel = rrel = 1;
				l = atoi(lstr);
			}
			if (n > 1) {
				rrel = 0;
				if (issign(rstr[0]))
					rrel = 1;
				r = atoi(rstr);
			}
		}

		switch(argc > 1 ? n : t) {
		case 0:
			if (ioctl(baz, MIXER_READ(dev),&bar)== -1) {
				warn("MIXER_READ");
				argc--; argv++;
				continue;
			}
			if (shortflag)
				printf("%s %d:%d ", names[dev], bar & 0x7f,
				       (bar >> 8) & 0x7f);
			else
				printf("Mixer %-8s is currently set to %3d:%d\n",
				  names[dev], bar & 0x7f, (bar >> 8) & 0x7f);

			argc--; argv++;
			break;
		case 1:
			r = l;
		case 2:
			if (ioctl(baz, MIXER_READ(dev),&bar)== -1) {
				warn("MIXER_READ");
				argc--; argv++;
				continue;
			}

			if (lrel)
				l = (bar & 0x7f) + l;
			if (rrel)
				r = ((bar >> 8) & 0x7f) + r;

			if (l < 0)
				l = 0;
			else if (l > 100)
				l = 100;
			if (r < 0)
				r = 0;
			else if (r > 100)
				r = 100;

			printf("Setting the mixer %s from %d:%d to %d:%d.\n",
			    names[dev], bar & 0x7f, (bar >> 8) & 0x7f, l, r);

			l |= r << 8;
			if (ioctl(baz, MIXER_WRITE(dev), &l) == -1)
				warn("WRITE_MIXER");

			argc -= 2; argv += 2;
 			break;
		}
	}

	if (dusage) {
		close(baz);
		usage(devmask, recmask);
		/* Not reached */
	}

	if (orecsrc != recsrc)
		if (ioctl(baz, SOUND_MIXER_WRITE_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_WRITE_RECSRC");
 
	if (drecsrc) {
		if (ioctl(baz, SOUND_MIXER_READ_RECSRC, &recsrc) == -1)
			err(1, "SOUND_MIXER_READ_RECSRC");
		print_recsrc(recsrc, shortflag);
	}

	close(baz);

	exit(0);
}
