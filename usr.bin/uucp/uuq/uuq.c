/*-
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)uuq.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

/*
 * uuq - looks at uucp queues
 *
 * Lou Salkind
 * New York University
 *
 */

#include "uucp.h"
#include <stdio.h>

#ifdef NDIR
#include "libndir/ndir.h"
#else !NDIR
#include <sys/dir.h>
#endif !NDIR
#include <sys/stat.h>

#define	NOSYS		(struct sys *)0

#define W_TYPE		wrkvec[0]
#define W_FILE1		wrkvec[1]
#define W_FILE2		wrkvec[2]
#define W_USER		wrkvec[3]
#define W_OPTNS		wrkvec[4]
#define W_DFILE		wrkvec[5]
#define W_MODE		wrkvec[6]
#define WSUFSIZE 5	/* work file name suffix size */

struct sys {
	char	s_name[8];
	int	s_njobs;
	off_t	s_bytes;
	struct job	*s_jobp;
	struct sys	*s_sysp;
};

struct job {
	int	j_files;
	int	j_flags;
	char	j_jobno[WSUFSIZE];
	char	j_user[22];
	char	j_fname[128];
	char	j_grade;
	off_t	j_bytes;
	time_t	j_date;
	struct job	*j_jobp;
};

struct sys *syshead;
struct sys *getsys();
int jcompare();
char *sysname;
char *user;
char *rmjob;
int hflag;
int lflag;

char *malloc(), *calloc();
double atof();
float baudrate = 2400.;
char Username[BUFSIZ];
char Filename[BUFSIZ];
int Maxulen = 0;
struct timeb Now;

main(argc, argv)
int argc;
char **argv;
{
	register int i;
	register struct sys *sp;
	register struct job *jp;
	struct job **sortjob;
	int nsys;
	extern char *optarg;
	extern int optind;

	strcpy(Progname, "uuq");
	uucpname(Myname);

	while ((i = getopt(argc, argv, "r:S:s:u:d:b:hl")) != EOF)
		switch (i) {
		case 'r':
		case 'S':
			Spool = optarg;
			break;
		case 's':
			sysname = optarg;
			if (strlen(sysname) > SYSNSIZE)
				sysname[SYSNSIZE] = '\0';
			break;
		case 'u':
			user = optarg;
			break;
		case 'd':
			rmjob = optarg;
			break;
		case 'b':
			baudrate = atof(optarg);
			break;
		case 'h':
			hflag++;
			break;
		case 'l':
			lflag++;
			break;
		default:
			fprintf(stderr,
	"usage: uuq [-l] [-h] [-ssystem] [-uuser] [-djobno] [-rspool] [-bbaudrate]\n");
			exit(0);
		}

	subchdir(Spool);
	baudrate *= 0.7;	/* reduce speed because of protocol overhead */
	baudrate *= 7.5; 	/* convert to chars/minute (60/8) */
	gather();
	nsys = 0;
	for (sp = syshead; sp; sp = sp->s_sysp) {
		if (sp->s_njobs == 0)
			continue;
		if (!hflag && nsys++ > 0)
			putchar('\n');
		printf("%s: %d %s", sp->s_name,
			sp->s_njobs, sp->s_njobs > 1 ? "jobs" : "job");
		if (lflag) {
			float minutes;
			int hours;
			/* The 80 * njobs is because of the uucp handshaking */
			minutes = (float)(sp->s_bytes + 80 * sp->s_njobs)/baudrate;
			hours = minutes/60;
			printf(", %ld bytes, ", sp->s_bytes);
			if (minutes > 60){
				printf("%d hour%s, ",hours,
					hours > 1 ? "s": "");
				minutes -= 60 * hours;
			}
			printf("%3.1f minutes (@ effective baudrate of %d)",
				minutes,(int)(baudrate/6));
		}
		putchar('\n');
		if (hflag)
			continue;
		/* sort them babies! */
		sortjob = (struct job **)calloc(sp->s_njobs, sizeof (struct job *));
		for (i=0, jp=sp->s_jobp; i < sp->s_njobs; i++, jp=jp->j_jobp)
			sortjob[i] = jp;
		qsort(sortjob, sp->s_njobs, sizeof (struct job *), jcompare);
		for (i = 0; i < sp->s_njobs; i++) {
			jp = sortjob[i];
			if (lflag) {
				printf("%s %2d %-*s%7ld%5.1f %-12.12s %c %.*s\n",
	jp->j_jobno, jp->j_files, Maxulen, jp->j_user, jp->j_bytes, jp->j_bytes/baudrate,
	ctime(&jp->j_date) + 4, jp->j_flags, sizeof (jp->j_fname), jp->j_fname
				);
			} else {
				printf("%s", jp->j_jobno);
				putchar((i+1)%10 ? '\t' : '\n');
			}
			/* There's no need to keep the force poll if jobs > 1*/
			if (sp->s_njobs > 1 && strcmp("POLL", jp->j_jobno)==0) {
				char pbuf[BUFSIZ];
				sprintf(pbuf,"%s/%c.%s%cPOLL",
					subdir(Spool, CMDPRE), CMDPRE,
					sp->s_name, jp->j_grade);
				(void) unlink(pbuf);
			}
		}
		if (!lflag && (sp->s_njobs%10))
			putchar('\n');
	}
	exit(0);
}

jcompare(j1, j2)
struct job **j1, **j2;
{
	int delta;

	delta = (*j1)->j_grade - (*j2)->j_grade;
	if (delta)
		return delta;
	return(strcmp((*j1)->j_jobno,(*j2)->j_jobno));
}

/*
 * Get all the command file names
 */
gather()
{
	struct direct *d;
	DIR *df;

	/*
	 * Find all the spool files in the spooling directory
	 */
	if ((df = opendir(subdir(Spool, CMDPRE))) == NULL) {
		fprintf(stderr, "can't examine spooling area\n");
		exit(1);
	}
	for (;;) {
		if ((d = readdir(df)) == NULL)
			break;
		if (d->d_namlen <= 2 || d->d_name[0] != CMDPRE ||
		    d->d_name[1] != '.')
			continue;
		if (analjob(d->d_name) < 0) {
			fprintf(stderr, "out of memory\n");
			break;
		}
	}
	closedir(df);
}

/*
 * analjob does the grunge work of verifying jobs
 */
#include <pwd.h>
analjob(filename)
char *filename;
{
	struct job *jp;
	struct sys *sp;
	char sbuf[MAXNAMLEN+1], str[256], nbuf[256];
	char  *jptr, *wrkvec[20];
	char grade;
	FILE *fp, *df;
	struct stat statb;
	int files, gotname, i;
	off_t bytes;

	strncpy(sbuf, filename, MAXNAMLEN);
	sbuf[MAXNAMLEN] = '\0';
	jptr = sbuf + strlen(sbuf) - WSUFSIZE;
	grade = *jptr;
	*jptr++ = 0;
	/*
	 * sbuf+2 now points to sysname name (null terminated)
	 * jptr now points to job number (null terminated)
	 */
	if (rmjob) {
		if (strcmp(rmjob, jptr))
			return(0);
	} else {
		if ((sp = getsys(sbuf+2)) == NOSYS)
			return(0);
		if (!lflag) {
			/* SHOULD USE A SMALLER STRUCTURE HERE */
			jp = (struct job *)malloc(sizeof(struct job));
			if (jp == (struct job *)0)
				return(-1);
			strcpy(jp->j_jobno, jptr);
			jp->j_jobp = sp->s_jobp;
			jp->j_grade = grade;
			sp->s_jobp = jp;
			sp->s_njobs++;
			return(1);
		}
	}
	if ((fp = fopen(subfile(filename), "r")) == NULL) {
		perror(subfile(filename));
		return(0);
	}
	files = 0;
	bytes = 0;
	gotname = 0;
	while (fgets(str, sizeof str, fp)) {
		if (getargs(str, wrkvec, 20) <= 0)
			continue;
		if (rmjob) {
			int myuid;
			struct passwd *pw;
			/*
			 * Make sure person who is removing data files is
			 * the person who created it or root.
			 */
			myuid = getuid();
			pw = getpwnam(W_USER);
			if (myuid && (pw == NULL || myuid != pw->pw_uid)) {
				fprintf(stderr, "Permission denied.\n");
				exit(1);
			}
			if (W_TYPE[0] == 'S' && !index(W_OPTNS, 'c')) {
				unlink(subfile(W_DFILE));
				fprintf(stderr, "Removing data file %s\n", W_DFILE);
			}
			continue;
		}
		if (user && (W_TYPE[0] == 'X' || !prefix(user, W_USER))) {
			fclose(fp);
			return(0);
		}
		files++;
		if (W_TYPE[0] == 'S') {
			if (strcmp(W_DFILE, "D.0") &&
			    stat(subfile(W_DFILE), &statb) >= 0)
				bytes += statb.st_size;
			else if (stat(subfile(W_FILE1), &statb) >= 0)
				bytes += statb.st_size;
		}
		/* amusing heuristic */
#define	isXfile(s)	(s[0]=='D' && s[strlen(s)-WSUFSIZE]=='X')
		if (gotname == 0 && isXfile(W_FILE1)) {
			if ((df = fopen(subfile(W_FILE1), "r")) == NULL)
				continue;
			while (fgets(nbuf, sizeof nbuf, df)) {
				nbuf[strlen(nbuf) - 1] = '\0';
				if (nbuf[0] == 'C' && nbuf[1] == ' ') {
					strcpy(Filename, nbuf+2);
					gotname++;
				} else if (nbuf[0] == 'R' && nbuf[1] == ' ') {
					register char *p, *q, *r;
					r = q = p = nbuf+2;
					do {
						if (*p == '!' || *p == '@'){
							r = q;
							q = p+1;
						}
					} while (*p++);

					strcpy(Username, r);
					W_USER = Username;
				}
			}
			fclose(df);
		}
	}
	fclose(fp);
	if (rmjob) {
		unlink(subfile(filename));
		fprintf(stderr, "Removing command file %s\n", filename);
		exit(0);
	}
	if (files == 0) {
		static char *wtype = "X";
		static char *wfile = "forced poll";
		if (strcmp("POLL", &filename[strlen(filename)-4])) {
			fprintf(stderr, "%.14s: empty command file\n", filename);
			return(0);
		}
		W_TYPE = wtype;
		W_FILE1 = wfile;
	}
	jp = (struct job *)malloc(sizeof(struct job));
	if (jp == (struct job *)0)
		return(-1);
	strcpy(jp->j_jobno, jptr);
	jp->j_files = files;
	jp->j_bytes = bytes;
	jp->j_grade = grade;
	jp->j_flags = W_TYPE[0];
	strncpy(jp->j_user, W_TYPE[0]=='X' ? "---" : W_USER, 20 );
	jp->j_user[20] = '\0';
	i = strlen(jp->j_user);
	if (i > Maxulen)
		Maxulen = i;
	/* SHOULD ADD ALL INFORMATION IN THE WHILE LOOP */
	if (gotname)
		strncpy(jp->j_fname, Filename, sizeof jp->j_fname);
	else
		strncpy(jp->j_fname, W_FILE1, sizeof jp->j_fname);
	stat(subfile(filename), &statb);
	jp->j_date = statb.st_mtime;
	jp->j_jobp = sp->s_jobp;
	sp->s_jobp = jp;
	sp->s_njobs++;
	sp->s_bytes += jp->j_bytes;
	return(1);
}

struct sys *
getsys(s)
register char *s;
{
	register struct sys *sp;

	for (sp = syshead; sp; sp = sp->s_sysp)
		if (strcmp(s, sp->s_name) == 0)
			return(sp);
	if (sysname && !prefix(sysname, s))
		return(NOSYS);
	sp = (struct sys *)malloc(sizeof(struct sys));
	if (sp == NOSYS)
		return(NOSYS);
	strcpy(sp->s_name, s);
	sp->s_njobs = 0;
	sp->s_jobp = (struct job *)0;
	sp->s_sysp = syshead;
	sp->s_bytes = 0;
	syshead = sp;
	return(sp);
}
