/*
    YPS-0.2, NIS-Server for Linux
    Copyright (C) 1994  Tobias Reber

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    Modified for use with FreeBSD 2.x by Bill Paul (wpaul@ctr.columbia.edu)
*/

/*
 * $Id: yp_mkdb.c,v 1.3 1995/05/30 05:05:26 rgrimes Exp $
 */

#define BUFFERSIZE 4096

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <netdb.h>
#include <sys/types.h>
#include <db.h>
#include <limits.h>

#define PERM_SECURE (S_IRUSR|S_IWUSR)
HASHINFO openinfo = {
	4096,		/* bsize */
	32,		/* ffactor */
	256,		/* nelem */
	2048 * 1024,	/* cachesize */
	NULL,		/* hash */
	0		/* lorder */
};

extern int optind;
extern char *optarg;

static char *DomainName=NULL;
static char *InputFileName=NULL;
static char *OutputFileName=NULL;
static char *MasterName=NULL;
static char thisHost[MAXHOSTNAMELEN+1];

static void
unLoad(char *DbName)
{
	DB *dp;
	DBT key, data;
	int flag = R_FIRST;

	if ((dp = dbopen(DbName,O_RDONLY|O_EXCL, PERM_SECURE,
				DB_HASH, &openinfo)) == NULL) {
		fprintf(stderr, "%s: Cannot open\n", DbName);
		perror(DbName);
		exit(1);
	}

	while (!(dp->seq) (dp, &key, &data, flag)) {
		if (!data.data) {
			fprintf(stderr, "Error\n");
			perror(DbName);
			exit (1);
		}
		flag = R_NEXT;
		fwrite(key.data, key.size, 1, stdout);
		putc(' ', stdout);
		fwrite(data.data, data.size, 1, stdout);
		putc('\n', stdout);
	}
	(void)(dp->close) (dp);
}


static void
load( char *FileName, char *DbName)
{
	static char Buffer[BUFFERSIZE];
	static char filename[1024], filename2[1024];
	FILE *infile;
	DB *dp;
	DBT key, data;

	infile=strcmp(FileName, "-")?fopen(FileName, "r"):stdin;
	if (infile==NULL) {
		fprintf(stderr, "%s: Cannot open\n", FileName);
		exit(1);
	}

	sprintf(filename, "%s~.db", DbName);

	if ((dp = dbopen(DbName,O_RDWR|O_EXCL|O_CREAT, PERM_SECURE,
				DB_HASH, &openinfo)) == NULL) {
		perror("dbopen");
		fprintf(stderr, "%s: Cannot open\n", filename);
		exit(1);
	}

	if (MasterName && *MasterName) {
		key.data="YP_MASTER_NAME"; key.size=strlen(key.data);
		data.data=MasterName; data.size=strlen(MasterName);
		(dp->put)(dp,&key,&data,0);
	}

	if (DomainName && *DomainName) {
		key.data="YP_DOMAIN_NAME"; key.size=strlen(key.data);
		data.data=DomainName; data.size=strlen(DomainName);
		(dp->put)(dp,&key,&data,0);
	}

	if (InputFileName && *InputFileName) {
		key.data="YP_INPUT_NAME"; key.size=strlen(key.data);
		data.data=InputFileName; data.size=strlen(InputFileName);
		(dp->put)(dp,&key,&data,0);
	}

	if (OutputFileName && *OutputFileName) {
		key.data="YP_OUTPUT_NAME"; key.size=strlen(key.data);
		data.data=OutputFileName; data.size=strlen(OutputFileName);
		(dp->put)(dp,&key,&data,0);
	}

	{
		char OrderNum[12];
		struct timeval tv;
		struct timezone tz;
		gettimeofday(&tv, &tz);
		sprintf(OrderNum, "%ld", tv.tv_sec);
		key.data="YP_LAST_MODIFIED"; key.size=strlen(key.data);
		data.data=OrderNum; data.size=strlen(OrderNum);
		(dp->put)(dp,&key,&data,0);
	}

	for(;;) {
		register int r;

		fgets(Buffer, BUFFERSIZE, infile);
		if (feof(infile)) break;
		if (Buffer[0] == '+' || Buffer[0] == '-') break;
		r=strlen(Buffer)-1;
		if (Buffer[r]!='\n' && r>=BUFFERSIZE) {
			fprintf(stderr, "%s: Buffer overflow\n", FileName);
			exit(1);
		} else
			Buffer[r]='\0';

		for (r=0; Buffer[r]; r++) {
			if (Buffer[r]==' ' || Buffer[r]=='\t') {
				Buffer[r]='\0';
				r++;
				break;
			}
		}
		for (; Buffer[r]; r++)
			if (Buffer[r]!=' ' && Buffer[r]!='\t') break;

		if (Buffer[r] == '+' || Buffer[r] == '-') break;

		key.data=Buffer; key.size=strlen(Buffer);
		data.data=Buffer+r; data.size=strlen(Buffer+r);
		(dp->put)(dp,&key,&data,0);
	}
	(void)(dp->close)(dp);

	sprintf(filename, "%s.db", DbName);
	sprintf(filename2, "%s~.db", DbName);
	unlink(filename);
	rename(filename2, filename);
}

static void
Usage( void)
{
	fprintf(stderr, "usage: yp_mkdb -u dbname\n");
	fprintf(stderr, "       yp_mkdb [-i inputfile] [-o outputfile]\n");
	fprintf(stderr, "               [-m mastername] inputfile dbname\n");
	exit(1);
}

void
main(int argc, char **argv)
{
	int UFlag=0;

	while(1) {
		int c=getopt(argc, argv, "ui:o:m:d:");
		if (c==EOF) break;
		switch (c) {
		case 'u':
			UFlag++;
			break;
		case 'd':
			DomainName=optarg;
			break;
		case 'i':
			InputFileName=optarg;
			break;
		case 'o':
			OutputFileName=optarg;
			break;
		case 'm':
			MasterName=optarg;
			break;
		case '?':
			Usage();
			break;
		}
	}
	argc-=optind;
	argv+=optind;

	if (!MasterName) {
		if (gethostname(thisHost, sizeof thisHost)<0) {
			perror("gethostname");
		} else {
			struct hostent *h;
			h=gethostbyname(thisHost);
			if (h) strncpy(thisHost, h->h_name, sizeof thisHost);
			MasterName=thisHost;
		}
	}
	if (UFlag) {
		if (argc<1) Usage();
		unLoad(argv[0]);
	} else {
		if (argc<2) Usage();
		load(argv[0], argv[1]);
	}
	exit(0);
}

