/*
 * Sample program to attach to the "targ" processor target, target mode
 * peripheral driver and push or receive data.
 *
 * Copyright (c) 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id$
 */

#include <sys/types.h>

#include <fcntl.h>
#include <poll.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#include <cam/scsi/scsi_targetio.h>

char  *appname;
int    ifd;
char  *ifilename;
int    ofd;
char  *ofilename;
size_t bufsize = 64 * 1024;
void  *buf;
char  *targdevname;
int    targfd;

static void pump_events();
static void handle_exception();
static void usage();

int
main(int argc, char *argv[])
{
	int  ch;

	appname = *argv;
	while ((ch = getopt(argc, argv, "i:o:")) != -1) {
		switch(ch) {
		case 'i':
			if ((ifd = open(optarg, O_RDONLY)) == -1) {
				perror(optarg);
				exit(EX_NOINPUT);
			}
			ifilename = optarg;
			break;
		case 'o':
			if ((ofd = open(optarg,
					O_WRONLY|O_CREAT), 0600) == -1) {
				perror(optarg);
				exit(EX_CANTCREAT);
			}
			ofilename = optarg;
			break;
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc != 1) {
		fprintf(stderr, "%s: No target device specifiled\n", appname);
		usage();
		/* NOTREACHED */
	}

	targdevname = *argv;
	if ((targfd = open(targdevname, O_RDWR)) == -1) {
		perror(targdevname);
		exit(EX_NOINPUT);
	}
	
	buf = malloc(bufsize);

	if (buf == NULL) {
		fprintf(stderr, "%s: Could not malloc I/O buffer", appname);
		exit(EX_OSERR);
	}

	pump_events();

	return (0);
}

static void
pump_events()
{
	struct pollfd targpoll;

	targpoll.fd = targfd;
	targpoll.events = POLLRDNORM|POLLWRNORM;

	while (1) {
		int retval;

		retval = poll(&targpoll, 1, INFTIM);

		if (retval == -1) {
			perror("Poll Failed");
			exit(EX_SOFTWARE);
		}

		if (retval == 0) {
			perror("Poll returned 0 although timeout infinite???");
			exit(EX_SOFTWARE);
		}

		if (retval > 1) {
			perror("Poll returned more fds ready than allocated");
			exit(EX_SOFTWARE);
		}

		/* Process events */
		if ((targpoll.revents & POLLERR) != 0) {
			handle_exception();
		}

		if ((targpoll.revents & POLLRDNORM) != 0) {
			printf("Read Poll event came true\n");
			retval = read(targfd, buf, bufsize);

			if (retval == -1) {
				perror("Read from targ failed");
			} else {
				retval = write(ofd, buf, retval);
				if (retval == -1) {
					perror("Write to file failed");
				}
			}
		}

		if ((targpoll.revents & POLLWRNORM) != 0) {
			int amount_read;

			printf("Write Poll event came true\n");
			retval = read(ifd, buf, bufsize);
			if (retval == -1) {
				perror("Read from file failed");
				exit(EX_SOFTWARE);
			}

			amount_read = retval;
			retval = write(targfd, buf, retval);
			if (retval == -1) {
				perror("Write to targ failed");
				retval = 0;
			}

			/* Backup in our input stream on short writes */
			if (retval != amount_read)
				lseek(ifd, retval - amount_read, SEEK_CUR);
		}
	}
}

static void
handle_exception()
{
	targ_exception exceptions;

	if (ioctl(targfd, TARGIOCFETCHEXCEPTION, &exceptions) == -1) {
		perror("TARGIOCFETCHEXCEPTION");
		exit(EX_SOFTWARE);
	}

	if ((exceptions & TARG_EXCEPT_DEVICE_INVALID) != 0) {
		/* Device went away.  Nothing more to do. */
		exit(0);
	}

	if ((exceptions & TARG_EXCEPT_UNKNOWN_ATIO) != 0) {
		struct ccb_accept_tio atio;
		struct ioc_initiator_state ioc_istate;
		struct scsi_sense_data *sense;
		union  ccb ccb;

		if (ioctl(targfd, TARGIOCFETCHATIO, &atio) == -1) {
			perror("TARGIOCFETCHATIO");
			exit(EX_SOFTWARE);
		}

		printf("Ignoring unhandled command 0x%x for Id %d\n",
		       atio.cdb_io.cdb_bytes[0], atio.init_id);

		ioc_istate.initiator_id = atio.init_id;
		if (ioctl(targfd, TARGIOCGETISTATE, &ioc_istate) == -1) {
			perror("TARGIOCGETISTATE");
			exit(EX_SOFTWARE);
		}

		/* Send back Illegal Command code status */
		ioc_istate.istate.pending_ca |= CA_CMD_SENSE;
		sense = &ioc_istate.istate.sense_data;
		bzero(sense, sizeof(*sense));
		sense->error_code = SSD_CURRENT_ERROR;     
		sense->flags = SSD_KEY_ILLEGAL_REQUEST;
		sense->add_sense_code = 0x20;    
		sense->add_sense_code_qual = 0x00;
		sense->extra_len = offsetof(struct scsi_sense_data, fru)
				 - offsetof(struct scsi_sense_data, extra_len);

		if (ioctl(targfd, TARGIOCSETISTATE, &ioc_istate) == -1) {
			perror("TARGIOCSETISTATE");
			exit(EX_SOFTWARE);
		}

		bzero(&ccb, sizeof(ccb));
		cam_fill_ctio(&ccb.csio, /*retries*/2,
			      /*cbfcnp*/NULL,
			      /*flags*/CAM_DIR_NONE
			     | (atio.ccb_h.flags & CAM_TAG_ACTION_VALID)
			     | CAM_SEND_STATUS,
                              /*tag_action*/MSG_SIMPLE_Q_TAG,
			      atio.tag_id,
			      atio.init_id,
			      SCSI_STATUS_CHECK_COND,
			      /*data_ptr*/NULL,
			      /*dxfer_len*/0,
			      /*timeout*/5 * 1000);
		if (ioctl(targfd, TARGIOCCOMMAND, &ccb) == -1) {
			perror("TARGIOCCOMMAND");
			exit(EX_SOFTWARE);
		}
		
	}

	if (ioctl(targfd, TARGIOCCLEAREXCEPTION, &exceptions) == -1) {
		perror("TARGIOCCLEAREXCEPTION");
		exit(EX_SOFTWARE);
	}
}

static void
usage()
{

	(void)fprintf(stderr,
"usage: %-16s [-o output_file] [-i input_file] /dev/targ?\n", appname);

	exit(EX_USAGE);
}

