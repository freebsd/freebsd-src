/*
 * Copyright (c) 1989, 1993, 1994
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
 *
 * $FreeBSD$
 */


extern struct passwd *pw;
extern int doall;
extern struct iovec header[];
extern struct tm *tp;
extern const char *calendarFile;
extern char *optarg;

void	 cal(void);
void	 closecal(FILE *);
int	 getday(char *);
int	 getdayvar(char *);
int	 getfield(char *, char **, int *);
int	 getmonth(char *);
int	 geteaster(char *, int);
int      getpaskha(char *, int);
int      easter(int);
int	 isnow(char *, int *, int *, int *);
FILE	*opencal(void);
void	 settime(time_t);
time_t   Mktime(char *);
void	 usage(void);
void     setnnames(void);

#define isleap(y) ((((y) % 4) == 0 && ((y) % 100) != 0) || ((y) % 400) == 0)

/* some flags */
#define	F_ISMONTH	0x01 /* month (Januar ...) */
#define	F_ISDAY		0x02 /* day of week (Sun, Mon, ...) */
#define	F_ISDAYVAR	0x04 /* variables day of week, like SundayLast */
#define	F_EASTER	0x08 /* Easter or easter depending days */

extern int f_dayAfter; /* days after current date */
extern int f_dayBefore; /* days bevore current date */
extern int Friday;	/* day before weekend */

struct fixs {
	char *name;
	int len;
};

