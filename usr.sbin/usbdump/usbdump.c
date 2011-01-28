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
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct usbcap {
	int		fd;		/* fd for /dev/usbpf */
	u_int		bufsize;
	char		*buffer;

	/* for -w option */
	int		wfd;
	/* for -r option */
	int		rfd;
};

struct usbcap_filehdr {
	u_int		magic;
#define	USBCAP_FILEHDR_MAGIC	0x9a90000e
	u_char		major;
	u_char		minor;
	u_char		reserved[26];
} __packed;

static int doexit = 0;
static int pkt_captured = 0;
static int verbose = 0;
static const char *i_arg = "usbus0";
static const char *r_arg = NULL;
static const char *w_arg = NULL;
static const char *errstr_table[USB_ERR_MAX] = {
	[USB_ERR_NORMAL_COMPLETION]	= "NORMAL_COMPLETION",
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

static const char *xfertype_table[] = {
	[UE_CONTROL]			= "CTRL",
	[UE_ISOCHRONOUS]		= "ISOC",
	[UE_BULK]			= "BULK",
	[UE_INTERRUPT]			= "INTR"
};

static void
handle_sigint(int sig)
{

	(void)sig;
	doexit = 1;
}

static void
print_flags(u_int32_t flags)
{
#define	PRINTFLAGS(name)			\
	if ((flags & USBPF_FLAG_##name) != 0)	\
		printf("%s ", #name);
	printf(" flags %#x", flags);
	printf(" < ");
	PRINTFLAGS(FORCE_SHORT_XFER);
	PRINTFLAGS(SHORT_XFER_OK);
	PRINTFLAGS(SHORT_FRAMES_OK);
	PRINTFLAGS(PIPE_BOF);
	PRINTFLAGS(PROXY_BUFFER);
	PRINTFLAGS(EXT_BUFFER);
	PRINTFLAGS(MANUAL_STATUS);
	PRINTFLAGS(NO_PIPE_OK);
	PRINTFLAGS(STALL_PIPE);
	printf(">\n");
#undef PRINTFLAGS
}

static void
print_status(u_int32_t status)
{
#define	PRINTSTATUS(name)				\
	if ((status & USBPF_STATUS_##name) != 0)	\
		printf("%s ", #name);

	printf(" status %#x", status);
	printf(" < ");
	PRINTSTATUS(OPEN);
	PRINTSTATUS(TRANSFERRING);
	PRINTSTATUS(DID_DMA_DELAY);
	PRINTSTATUS(DID_CLOSE);
	PRINTSTATUS(DRAINING);
	PRINTSTATUS(STARTED);
	PRINTSTATUS(BW_RECLAIMED);
	PRINTSTATUS(CONTROL_XFR);
	PRINTSTATUS(CONTROL_HDR);
	PRINTSTATUS(CONTROL_ACT);
	PRINTSTATUS(CONTROL_STALL);
	PRINTSTATUS(SHORT_FRAMES_OK);
	PRINTSTATUS(SHORT_XFER_OK);
#if USB_HAVE_BUSDMA
	PRINTSTATUS(BDMA_ENABLE);
	PRINTSTATUS(BDMA_NO_POST_SYNC);
	PRINTSTATUS(BDMA_SETUP);
#endif
	PRINTSTATUS(ISOCHRONOUS_XFR);
	PRINTSTATUS(CURR_DMA_SET);
	PRINTSTATUS(CAN_CANCEL_IMMED);
	PRINTSTATUS(DOING_CALLBACK);
	printf(">\n");
#undef PRINTSTATUS
}

/*
 * Display a region in traditional hexdump format.
 */
static void
hexdump(const char *region, size_t len)
{
	const char *line;
	int x;
	int c;
#define EMIT(fmt, ...)	do {		\
	printf(fmt,## __VA_ARGS__);	\
} while (0)

	for (line = region; line < (region + len); line += 16) {
		EMIT(" %04lx  ", (long) (line - region));
		for (x = 0; x < 16; x++) {
			if ((line + x) < (region + len))
				EMIT("%02x ", *(const u_int8_t *)(line + x));
			else
				EMIT("-- ");
			if (x == 7)
				EMIT(" ");
		}
		EMIT(" |");
		for (x = 0; x < 16; x++) {
			if ((line + x) < (region + len)) {
				c = *(const u_int8_t *)(line + x);
				/* !isprint(c) */
				if ((c < ' ') || (c > '~'))
					c = '.';
				EMIT("%c", c);
			} else
				EMIT(" ");
		}
		EMIT("|\n");
	}
#undef EMIT
}

static void
print_apacket(const struct bpf_xhdr *hdr, struct usbpf_pkthdr *up,
    const char *payload)
{
	struct tm *tm;
	struct timeval tv;
	size_t len;
	u_int32_t framelen, x;
	const char *ptr = payload;
	char buf[64];

	/* A packet from the kernel is based on little endian byte order. */
	up->up_busunit = le32toh(up->up_busunit);
	up->up_flags = le32toh(up->up_flags);
	up->up_status = le32toh(up->up_status);
	up->up_length = le32toh(up->up_length);
	up->up_frames = le32toh(up->up_frames);
	up->up_error = le32toh(up->up_error);
	up->up_interval = le32toh(up->up_interval);

	tv.tv_sec = hdr->bh_tstamp.bt_sec;
	tv.tv_usec = hdr->bh_tstamp.bt_frac;
	tm = localtime(&tv.tv_sec);

	len = strftime(buf, sizeof(buf), "%H:%M:%S", tm);
	printf("%.*s.%06ju", (int)len, buf, tv.tv_usec);
	printf(" usbus%d.%d 0x%02x %s %s", up->up_busunit, up->up_address,
	    up->up_endpoint,
	    xfertype_table[up->up_xfertype],
	    up->up_type == USBPF_XFERTAP_SUBMIT ? "S" : "D");
	printf(" (%d/%d)", up->up_frames, up->up_length);
	if (up->up_type == USBPF_XFERTAP_DONE)
		printf(" %s", errstr_table[up->up_error]);
	if (up->up_xfertype == UE_BULK || up->up_xfertype == UE_ISOCHRONOUS)
		printf(" %d", up->up_interval);
	printf("\n");

	if (verbose >= 1) {
		for (x = 0; x < up->up_frames; x++) {
			framelen = le32toh(*((const u_int32_t *)ptr));
			ptr += sizeof(u_int32_t);
			printf(" frame[%u] len %d\n", x, framelen);
			assert(framelen < (1024 * 4));
			hexdump(ptr, framelen);
			ptr += framelen;
		}
	}
	if (verbose >= 2) {
		print_flags(up->up_flags);
		print_status(up->up_status);
	}
}

static void
print_packets(char *data, const int datalen)
{
	struct usbpf_pkthdr *up;
	const struct bpf_xhdr *hdr;
	u_int32_t framelen, x;
	char *ptr, *next;

	for (ptr = data; ptr < (data + datalen); ptr = next) {
		hdr = (const struct bpf_xhdr *)ptr;
		up = (struct usbpf_pkthdr *)(ptr + hdr->bh_hdrlen);
		next = ptr + BPF_WORDALIGN(hdr->bh_hdrlen + hdr->bh_caplen);

		ptr = ((char *)up) + sizeof(struct usbpf_pkthdr);
		if (w_arg == NULL)
			print_apacket(hdr, up, ptr);
		pkt_captured++;
		for (x = 0; x < up->up_frames; x++) {
			framelen = le32toh(*((const u_int32_t *)ptr));
			ptr += sizeof(u_int32_t) + framelen;
		}
	}
}

static void
write_packets(struct usbcap *p, const char *data, const int datalen)
{
	int len = htole32(datalen), ret;

	ret = write(p->wfd, &len, sizeof(int));
	assert(ret == sizeof(int));
	ret = write(p->wfd, data, datalen);
	assert(ret == datalen);
}

static void
read_file(struct usbcap *p)
{
	int datalen, ret;
	char *data;

	while ((ret = read(p->rfd, &datalen, sizeof(int))) == sizeof(int)) {
		datalen = le32toh(datalen);
		data = malloc(datalen);
		assert(data != NULL);
		ret = read(p->rfd, data, datalen);
		assert(ret == datalen);
		print_packets(data, datalen);
		free(data);
	}
	if (ret == -1)
		fprintf(stderr, "read: %s\n", strerror(errno));
}

static void
do_loop(struct usbcap *p)
{
	int cc;

	while (doexit == 0) {
		cc = read(p->fd, (char *)p->buffer, p->bufsize);
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
		fprintf(stderr, "open: %s (%s)\n", r_arg, strerror(errno));
		exit(EXIT_FAILURE);
	}
	ret = read(p->rfd, &uf, sizeof(uf));
	assert(ret == sizeof(uf));
	assert(le32toh(uf.magic) == USBCAP_FILEHDR_MAGIC);
	assert(uf.major == 0);
	assert(uf.minor == 1);
}

static void
init_wfile(struct usbcap *p)
{
	struct usbcap_filehdr uf;
	int ret;

	p->wfd = open(w_arg, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
	if (p->wfd < 0) {
		fprintf(stderr, "open: %s (%s)\n", w_arg, strerror(errno));
		exit(EXIT_FAILURE);
	}
	bzero(&uf, sizeof(uf));
	uf.magic = htole32(USBCAP_FILEHDR_MAGIC);
	uf.major = 0;
	uf.minor = 1;
	ret = write(p->wfd, (const void *)&uf, sizeof(uf));
	assert(ret == sizeof(uf));
}

static void
usage(void)
{

#define FMT "    %-14s %s\n"
	fprintf(stderr, "usage: usbdump [options]\n");
	fprintf(stderr, FMT, "-i ifname", "Listen on USB bus interface");
	fprintf(stderr, FMT, "-r file", "Read the raw packets from file");
	fprintf(stderr, FMT, "-s snaplen", "Snapshot bytes from each packet");
	fprintf(stderr, FMT, "-v", "Increases the verbose level");
	fprintf(stderr, FMT, "-w file", "Write the raw packets to file");
#undef FMT
	exit(1);
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
	u_int v;
	int fd, o;
	const char *optstring;

	bzero(&uc, sizeof(struct usbcap));

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
	if (p->fd < 0) {
		fprintf(stderr, "(no devices found)\n");
		return (EXIT_FAILURE);
	}

	if (ioctl(fd, BIOCVERSION, (caddr_t)&bv) < 0) {
		fprintf(stderr, "BIOCVERSION: %s\n", strerror(errno));
		return (EXIT_FAILURE);
	}
	if (bv.bv_major != BPF_MAJOR_VERSION ||
	    bv.bv_minor < BPF_MINOR_VERSION) {
		fprintf(stderr, "kernel bpf filter out of date");
		return (EXIT_FAILURE);
	}

	if ((ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) || v < 4096)
		v = 4096;
	for ( ; v != 0; v >>= 1) {
		(void)ioctl(fd, BIOCSBLEN, (caddr_t)&v);
		(void)strncpy(ifr.ifr_name, i_arg, sizeof(ifr.ifr_name));
		if (ioctl(fd, BIOCSETIF, (caddr_t)&ifr) >= 0)
			break;
	}
	if (v == 0) {
		fprintf(stderr, "BIOCSBLEN: %s: No buffer size worked", i_arg);
		return (EXIT_FAILURE);
	}

	if (ioctl(fd, BIOCGBLEN, (caddr_t)&v) < 0) {
		fprintf(stderr, "BIOCGBLEN: %s", strerror(errno));
		return (EXIT_FAILURE);
	}

	p->bufsize = v;
	p->buffer = (u_char *)malloc(p->bufsize);
	if (p->buffer == NULL) {
		fprintf(stderr, "malloc: %s", strerror(errno));
		return (EXIT_FAILURE);
	}

	/* XXX no read filter rules yet so at this moment accept everything */
	total_insn.code = (u_short)(BPF_RET | BPF_K);
	total_insn.jt = 0;
	total_insn.jf = 0;
	total_insn.k = snapshot;

	total_prog.bf_len = 1;
	total_prog.bf_insns = &total_insn;
	if (ioctl(p->fd, BIOCSETF, (caddr_t)&total_prog) < 0) {
		fprintf(stderr, "BIOCSETF: %s", strerror(errno));
		return (EXIT_FAILURE);
	}

	/* 1 second read timeout */
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	if (ioctl(p->fd, BIOCSRTIMEOUT, (caddr_t)&tv) < 0) {
		fprintf(stderr, "BIOCSRTIMEOUT: %s", strerror(errno));
		return (EXIT_FAILURE);
	}

	(void)signal(SIGINT, handle_sigint);

	do_loop(p);

	if (ioctl(fd, BIOCGSTATS, (caddr_t)&us) < 0) {
		fprintf(stderr, "BIOCGSTATS: %s", strerror(errno));
		return (EXIT_FAILURE);
	}

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
