/*
 * Copyright (C) 2002
 * 	Hidetoshi Shimokawa. All rights reserved.
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
 *
 *	This product includes software developed by Hidetoshi Shimokawa.
 *
 * 4. Neither the name of the author nor the names of its contributors
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
 * 
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/poll.h>
#include <dev/firewire/firewire.h>

#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include <err.h>
#include <stdlib.h>
#include <strings.h>
#include <unistd.h>


struct fwinfo {
	int fd;
	struct fw_asyreq *asyreq;
} fw;

void
usage(void)
{
	printf("fw_tap [-f fwdev] [-t tapdev] channel\n");
	exit(0);
}
static struct fwinfo
init_fw(char *fwdev, int ch)
{
        struct fw_isochreq chreq;
        struct fw_asyreq *asyreq;
	int fd;
	struct fwinfo fw;

	if ((fd = open(fwdev, O_RDWR)) < 0)
		err(1, "open");

	printf("ch=%d\n", ch);
	chreq.ch = ch;
	chreq.tag = 0;
        if (ioctl(fd, FW_SRSTREAM, &chreq) < 0)
		err(1, "ioctl");

        asyreq = (struct fw_asyreq *)malloc(sizeof(struct fw_asyreq));
	asyreq->req.type = FWASREQSTREAM;
	asyreq->req.sped = 2; /* S400 */
	asyreq->pkt.mode.stream.tcode = FWTCODE_STREAM;
	asyreq->pkt.mode.stream.sy = 0;
	asyreq->pkt.mode.stream.chtag = ch;

	fw.asyreq = asyreq;
	fw.fd = fd;

	return fw;
}

static int
init_tap(char *tapdev)
{
	int fd;

	if ((fd = open(tapdev, O_RDWR)) < 0)
		err(1, "open");
	/*
	 * XXX We need to make it be promiscuous mode to discard packets
	 * which upper layer shouldn't see in ether_demux() of if_ethersub.c.
	 * use netgraph?
	 */

	return fd;
}


static void
send_stream(struct fwinfo *fw, int len)
{
        struct fw_asyreq *asyreq;

	asyreq = fw->asyreq;
	asyreq->req.len = len + 4;
	asyreq->pkt.mode.stream.len = htons(len);
	if (ioctl(fw->fd, FW_ASYREQ, asyreq) < 0)
		err(1, "ioctl");
}


#define MTU 2048
#define HDR 4
static void
fw2tap(struct fwinfo fw, int tapd)
{
	int res, size;
	struct pollfd pfd[2];
	char *buf, *fwbuf;
	int fwd;

	fwd = fw.fd;
	pfd[0].fd = fwd;
	pfd[0].events = POLLIN;
	pfd[1].fd = tapd;
	pfd[1].events = POLLIN;
	fwbuf = (char *)&fw.asyreq->pkt.mode.stream.payload[0];
	if ((buf = malloc(MTU + HDR)) == NULL)
		err(1, "malloc");


	while (1) {
		res = poll(pfd, 2, -1);
		if (pfd[0].revents & POLLIN) {
			size = read(fwd, buf, MTU + HDR);
#if 0
			printf("in  %5d bytes\n", size - HDR);
#endif
			write(tapd, buf + HDR, size - HDR);
		}
		if (pfd[1].revents & POLLIN) {
			size = read(tapd, fwbuf, MTU);
#if 0
			printf("out %5d bytes\n", size);
#endif
			send_stream(&fw, size);
		}
	}
}
		
int
main(int argc, char **argv)
{
	int ch;
	int tapd;
	struct fwinfo fw;
	char *fwdev, *tapdev;

	if (argc < 2) {
		usage();
	}

	fwdev = "/dev/fw2";
	tapdev = "/dev/tap0";
	ch = 0;

	argv++;
	argc--;
	while (argc > 0) {
		if (strcmp(*argv, "-f") == 0) {
			/* fw device */
			argv++;
			argc--;
			if (argc > 0) {
				fwdev = *argv;
				argv++;
				argc--;
			} else {
				usage();
			}
		} else if (strcmp(*argv, "-t") == 0) {
			/* tap device */
			argv++;
			argc--;
			if (argc > 0) {
				tapdev = *argv;
				argv++;
				argc--;
			} else {
				usage();
			}
		} else if (argc > 0) {
			ch = strtoul(*argv, (char **)NULL, 0);
			argv++;
			argc--;
		} else {
			usage();
		}
	}
	fw = init_fw(fwdev, ch);
	tapd = init_tap(tapdev);
	fw2tap(fw, tapd);
	return 0;
}
