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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 * Taught to send *real* morse by Lyndon Nerenberg (VE7TCP/VE6BBM)
 * <lyndon@orthanc.com>
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
 "$FreeBSD: src/games/morse/morse.c,v 1.12 2000/02/27 01:21:28 joerg Exp $";
#endif /* not lint */

#include <sys/time.h>

#include <ctype.h>
#include <fcntl.h>
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#ifdef SPEAKER
#include <machine/speaker.h>
#endif

struct morsetab {
	char            inchar;
	char           *morse;
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

	/* prosigns without already assigned values */

	{'#', ".-..."},		/* AS */
	{'@', "...-.-"},	/* SK */
	{'*', "...-."},		/* VE */
	{'%', "-...-.-"},	/* BK */

	{'\0', ""}
};


static const struct morsetab iso8859tab[] = {
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

static const struct morsetab koi8rtab[] = {
	/*
	 * the cyrillic alphabet; you'll need a KOI8R font in order
	 * to see the actual characters
	 */
	{'Á', ".-"},		/* a */
	{'Â', "-..."},	/* be */
	{'×', ".--"},	/* ve */
	{'Ç', "--."},	/* ge */
	{'Ä', "-.."},	/* de */
	{'Å', "."},		/* ye */
	{'£', "."},         	/* yo, the same as ye */
	{'Ö', "...-"},	/* she */
	{'Ú', "--.."},	/* ze */
	{'É', ".."},		/* i */
	{'Ê', ".---"},	/* i kratkoye */
	{'Ë', "-.-"},	/* ka */
	{'Ì', ".-.."},	/* el */
	{'Í', "--"},		/* em */
	{'Î', "-."},		/* en */
	{'Ï', "---"},	/* o */
	{'Ð', ".--."},	/* pe */
	{'Ò', ".-."},	/* er */
	{'Ó', "..."},	/* es */
	{'Ô', "-"},		/* te */
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

#define GETOPTOPTS "d:ef:sw:"
#define USAGE \
"usage: morse [-s] [-e] [-d device] [-w speed] [-f frequency] [string ...]\n"

static int      pflag, sflag, eflag;
static int      wpm = 20;	/* words per minute */
#define FREQUENCY 600
static int      freq = FREQUENCY;
static char	*device;	/* for tty-controlled generator */

#define DASH_LEN 3
#define CHAR_SPACE 3
#define WORD_SPACE (7 - CHAR_SPACE - 1)
static float    dot_clock;
int             spkr, line;
struct termios	otty, ntty;
int		olflags;

#ifdef SPEAKER
tone_t          sound;
#undef GETOPTOPTS
#define GETOPTOPTS "d:ef:psw:"
#undef USAGE
#define USAGE \
"usage: morse [-s] [-p] [-e] [-d device] [-w speed] [-f frequency] [string ...]\n"
#endif

static const struct morsetab *hightab = iso8859tab;

int
main(int argc, char **argv)
{
	int    ch, lflags;
	char  *p;

	while ((ch = getopt(argc, argv, GETOPTOPTS)) != -1)
		switch ((char) ch) {
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
	if ((pflag || device) && sflag) {
		fputs("morse: only one of -p, -d and -s allowed\n", stderr);
		exit(1);
	}
	if ((pflag || device) && ((wpm < 1) || (wpm > 60))) {
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
	}

	argc -= optind;
	argv += optind;

	if((p = getenv("LC_CTYPE")) ||
	   (p = getenv("LC_ALL")) ||
	   (p = getenv("LANG"))) {
		if(strlen(p) >= sizeof(".KOI8-R") &&
		   strcasecmp(&p[strlen(p) + 1 - sizeof(".KOI8-R")], ".KOI8-R") == 0)
			hightab = koi8rtab;
	}
	(void) setlocale(LC_CTYPE, "");

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
		if (pflag) {
			play(" ");
			return;
		} else if (device) {
			ttyout(" ");
			return;
		} else {
			show("");
			return;
		}
	}
	for (m = ((unsigned char)c < 0x80? mtab: hightab);
	     m->inchar != '\0';
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
	if (sflag)
		printf(" %s", s);
	else
		for (; *s; ++s)
			printf(" %s", *s == '.' ? "dit" : "dah");
	printf("\n");
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
			sound.duration = dot_clock * WORD_SPACE;
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
	sound.duration = dot_clock * CHAR_SPACE;
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
			duration = dot_clock * WORD_SPACE;
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
	duration = dot_clock * CHAR_SPACE * 10000;
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
