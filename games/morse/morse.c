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
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)morse.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include <stdio.h>
#include <ctype.h>
#include <locale.h>
#include <stdlib.h>

#ifdef SPEAKER
#include <machine/speaker.h>
#include <fcntl.h>
#endif

struct morsetab {
	char            inchar;
	char           *morse;
};

static struct morsetab mtab[] = {

	/* letters */

	'a', ".-",
	'b', "-...",
	'c', "-.-.",
	'd', "-..",
	'e', ".",
	'f', "..-.",
	'g', "--.",
	'h', "....",
	'i', "..",
	'j', ".---",
	'k', "-.-",
	'l', ".-..",
	'm', "--",
	'n', "-.",
	'o', "---",
	'p', ".--.",
	'q', "--.-",
	'r', ".-.",
	's', "...",
	't', "-",
	'u', "..-",
	'v', "...-",
	'w', ".--",
	'x', "-..-",
	'y', "-.--",
	'z', "--..",

	/* digits */

	'0', "-----",
	'1', ".----",
	'2', "..---",
	'3', "...--",
	'4', "....-",
	'5', ".....",
	'6', "-....",
	'7', "--...",
	'8', "---..",
	'9', "----.",

	/* punctuation */

	',', "--..--",
	'.', ".-.-.-",
	'?', "..--..",
	'/', "-..-.",
	'-', "-....-",
	'=', "-...-",		/* BT */
	':', "---...",
	';', "-.-.-.",
	'(', "-.--.",		/* KN */
	')', "-.--.-",
	'$', "...-..-",
	'+', ".-.-.",		/* AR */

	/* prosigns without already assigned values */

	'#', ".-...",		/* AS */
	'@', "...-.-",		/* SK */
	'*', "...-.",		/* VE */
	'%', "-...-.-",		/* BK */

	'\0', ""
};


static struct morsetab iso8859tab[] = {
	'á', ".--.-",
	'à', ".--.-",
	'â', ".--.-",
	'ä', ".-.-",
	'ç', "-.-..",
	'é', "..-..",
	'è', "..-..",
	'ê', "-..-.",
	'ö', "---.",
	'ü', "..--",

	'\0', ""
};

static struct morsetab koi8rtab[] = {
	/*
	 * the cyrillic alphabet; you'll need a KOI8R font in order
	 * to see the actual characters
	 */
	'Á', ".-",		/* a */
	'Â', "-...",		/* be */
	'×', ".--",		/* ve */
	'Ç', "--.",		/* ge */
	'Ä', "-..",		/* de */
	'Å', ".",		/* ye */
	'Ö', "...-",		/* she */
	'Ú', "--..",		/* ze */
	'É', "..",		/* i */
	'Ê', ".---",		/* i kratkoye */
	'Ë', "-.-",		/* ka */
	'Ì', ".-..",		/* el */
	'Í', "--",		/* em */
	'Î', "-.",		/* en */
	'Ï', "---",		/* o */
	'Ð', ".--.",		/* pe */
	'Ò', ".-.",		/* er */
	'Ó', "...",		/* es */
	'Ô', "-",		/* te */
	'Õ', "..-",		/* u */
	'Æ', "..-.",		/* ef */
	'È', "....",		/* kha */
	'Ã', "-.-.",		/* ce */
	'Þ', "---.",		/* che */
	'Û', "----",		/* sha */
	'Ý', "--.-",		/* shcha */
	'Ù', "-.--",		/* yi */
	'Ø', "-..-",		/* myakhkij znak */
	'Ü', "..-..",		/* ae */
	'À', "..--",		/* yu */
	'Ñ', ".-.-",		/* ya */

	'\0', ""
};

void            show(const char *), play(const char *), morse(char);

static int      pflag, sflag;
static int      wpm = 20;	/* words per minute */
#define FREQUENCY 600
static int      freq = FREQUENCY;

#ifdef SPEAKER
#define DASH_LEN 3
#define CHAR_SPACE 3
#define WORD_SPACE (7 - CHAR_SPACE - 1)
static float    dot_clock;
int             spkr;
tone_t          sound;
#endif

static struct morsetab *hightab = iso8859tab;

int
main(int argc, char **argv)
{
	extern char    *optarg;
	extern int      optind;
	register int    ch;
	register char  *p;

	while ((ch = getopt(argc, argv, "spw:f:")) != EOF)
		switch ((char) ch) {
		case 'f':
			freq = atoi(optarg);
			break;
		case 'p':
			pflag = 1;
			break;
		case 's':
			sflag = 1;
			break;
		case 'w':
			wpm = atoi(optarg);
			break;
		case '?':
		default:
			fputs("usage: morse [-s] [-p] [-w speed] [-f frequency] [string ...]\n", stderr);
			exit(1);
		}
	if (pflag && sflag) {
		fputs("morse: only one of -p and -s allowed\n", stderr);
		exit(1);
	}
	if (pflag && ((wpm < 1) || (wpm > 60))) {
		fputs("morse: insane speed\n", stderr);
		exit(1);
	}
	if (pflag && (freq == 0))
		freq = FREQUENCY;

	(void)setuid(getuid());
#ifdef SPEAKER
	if (pflag) {
		if ((spkr = open(SPEAKER, O_WRONLY, 0)) == -1) {
			perror(SPEAKER);
			exit(1);
		}
		dot_clock = wpm / 2.4;		/* dots/sec */
		dot_clock = 1 / dot_clock;	/* duration of a dot */
		dot_clock = dot_clock / 2;	/* dot_clock runs at twice */
						/* the dot rate */
		dot_clock = dot_clock * 100;	/* scale for ioctl */
	}
#endif
	argc -= optind;
	argv += optind;

	if((p = getenv("LC_CTYPE")) || (p = getenv("LANG"))) {
		if(strlen(p) >= strlen("KOI8-R") &&
		   strcasecmp(&p[strlen(p) - strlen("KOI8-R")], "KOI8-R") == 0)
			hightab = koi8rtab;
		setlocale(LC_CTYPE, p);
	} else {
		setlocale(LC_CTYPE, "");
	}

	if (*argv) {
		do {
			for (p = *argv; *p; ++p) {
				morse((int) *p);
			}
			morse((int) ' ');
		} while (*++argv);
	} else {
		while ((ch = getchar()) != EOF)
			morse(ch);
	}
	exit(0);
}

void
morse(char c)
{
	struct morsetab *m;

	if (isalpha(c))
		c = tolower(c);
	if ((c == '\r') || (c == '\n'))
		c = ' ';
	if (c == ' ') {
		if (pflag) {
			play(" ");
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
		switch ((int) *c) {
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
