/*-
 * Copyright (c) 2010 Weongyo Jeong <weongyo@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <net/if.h>
#include <net/bpf.h>
#include <dev/usb/usb.h>
#include <dev/usb/usb_pf.h>
#include <dev/usb/usbdi.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sysexits.h>
#include <err.h>

struct usbcap {
	int		fd;		/* fd for /dev/usbpf */
	uint32_t	bufsize;
	uint8_t		*buffer;

	/* for -w option */
	int		wfd;
	/* for -r option */
	int		rfd;
};

struct usbcap_filehdr {
	uint32_t	magic;
#define	USBCAP_FILEHDR_MAGIC	0x9a90000e
	uint8_t   	major;
	uint8_t		minor;
	uint8_t		reserved[26];
} __packed;

#if __FreeBSD_version < 900000
#define	bpf_xhdr bpf_hdr
#define	bt_sec tv_sec
#define	bt_frac tv_usec
#endif

static int doexit = 0;
static int pkt_captured = 0;
static int verbose = 0;
static const char *i_arg = "usbus0";
static const char *r_arg = NULL;
static const char *w_arg = NULL;
static const char *errstr_table[USB_ERR_MAX] = {
	[USB_ERR_NORMAL_COMPLETION]	= "0",
	[USB_ERR_PENDING_REQUESTS]	= "PENDING_REQUESTS",
	[USB_ERR_NOT_STARTED]		= "NOT_STARTED",
	[USB_ERR_INVAL]			= "INVAL",
	[USB_ERR_NOMEM]			= "NOMEM",
	[USB_ERR_CANCELLED]		= "CANCELLED",
	[USB_ERR_BAD_ADDRESS]		= "BAD_ADDRESS",
	[USB_ERR_BAD_BUFSIZE]		= "BAD_BUFSIZE",
	[USB_ERR_BAD_FLAG]		= "BAD_FLAG",
	[USB_ERR_NO_CALLBACK]		= "NO_CALLBACK",
	[USB_ERR_IN_USE]		= "IN_USE",
	[USB_ERR_NO_ADDR]		= "NO_ADDR",
	[USB_ERR_NO_PIPE]		= "NO_PIPE",
	[USB_ERR_ZERO_NFRAMES]		= "ZERO_NFRAMES",
	[USB_ERR_ZERO_MAXP]		= "ZERO_MAXP",
	[USB_ERR_SET_ADDR_FAILED]	= "SET_ADDR_FAILED",
	[USB_ERR_NO_POWER]		= "NO_POWER",
	[USB_ERR_TOO_DEEP]		= "TOO_DEEP",
	[USB_ERR_IOERROR]		= "IOERROR",
	[USB_ERR_NOT_CONFIGURED]	= "NOT_CONFIGURED",
	[USB_ERR_TIMEOUT]		= "TIMEOUT",
	[USB_ERR_SHORT_XFER]		= "SHORT_XFER",
	[USB_ERR_STALLED]		= "STALLED",
	[USB_ERR_INTERRUPTED]		= "INTERRUPTED",
	[USB_ERR_DMA_LOAD_FAILED]	= "DMA_LOAD_FAILED",
	[USB_ERR_BAD_CONTEXT]		= "BAD_CONTEXT",
	[USB_ERR_NO_ROOT_HUB]		= "NO_ROOT_HUB",
	[USB_ERR_NO_INTR_THREAD]	= "NO_INTR_THREAD",
	[USB_ERR_NOT_LOCKED]		= "NOT_LOCKED",
};

static const char *xfertype_table[4] = {
	[UE_CONTROL]			= "CTRL",
	[UE_ISOCHRONOUS]		= "ISOC",
	[UE_BULK]			= "BULK",
	[UE_INTERRUPT]			= "INTR"
};

static const char *speed_table[USB_SPEED_MAX] = {
	[USB_SPEED_FULL] = "FULL",
	[USB_SPEED_HIGH] = "HIGH",
	[USB_SPEED_LOW] = "LOW",
	[USB_SPEED_VARIABLE] = "VARI",
	[USB_SPEED_SUPER] = "SUPER",
};

static void
handle_sigint(int sig)
{

	(void)sig;
	doexit = 1;
}

#define	FLAGS(x, name)	\
	(((x) & USBPF_FLAG_##name) ? #name "|" : "")

#define	STATUS(x, name) \
	(((x) & USBPF_STATUS_##name) ? #name "|" : "")

static const char *
usb_errstr(uint32_t error)
{
	if (error >= USB_ERR_MAX || errstr_table[error] == NULL)
		return ("UNKNOWN");
	else
		return (errstr_table[error]);
}

static const char *
usb_speedstr(uint8_t speed)
{
	if (speed >= USB_SPEED_MAX  || speed_table[speed] == NULL)
		return ("UNKNOWN");
	else
		return (speed_table[speed]);
}

static void
print_flags(uint32_t flags)
{
	printf(" flags %#x <%s%s%s%s%s%s%s%s%s0>\n",
	    flags,
	    FLAGS(flags, FORCE_SHORT_XFER),
	    FLAGS(flags, SHORT_XFER_OK),
	    FLAGS(flags, SHORT_FRAMES_OK),
	    FLAGS(flags, PIPE_BOF),
	    FLAGS(flags, PROXY_BUFFER),
	    FLAGS(flags, EXT_BUFFER),
	    FLAGS(flags, MANUAL_STATUS),
	    FLAGS(flags, NO_PIPE_OK),
	    FLAGS(flags, STALL_PIPE));
}

static void
print_status(uint32_t status)
{
	printf(" status %#x <%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s0>\n",
	    status, 
	    STATUS(status, OPEN),
	    STATUS(status, TRANSFERRING),
	    STATUS(status, DID_DMA_DELAY),
	    STATUS(status, DID_CLOSE),
	    STATUS(status, DRAINING),
	    STATUS(status, STARTED),
	    STATUS(status, BW_RECLAIMED),
	    STATUS(status, CONTROL_XFR),
	    STATUS(status, CONTROL_HDR),
	    STATUS(status, CONTROL_ACT),
	    STATUS(status, CONTROL_STALL),
	    STATUS(status, SHORT_FRAMES_OK),
	    STATUS(status, SHORT_XFER_OK),
	    STATUS(status, BDMA_ENABLE),
	    STATUS(status, BDMA_NO_POST_SYNC),
	    STATUS(status, BDMA_SETUP),
	    STATUS(status, ISOCHRONOUS_XFR),
	    STATUS(status, CURR_DMA_SET),
	    STATUS(status, CAN_CANCEL_IMMED),
	    STATUS(status, DOING_CALLBACK));
}

/*
 * Dump a byte into hex format.
 */
static void
hexbyte(char *buf, uint8_t temp)
{
	uint8_t lo;
	uint8_t hi;

	lo = temp & 0xF;
	hi = temp >> 4;

	if (hi < 10)
		buf[0] = '0' + hi;
	else
		buf[0] = 'A' + hi - 10;

	if (lo < 10)
		buf[1] = '0' + lo;
	else
		buf[1] = 'A' + lo - 10;
}

/*
 * Display a region in traditional hexdump format.
 */
static void
hexdump(const uint8_t *region, uint32_t len)
{
	const uint8_t *line;
	char linebuf[128];
	int i;
	int x;
	int c;

	for (line = region; line < (region + len); line += 16) {

		i = 0;

		linebuf[i] = ' ';
		hexbyte(linebuf + i + 1, ((line - region) >> 8) & 0xFF);
		hexbyte(linebuf + i + 3, (line - region) & 0xFF);
		linebuf[i + 5] = ' ';
		linebuf[i + 6] = ' ';
		i += 7;

		for (x = 0; x < 16; x++) {
		  if ((line + x) < (region + len)) {
			hexbyte(linebuf + i,
			    *(const u_int8_t *)(line + x));
		  } else {
			  linebuf[i] = '-';
			  linebuf[i + 1] = '-';
			}
			linebuf[i + 2] = ' ';
			if (x == 7) {
			  linebuf[i + 3] = ' ';
			  i += 4;
			} else {
			  i += 3;
			}
		}
		linebuf[i] = ' ';
		linebuf[i + 1] = '|';
		i += 2;
		for (x = 0; x < 16; x++) {
			if ((line + x) < (region + len)) {
				c = *(const u_int8_t *)(line + x);
				/* !isprint(c) */
				if ((c < ' ') || (c > '~'))
					c = '.';
				linebuf[i] = c;
			} else {
				linebuf[i] = ' ';
			}
			i++;
		}
		linebuf[i] = '|';
		linebuf[i + 1] = 0;
		i += 2;
		puts(linebuf);
	}
}

static void
print_apacket(const struct bpf_xhdr *hdr, const uint8_t *ptr, int ptr_len)
{
	struct tm *tm;
	struct usbpf_pkthdr up_temp;
	struct usbpf_pkthdr *up;
	struct timeval tv;
	size_t len;
	uint32_t x;
	char buf[64];

	ptr += USBPF_HDR_LEN;
	ptr_len -= USBPF_HDR_LEN;
	if (ptr_len < 0)
		return;

	/* make sure we don't change the source buffer */
	memcpy(&up_temp, ptr - USBPF_HDR_LEN, sizeof(up_temp));
	up = &up_temp;

	/*
	 * A packet from the kernel is based on little endian byte
	 * order.
	 */
	up->up_totlen = le32toh(up->up_totlen);
	up->up_busunit = le32toh(up->up_busunit);
	up->up_address = le32toh(up->up_address);
	up->up_flags = le32toh(up->up_flags);
	up->up_status = le32toh(up->up_status);
	up->up_error = le32toh(up->up_error);
	up->up_interval = le32toh(up->up_interval);
	up->up_frames = le32toh(up->up_frames);
	up->up_packet_size = le32toh(up->up_packet_size);
	up->up_packet_count = le32toh(up->up_packet_count);
	up->up_endpoint = le32toh(up->up_endpoint);

	tv.tv_sec = hdr->bh_tstamp.bt_sec;
	tv.tv_usec = hdr->bh_tstamp.bt_frac;
	tm = localtime(&tv.tv_sec);

	len = strftime(buf, sizeof(buf), "%H:%M:%S", tm);

	printf("%.*s.%06ld usbus%d.%d %s-%s-EP=%08x,SPD=%s,NFR=%d,SLEN=%d,IVAL=%d%s%s\n",
	    (int)len, buf, tv.tv_usec,
	    (int)up->up_busunit, (int)up->up_address,
	    (up->up_type == USBPF_XFERTAP_SUBMIT) ? "SUBM" : "DONE",
	    xfertype_table[up->up_xfertype],
	    (unsigned int)up->up_endpoint,
	    usb_speedstr(up->up_speed),
	    (int)up->up_frames,
	    (int)(up->up_totlen - USBPF_HDR_LEN -
	    (USBPF_FRAME_HDR_LEN * up->up_frames)),
	    (int)up->up_interval,
	    (up->up_type == USBPF_XFERTAP_DONE) ? ",ERR=" : "",
	    (up->up_type == USBPF_XFERTAP_DONE) ?
	    usb_errstr(up->up_error) : "");

	if (verbose >= 1) {
		for (x = 0; x != up->up_frames; x++) {
			const struct usbpf_framehdr *uf;
			uint32_t framelen;
			uint32_t flags;

			uf = (const struct usbpf_framehdr *)ptr;
			ptr += USBPF_FRAME_HDR_LEN;
			ptr_len -= USBPF_FRAME_HDR_LEN;
			if (ptr_len < 0)
				return;

			framelen = le32toh(uf->length);
			flags = le32toh(uf->flags);

			printf(" frame[%u] %s %d bytes\n",
			    (unsigned int)x,
			    (flags & USBPF_FRAMEFLAG_READ) ? "READ" : "WRITE",
			    (int)framelen);

			if (flags & USBPF_FRAMEFLAG_DATA_FOLLOWS) {

				int tot_frame_len;

				tot_frame_len = USBPF_FRAME_ALIGN(framelen);

				ptr_len -= tot_frame_len;

				if (tot_frame_len < 0 ||
				    (int)framelen < 0 || (int)ptr_len < 0)
					break;

				hexdump(ptr, framelen);

				ptr += tot_frame_len;
			}
		}
	}
	if (verbose >= 2)
		print_flags(up->up_flags);
	if (verbose >= 3)
		print_status(up->up_status);
}

static void
print_packets(uint8_t *data, const int datalen)
{
	const struct bpf_xhdr *hdr;
	uint8_t *ptr;
	uint8_t *next;

	for (ptr = data; ptr < (data + datalen); ptr = next) {
		hdr = (const struct bpf_xhdr *)ptr;
		next = ptr + BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen);

		if (w_arg == NULL) {
			print_apacket(hdr, ptr +
			    hdr->bh_hdrlen, hdr->bh_caplen);
		}
		pkt_captured++;
	}
}

static void
write_packets(struct usbcap *p, const uint8_t *data, const int datalen)
{
	int len = htole32(datalen);
	int ret;

	ret = write(p->wfd, &len, sizeof(int));
	if (ret != sizeof(int)) {
		err(EXIT_FAILURE, "Could not write length "
		    "field of USB data payload");
	}
	ret = write(p->wfd, data, datalen);
	if (ret != datalen) {
		err(EXIT_FAILURE, "Could not write "
		    "complete USB data payload");
	}
}

static void
read_file(struct usbcap *p)
{
	int datalen;
	int ret;
	uint8_t *data;

	while ((ret = read(p->rfd, &datalen, sizeof(int))) == sizeof(int)) {
		datalen = le32toh(datalen);
		data = malloc(datalen);
		if (data == NULL)
			errx(EX_SOFTWARE, "Out of memory.");
		ret = read(p->rfd, data, datalen);
		if (ret != datalen) {
			err(EXIT_FAILURE, "Could not read complete "
			    "USB data payload");
		}
		print_packets(data, datalen);
		free(data);
	}
}

static void
do_loop(struct usbcap *p)
{
	int cc;

	while (doexit == 0) {
		cc = read(p->fd, (uint8_t *)p->buffer, p->bufsize);
		if (cc < 0) {
			switch (errno) {
			case EINTR:
				break;
			default:
				fprintf(stderr, "read: %s\n", strerror(errno));
				return;
			}
			continue;
		}
		if (cc == 0)
			continue;
		if (w_arg != NULL)
			write_packets(p, p->buffer, cc);
		print_packets(p->buffer, cc);
	}
}

static void
init_rfile(struct usbcap *p)
{
	struct usbcap_filehdr uf;
	int ret;

	p->rfd = open(r_arg, O_RDONLY);
	if (p->rfd < 0) {
		err(EXIT_FAILURE, "Could not open "
		    "'%s' for read", r_arg);
	}
	ret = read(p->rfd, &uf, sizeof(uf));
	if (ret != sizeof(uf)) {
		err(EXIT_FAILURE, "Could not read USB capture "
		    "file header");
	}
	if (le32toh(uf.magic) != USBCAP_FILEHDR_MAGIC) {
		errx(EX_SOFTWARE, "Invalid magic field(0x%08x) "
		    "in USB capture file header.",
		    (unsigned int)le32toh(uf.magic));
	}
	if (uf.major != 0) {
		errx(EX_SOFTWARE, "Invalid major version(%d) "
		    "field in USB capture file header.", (int)uf.major);
	}
	if (uf.minor != 2) {
		errx(EX_SOFTWARE, "Invalid minor version(%d) "
		    "field in USB capture file header.", (int)uf.minor);
	}
}

static void
init_wfile(struct usbcap *p)
{
	struct usbcap_filehdr uf;
	int ret;

	p->wfd = open(w_arg, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (p->wfd < 0) {
		err(EXIT_FAILURE, "Could not open "
		    "'%s' for write", r_arg);
	}
	memset(&uf, 0, sizeof(uf));
	uf.magic = htole32(USBCAP_FILEHDR_MAGIC);
	uf.major = 0;
	uf.minor = 2;
	ret = write(p->wfd, (const void *)&uf, sizeof(uf));
	if (ret != sizeof(uf)) {
		err(EXIT_FAILURE, "Could not write "
		    "USB capture header");
	}
}

static void
usage(void)
{

#define FMT "    %-14s %s\n"
	fprintf(stderr, "usage: usbdump [options]\n");
	fprintf(stderr, FMT, "-i <usbusX>", "Listen on USB bus interface");
	fprintf(stderr, FMT, "-r <file>", "Read the raw packets from file");
	fprintf(stderr, FMT, "-s <snaplen>", "Snapshot bytes from each packet");
	fprintf(stderr, FMT, "-v", "Increase the verbose level");
	fprintf(stderr, FMT, "-w <file>", "Write the raw packets to file");
#undef FMT
	exit(EX_USAGE);
}

int
main(int argc, char *argv[])
{
	struct timeval tv;
	struct bpf_insn total_insn;
	struct bpf_program total_prog;
	struct bpf_stat us;
	struct bpf_version bv;
	struct usbcap uc, *p = &uc;
	struct ifreq ifr;
	long snapshot = 192;
	uint32_t v;
	int fd, o;
	const char *optstring;

	memset(&uc, 0, sizeof(struct usbcap));

	optstring = "i:r:s:vw:";
	while ((o = getopt(argc, argv, optstring)) != -1) {
		switch (o) {
		case 'i':
			i_arg = optarg;
			break;
		case 'r':
			r_arg = optarg;
			init_rfile(p);
			break;
		case 's':
			snapshot = strtol(optarg, NULL, 10);
			errno = 0;
			if (snapshot == 0 && errno == EINVAL)
				usage();
			/* snapeshot == 0 is special */
			if (snapshot == 0)
				snapshot = -1;
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			w_arg = optarg;
			init_wfile(p);
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (r_arg != NULL) {
		read_file(p);
		exit(EXIT_SUCCESS);
	}

	p->fd = fd = open("/dev/bpf", O_RDONLY);
	if (p->fd < 0)
		err(EXIT_FAILURE, "Could not open BPF device");

	if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0)
		err(EXIT_FAILURE, "BIOCVERSION ioctl failed");

	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION)
		errx(EXIT_FAILURE, "Kernel BPF filter out of date");

	/* USB transfers can be greater than 64KByte */
	v = 1U << 16;

	/* clear ifr structure */
	memset(&ifr, 0, sizeof(ifr));

	for ( ; v >= USBPF_HDR_LEN; v >>= 1) {
		(void)ioctl(fd, BIOCSBLEN, (caddr_t)&v);
		(void)strncpy(ifr.ifr_name, i_arg, sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
			break;
	}
	if (v == 0)
		errx(EXIT_FAILURE, "No buffer size worked.");

	if (ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0)
		err(EXIT_FAILURE, "BIOCGBLEN ioctl failed");

	p->bufsize = v;
	p->buffer = (uint8_t *)malloc(p->bufsize);
	if (p->buffer == NULL)
		errx(EX_SOFTWARE, "Out of memory.");

	/* XXX no read filter rules yet so at this moment accept everything */
	total_insn.code = (u_short)(BPF_RET | BPF_K);
	total_insn.jt = 0;
	total_insn.jf = 0;
	total_insn.k = snapshot;

	total_prog.bf_len = 1;
	total_prog.bf_insns = &total_insn;
	if (ioctl(p->fd, BIOCSETF, (caddr_t)&total_prog) < 0)
		err(EXIT_FAILURE, "BIOCSETF ioctl failed");

	/* 1 second read timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (ioctl(p->fd, BIOCSRTIMEOUT, (caddr_t)&tv) < 0)
		err(EXIT_FAILURE, "BIOCSRTIMEOUT ioctl failed");

	(void)signal(SIGINT, handle_sigint);

	do_loop(p);

	if (ioctl(fd, BIOCGSTATS, (caddr_t)&us) < 0)
		err(EXIT_FAILURE, "BIOCGSTATS ioctl failed");

	/* XXX what's difference between pkt_captured and us.us_recv? */
	printf("\n");
	printf("%d packets captured\n", pkt_captured);
	printf("%d packets received by filter\n", us.bs_recv);
	printf("%d packets dropped by kernel\n", us.bs_drop);

	if (p->fd > 0)
		close(p->fd);
	if (p->rfd > 0)
		close(p->rfd);
	if (p->wfd > 0)
		close(p->wfd);

	return (EXIT_SUCCESS);
}
