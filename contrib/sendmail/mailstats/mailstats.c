/*
 * Copyright (c) 1998 Sendmail, Inc.  All rights reserved.
 * Copyright (c) 1983 Eric P. Allman.  All rights reserved.
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the sendmail distribution.
 *
 *
 */

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)mailstats.c	8.28 (Berkeley) 9/14/1998";
#endif /* not lint */

#ifndef NOT_SENDMAIL
# define NOT_SENDMAIL
#endif
#include <sendmail.h>
#include <mailstats.h>
#include <pathnames.h>

#define MNAMELEN	20	/* max length of mailer name */

int
main(argc, argv)
	int argc;
	char **argv;
{
	extern char *optarg;
	extern int optind;
	struct statistics stat;
	register int i;
	int mno;
	int ch, fd;
	char *sfile;
	char *cfile;
	FILE *cfp;
	bool mnames;
	bool progmode;
	long frmsgs = 0, frbytes = 0, tomsgs = 0, tobytes = 0, rejmsgs = 0;
	long dismsgs = 0;
	char mtable[MAXMAILERS][MNAMELEN+1];
	char sfilebuf[MAXLINE];
	char buf[MAXLINE];
	time_t now;
	extern char *ctime();

	cfile = _PATH_SENDMAILCF;
	sfile = NULL;
	mnames = TRUE;
	progmode = FALSE;
	while ((ch = getopt(argc, argv, "C:f:op")) != EOF)
	{
		switch (ch)
		{
		  case 'C':
			cfile = optarg;
			break;

		  case 'f':
			sfile = optarg;
			break;

		  case 'o':
			mnames = FALSE;
			break;

#if _FFR_MAILSTATS_PROGMODE
		  case 'p':
			progmode = TRUE;
			break;
#endif

		  case '?':
		  default:
  usage:
#if _FFR_MAILSTATS_PROGMODE
			fputs("usage: mailstats [-C cffile] [-f stfile] -o -p\n",
				stderr);
#else
			fputs("usage: mailstats [-C cffile] [-f stfile] -o \n",
				stderr);
#endif
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		goto usage;

	if ((cfp = fopen(cfile, "r")) == NULL)
	{
		fprintf(stderr, "mailstats: ");
		perror(cfile);
		exit(EX_NOINPUT);
	}

	mno = 0;
	(void) strcpy(mtable[mno++], "prog");
	(void) strcpy(mtable[mno++], "*file*");
	(void) strcpy(mtable[mno++], "*include*");

	while (fgets(buf, sizeof(buf), cfp) != NULL)
	{
		register char *b;
		char *s;
		register char *m;

		b = buf;
		switch (*b++)
		{
		  case 'M':		/* mailer definition */
			break;

		  case 'O':		/* option -- see if .st file */
			if (strncasecmp(b, " StatusFile", 11) == 0 &&
			    !(isascii(b[11]) && isalnum(b[11])))
			{
				/* new form -- find value */
				b = strchr(b, '=');
				if (b == NULL)
					continue;
				while (isascii(*++b) && isspace(*b))
					continue;
			}
			else if (*b++ != 'S')
			{
				/* something else boring */
				continue;
			}

			/* this is the S or StatusFile option -- save it */
			if (strlen(b) >= sizeof sfilebuf)
			{
				fprintf(stderr,
					"StatusFile filename too long: %.30s...\n",
					b);
				exit(EX_CONFIG);
			}
			strcpy(sfilebuf, b);
			b = strchr(sfilebuf, '#');
			if (b == NULL)
				b = strchr(sfilebuf, '\n');
			if (b == NULL)
				b = &sfilebuf[strlen(sfilebuf)];
			while (isascii(*--b) && isspace(*b))
				continue;
			*++b = '\0';
			if (sfile == NULL)
				sfile = sfilebuf;

		  default:
			continue;
		}

		if (mno >= MAXMAILERS)
		{
			fprintf(stderr,
				"Too many mailers defined, %d max.\n",
				MAXMAILERS);
			exit(EX_SOFTWARE);
		}
		m = mtable[mno];
		s = m + MNAMELEN;		/* is [MNAMELEN+1] */
		while (*b != ',' && !(isascii(*b) && isspace(*b)) &&
		       *b != '\0' && m < s)
			*m++ = *b++;
		*m = '\0';
		for (i = 0; i < mno; i++)
		{
			if (strcmp(mtable[i], mtable[mno]) == 0)
				break;
		}
		if (i == mno)
			mno++;
	}
	(void) fclose(cfp);
	for (; mno < MAXMAILERS; mno++)
		mtable[mno][0]='\0';

	if (sfile == NULL)
	{
		fprintf(stderr, "mailstats: no statistics file located\n");
		exit (EX_OSFILE);
	}

	if ((fd = open(sfile, O_RDONLY)) < 0 ||
	    (i = read(fd, &stat, sizeof stat)) < 0)
	{
		fputs("mailstats: ", stderr);
		perror(sfile);
		exit(EX_NOINPUT);
	}
	if (i == 0)
	{
		sleep(1);
		if ((i = read(fd, &stat, sizeof stat)) < 0)
		{
			fputs("mailstats: ", stderr);
			perror(sfile);
			exit(EX_NOINPUT);
		}
		else if (i == 0)
		{
			bzero((ARBPTR_T) &stat, sizeof stat);
			(void) time(&stat.stat_itime);
		}
	}
	if (i != 0)
	{
		if (stat.stat_magic != STAT_MAGIC)
		{
			fprintf(stderr,
				"mailstats: incorrect magic number in %s\n",
				sfile);
			exit(EX_OSERR);
		}
		else if (stat.stat_version != STAT_VERSION)
		{
			fprintf(stderr,
				"mailstats version (%d) incompatible with %s version(%d)\n",
				STAT_VERSION, sfile, stat.stat_version);
			exit(EX_OSERR);
		}
		else if (i != sizeof stat || stat.stat_size != sizeof(stat))
		{
			fputs("mailstats: file size changed.\n", stderr);
			exit(EX_OSERR);
		}
	}

	if (progmode)
	{
		time(&now);
		printf("%ld %ld\n", (long) stat.stat_itime, (long) now);
	}
	else
	{
		printf("Statistics from %s", ctime(&stat.stat_itime));
		printf(" M   msgsfr  bytes_from   msgsto    bytes_to  msgsrej msgsdis%s\n",
			mnames ? "  Mailer" : "");
	}
	for (i = 0; i < MAXMAILERS; i++)
	{
		if (stat.stat_nf[i] || stat.stat_nt[i] ||
		    stat.stat_nr[i] || stat.stat_nd[i])
		{
			char *format;

			if (progmode)
				format = "%2d %8ld %10ld %8ld %10ld   %6ld  %6ld";
			else
				format = "%2d %8ld %10ldK %8ld %10ldK   %6ld  %6ld";
			printf(format, i,
			    stat.stat_nf[i], stat.stat_bf[i],
			    stat.stat_nt[i], stat.stat_bt[i],
			    stat.stat_nr[i], stat.stat_nd[i]);
			if (mnames)
				printf("  %s", mtable[i]);
			printf("\n");
			frmsgs += stat.stat_nf[i];
			frbytes += stat.stat_bf[i];
			tomsgs += stat.stat_nt[i];
			tobytes += stat.stat_bt[i];
			rejmsgs += stat.stat_nr[i];
			dismsgs += stat.stat_nd[i];
		}
	}
	if (progmode)
	{
		printf(" T %8ld %10ld %8ld %10ld   %6ld  %6ld\n",
		       frmsgs, frbytes, tomsgs, tobytes, rejmsgs, dismsgs);
		close(fd);
		fd = open(sfile, O_RDWR | O_TRUNC);
		if (fd > 0)
			close(fd);
	}
	else
	{
		printf("=============================================================\n");
		printf(" T %8ld %10ldK %8ld %10ldK   %6ld  %6ld\n",
			frmsgs, frbytes, tomsgs, tobytes, rejmsgs, dismsgs);
	}
	exit(EX_OK);
}
