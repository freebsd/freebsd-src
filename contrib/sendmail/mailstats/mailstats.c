/*
 * Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.
 *	All rights reserved.
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
"@(#) Copyright (c) 1998-2001 Sendmail, Inc. and its suppliers.\n\
	All rights reserved.\n\
     Copyright (c) 1988, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* ! lint */

#ifndef lint
static char id[] = "@(#)$Id: mailstats.c,v 8.53.16.13 2001/05/07 22:06:38 gshapiro Exp $";
#endif /* ! lint */

/* $FreeBSD$ */

#include <unistd.h>
#include <stddef.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#ifdef EX_OK
# undef EX_OK		/* unistd.h may have another use for this */
#endif /* EX_OK */
#include <sysexits.h>

#include <sendmail/sendmail.h>
#include <sendmail/mailstats.h>
#include <sendmail/pathnames.h>


#define MNAMELEN	20	/* max length of mailer name */


int
main(argc, argv)
	int argc;
	char **argv;
{
	register int i;
	int mno;
	int save_errno;
	int ch, fd;
	char *sfile;
	char *cfile;
	FILE *cfp;
	bool mnames;
	bool progmode;
	long frmsgs = 0, frbytes = 0, tomsgs = 0, tobytes = 0, rejmsgs = 0;
	long dismsgs = 0;
	time_t now;
	char mtable[MAXMAILERS][MNAMELEN + 1];
	char sfilebuf[MAXLINE];
	char buf[MAXLINE];
	struct statistics stats;
	extern char *ctime();
	extern char *optarg;
	extern int optind;


	cfile = _PATH_SENDMAILCF;
	sfile = NULL;
	mnames = TRUE;
	progmode = FALSE;
	while ((ch = getopt(argc, argv, "C:f:op")) != -1)
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

		  case 'p':
			progmode = TRUE;
			break;

		  case '?':
		  default:
  usage:
			(void) fputs("usage: mailstats [-C cffile] [-f stfile] [-o] [-p]\n",
				     stderr);
			exit(EX_USAGE);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 0)
		goto usage;

	if ((cfp = fopen(cfile, "r")) == NULL)
	{
		save_errno = errno;
		fprintf(stderr, "mailstats: ");
		errno = save_errno;
		perror(cfile);
		exit(EX_NOINPUT);
	}

	mno = 0;
	(void) strlcpy(mtable[mno++], "prog", MNAMELEN + 1);
	(void) strlcpy(mtable[mno++], "*file*", MNAMELEN + 1);
	(void) strlcpy(mtable[mno++], "*include*", MNAMELEN + 1);

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
			if (strlcpy(sfilebuf, b, sizeof sfilebuf) >=
			    sizeof sfilebuf)
			{
				fprintf(stderr,
					"StatusFile filename too long: %.30s...\n",
					b);
				exit(EX_CONFIG);
			}
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
		s = m + MNAMELEN;		/* is [MNAMELEN + 1] */
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

	fd = open(sfile, O_RDONLY);
	if ((fd < 0) || (i = read(fd, &stats, sizeof stats)) < 0)
	{
		save_errno = errno;
		(void) fputs("mailstats: ", stderr);
		errno = save_errno;
		perror(sfile);
		exit(EX_NOINPUT);
	}
	if (i == 0)
	{
		(void) sleep(1);
		if ((i = read(fd, &stats, sizeof stats)) < 0)
		{
			save_errno = errno;
			(void) fputs("mailstats: ", stderr);
			errno = save_errno;
			perror(sfile);
			exit(EX_NOINPUT);
		}
		else if (i == 0)
		{
			memset((ARBPTR_T) &stats, '\0', sizeof stats);
			(void) time(&stats.stat_itime);
		}
	}
	if (i != 0)
	{
		if (stats.stat_magic != STAT_MAGIC)
		{
			fprintf(stderr,
				"mailstats: incorrect magic number in %s\n",
				sfile);
			exit(EX_OSERR);
		}
		else if (stats.stat_version != STAT_VERSION)
		{
			fprintf(stderr,
				"mailstats version (%d) incompatible with %s version (%d)\n",
				STAT_VERSION, sfile, stats.stat_version);
			exit(EX_OSERR);
		}
		else if (i != sizeof stats || stats.stat_size != sizeof(stats))
		{
			(void) fputs("mailstats: file size changed.\n", stderr);
			exit(EX_OSERR);
		}
	}

	if (progmode)
	{
		(void) time(&now);
		printf("%ld %ld\n", (long) stats.stat_itime, (long) now);
	}
	else
	{
		printf("Statistics from %s", ctime(&stats.stat_itime));
		printf(" M   msgsfr  bytes_from   msgsto    bytes_to  msgsrej msgsdis%s\n",
			mnames ? "  Mailer" : "");
	}
	for (i = 0; i < MAXMAILERS; i++)
	{
		if (stats.stat_nf[i] || stats.stat_nt[i] ||
		    stats.stat_nr[i] || stats.stat_nd[i])
		{
			char *format;

			if (progmode)
				format = "%2d %8ld %10ld %8ld %10ld   %6ld  %6ld";
			else
				format = "%2d %8ld %10ldK %8ld %10ldK   %6ld  %6ld";
			printf(format, i,
			    stats.stat_nf[i], stats.stat_bf[i],
			    stats.stat_nt[i], stats.stat_bt[i],
			    stats.stat_nr[i], stats.stat_nd[i]);
			if (mnames)
				printf("  %s", mtable[i]);
			printf("\n");
			frmsgs += stats.stat_nf[i];
			frbytes += stats.stat_bf[i];
			tomsgs += stats.stat_nt[i];
			tobytes += stats.stat_bt[i];
			rejmsgs += stats.stat_nr[i];
			dismsgs += stats.stat_nd[i];
		}
	}
	if (progmode)
	{
		printf(" T %8ld %10ld %8ld %10ld   %6ld  %6ld\n",
		       frmsgs, frbytes, tomsgs, tobytes, rejmsgs, dismsgs);
		printf(" C %8ld %8ld %6ld\n",
		       stats.stat_cf, stats.stat_ct, stats.stat_cr);
		(void) close(fd);
		fd = open(sfile, O_RDWR | O_TRUNC);
		if (fd >= 0)
			(void) close(fd);
	}
	else
	{
		printf("=============================================================\n");
		printf(" T %8ld %10ldK %8ld %10ldK   %6ld  %6ld\n",
			frmsgs, frbytes, tomsgs, tobytes, rejmsgs, dismsgs);
		printf(" C %8ld %10s  %8ld %10s    %6ld\n",
		       stats.stat_cf, "", stats.stat_ct, "", stats.stat_cr);
	}
	exit(EX_OK);
	/* NOTREACHED */
	return EX_OK;
}
