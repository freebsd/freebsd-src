/*
 * Written By Julian ELischer
 * Copyright julian Elischer 1993.
 * Permission is granted to use or redistribute this file in any way as long
 * as this notice remains. Julian Elischer does not guarantee that this file
 * is totally correct for any given task and users of this file must
 * accept responsibility for any damage that occurs from the application of this
 * file.
 *
 * (julian@tfs.com julian@dialix.oz.au)
 *
 * User SCSI hooks added by Peter Dufault:
 *
 * Copyright (c) 1994 HD Associates
 * (contact: dufault@hda.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of HD Associates
 *    may not be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY HD ASSOCIATES ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL HD ASSOCIATES BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: scsi.c,v 1.15 1997/03/29 03:33:04 imp Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/scsiio.h>
#include <sys/file.h>
#include <scsi.h>
#include <ctype.h>
#include <signal.h>
#include <err.h>

int	fd;
int	debuglevel;
int	debugflag;
int commandflag;
int	reprobe;
int	probe_all;
int verbose = 0;
int	bus = -1;	/* all busses */
int	targ = -1;	/* all targs */
int	lun = 0;	/* just lun 0 */
int	freeze = 0;	/* Freeze this many seconds */

int modeflag;
int editflag;
int modepage = 0; /* Read this mode page */
int pagectl = 0;  /* Mode sense page control */
int seconds = 2;

void usage(void)
{
	fprintf(stderr, "%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
	"usage: scsi -f device -d debug_level",
	"       scsi -f device [-v] -z seconds",
	"       scsi -f device -m page [-P pc] [-e]",
	"       scsi -f device -p [-b bus] [-l lun]",
	"       scsi -f device -r [-b bus] [-t targ] [-l lun]",
	"       scsi -f device [-v] [-s seconds] -c cmd_fmt [arg0 ... argn]",
	"                      -o count out_fmt [arg0 ... argn]",
	"                      -i count in_fmt");
	exit (1);
}

void procargs(int *argc_p, char ***argv_p)
{
	int argc = *argc_p;
	char **argv = *argv_p;
	extern char        *optarg;
	extern int          optind;
	int		    fflag,
	                    ch;

	fflag = 0;
	commandflag = 0;
	debugflag = 0;
	while ((ch = getopt(argc, argv, "ceprvf:d:b:t:l:z:m:P:s:")) != -1) {
		switch (ch) {
		case 'p':
			probe_all = 1;
			break;
		case 'r':
			reprobe = 1;
			break;
		case 'c':
			commandflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'e':
			editflag = 1;
			break;
		case 'f':
			if ((fd = scsi_open(optarg, O_RDWR)) < 0)
				err(errno, "unable to open device %s", optarg);
			fflag = 1;
			break;
		case 'd':
			debuglevel = strtol(optarg, 0, 0);
			debugflag = 1;
			break;
		case 'b':
			bus = strtol(optarg, 0, 0);
			break;
		case 't':
			targ = strtol(optarg, 0, 0);
			break;
		case 'l':
			lun = strtol(optarg, 0, 0);
			break;
		case 'z':
			freeze = strtol(optarg, 0, 0);
			break;
		case 'P':
			pagectl = strtol(optarg, 0, 0);
			break;
		case 's':
			seconds = strtol(optarg, 0, 0);
			break;
		case 'm':
			modeflag = 1;
			modepage = strtol(optarg, 0, 0);
			break;
		case '?':
		default:
			usage();
		}
	}
	*argc_p = argc - optind;
	*argv_p = argv + optind;

	if (!fflag) usage();
}

/* get_hook: Structure for evaluating args in a callback.
 */
struct get_hook
{
	int argc;
	char **argv;
	int got;
};

/* iget: Integer argument callback
 */
int iget(void *hook, char *name)
{
	struct get_hook *h = (struct get_hook *)hook;
	int arg;

	if (h->got >= h->argc)
	{
		fprintf(stderr, "Expecting an integer argument.\n");
		usage();
	}
	arg = strtol(h->argv[h->got], 0, 0);
	h->got++;

	if (verbose && name && *name)
		printf("%s: %d\n", name, arg);

	return arg;
}

/* cget: char * argument callback
 */
char *cget(void *hook, char *name)
{
	struct get_hook *h = (struct get_hook *)hook;
	char *arg;

	if (h->got >= h->argc)
	{
		fprintf(stderr, "Expecting a character pointer argument.\n");
		usage();
	}
	arg = h->argv[h->got];
	h->got++;

	if (verbose && name)
		printf("cget: %s: %s", name, arg);

	return arg;
}

/* arg_put: "put argument" callback
 */
void arg_put(void *hook, int letter, void *arg, int count, char *name)
{
	if (verbose && name && *name)
		printf("%s:  ", name);

	switch(letter)
	{
		case 'i':
		case 'b':
		printf("%d ", (int)arg);
		break;

		case 'c':
		case 'z':
		{
			char *p = malloc(count + 1);
			p[count] = 0;
			strncpy(p, (char *)arg, count);
			if (letter == 'z')
			{
				int i;
				for (i = count - 1; i >= 0; i--)
					if (p[i] == ' ')
						p[i] = 0;
					else
						break;
			}
			printf("%s ", p);
		}

		break;

		default:
		printf("Unknown format letter: '%c'\n", letter);
	}
	if (verbose)
		putchar('\n');
}

int arg_get (void *hook, char *field_name)
{
	printf("get \"%s\".\n", field_name);
	return 0;
}

/* data_phase: SCSI bus data phase: DATA IN, DATA OUT, or no data transfer.
 */
enum data_phase {none = 0, in, out};

/* do_cmd: Send a command to a SCSI device
 */
static void
do_cmd(int fd, char *fmt, int argc, char **argv)
{
	struct get_hook h;
	scsireq_t *scsireq = scsireq_new();
	enum data_phase data_phase;
	int count, amount;
	char *data_fmt, *bp;

	h.argc = argc;
	h.argv = argv;
	h.got = 0;

	scsireq_reset(scsireq);

	scsireq_build_visit(scsireq, 0, 0, 0, fmt, iget, (void *)&h);

	/* Three choices here:
	 * 1. We've used up all the args and have no data phase.
	 * 2. We have input data ("-i")
	 * 3. We have output data ("-o")
	 */

	if (h.got >= h.argc)
	{
		data_phase = none;
		count = scsireq->datalen = 0;
	}
	else
	{
		char *flag = cget(&h, 0);

		if (strcmp(flag, "-o") == 0)
		{
			data_phase = out;
			scsireq->flags = SCCMD_WRITE;
		}
		else if (strcmp(flag, "-i") == 0)
		{
			data_phase = in;
			scsireq->flags = SCCMD_READ;
		}
		else
		{
			fprintf(stderr,
			"Need either \"-i\" or \"-o\" for data phase; not \"%s\".\n", flag);
			usage();
		}

		count = scsireq->datalen = iget(&h, 0);
		if (count)
		{
			data_fmt = cget(&h, 0);

			scsireq->databuf = malloc(count);

			if (data_phase == out)
			{
				if (strcmp(data_fmt, "-") == 0)	/* Read data from stdin */
				{
					bp = (char *)scsireq->databuf;
					while (count > 0 && (amount = read(0, bp, count)) > 0)
					{
						count -= amount;
						bp += amount;
					}
					if (amount == -1)
					{
						perror("read");
						exit(errno);
					}
					else if (amount == 0)
					{
						/* early EOF */
						fprintf(stderr,
							"Warning: only read %lu bytes out of %lu.\n",
							scsireq->datalen - (u_long)count,
							scsireq->datalen);
						scsireq->datalen -= (u_long)count;
					}
				}
				else
				{
					bzero(scsireq->databuf, count);
					scsireq_encode_visit(scsireq, data_fmt, iget, (void *)&h);
				}
			}
		}
	}


	scsireq->timeout = seconds * 1000;

	if (scsireq_enter(fd, scsireq) == -1)
	{
		scsi_debug(stderr, -1, scsireq);
		exit(errno);
	}

	if (SCSIREQ_ERROR(scsireq))
		scsi_debug(stderr, 0, scsireq);

	if (count && data_phase == in)
	{
		if (strcmp(data_fmt, "-") == 0)	/* stdout */
		{
			bp = (char *)scsireq->databuf;
			while (count > 0 && (amount = write(1, bp, count)) > 0)
			{
				count -= amount;
				bp += amount;
			}
			if (amount < 0)
			{
				perror("write");
				exit(errno);
			}
			else if (amount == 0)
				fprintf(stderr, "Warning: wrote only %d bytes out of %d.\n",
					scsireq->datalen - count,
					scsireq->datalen);
			
		}
		else
		{
			scsireq_decode_visit(scsireq, data_fmt, arg_put, 0);
			putchar('\n');
		}
	}
}

static void
freeze_ioctl(int fd, int op, void *data)
{
	if (ioctl(fd, SCIOCFREEZE, 0) == -1) {
		if (errno == ENODEV) {
			fprintf(stderr,
			"Your kernel must be configured with option SCSI_FREEZE.\n");
		}
		else
			perror("SCIOCFREEZE");
		exit(errno);
	}
}

/* do_freeze: Freeze the bus for a given number of seconds.
 */
static void do_freeze(int seconds)
{
	if (seconds == -1) {
		printf("Hit return to thaw:  ");
		fflush(stdout);
		sync();

		freeze_ioctl(fd, SCIOCFREEZE, 0);

		(void)getchar();

		freeze_ioctl(fd, SCIOCTHAW, 0);
	}
	else {
		sync();
		freeze_ioctl(fd, SCIOCFREEZETHAW, &seconds);
		if (verbose) {
			putchar('\007');
			fflush(stdout);
		}

		freeze_ioctl(fd, SCIOCWAITTHAW, 0);
		if (verbose) {
			putchar('\007');
			fflush(stdout);
		}
	}
}

void mode_sense(int fd, u_char *data, int len, int pc, int page)
{
	scsireq_t *scsireq;

	bzero(data, len);

	scsireq = scsireq_new();

	if (scsireq_enter(fd, scsireq_build(scsireq,
	 len, data, SCCMD_READ,
	 "1A 0 v:2 {Page Control} v:6 {Page Code} 0 v:i1 {Allocation Length} 0",
	 pc, page, len)) == -1)	/* Mode sense */
	{
		scsi_debug(stderr, -1, scsireq);
		exit(errno);
	}

	if (SCSIREQ_ERROR(scsireq))
	{
		scsi_debug(stderr, 0, scsireq);
		exit(-1);
	}

	free(scsireq);
}

void mode_select(int fd, u_char *data, int len, int perm)
{
	scsireq_t *scsireq;

	scsireq = scsireq_new();

	if (scsireq_enter(fd, scsireq_build(scsireq,
	 len, data, SCCMD_WRITE,
	 "15 0:7 v:1 {SP} 0 0 v:i1 {Allocation Length} 0", perm, len)) == -1)	/* Mode select */
	{
		scsi_debug(stderr, -1, scsireq);
		exit(errno);
	}

	if (SCSIREQ_ERROR(scsireq))
	{
		scsi_debug(stderr, 0, scsireq);
		exit(-1);
	}

	free(scsireq);
}


#define START_ENTRY '{'
#define END_ENTRY '}'

static void
skipwhite(FILE *f)
{
	int c;

skip_again:

	while (isspace(c = getc(f)))
		;

	if (c == '#') {
		while ((c = getc(f)) != '\n' && c != EOF)
			;
		goto skip_again;
	}

	ungetc(c, f);
}

/* mode_lookup: Lookup a format description for a given page.
 */
char *mode_db = "/usr/share/misc/scsi_modes";
static char *mode_lookup(int page)
{
	char *new_db;
	FILE *modes;
	int match, next, found, c;
	static char fmt[1024];	/* XXX This should be with strealloc */
	int page_desc;
	new_db = getenv("SCSI_MODES");

	if (new_db)
		mode_db = new_db;

	modes = fopen(mode_db, "r");
	if (modes == 0)
		return 0;

	next = 0;
	found = 0;

	while (!found) {

		skipwhite(modes);

		if (fscanf(modes, "%i", &page_desc) != 1)
			break;

		if (page_desc == page)
			found = 1;

		skipwhite(modes);
		if (getc(modes) != START_ENTRY) {
			fprintf(stderr, "Expected %c.\n", START_ENTRY);
			exit(-1);
		}

		match = 1;
		while (match != 0) {
			c = getc(modes);
			if (c == EOF) {
				fprintf(stderr, "Expected %c.\n", END_ENTRY);
			}

			if (c == START_ENTRY) {
				match++;
			}
			if (c == END_ENTRY) {
				match--;
				if (match == 0)
					break;
			}
			if (found && c != '\n') {
				if (next >= sizeof(fmt)) {
					fprintf(stderr, "Stupid program: Buffer overflow.\n");
					exit(ENOMEM);
				}

				fmt[next++] = (u_char)c;
			}
		}
	}
	fmt[next] = 0;

	return (found) ? fmt : 0;
}

/* -------- edit: Mode Select Editor ---------
 */
struct editinfo
{
	int can_edit;
	int default_value;
} editinfo[64];	/* XXX Bogus fixed size */

static int editind;
volatile int edit_opened;
static FILE *edit_file;
static char edit_name[L_tmpnam];

static inline void
edit_rewind(void)
{
	editind = 0;
}

static void
edit_done(void)
{
	int opened;

	sigset_t all, prev;
	sigfillset(&all);

	(void)sigprocmask(SIG_SETMASK, &all, &prev);

	opened = (int)edit_opened;
	edit_opened = 0;

	(void)sigprocmask(SIG_SETMASK, &prev, 0);

	if (opened)
	{
		if (fclose(edit_file))
			perror(edit_name);
		if (unlink(edit_name))
			perror(edit_name);
	}
}

static void
edit_init(void)
{
	edit_rewind();
	if (tmpnam(edit_name) == 0) {
		perror("tmpnam failed");
		exit(errno);
	}
	if ( (edit_file = fopen(edit_name, "w")) == 0) {
		perror(edit_name);
		exit(errno);
	}
	edit_opened = 1;

	atexit(edit_done);
}

static void
edit_check(void *hook, int letter, void *arg, int count, char *name)
{
	if (letter != 'i' && letter != 'b') {
		fprintf(stderr, "Can't edit format %c.\n", letter);
		exit(-1);
	}

	if (editind >= sizeof(editinfo) / sizeof(editinfo[0])) {
		fprintf(stderr, "edit table overflow\n");
		exit(ENOMEM);
	}
	editinfo[editind].can_edit = ((int)arg != 0);
	editind++;
}

static void
edit_defaults(void *hook, int letter, void *arg, int count, char *name)
{
	if (letter != 'i' && letter != 'b') {
		fprintf(stderr, "Can't edit format %c.\n", letter);
		exit(-1);
	}

	editinfo[editind].default_value = ((int)arg);
	editind++;
}

static void
edit_report(void *hook, int letter, void *arg, int count, char *name)
{
	if (editinfo[editind].can_edit) {
		if (letter != 'i' && letter != 'b') {
			fprintf(stderr, "Can't report format %c.\n", letter);
			exit(-1);
		}

		fprintf(edit_file, "%s:  %d\n", name, (int)arg);
	}

	editind++;
}

static int
edit_get(void *hook, char *name)
{
	int arg = editinfo[editind].default_value;

	if (editinfo[editind].can_edit) {
		char line[80];
		if (fgets(line, sizeof(line), edit_file) == 0) {
			perror("fgets");
			exit(errno);
		}

		line[strlen(line) - 1] = 0;

		if (strncmp(name, line, strlen(name)) != 0) {
			fprintf(stderr, "Expected \"%s\" and read \"%s\"\n",
			name, line);
			exit(-1);
		}

		arg = strtoul(line + strlen(name) + 2, 0, 0);
	}

	editind++;
	return arg;
}

static void
edit_edit(void)
{
	char *system_line;
	char *editor = getenv("EDITOR");
	if (!editor)
		editor = "vi";

	fclose(edit_file);

	system_line = malloc(strlen(editor) + strlen(edit_name) + 6);
	sprintf(system_line, "%s %s", editor, edit_name);
	system(system_line);
	free(system_line);

	if ( (edit_file = fopen(edit_name, "r")) == 0) {
		perror(edit_name);
		exit(errno);
	}
}

static void
mode_edit(int fd, int page, int edit, int argc, char *argv[])
{
	int i;
	u_char data[255];
	u_char *mode_pars;
	struct mode_header
	{
		u_char mdl;	/* Mode data length */
		u_char medium_type;
		u_char dev_spec_par;
		u_char bdl;	/* Block descriptor length */
	};

	struct mode_page_header
	{
		u_char page_code;
		u_char page_length;
	};

	struct mode_header *mh;
	struct mode_page_header *mph;

	char *fmt = mode_lookup(page);
	if (!fmt && verbose) {
		fprintf(stderr,
		"No mode data base entry in \"%s\" for page %d;  binary %s only.\n",
		mode_db, page, (edit ? "edit" : "display"));
	}

	if (edit) {
		if (!fmt) {
			fprintf(stderr, "Sorry: can't edit without a format.\n");
			exit(-1);
		}

		if (pagectl != 0 && pagectl != 3) {
			fprintf(stderr,
"It only makes sense to edit page 0 (current) or page 3 (saved values)\n");
			exit(-1);
		}

		verbose = 1;

		mode_sense(fd, data, sizeof(data), 1, page);

		mh = (struct mode_header *)data;
		mph = (struct mode_page_header *)
		(((char *)mh) + sizeof(*mh) + mh->bdl);

		mode_pars = (char *)mph + sizeof(*mph);

		edit_init();
		scsireq_buff_decode_visit(mode_pars, mh->mdl,
		fmt, edit_check, 0);

		mode_sense(fd, data, sizeof(data), 0, page);

		edit_rewind();
		scsireq_buff_decode_visit(mode_pars, mh->mdl,
		fmt, edit_defaults, 0);

		edit_rewind();
		scsireq_buff_decode_visit(mode_pars, mh->mdl,
		fmt, edit_report, 0);

		edit_edit();

		edit_rewind();
		scsireq_buff_encode_visit(mode_pars, mh->mdl,
		fmt, edit_get, 0);

		/* Eliminate block descriptors:
		 */
		bcopy((char *)mph, ((char *)mh) + sizeof(*mh),
		sizeof(*mph) + mph->page_length);

		mh->bdl = mh->dev_spec_par = 0;
		mph = (struct mode_page_header *) (((char *)mh) + sizeof(*mh));
		mode_pars = ((char *)mph) + 2;

#if 0
		/* Turn this on to see what you're sending to the
		 * device:
		 */
		edit_rewind();
		scsireq_buff_decode_visit(mode_pars,
		mh->mdl, fmt, arg_put, 0);
#endif

		edit_done();

		/* Make it permanent if pageselect is three.
		 */

		mph->page_code &= ~0xC0;	/* Clear PS and RESERVED */
		mh->mdl = 0;				/* Reserved for mode select */

		mode_select(fd, (char *)mh,
		sizeof(*mh) + mh->bdl + sizeof(*mph) + mph->page_length,
		(pagectl == 3));

		exit(0);
	}

	mode_sense(fd, data, sizeof(data), pagectl, page);

	/* Skip over the block descriptors.
	 */
	mh = (struct mode_header *)data;
	mph = (struct mode_page_header *)(((char *)mh) + sizeof(*mh) + mh->bdl);
	mode_pars = (char *)mph + sizeof(*mph);

	if (!fmt) {
		for (i = 0; i < mh->mdl; i++) {
			printf("%02x%c",mode_pars[i],
			(((i + 1) % 8) == 0) ? '\n' : ' ');
		}
		putc('\n', stdout);
	} else {
			verbose = 1;
			scsireq_buff_decode_visit(mode_pars,
			mh->mdl, fmt, arg_put, 0);
	}
}

/* do_probe_all: Loop over all SCSI IDs and see if something is
 * there.  This only does BUS 0 LUN 0.
 */
void do_probe_all(void)
{
	scsireq_t *scsireq;

	char vendor_id[8 + 1], product_id[16 + 1], revision[4 + 1];
	int id;
	u_char *inq_buf = malloc(96);
	struct scsi_addr addr;

	scsireq = scsireq_build(scsireq_new(),
	96, inq_buf, SCCMD_READ,
	"12 0 0 0 v 0", 96);

	addr.scbus = (bus == -1) ? 0 : bus;
	addr.lun = lun;

	if (addr.scbus || addr.lun)
	{
		printf("For bus %d lun %d:\n", addr.scbus, addr.lun);
	}

	for (id = 0; id < 8; id++)
	{
		addr.target = id;

		printf("%d: ", id);
		if (ioctl(fd, SCIOCADDR, &addr) == -1) {
			if (errno == ENXIO)
			{
				errno = 0;
				printf("nothing.\n");
			}
			else
				printf("SCIOCADDR: %s\n", strerror(errno));

			continue;
		}

		if (scsireq_enter(fd, scsireq) == -1) {
			printf("scsireq_enter: %s\n", strerror(errno));
			continue;
		}

		vendor_id[sizeof(vendor_id) - 1] = 0;
		product_id[sizeof(product_id) - 1] = 0;
		revision[sizeof(revision) - 1] = 0;

		scsireq_decode(scsireq, "s8 c8 c16 c4",
		vendor_id, product_id, revision);

		printf("%s %s %s\n", vendor_id, product_id, revision);
	}
}

void main(int argc, char **argv)
{
	struct scsi_addr scaddr;

	procargs(&argc,&argv);

	/* XXX This has grown to the point that it should be cleaned up.
	 */
	if (freeze) {
		do_freeze(freeze);
	} else if (probe_all) {
		do_probe_all();
	} else if(reprobe) {
		scaddr.scbus = bus;
		scaddr.target = targ;
		scaddr.lun = lun;

		if (ioctl(fd,SCIOCREPROBE,&scaddr) == -1)
			perror("ioctl");
	} else if(debugflag) {
		if (ioctl(fd,SCIOCDEBUG,&debuglevel) == -1)
		{
			perror("ioctl [SCIODEBUG]");
			exit(1);
		}
	} else if (commandflag) {
		char *fmt;

		if (argc < 1) {
			fprintf(stderr, "Need the command format string.\n");
			usage();
		}


		fmt = argv[0];

		argc -= 1;
		argv += 1;

		do_cmd(fd, fmt, argc, argv);
	} else if (modeflag) {
		mode_edit(fd, modepage, editflag, argc, argv);
	}
	exit(0);
}
