/*-
 * Copyright (c) 2000,2001 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <sys/param.h>

#define BLOCKS	16

void cleanup(int);
void write_file(const char *, int);
void usage(const char *);

static int fd, quiet, saved_block_size;
static struct cdr_track track;

int
main(int argc, char **argv)
{
	int ch, arg, addr;
	int eject=0, list=0, multi=0, preemp=0, speed=1, test_write=0;
	char *devname = "/dev/acd0c", *prog_name;
	int block_size = 0;

	prog_name = argv[0];
	while ((ch = getopt(argc, argv, "ef:lmpqs:t")) != -1) {
		switch (ch) {
		case 'e':
			eject = 1;
			break;

		case 'f':
			devname = optarg;
			break;

		case 'l':
			list = 1;
			break;

		case 'm':
			multi = 1;
			break;

		case 'p':
			preemp = 1;
			break;

		case 'q':
			quiet = 1;
			break;

		case 's':
			speed = atoi(optarg);
			if (speed <= 0)
				errx(EX_USAGE, "Invalid speed: %s", optarg);
			break;

		case 't':
			test_write = 1;
			break;

		default: 
			usage(prog_name);
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage(prog_name);

	if ((fd = open(devname, O_RDWR, 0)) < 0)
		err(EX_NOINPUT, "open(%s)", devname);

	if (ioctl(fd, CDRIOCWRITESPEED, &speed) < 0) 
       		err(EX_IOERR, "ioctl(CDRIOCWRITESPEED)");

	if (ioctl(fd, CDRIOCGETBLOCKSIZE, &saved_block_size) < 0) 
       		err(EX_IOERR, "ioctl(CDRIOCGETBLOCKSIZE)");

	err_set_exit(cleanup);

	for (arg = 0; arg < argc; arg++) {
		if (!strcmp(argv[arg], "fixate")) {
			if (!quiet)
				fprintf(stderr, "fixating CD, please wait..\n");
			if (ioctl(fd, CDRIOCCLOSEDISK, &multi) < 0)
        			err(EX_IOERR, "ioctl(CDRIOCCLOSEDISK)");
			break;
		}
		if (!strcmp(argv[arg], "msinfo")) {
		        struct ioc_read_toc_single_entry entry;
			struct ioc_toc_header header;

			if (ioctl(fd, CDIOREADTOCHEADER, &header) < 0)
				err(EX_IOERR, "ioctl(CDIOREADTOCHEADER)");
			bzero(&entry, sizeof(struct ioc_read_toc_single_entry));
			entry.address_format = CD_LBA_FORMAT;
			entry.track = header.ending_track;
			if (ioctl(fd, CDIOREADTOCENTRY, &entry) < 0) 
				err(EX_IOERR, "ioctl(CDIOREADTOCENTRY)");
			if (ioctl(fd, CDRIOCNEXTWRITEABLEADDR, &addr) < 0) 
				err(EX_IOERR, "ioctl(CDRIOCNEXTWRITEABLEADDR)");
			fprintf(stderr, "%d, %d\n", 
				ntohl(entry.entry.addr.lba), addr);
			break;
		}
		if (!strcmp(argv[arg], "erase") || !strcmp(argv[arg], "blank")){
		    	int error, blank;
			if (!quiet)
				fprintf(stderr, "%sing CD, please wait..\n",
					argv[arg]);
			if (!strcmp(argv[arg], "erase"))
				blank = CDR_B_ALL;
			else
				blank = CDR_B_MIN;

			if (ioctl(fd, CDRIOCBLANK, &blank) < 0)
        			err(EX_IOERR, "ioctl(CDRIOCBLANK)");
			continue;
		}
		if (!strcmp(argv[arg], "audio") || !strcmp(argv[arg], "raw")) {
			track.test_write = test_write;
			track.datablock_type = CDR_DB_RAW;
			track.preemp = preemp;
			block_size = 2352;
			continue;
		}
		if (!strcmp(argv[arg], "data") || !strcmp(argv[arg], "mode1")) {
			track.test_write = test_write;
			track.datablock_type = CDR_DB_ROM_MODE1;
			track.preemp = 0;
			block_size = 2048;
			continue;
		}
		if (!strcmp(argv[arg], "mode2")) {
			track.test_write = test_write;
			track.datablock_type = CDR_DB_ROM_MODE2;
			track.preemp = 0;
			block_size = 2336;
			continue;
		}
		if (!strcmp(argv[arg], "XAmode1")) {
			track.test_write = test_write;
			track.datablock_type = CDR_DB_XA_MODE1;
			track.preemp = 0;
			block_size = 2048;
			continue;
		}
		if (!block_size)
			err(EX_NOINPUT, "no data format selected");
		if (list) {
			char file_buf[MAXPATHLEN + 1], *eol;
			FILE *fp;

			if ((fp = fopen(argv[arg], "r")) == NULL)
	 			err(EX_NOINPUT, "fopen(%s)", argv[arg]);

			while (fgets(file_buf, sizeof(file_buf), fp) != NULL) {
				if (*file_buf == '#' || *file_buf == '\n')
					continue;
				if (eol = strchr(file_buf, '\n'))
					*eol = NULL;
				write_file(file_buf, block_size);
			}
			if (feof(fp))
				fclose(fp);
			else
				err(EX_IOERR, "fgets(%s)", file_buf);
		}
		else
			write_file(argv[arg], block_size);
	}

	if (ioctl(fd, CDRIOCSETBLOCKSIZE, &saved_block_size) < 0) {
		err_set_exit(NULL);
		err(EX_IOERR, "ioctl(CDRIOCSETBLOCKSIZE)");
	}

	if (eject)
		if (ioctl(fd, CDIOCEJECT) < 0)
			err(EX_IOERR, "ioctl(CDIOCEJECT)");
	close(fd);
	exit(EX_OK);
}

void
cleanup(int dummy)
{
	if (ioctl(fd, CDRIOCSETBLOCKSIZE, &saved_block_size) < 0) 
		err(EX_IOERR, "ioctl(CDRIOCSETBLOCKSIZE)");
}

void
usage(const char *prog_name)
{
	fprintf(stderr, "Usage: %s [-f device] [-s speed] [-e] [-l] [-m] [-p]\n"
		"\t[-q] [command] [command filename...]\n", prog_name);
	exit(EX_USAGE);
}

void
write_file(const char *name, int block_size)
{
	int addr, count, file, filesize, size;
	char buf[2352*BLOCKS];
	struct stat stat;
	static int cdopen, done_stdin, tot_size = 0;

	if (!strcmp(name, "-")) {
		if (done_stdin) {
			warn("skipping multiple usages of stdin");
			return;
		}
		file = STDIN_FILENO;
		done_stdin = 1;
	}
	else if ((file = open(name, O_RDONLY, 0)) < 0)
		err(EX_NOINPUT, "open(%s)", name);

	if (!cdopen) {
		if (ioctl(fd, CDRIOCOPENDISK) < 0)
			err(EX_IOERR, "ioctl(CDRIOCOPENDISK)");
		cdopen = 1;
	}

	if (ioctl(fd, CDRIOCOPENTRACK, &track) < 0)
       		err(EX_IOERR, "ioctl(CDRIOCOPENTRACK)");

	if (ioctl(fd, CDRIOCNEXTWRITEABLEADDR, &addr) < 0) 
       		err(EX_IOERR, "ioctl(CDRIOCNEXTWRITEABLEADDR)");

	if (fstat(file, &stat) < 0)
		err(EX_IOERR, "fstat(%s)", name);
	filesize = stat.st_size / 1024;

	if (!quiet) {
		fprintf(stderr, "next writeable LBA %d\n", addr);
		if (file == STDIN_FILENO)
			fprintf(stderr, "writing from stdin\n");
		else
			fprintf(stderr, 
				"writing from file %s size %d KB\n",
				name, filesize);
	}

	lseek(fd, addr * block_size, SEEK_SET);
	size = 0;
	if (filesize == 0)
		filesize++;	/* cheat, avoid divide by zero */

	while ((count = read(file, buf, block_size * BLOCKS)) > 0) {	
		int res;
		if (count % block_size) {
			/* pad file to % block_size */
			bzero(&buf[count], block_size * BLOCKS - count);
			count = ((count / block_size) + 1) * block_size;
		}
		if ((res = write(fd, buf, count)) != count) {
			fprintf(stderr, "\nonly wrote %d of %d bytes\n",
				res, count);
			break;
		}
		size += count;
		tot_size += count;
		if (!quiet) {
			int pct;

			fprintf(stderr, "written this track %d KB", size/1024);
			if (file != STDIN_FILENO) {
				pct = (size / 1024) * 100 / filesize;
				fprintf(stderr, " (%d%%)", pct);
			}
			fprintf(stderr, " total %d KB\r", tot_size/1024);
		}
	}

	if (!quiet)
		fprintf(stderr, "\n");

	close(file);
	if (ioctl(fd, CDRIOCCLOSETRACK) < 0)
		err(EX_IOERR, "ioctl(CDRIOCCLOSETRACK)");
}
