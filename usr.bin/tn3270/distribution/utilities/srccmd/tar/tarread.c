/* tarread.c */
/* Copyright (c) 1985, by Carnegie-Mellon University */

#include <stdio.h>
#include <v2tov3.h>
#include <sys\types.h>
#include <sys\stat.h>
#include "tar.h"

char usage[] = "tarread: usage: tarread tx[vwz] tarfile\n";
union hblock hbuf;

int verbose = 0;
int confirm = 0;
int binary = 0;
char cmd;

main(argc, argv)
int argc;
char *argv[];
{
	FILE *fp;
	char *cp;

	if (argc != 3) {
		fprintf(stderr, usage);
		exit(1);
	}

	for (cp = argv[1]; *cp; cp++)
		switch (*cp) {
			case 't':
			case 'x':
				cmd = *cp;
				break;

			case 'v':
				verbose++;
				break;
			case 'z':
				binary++;
				break;
			case 'w':
				confirm++;
				break;
			default:
				fprintf(stderr, "tarread: unknown switch %c\n", *cp);
				fprintf(stderr, usage);
				exit(1);
		}

	if ((fp = fopen(argv[2], "rb")) == NULL) {
		fprintf(stderr, "tarrread: cannot open %s\n", argv[2]);
		exit(1);
	}

	for (;;) {
		if (fread(&hbuf, sizeof(hbuf), 1, fp) != 1) {
			perror("fread");
			exit(1);
		}
		if (!proc_file(fp))
			break;
	}
}


int proc_file(fp)
FILE *fp;
{
	char name[NAMSIZ];
	unsigned short mode;
	short uid, gid;
	long size, mtime;
	char c;
	int confrmd;
	long skip;

	if (hbuf.dbuf.name[0] == '\0')
		return (NULL);

	strcpy(name, hbuf.dbuf.name);
	if (sscanf(hbuf.dbuf.mode, "%o", &mode) != 1)
		fprintf("Couldn't read mode\n");
	if (sscanf(hbuf.dbuf.uid, "%o", &uid) != 1)
		fprintf("Couldn't read uid\n");
	if (sscanf(hbuf.dbuf.gid, "%o", &gid) != 1)
		fprintf("Couldn't read gid\n");
	if (sscanf(hbuf.dbuf.size, "%12lo %12lo", &size, &mtime) != 2)
		fprintf("Couldn't read size or mtime\n");

	skip = (size + TBLOCK - 1) / TBLOCK * TBLOCK;

	switch (cmd) {
		case 't':
			if (verbose)
				printf("%8o %d/%d\t %6ld %.24s %s\n", mode,
					uid, gid, size, ctime(&mtime), name);
			else
				printf("%s\n", name);

			break;

		case 'x':
			if (verbose)
				printf("x %s: ", name);
			confrmd = 1;

			if (confirm) {
				confrmd = 0;
				if ((c = getchar()) == 'y')
					confrmd++;
				while (c != '\n')
					c = getchar();
				if(!confrmd)
					break;
			}

			if(extract(name, size, mode, mtime, fp))
				skip = 0;
			
			if (verbose)
				printf("\n");
			break;
	}
	if (fseek(fp, skip, 1)) {
		perror("fseek");
		exit(1);
	}
	return (1);
}


int extract(fname, size, mode, mtime, ifp)
char *fname;
long size;
unsigned short mode;
long mtime;
FILE *ifp;
{
	FILE *ofp;
	char fbuf[TBLOCK];
	long copied, left;
	char *s, *np, *strchr();
	struct stat sbuf;

	for(np = fname; s = strchr(np, '/'); np = s+1) {
		*s = '\0';
		if(stat(fname, &sbuf)) {
			if(mkdir(fname))
				perror("mkdir");
		} else if(!(sbuf.st_mode & S_IFDIR)) {
			fprintf(stderr, "\n%s: Not a directory", fname);
			*s = '/';
			fprintf(stderr, "\ntar: %s - cannot create", fname);
			return (0);
		}
		*s = '/';
	}
	if(!*np)
		return (0);

	if (binary) {
		if ((ofp = fopen(fname, "wb")) == NULL) {
			perror("extract:");
			return (0);
		}
	} else {
		if ((ofp = fopen(fname, "w")) == NULL) {
			perror("extract:");
			return (0);
		}
	}
	
	for(copied = 0; copied < size; copied += TBLOCK) {
		if(fread(fbuf, TBLOCK, 1, ifp) != 1) {
			perror("fread");
			exit(1);
		}
		left = size - copied;
		if(fwrite(fbuf, (int)min(left, TBLOCK), 1, ofp) != 1) {
			perror("fwrite");
			exit(1);
		}
	}

	if(fclose(ofp)) {
		perror("fclose");
		exit(1);
	}

	/*
	 * Now, set modification time.
	 */
	{
#include <sys\utime.h>
	    struct utimbuf utim;

	    utim.modtime = mtime;

	    if (utime(fname, &utim) == -1) {
		perror("utime");
		exit(1);
	    }
	}

	return (1);
}
