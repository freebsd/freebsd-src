/*
 * Copyright (C) 2003
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
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>

#if __FreeBSD_version >= 500000
#include <arpa/inet.h>
#endif

#include <err.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <dev/firewire/firewire.h>
#include <dev/firewire/iec68113.h>


struct frac {
	int n,d;	
};

struct frac frame_cycle[2]  = {
	{8000*100, 2997},	/* NTSC 8000 cycle / 29.97 Hz */
	{320, 1},		/* PAL  8000 cycle / 25 Hz */
};
int npackets[] = {
	250		/* NTSC */,
	300		/* PAL */
};
struct frac pad_rate[2]  = {
	{203, 2997},	/* = (8000 - 29.97 * 250)/(29.97 * 250) */
	{1, 15},	/* = (8000 - 25 * 300)/(25 * 300) */
};

#define PSIZE 512
#define DSIZE 480
#define NVEC 50
#define BUFSIZE (PSIZE * 256)
#define	BLOCKSIZE 80
#define MAXBLOCKS (300*6)
#define CYCLE_FRAC 0xc00

int dvrecv(int d, char *filename, char ich, int count)
{
	struct fw_isochreq isoreq;
	struct fw_isobufreq bufreq;
	struct dvdbc *dv;
	struct ciphdr *ciph;
	struct fw_pkt *pkt;
	char *pad, *buf;
	u_int32_t *ptr;
	int len, npad, fd, k, m, vec, pal, nb;
	int nblocks[] = {250*6 /* NTSC */, 300*6 /* PAL */};
	struct iovec wbuf[NVEC];
	struct iovec *v;

	fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0660);
	buf = (char *)malloc(BUFSIZE);
	pad = (char *)malloc(BLOCKSIZE*MAXBLOCKS);
	bzero(pad, BLOCKSIZE*MAXBLOCKS);
	bzero(wbuf, sizeof(wbuf));

	bufreq.rx.nchunk = 16;
	bufreq.rx.npacket = 256;
	bufreq.rx.psize = PSIZE;
	bufreq.tx.nchunk = 0;
	bufreq.tx.npacket = 0;
	bufreq.tx.psize = 0;
	if (ioctl(d, FW_SSTBUF, &bufreq) < 0) {
		err(1, "ioctl");
	}

	isoreq.ch = ich & 0x3f;
	isoreq.tag = (ich >> 6) & 3;

	if( ioctl(d, FW_SRSTREAM, &isoreq) < 0)
       		err(1, "ioctl");

	k = m = 0;
	while (count <= 0 || k <= count) {
		vec = 0;
		wbuf[0].iov_len = 0;
		len = read(d, buf, BUFSIZE);
		if (len < 0) {
			if (errno == EAGAIN) {
				fprintf(stderr, "(EAGAIN)\n");
				fflush(stderr);
				continue;
			}
			err(1, "read failed");
		}
		ptr = (u_int32_t *) buf;
again:
		pkt = (struct fw_pkt *) ptr;	
		if (ntohs(pkt->mode.stream.len) <= sizeof(struct ciphdr)) {
			ptr ++; 	/* skip header */
			ptr += ntohs(pkt->mode.stream.len)/sizeof(u_int32_t);
			goto next;
		}
#if 0
		fprintf(stderr, "%08x %08x %08x %08x\n",
			htonl(ptr[0]), htonl(ptr[1]),
			htonl(ptr[2]), htonl(ptr[3]));
#endif
		ciph = (struct ciphdr *)(ptr + 1);	/* skip iso header */
		if (ciph->fmt != CIP_FMT_DVCR)
			err(1, "unknown format 0x%x", ciph->fmt);
		ptr = (u_int32_t *) (ciph + 1);		/* skip cip header */
#if 0
		if (ciph->fdf.dv.cyc != 0xffff && k == 0) {
			fprintf(stderr, "0x%04x\n", ntohs(ciph->fdf.dv.cyc));
		}
#endif
		for (dv = (struct dvdbc *)ptr;
				(char *)dv < (char *)(ptr + ciph->len);
				dv++) {

#if 0
			fprintf(stderr, "(%d,%d) ", dv->sct, dv->dseq);
#endif
			if  (dv->sct == DV_SCT_HEADER && dv->dseq == 0) {
#if 0
				fprintf(stderr, "%d(%d) ", k, m);
#else
				fprintf(stderr, "%d", k%10);
#endif
				pal = ((dv->payload[0] & DV_DSF_12) != 0);
				nb = nblocks[pal];
#if 1
				if (m > 0 && m != nb) {
					/* padding bad frame */
					npad = ((nb - m) % nb);
					if (npad < 0)
						npad += nb;
					fprintf(stderr, "(%d blocks padded)",
								npad);
					npad *= BLOCKSIZE;
					npad = write(fd, pad, npad);
				}
#endif
				k++;
				if (k % 30 == 0) { /* every second */
					fprintf(stderr, "\n");
				}
				fflush(stderr);
				m = 0;
			}
			if (k == 0 || (count > 0 && k > count))
				continue;
			m++;
			v = &wbuf[vec];
			if ((v->iov_base + v->iov_len) == (char *) dv) {
				v->iov_len += sizeof(struct dvdbc);
			} else {
				if (v->iov_len != 0)
					vec++;
				if (vec >= NVEC) {
					writev(fd, wbuf, vec);
					vec = 0;
				}
				v = &wbuf[vec];
				v->iov_base = (char *) dv;
				v->iov_len = sizeof(struct dvdbc);
			}
		}
		ptr = (u_int32_t *)dv;
next:
		if ((char *)ptr < buf + len) {
			goto again;
		}
		if (wbuf[0].iov_len > 0) {
			writev(fd, wbuf, vec+1);
		}
	}
	close(fd);
	fprintf(stderr, "\n");
	return 0;
}

int dvsend(int d, char *filename, char ich, int count)
{
	struct fw_isochreq isoreq;
	struct fw_isobufreq bufreq;
	struct dvdbc *dv;
	struct ciphdr *ciph;
	struct fw_pkt *pkt;
	int len, dlen, header, fd, frames, packets, vec;
	int system=0, pad_acc, cycle_acc, cycle, f_cycle, f_frac; 
	struct iovec wbuf[NVEC];
	char *pbuf;
	u_int32_t hdr[3];
	struct timeval start, end;
	double rtime;

	fd = open(filename, O_RDONLY);
	pbuf = (char *)malloc(BUFSIZE);
	bzero(wbuf, sizeof(wbuf));

	bufreq.rx.nchunk = 0;
	bufreq.rx.npacket = 0;
	bufreq.rx.psize = 0;
	bufreq.tx.nchunk = 10;
	bufreq.tx.npacket = 255;
	bufreq.tx.psize = PSIZE;
	if (ioctl(d, FW_SSTBUF, &bufreq) < 0) {
		err(1, "ioctl");
	}

	isoreq.ch = ich & 0x3f;
	isoreq.tag = (ich >> 6) & 3;

	if( ioctl(d, FW_STSTREAM, &isoreq) < 0)
       		err(1, "ioctl");

	bzero(hdr, sizeof(hdr));
	pkt = (struct fw_pkt *) &hdr[0];
	pkt->mode.stream.len = htons(DSIZE + sizeof(struct ciphdr));
	pkt->mode.stream.sy = 0;
	pkt->mode.stream.tcode = FWTCODE_STREAM;
	pkt->mode.stream.chtag = ich;

	ciph = (struct ciphdr *) &hdr[1];
	ciph->src = 0;	/* XXX */
	ciph->len = 120;
	ciph->dbc = 0;
	ciph->eoh1 = 1;
	ciph->fdf.dv.cyc = 0xffff;

	gettimeofday(&start, NULL);
#if 0
	fprintf(stderr, "%08x %08x %08x\n",
			htonl(hdr[0]), htonl(hdr[1]), htonl(hdr[2]));
#endif
	frames = 0;
	packets = 0;
	pad_acc = 0;
	cycle_acc = frame_cycle[system].d * 1;
	cycle = 1;
	while (1) {
		vec = 0;
		wbuf[0].iov_len = 0;
		dlen = 0;
		while (dlen < DSIZE) {
			len = read(fd, pbuf + dlen, DSIZE - dlen);
			if (len <= 0) {
				fprintf(stderr, "\nend of file(len=%d)\n", len);
				goto send_end;
			}
			dlen += len;
		}
		dv = (struct dvdbc *)pbuf;
#if 0
		header = (dv->sct == 0 && dv->dseq == 0);
#else
		header = (packets % npackets[system] == 0);
#endif

		if (header) {
			fprintf(stderr, "%d", frames % 10);
			frames ++;
			if (count > 0 && frames > count)
				break;
			if (frames % 30 == 0)
				fprintf(stderr, "\n");
			fflush(stderr);
			system = ((dv->payload[0] & DV_DSF_12) != 0);
			f_cycle = (cycle_acc / frame_cycle[system].d) & 0xf;
			f_frac = (cycle_acc % frame_cycle[system].d
					* CYCLE_FRAC) / frame_cycle[system].d;
#if 0
			ciph->fdf.dv.cyc = htons(f_cycle << 12 | f_frac);
#else
			ciph->fdf.dv.cyc = htons(cycle << 12 | f_frac);
#endif
			cycle_acc += frame_cycle[system].n;
			cycle_acc %= frame_cycle[system].d * 0x10;

		} else {
			ciph->fdf.dv.cyc = 0xffff;	/* XXX */
		}
		ciph->dbc = packets++ % 256;
		wbuf[0].iov_base = (char *)&hdr[0];
		wbuf[0].iov_len = 3 * sizeof(u_int32_t);
		wbuf[1].iov_base = pbuf;
		wbuf[1].iov_len = 480;

		pad_acc += pad_rate[system].n;
		if (pad_acc >= pad_rate[system].d) {
			pad_acc -= pad_rate[system].d;
			pkt->mode.stream.len = htons(sizeof(struct ciphdr));
			cycle ++;
again1:
			len = writev(d, wbuf, 1);
			if (len < 0) {
				if (errno == EAGAIN) {
					fprintf(stderr, "(EAGAIN)\n");
					fflush(stderr);
					goto again1;
				}
				err(1, "write failed");
			}
		}

		pkt->mode.stream.len = htons(480 + sizeof(struct ciphdr));
		cycle ++;
again2:
		len = writev(d, wbuf, 2);
		if (len < 0) {
			if (errno == EAGAIN) {
				fprintf(stderr, "(EAGAIN)\n");
				fflush(stderr);
				goto again2;
			}
			err(1, "write failed");
		}
	}
	close(fd);
	fprintf(stderr, "\n");
send_end:
	gettimeofday(&end, NULL);
	rtime = end.tv_sec - start.tv_sec 
			+ (end.tv_usec - start.tv_usec) * 1e-6;
	fprintf(stderr, "%d frames, %.2f secs, %.2f frames/sec\n",
			frames, rtime, frames/rtime);
	return 0;
}


#if 0
int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch;
	int fd;
	char devname[] = "/dev/fw0";

	while ((ch = getopt(argc, argv, "I:")) != -1){
		switch(ch) {
			case 'I':
				strcpy(devname, optarg);
		     		break;
			default:
				usage();
				break;
		}
	}
	argc -= optind;
	argv += optind;

	if (argc < 2)
		usage(); 

	fd = open(devname, O_RDWR);
	if (fd < 0)
		err(1, "open");
	dvrec(fd, argv[0], (TAG<<6) | CHANNEL, atoi(argv[1]));
	close(fd);
	exit(0);
}

#endif
