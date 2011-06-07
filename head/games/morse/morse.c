/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Taught to send *real* morse by Lyndon Nerenberg (VE6BBM)
 * <lyndon@orthanc.ca>
 */

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)morse.c	8.1 (Berkeley) 5/31/93";
#endif
static const char rcsid[] =
 "$FreeBSD$";
#endif /* not lint */

#include <sys/time.h>

#include <ctype.h>
#include <fcntl.h>
#include <langinfo.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

/* Always use the speaker, let the open fail if -p is selected */
#define SPEAKER "/dev/speaker"

#ifdef SPEAKER
#include <dev/speaker/speaker.h>
#endif

struct morsetab {
	const char      inchar;
	const char     *morse;
};

static const struct morsetab mtab[] = {

	/* letters */

	{'a', ".-"},
	{'b', "-..."},
	{'c', "-.-."},
	{'d', "-.."},
	{'e', "."},
	{'f', "..-."},
	{'g', "--."},
	{'h', "...."},
	{'i', ".."},
	{'j', ".---"},
	{'k', "-.-"},
	{'l', ".-.."},
	{'m', "--"},
	{'n', "-."},
	{'o', "---"},
	{'p', ".--."},
	{'q', "--.-"},
	{'r', ".-."},
	{'s', "..."},
	{'t', "-"},
	{'u', "..-"},
	{'v', "...-"},
	{'w', ".--"},
	{'x', "-..-"},
	{'y', "-.--"},
	{'z', "--.."},

	/* digits */

	{'0', "-----"},
	{'1', ".----"},
	{'2', "..---"},
	{'3', "...--"},
	{'4', "....-"},
	{'5', "....."},
	{'6', "-...."},
	{'7', "--..."},
	{'8', "---.."},
	{'9', "----."},

	/* punctuation */

	{',', "--..--"},
	{'.', ".-.-.-"},
	{'"', ".-..-."},
	{'!', "..--."},
	{'?', "..--.."},
	{'/', "-..-."},
	{'-', "-....-"},
	{'=', "-...-"},		/* BT */
	{':', "---..."},
	{';', "-.-.-."},
	{'(', "-.--."},		/* KN */
	{')', "-.--.-"},
	{'$', "...-..-"},
	{'+', ".-.-."},		/* AR */
	{'@', ".--.-."},	/* AC */

	/* prosigns without already assigned values */

	{'#', ".-..."},		/* AS */
	{'&', "...-.-"},	/* SK */
	{'*', "...-."},		/* VE */
	{'%', "-...-.-"},	/* BK */

	{'\0', ""}
};


static const struct morsetab iso8859_1tab[] = {
	{'á', ".--.-"},
	{'à', ".--.-"},
	{'â', ".--.-"},
	{'ä', ".-.-"},
	{'ç', "-.-.."},
	{'é', "..-.."},
	{'è', "..-.."},
	{'ê', "-..-."},
	{'ö', "---."},
	{'ü', "..--"},

	{'\0', ""}
};

static const struct morsetab iso8859_7tab[] = {
	/*
	 * The Greek alphabet; you'll need an ISO8859-7 font in order
	 * to see the actual characters.
	 * This table does not implement:
	 * - the special sequences for the seven diphthongs,
	 * - the punctuation differences.
	 * Implementing these features would introduce too many
	 * special-cases in the program's main loop.
	 * The diphthong sequences are:
	 * alpha iota		.-.-
	 * alpha upsilon	..--
	 * epsilon upsilon	---.
	 * eta upsilon		...-
	 * omicron iota		---..
	 * omicron upsilon	..-
	 * upsilon iota		.---
	 * The different punctuation symbols are:
	 * ;	..-.-
	 * !	--..--
	 */
	{'á', ".-"},	/* alpha */
	{'Ü', ".-"},	/* alpha with acute */
	{'â', "-..."},	/* beta */
	{'ã', "--."},	/* gamma */
	{'ä', "-.."},	/* delta */
	{'å', "."},	/* epsilon */
	{'Ý', "."},	/* epsilon with acute */
	{'æ', "--.."},	/* zeta */
	{'ç', "...."},	/* eta */
	{'Þ', "...."},	/* eta with acute */
	{'è', "-.-."},	/* theta */
	{'é', ".."},	/* iota */
	{'ß', ".."},	/* iota with acute */
	{'ú', ".."},	/* iota with diaeresis */
	{'À', ".."},	/* iota with acute and diaeresis */
	{'ê', "-.-"},	/* kappa */
	{'ë', ".-.."},	/* lambda */
	{'ì', "--"},	/* mu */
	{'í', "-."},	/* nu */
	{'î', "-..-"},	/* xi */
	{'ï', "---"},	/* omicron */
	{'ü', "---"},	/* omicron with acute */
	{'ð', ".--."},	/* pi */
	{'ñ', ".-."},	/* rho */
	{'ó', "..."},	/* sigma */
	{'ò', "..."},	/* final sigma */
	{'ô', "-"},	/* tau */
	{'õ', "-.--"},	/* upsilon */
	{'ý', "-.--"},	/* upsilon with acute */
	{'û', "-.--"},	/* upsilon and diaeresis */
	{'à', "-.--"},	/* upsilon with acute and diaeresis */
	{'ö', "..-."},	/* phi */
	{'÷', "----"},	/* chi */
	{'ø', "--.-"},	/* psi */
	{'ù', ".--"},	/* omega */
	{'þ', ".--"},	/* omega with acute */

	{'\0', ""}
};

static const struct morsetab koi8rtab[] = {
	/*
	 * The Cyrillic alphabet; you'll need a KOI8-R font in order
	 * to see the actual characters
	 */
	{'Á', ".-"},	/* a */
	{'Â', "-..."},	/* be */
	{'×', ".--"},	/* ve */
	{'Ç', "--."},	/* ge */
	{'Ä', "-.."},	/* de */
	{'Å', "."},	/* ye */
	{'£', "."},	/* yo, the same as ye */
	{'Ö', "...-"},	/* she */
	{'Ú', "--.."},	/* ze */
	{'É', ".."},	/* i */
	{'Ê', ".---"},	/* i kratkoye */
	{'Ë', "-.-"},	/* ka */
	{'Ì', ".-.."},	/* el */
	{'Í', "--"},	/* em */
	{'Î', "-."},	/* en */
	{'Ï', "---"},	/* o */
	{'Ð', ".--."},	/* pe */
	{'Ò', ".-."},	/* er */
	{'Ó', "..."},	/* es */
	{'Ô', "-"},	/* te */
	{'Õ', "..-"},	/* u */
	{'Æ', "..-."},	/* ef */
	{'È', "...."},	/* kha */
	{'Ã', "-.-."},	/* ce */
	{'Þ', "---."},	/* che */
	{'Û', "----"},	/* sha */
	{'Ý', "--.-"},	/* shcha */
	{'Ù', "-.--"},	/* yi */
	{'Ø', "-..-"},	/* myakhkij znak */
	{'Ü', "..-.."},	/* ae */
	{'À', "..--"},	/* yu */
	{'Ñ', ".-.-"},	/* ya */

	{'\0', ""}
};

void            show(const char *), play(const char *), morse(char);
void		ttyout(const char *);
void		sighandler(int);

#define GETOPTOPTS "c:d:ef:lsw:"
#define USAGE \
"usage: morse [-els] [-d device] [-w speed] [-c speed] [-f frequency] [string ...]\n"

static int      pflag, lflag, sflag, eflag;
static int      wpm = 20;	/* effective words per minute */
static int      cpm;		/* effective words per minute between
				 * characters */
#define FREQUENCY 600
static int      freq = FREQUENCY;
static char	*device;	/* for tty-controlled generator */

#define DASH_LEN 3
#define CHAR_SPACE 3
#define WORD_SPACE (7 - CHAR_SPACE - 1)
static float    dot_clock;
static float    cdot_clock;
int             spkr, line;
struct termios	otty, ntty;
int		olflags;

#ifdef SPEAKER
tone_t          sound;
#undef GETOPTOPTS
#define GETOPTOPTS "c:d:ef:lpsw:"
#undef USAGE
#define USAGE \
"usage: morse [-elps] [-d device] [-w speed] [-c speed] [-f frequency] [string ...]\n"
#endif

static const struct morsetab *hightab;

int
main(int argc, char **argv)
{
	int    ch, lflags;
	char  *p, *codeset;

	while ((ch = getopt(argc, argv, GETOPTOPTS)) != -1)
		switch ((char) ch) {
		case 'c':
			cpm = atoi(optarg);
			break;
		case 'd':
			device = optarg;
			break;
		case 'e':
			eflag = 1;
			setvbuf(stdout, 0, _IONBF, 0);
			break;
		case 'f':
			freq = atoi(optarg);
			break;
		case 'l':
			lflag = 1;
			break;
#ifdef SPEAKER
		case 'p':
			pflag = 1;
			break;
#endif
		case 's':
			sflag = 1;
			break;
		case 'w':
			wpm = atoi(optarg);
			break;
		case '?':
		default:
			fputs(USAGE, stderr);
			exit(1);
		}
	if (sflag && lflag) {
		fputs("morse: only one of -l and -s allowed\n", stderr);
		exit(1);
	}
	if ((pflag || device) && (sflag || lflag)) {
		fputs("morse: only one of -p, -d and -l, -s allowed\n", stderr);
		exit(1);
	}
	if (cpm == 0)
		cpm = wpm;
	if ((pflag || device) && ((wpm < 1) || (wpm > 60) || (cpm < 1) || (cpm > 60))) {
		fputs("morse: insane speed\n", stderr);
		exit(1);
	}
	if ((pflag || device) && (freq == 0))
		freq = FREQUENCY;

#ifdef SPEAKER
	if (pflag) {
		if ((spkr = open(SPEAKER, O_WRONLY, 0)) == -1) {
			perror(SPEAKER);
			exit(1);
		}
	} else
#endif
	if (device) {
		if ((line = open(device, O_WRONLY | O_NONBLOCK)) == -1) {
			perror("open tty line");
			exit(1);
		}
		if (tcgetattr(line, &otty) == -1) {
			perror("tcgetattr() failed");
			exit(1);
		}
		ntty = otty;
		ntty.c_cflag |= CLOCAL;
		tcsetattr(line, TCSANOW, &ntty);
		lflags = fcntl(line, F_GETFL);
		lflags &= ~O_NONBLOCK;
		fcntl(line, F_SETFL, &lflags);
		ioctl(line, TIOCMGET, &lflags);
		lflags &= ~TIOCM_RTS;
		olflags = lflags;
		ioctl(line, TIOCMSET, &lflags);
		(void)signal(SIGHUP, sighandler);
		(void)signal(SIGINT, sighandler);
		(void)signal(SIGQUIT, sighandler);
		(void)signal(SIGTERM, sighandler);
	}
	if (pflag || device) {
		dot_clock = wpm / 2.4;		/* dots/sec */
		dot_clock = 1 / dot_clock;	/* duration of a dot */
		dot_clock = dot_clock / 2;	/* dot_clock runs at twice */
						/* the dot rate */
		dot_clock = dot_clock * 100;	/* scale for ioctl */

		cdot_clock = cpm / 2.4;		/* dots/sec */
		cdot_clock = 1 / cdot_clock;	/* duration of a dot */
		cdot_clock = cdot_clock / 2;	/* dot_clock runs at twice */
						/* the dot rate */
		cdot_clock = cdot_clock * 100;	/* scale for ioctl */
	}

	argc -= optind;
	argv += optind;

	if (setlocale(LC_CTYPE, "") != NULL &&
	    *(codeset = nl_langinfo(CODESET)) != '\0') {
		if (strcmp(codeset, "KOI8-R") == 0)
			hightab = koi8rtab;
		else if (strcmp(codeset, "ISO8859-1") == 0 ||
			 strcmp(codeset, "ISO8859-15") == 0)
			hightab = iso8859_1tab;
		else if (strcmp(codeset, "ISO8859-7") == 0)
			hightab = iso8859_7tab;
	}

	if (lflag)
		printf("m");
	if (*argv) {
		do {
			for (p = *argv; *p; ++p) {
				if (eflag)
					putchar(*p);
				morse(*p);
			}
			if (eflag)
				putchar(' ');
			morse(' ');
		} while (*++argv);
	} else {
		while ((ch = getchar()) != EOF) {
			if (eflag)
				putchar(ch);
			morse(ch);
		}
	}
	if (device)
		tcsetattr(line, TCSANOW, &otty);
	exit(0);
}

void
morse(char c)
{
	const struct morsetab *m;

	if (isalpha((unsigned char)c))
		c = tolower((unsigned char)c);
	if ((c == '\r') || (c == '\n'))
		c = ' ';
	if (c == ' ') {
		if (pflag)
			play(" ");
		else if (device)
			ttyout(" ");
		else if (lflag)
			printf("\n");
		else
			show("");
		return;
	}
	for (m = ((unsigned char)c < 0x80? mtab: hightab);
	     m != NULL && m->inchar != '\0';
	     m++) {
		if (m->inchar == c) {
			if (pflag) {
				play(m->morse);
			} else if (device) {
				ttyout(m->morse);
			} else
				show(m->morse);
		}
	}
}

void
show(const char *s)
{
	if (lflag) {
		printf("%s ", s);
	} else if (sflag) {
		printf(" %s\n", s);
	} else {
		for (; *s; ++s)
			printf(" %s", *s == '.' ? *(s + 1) == '\0' ? "dit" :
			    "di" : "dah");
		printf("\n");
	}
}

void
play(const char *s)
{
#ifdef SPEAKER
	const char *c;

	for (c = s; *c != '\0'; c++) {
		switch (*c) {
		case '.':
			sound.frequency = freq;
			sound.duration = dot_clock;
			break;
		case '-':
			sound.frequency = freq;
			sound.duration = dot_clock * DASH_LEN;
			break;
		case ' ':
			sound.frequency = 0;
			sound.duration = cdot_clock * WORD_SPACE;
			break;
		default:
			sound.duration = 0;
		}
		if (sound.duration) {
			if (ioctl(spkr, SPKRTONE, &sound) == -1) {
				perror("ioctl play");
				exit(1);
			}
		}
		sound.frequency = 0;
		sound.duration = dot_clock;
		if (ioctl(spkr, SPKRTONE, &sound) == -1) {
			perror("ioctl rest");
			exit(1);
		}
	}
	sound.frequency = 0;
	sound.duration = cdot_clock * CHAR_SPACE;
	ioctl(spkr, SPKRTONE, &sound);
#endif
}

void
ttyout(const char *s)
{
	const char *c;
	int duration, on, lflags;

	for (c = s; *c != '\0'; c++) {
		switch (*c) {
		case '.':
			on = 1;
			duration = dot_clock;
			break;
		case '-':
			on = 1;
			duration = dot_clock * DASH_LEN;
			break;
		case ' ':
			on = 0;
			duration = cdot_clock * WORD_SPACE;
			break;
		default:
			on = 0;
			duration = 0;
		}
		if (on) {
			ioctl(line, TIOCMGET, &lflags);
			lflags |= TIOCM_RTS;
			ioctl(line, TIOCMSET, &lflags);
		}
		duration *= 10000;
		if (duration)
			usleep(duration);
		ioctl(line, TIOCMGET, &lflags);
		lflags &= ~TIOCM_RTS;
		ioctl(line, TIOCMSET, &lflags);
		duration = dot_clock * 10000;
		usleep(duration);
	}
	duration = cdot_clock * CHAR_SPACE * 10000;
	usleep(duration);
}

void
sighandler(int signo)
{

	ioctl(line, TIOCMSET, &olflags);
	tcsetattr(line, TCSANOW, &otty);

	signal(signo, SIG_DFL);
	(void)kill(getpid(), signo);
}
