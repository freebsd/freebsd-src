/*-
 * Copyright (c) 2000,2001,2002 Søren Schmidt <sos@freebsd.org>
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

#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <sysexits.h>
#include <fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <sys/param.h>

#define BLOCKS	16

struct track_info {
	int	file;
	char	*file_name;
	int	file_size;
	int	block_size;
	int	block_type;
	int	pregap;
	int	addr;
};
static struct track_info tracks[100];
static int fd, quiet, verbose, saved_block_size, notracks;

void add_track(char *, int, int, int);
void do_DAO(int, int);
void do_TAO(int, int);
int write_file(struct track_info *);
int roundup_blocks(struct track_info *);
void cue_ent(struct cdr_cue_entry *, int, int, int, int, int, int, int);
void cleanup(int);
void usage(void);

int
main(int argc, char **argv)
{
	int ch, arg, addr;
	int dao = 0, eject = 0, fixate = 0, list = 0, multi = 0, preemp = 0;
	int nogap = 0, speed = 4, test_write = 0;
	int block_size = 0, block_type = 0, cdopen = 0;
	const char *dev = "/dev/acd0c";

	while ((ch = getopt(argc, argv, "def:lmnpqs:tv")) != -1) {
		switch (ch) {
		case 'd':
			dao = 1;
			break;

		case 'e':
			eject = 1;
			break;

		case 'f':
			dev = optarg;
			break;

		case 'l':
			list = 1;
			break;

		case 'm':
			multi = 1;
			break;

		case 'n':
			nogap = 1;
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

		case 'v':
			verbose = 1;
			break;

		default: 
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if ((fd = open(dev, O_RDWR, 0)) < 0)
		err(EX_NOINPUT, "open(%s)", dev);

	if (ioctl(fd, CDRIOCGETBLOCKSIZE, &saved_block_size) < 0) 
       		err(EX_IOERR, "ioctl(CDRIOCGETBLOCKSIZE)");

	if (ioctl(fd, CDRIOCWRITESPEED, &speed) < 0) 
       		err(EX_IOERR, "ioctl(CDRIOCWRITESPEED)");

	err_set_exit(cleanup);

	for (arg = 0; arg < argc; arg++) {
		if (!strcasecmp(argv[arg], "fixate")) {
			fixate = 1;
			break;
		}
		if (!strcasecmp(argv[arg], "msinfo")) {
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
			fprintf(stdout, "%d,%d\n", 
				ntohl(entry.entry.addr.lba), addr);

			break;
		}
		if (!strcasecmp(argv[arg], "erase") || !strcasecmp(argv[arg], "blank")){
		    	int error, blank, percent;

			if (!strcasecmp(argv[arg], "erase"))
				blank = CDR_B_ALL;
			else
				blank = CDR_B_MIN;
			if (!quiet)
				fprintf(stderr, "%sing CD, please wait..\r",
					blank == CDR_B_ALL ? "eras" : "blank");

			if (ioctl(fd, CDRIOCBLANK, &blank) < 0)
        			err(EX_IOERR, "ioctl(CDRIOCBLANK)");
			while (1) {
				sleep(1);
				error = ioctl(fd, CDRIOCGETPROGRESS, &percent);
				if (percent > 0 && !quiet)
					fprintf(stderr, 
						"%sing CD - %d %% done     \r",
						blank == CDR_B_ALL ? 
						"eras" : "blank", percent);
				if (error || percent == 100)
					break;
			}
			if (!quiet)
				printf("\n");
			continue;
		}
		if (!strcasecmp(argv[arg], "audio") || !strcasecmp(argv[arg], "raw")) {
			block_type = CDR_DB_RAW;
			block_size = 2352;
			continue;
		}
		if (!strcasecmp(argv[arg], "data") || !strcasecmp(argv[arg], "mode1")) {
			block_type = CDR_DB_ROM_MODE1;
			block_size = 2048;
			continue;
		}
		if (!strcasecmp(argv[arg], "mode2")) {
			block_type = CDR_DB_ROM_MODE2;
			block_size = 2336;
			continue;
		}
		if (!strcasecmp(argv[arg], "xamode1")) {
			block_type = CDR_DB_XA_MODE1;
			block_size = 2048;
			continue;
		}
		if (!strcasecmp(argv[arg], "xamode2")) {
			block_type = CDR_DB_XA_MODE2_F2;
			block_size = 2324;
			continue;
		}
		if (!strcasecmp(argv[arg], "vcd")) {
			block_type = CDR_DB_XA_MODE2_F2;
			block_size = 2352;
			dao = 1;
			nogap = 1;
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
				if ((eol = strchr(file_buf, '\n')))
					*eol = NULL;
				add_track(file_buf, block_size, block_type, nogap);
			}
			if (feof(fp))
				fclose(fp);
			else
				err(EX_IOERR, "fgets(%s)", file_buf);
		}
		else
			add_track(argv[arg], block_size, block_type, nogap);
	}
	if (notracks) {
		if (ioctl(fd, CDIOCSTART, 0) < 0)
			err(EX_IOERR, "ioctl(CDIOCSTART)");
		if (!cdopen) {
			if (ioctl(fd, CDRIOCINITWRITER, &test_write) < 0)
				err(EX_IOERR, "ioctl(CDRIOCINITWRITER)");
			cdopen = 1;
		}
		if (dao) 
			do_DAO(test_write, multi);
		else
			do_TAO(test_write, preemp);
	}
	if (fixate && !dao) {
		if (!quiet)
			fprintf(stderr, "fixating CD, please wait..\n");
		if (ioctl(fd, CDRIOCFIXATE, &multi) < 0)
        		err(EX_IOERR, "ioctl(CDRIOCFIXATE)");
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
add_track(char *name, int block_size, int block_type, int nogap)
{
	struct stat sb;
	int file;
	static int done_stdin = 0;

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
	if (fstat(file, &sb) < 0)
		err(EX_IOERR, "fstat(%s)", name);
	tracks[notracks].file = file;
	tracks[notracks].file_name = name;
	tracks[notracks].file_size = sb.st_size;
	tracks[notracks].block_size = block_size;
	tracks[notracks].block_type = block_type;

	if (nogap && notracks)
		tracks[notracks].pregap = 0;
	else {
		if (tracks[notracks - (notracks > 0)].block_type == block_type)
			tracks[notracks].pregap = 150;
		else
			tracks[notracks].pregap = 255;
	}

	if (verbose) {
		int pad = 0;

		if (tracks[notracks].file_size / tracks[notracks].block_size !=
		    roundup_blocks(&tracks[notracks]))
			pad = 1;
		fprintf(stderr, 
			"adding type 0x%02x file %s size %d KB %d blocks %s\n",
			tracks[notracks].block_type, name, (int)sb.st_size/1024,
			roundup_blocks(&tracks[notracks]),
			pad ? "(0 padded)" : "");
	}
	notracks++;
}

void
do_DAO(int test_write, int multi)
{
	struct cdr_cuesheet sheet;
	struct cdr_cue_entry cue[100];
	int format = CDR_SESS_CDROM;
	int addr, i, j = 0;

	int bt2ctl[16] = { 0x0,  -1,  -1,  -1,  -1,  -1,  -1,  -1,
			   0x4, 0x4, 0x4, 0x4, 0x4, 0x4,  -1,  -1 };

	int bt2df[16] = { 0x0,    -1,   -1,   -1,   -1,   -1,   -1,   -1,
			  0x10, 0x30, 0x20,   -1, 0x21,   -1,   -1,   -1 };
	
	if (ioctl(fd, CDRIOCNEXTWRITEABLEADDR, &addr) < 0) 
		err(EX_IOERR, "ioctl(CDRIOCNEXTWRITEABLEADDR)");
	if (verbose)
		fprintf(stderr, "next writeable LBA %d\n", addr);

	cue_ent(&cue[j++], bt2ctl[tracks[0].block_type], 0x01, 0x00, 0x0,
		(bt2df[tracks[0].block_type] & 0xf0) | 
		(tracks[0].block_type < 8 ? 0x01 : 0x04), 0x00, addr);

	for (i = 0; i < notracks; i++) {
		if (bt2ctl[tracks[i].block_type] < 0 ||
		    bt2df[tracks[i].block_type] < 0)
			err(EX_IOERR, "track type not supported in DAO mode");

		if (tracks[i].block_type >= CDR_DB_XA_MODE1)
			format = CDR_SESS_CDROM_XA;

		if (i == 0) {
			addr += tracks[i].pregap;
			tracks[i].addr = addr;

			cue_ent(&cue[j++], bt2ctl[tracks[i].block_type], 
				0x01, i+1, 0x1, bt2df[tracks[i].block_type],
				0x00, addr);

		}
		else {
			if (tracks[i].pregap) {
				if (tracks[i].block_type > 0x7) {
					cue_ent(&cue[j++],bt2ctl[tracks[i].block_type], 
						0x01, i+1, 0x0,
						(bt2df[tracks[i].block_type] & 0xf0) | 
						(tracks[i].block_type < 8 ? 0x01 :0x04),
						0x00, addr);
				}
				else
					cue_ent(&cue[j++],bt2ctl[tracks[i].block_type], 
						0x01, i+1, 0x0,
						bt2df[tracks[i].block_type],
						0x00, addr);
			}
			tracks[i].addr = tracks[i - 1].addr +
				roundup_blocks(&tracks[i - 1]);

			cue_ent(&cue[j++], bt2ctl[tracks[i].block_type],
				0x01, i+1, 0x1, bt2df[tracks[i].block_type],
				0x00, addr + tracks[i].pregap);

			if (tracks[i].block_type > 0x7)
				addr += tracks[i].pregap;
		}
		addr += roundup_blocks(&tracks[i]);
	}

	cue_ent(&cue[j++], bt2ctl[tracks[i - 1].block_type], 0x01, 0xaa, 0x01,
		(bt2df[tracks[i - 1].block_type] & 0xf0) | 
		(tracks[i - 1].block_type < 8 ? 0x01 : 0x04), 0x00, addr);

	sheet.len = j * 8;
	sheet.entries = cue;
	sheet.test_write = test_write;
	sheet.session_type = multi ? CDR_SESS_MULTI : CDR_SESS_NONE;
	sheet.session_format = format;
	if (verbose) {
		u_int8_t *ptr = (u_int8_t *)sheet.entries;
		
		fprintf(stderr,"CUE sheet:");
		for (i = 0; i < sheet.len; i++)
			if (i % 8)
				fprintf(stderr," %02x", ptr[i]);
			else
				fprintf(stderr,"\n%02x", ptr[i]);
		fprintf(stderr,"\n");
	}
	
	if (ioctl(fd, CDRIOCSENDCUE, &sheet) < 0)
		err(EX_IOERR, "ioctl(CDRIOCSENDCUE)");

	for (i = 0; i < notracks; i++) {
		if (write_file(&tracks[i]))
			err(EX_IOERR, "write_file");
	}

	ioctl(fd, CDRIOCFLUSH);
}

void
do_TAO(int test_write, int preemp)
{
	struct cdr_track track;
	int i;

	for (i = 0; i < notracks; i++) {
		track.test_write = test_write;
		track.datablock_type = tracks[i].block_type;
		track.preemp = preemp;
		if (ioctl(fd, CDRIOCINITTRACK, &track) < 0)
			err(EX_IOERR, "ioctl(CDRIOCINITTRACK)");

		if (ioctl(fd, CDRIOCNEXTWRITEABLEADDR, &tracks[i].addr) < 0)
			err(EX_IOERR, "ioctl(CDRIOCNEXTWRITEABLEADDR)");
		if (!quiet)
			fprintf(stderr, "next writeable LBA %d\n",
				tracks[i].addr);
		if (write_file(&tracks[i]))
			err(EX_IOERR, "write_file");
		if (ioctl(fd, CDRIOCFLUSH) < 0)
			err(EX_IOERR, "ioctl(CDRIOCFLUSH)");
	}
}

int
write_file(struct track_info *track_info)
{
	int size, count, filesize;
	char buf[2352*BLOCKS];
	static int tot_size = 0;

	filesize = track_info->file_size / 1024;

	if (ioctl(fd, CDRIOCSETBLOCKSIZE, &track_info->block_size) < 0)
		err(EX_IOERR, "ioctl(CDRIOCSETBLOCKSIZE)");

	if (track_info->addr >= 0)
		lseek(fd, track_info->addr * track_info->block_size, SEEK_SET);

	if (verbose)
		fprintf(stderr, "addr = %d size = %d blocks = %d\n",
			track_info->addr, track_info->file_size,
			roundup_blocks(track_info));

	if (!quiet) {
		if (track_info->file == STDIN_FILENO)
			fprintf(stderr, "writing from stdin\n");
		else
			fprintf(stderr, 
				"writing from file %s size %d KB\n",
				track_info->file_name, filesize);
	}
	size = 0;
	if (filesize == 0)
		filesize++;	/* cheat, avoid divide by zero */

	while ((count = read(track_info->file, buf,
			     MIN((track_info->file_size - size),
				 track_info->block_size * BLOCKS))) > 0) {	
		int res;

		if (count % track_info->block_size) {
			/* pad file to % block_size */
			bzero(&buf[count],
			      (track_info->block_size * BLOCKS) - count);
			count = ((count / track_info->block_size) + 1) *
				track_info->block_size;
		}
		if ((res = write(fd, buf, count)) != count) {
			fprintf(stderr, "\nonly wrote %d of %d bytes err=%d\n",
				res, count, errno);
			break;
		}
		size += count;
		tot_size += count;
		if (!quiet) {
			int pct;

			fprintf(stderr, "written this track %d KB", size/1024);
			if (track_info->file != STDIN_FILENO) {
				pct = (size / 1024) * 100 / filesize;
				fprintf(stderr, " (%d%%)", pct);
			}
			fprintf(stderr, " total %d KB\r", tot_size/1024);
		}
		if (size >= track_info->file_size)
			break;
	}

	if (!quiet)
		fprintf(stderr, "\n");
	close(track_info->file);
	return 0;
}

int
roundup_blocks(struct track_info *track)
{
	return ((track->file_size + track->block_size - 1) / track->block_size);
}

void
cue_ent(struct cdr_cue_entry *cue, int ctl, int adr, int track, int idx,
	int dataform, int scms, int lba)
{
	cue->adr = adr;
	cue->ctl = ctl;
	cue->track = track;
	cue->index = idx;
	cue->dataform = dataform;
	cue->scms = scms;
	lba += 150;
	cue->min = lba / (60*75);
	cue->sec = (lba % (60*75)) / 75;
	cue->frame = (lba % (60*75)) % 75;
}

void
cleanup(int dummy __unused)
{
	if (ioctl(fd, CDRIOCSETBLOCKSIZE, &saved_block_size) < 0) 
		err(EX_IOERR, "ioctl(CDRIOCSETBLOCKSIZE)");
}

void
usage(void)
{
	fprintf(stderr,
	    "Usage: %s [-delmnpqtv] [-f device] [-s speed] [command]"
	    " [command file ...]\n", getprogname());
	exit(EX_USAGE);
}
