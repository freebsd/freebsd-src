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
 *	$Id: scsi.c,v 1.1.1.1 1995/01/24 12:07:27 dufault Exp $
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/scsiio.h>
#include <sys/file.h>
#include <scsi.h>

int	fd;
int	debuglevel;
int	dflag,cmd;
int	reprobe;
int	probe_all;
int verbose = 0;
int	bus = -1;	/* all busses */
int	targ = -1;	/* all targs */
int	lun = 0;	/* just lun 0 */

usage()
{
	printf(

"Usage:\n"
"\n"
"  scsi -f device -d debug_level                    # To set debug level\n"
"  scsi -f device -p [-b bus] [-l lun]              # To probe all devices\n"
"  scsi -f device -r [-b bus] [-t targ] [-l lun]    # To reprobe a device\n"
"  scsi -f device [-v] -c cmd_fmt [arg0 ... argn] \\ # To send a command...\n"
"                 -o count out_fmt [arg0 ... argn]  #   EITHER (for data out)\n"
"                 -i count in_fmt                   #   OR (for data in)\n"
"\n"
"\"out_fmt\" can be \"-\" to read output data from stdin;\n"
"\"in_fmt\" can be \"-\" to write input data to stdout;\n"
"\n"
"If debugging is not compiled in the kernel, \"-d\" will have no effect\n"

);

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
	cmd = 0;
	dflag = 0;
	while ((ch = getopt(argc, argv, "vpcrf:d:b:t:l:")) != EOF) {
		switch (ch) {
		case 'p':
			probe_all = 1;
			break;
		case 'r':
			reprobe = 1;
			break;
		case 'c':
			cmd = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 'f':
			if ((fd = scsi_open(optarg, O_RDWR)) < 0) {
				(void) fprintf(stderr,
					  "%s: unable to open device %s: %s\n",
					       argv[0], optarg, strerror(errno));
				exit(errno);
			}
			fflag = 1;
			break;
		case 'd':
			debuglevel = atoi(optarg);
			dflag = 1;
			break;
		case 'b':
			bus = atoi(optarg);
			break;
		case 't':
			targ = atoi(optarg);
			break;
		case 'l':
			lun = atoi(optarg);
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
	arg = atol(h->argv[h->got]);
	h->got++;

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

	return arg;
}

/* arg_put: "put argument" callback
 */
void arg_put(void *hook, int letter, void *arg, int count, char *name)
{
	if (verbose && name && *name)
		printf("%s: ", name);

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

/* data_phase: SCSI bus data phase: DATA IN, DATA OUT, or no data transfer.
 */
enum data_phase {none = 0, in, out};

/* do_cmd: Send a command to a SCSI device
 */
void do_cmd(int fd, char *fmt, int argc, char **argv)
{
	struct get_hook h;
	scsireq_t *scsireq = scsireq_new();
	enum data_phase data_phase;
	int output = 0;

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
		data_phase = none;
	else
	{
		char *flag = cget(&h, 0);

		if (strcmp(flag, "-o") == 0)
			data_phase = out;
		else if (strcmp(flag, "-i") == 0)
			data_phase = in;
		else
		{
			fprintf(stderr,
			"Need either \"-i\" or \"-o\" for data phase; not \"%s\".\n", flag);
			usage();
		}
	}

	if (data_phase == none)
		scsireq->datalen = 0;
	else
	{
		int count;

		if (data_phase == out)
			scsireq->flags = SCCMD_WRITE;
		else
			scsireq->flags = SCCMD_READ;

		count = iget(&h, 0);
		scsireq->datalen = count;

		if (count)
		{
			char *data_fmt;
			data_fmt = cget(&h, 0);

			scsireq->databuf = malloc(count);

			if (data_phase == out)
			{
				if (strcmp(data_fmt, "-") == 0)	/* stdin */
				{
					if (read(0, scsireq->databuf, count) != count)
					{
						perror("read");
						exit(errno);
					}
				}
				else	/* XXX: Not written yet */
				{
					fprintf(stderr, "Can't set up output data using %s.\n",
					data_fmt);
					exit(-1);
				}
			}

			if (scsireq_enter(fd, scsireq) == -1)
			{
				scsi_debug(stderr, -1, scsireq);
				exit(errno);
			}

			if (data_phase == in)
			{
				if (strcmp(data_fmt, "-") == 0)	/* stdout */
				{
					if (write(1, scsireq->databuf, count) != count)
					{
						perror("write");
						exit(errno);
					}
				}
				else
				{
					scsireq_decode_visit(scsireq, data_fmt, arg_put, 0);
					putchar('\n');
				}
			}
		}
	}
}

/* do_probe_all: Loop over all SCSI IDs and see if something is
 * there.  This only does BUS 0 LUN 0.
 */
do_probe_all()
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

main(int argc, char **argv)
{
	struct scsi_addr scaddr;

	procargs(&argc,&argv);

	if (probe_all) {
		do_probe_all();
	}

	if(reprobe) {
		scaddr.scbus = bus;
		scaddr.target = targ;
		scaddr.lun = lun;	

		if (ioctl(fd,SCIOCREPROBE,&scaddr) == -1)
			perror("ioctl");
	}

	if(dflag) {
		if (ioctl(fd,SCIOCDEBUG,&debuglevel) == -1)
		{
			perror("ioctl [SCIODEBUG]");
			exit(1);
		}
	}

	if (cmd) {
		char *fmt;

		if (argc <= 1)
		{
			fprintf(stderr, "Need the command format string.\n");
			usage();
		}

		fmt = argv[0];

		argc -= 1;
		argv += 1;

		do_cmd(fd, fmt, argc, argv);
	}
}
