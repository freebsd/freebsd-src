/*-
 * Copyright (c) 2020 Hans Petter Selasky
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/endian.h>
#include <sys/uio.h>
#include <sys/soundcard.h>

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <poll.h>
#include <sysexits.h>

#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#include <net/if.h>
#include <net/if_vlan_var.h>
#include <net/bpf.h>

#include <arpa/inet.h>

#include <pthread.h>

#include "int.h"

#define	VOSS_HTTPD_BIND_MAX 8
#define	VOSS_HTTPD_MAX_STREAM_TIME (60 * 60 * 3)	/* seconds */

struct http_state {
	int	fd;
	uint64_t ts;
};

struct rtp_raw_packet {
	struct {
		uint32_t padding;
		uint8_t	dhost[6];
		uint8_t	shost[6];
		uint16_t ether_type;
	} __packed eth;
	struct {
		uint8_t	hl_ver;
		uint8_t	tos;
		uint16_t len;
		uint16_t ident;
		uint16_t offset;
		uint8_t	ttl;
		uint8_t	protocol;
		uint16_t chksum;
		union {
			uint32_t sourceip;
			uint16_t source16[2];
		};
		union {
			uint32_t destip;
			uint16_t dest16[2];
		};
	} __packed ip;
	struct {
		uint16_t srcport;
		uint16_t dstport;
		uint16_t len;
		uint16_t chksum;
	} __packed udp;
	union {
		uint8_t	header8[12];
		uint16_t header16[6];
		uint32_t header32[3];
	} __packed rtp;

} __packed;

static const char *
voss_httpd_bind_rtp(vclient_t *pvc, const char *ifname, int *pfd)
{
	const char *perr = NULL;
	struct vlanreq vr = {};
	struct ifreq ifr = {};
	int fd;

	fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (fd < 0) {
		perr = "Cannot open raw RTP socket";
		goto done;
	}

	strlcpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	ifr.ifr_data = (void *)&vr;

	if (ioctl(fd, SIOCGETVLAN, &ifr) == 0)
		pvc->profile->http.rtp_vlanid = vr.vlr_tag;
	else
		pvc->profile->http.rtp_vlanid = 0;

	close(fd);

	ifr.ifr_data = NULL;

	*pfd = fd = open("/dev/bpf", O_RDWR);
	if (fd < 0) {
		perr = "Cannot open BPF device";
		goto done;
	}

	if (ioctl(fd, BIOCSETIF, &ifr) != 0) {
		perr = "Cannot bind BPF device to network interface";
		goto done;
	}
done:
	if (perr != NULL && fd > -1)
		close(fd);
	return (perr);
}

static uint16_t
voss_ipv4_csum(const void *vptr, size_t count)
{
	const uint16_t *ptr = vptr;
	uint32_t sum = 0;

	while (count--)
		sum += *ptr++;

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (~sum);
}

static uint16_t
voss_udp_csum(uint32_t sum, const void *vhdr, size_t count,
    const uint16_t *ptr, size_t length)
{
	const uint16_t *hdr = vhdr;

	while (count--)
		sum += *hdr++;

	while (length > 1) {
		sum += *ptr++;
		length -= 2;
	}

	if (length & 1)
		sum += *__DECONST(uint8_t *, ptr);

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return (~sum);
}

static void
voss_httpd_send_rtp_sub(vclient_t *pvc, int fd, void *ptr, size_t len, uint32_t ts)
{
	struct rtp_raw_packet pkt = {};
	struct iovec iov[2];
	size_t total_ip;
	uint16_t port = atoi(pvc->profile->http.rtp_port);
	size_t x;

	/* NOTE: BPF filter will insert VLAN header for us */
	memset(pkt.eth.dhost, 255, sizeof(pkt.eth.dhost));
	memset(pkt.eth.shost, 1, sizeof(pkt.eth.shost));
	pkt.eth.ether_type = htobe16(0x0800);
	total_ip = sizeof(pkt.ip) + sizeof(pkt.udp) + sizeof(pkt.rtp) + len;

	iov[0].iov_base = pkt.eth.dhost;
	iov[0].iov_len = 14 + total_ip - len;

	iov[1].iov_base = alloca(len);
	iov[1].iov_len = len;

	/* byte swap data - WAV files are 16-bit little endian */
	for (x = 0; x != (len / 2); x++)
		((uint16_t *)iov[1].iov_base)[x] = bswap16(((uint16_t *)ptr)[x]);

	pkt.ip.hl_ver = 0x45;
	pkt.ip.len = htobe16(total_ip);
	pkt.ip.ttl = 8;
	pkt.ip.protocol = 17;	/* UDP */
	pkt.ip.sourceip = 0x01010101U;
	pkt.ip.destip = htobe32((239 << 24) + (255 << 16) + (1 << 0));
	pkt.ip.chksum = voss_ipv4_csum((void *)&pkt.ip, sizeof(pkt.ip) / 2);

	pkt.udp.srcport = htobe16(port);
	pkt.udp.dstport = htobe16(port);
	pkt.udp.len = htobe16(total_ip - sizeof(pkt.ip));

	pkt.rtp.header8[0] = (2 << 6);
	pkt.rtp.header8[1] = ((pvc->channels == 2) ? 10 : 11) | 0x80;

	pkt.rtp.header16[1] = htobe16(pvc->profile->http.rtp_seqnum);
	pkt.rtp.header32[1] = htobe32(ts);
	pkt.rtp.header32[2] = htobe32(0);

	pkt.udp.chksum = voss_udp_csum(pkt.ip.dest16[0] + pkt.ip.dest16[1] +
	    pkt.ip.source16[0] + pkt.ip.source16[1] + 0x1100 + pkt.udp.len,
	    (void *)&pkt.udp, sizeof(pkt.udp) / 2 + sizeof(pkt.rtp) / 2,
	    iov[1].iov_base, iov[1].iov_len);

	pvc->profile->http.rtp_seqnum++;
	pvc->profile->http.rtp_ts += len / (2 * pvc->channels);

	(void)writev(fd, iov, 2);
}

static void
voss_httpd_send_rtp(vclient_t *pvc, int fd, void *ptr, size_t len, uint32_t ts)
{
	const uint32_t mod = pvc->channels * vclient_sample_bytes(pvc);
	const uint32_t max = 1420 - (1420 % mod);

	while (len >= max) {
		voss_httpd_send_rtp_sub(pvc, fd, ptr, max, ts);
		len -= max;
		ptr = (uint8_t *)ptr + max;
	}

	if (len != 0)
		voss_httpd_send_rtp_sub(pvc, fd, ptr, len, ts);
}

static size_t
voss_httpd_usage(vclient_t *pvc)
{
	size_t usage = 0;
	size_t x;

	for (x = 0; x < pvc->profile->http.nstate; x++)
		usage += (pvc->profile->http.state[x].fd != -1);
	return (usage);
}

static char *
voss_httpd_read_line(FILE *io, char *linebuffer, size_t linelen)
{
	char buffer[2];
	size_t size = 0;

	if (fread(buffer, 1, 2, io) != 2)
		return (NULL);

	while (1) {
		if (buffer[0] == '\r' && buffer[1] == '\n')
			break;
		if (size == (linelen - 1))
			return (NULL);
		linebuffer[size++] = buffer[0];
		buffer[0] = buffer[1];
		if (fread(buffer + 1, 1, 1, io) != 1)
			return (NULL);
	}
	linebuffer[size++] = 0;

	return (linebuffer);
}

static int
voss_http_generate_wav_header(vclient_t *pvc, FILE *io,
    uintmax_t r_start, uintmax_t r_end, bool is_partial)
{
	uint8_t buffer[256];
	uint8_t *ptr;
	uintmax_t dummy_len;
	uintmax_t delta;
	size_t mod;
	size_t len;
	size_t buflen;

	ptr = buffer;
	mod = pvc->channels * vclient_sample_bytes(pvc);

	if (mod == 0 || sizeof(buffer) < (44 + mod - 1))
		return (-1);

	/* align to next sample */
	len = 44 + mod - 1;
	len -= len % mod;

	buflen = len;

	/* clear block */
	memset(ptr, 0, len);

	/* fill out data header */
	ptr[len - 8] = 'd';
	ptr[len - 7] = 'a';
	ptr[len - 6] = 't';
	ptr[len - 5] = 'a';

	/* magic for unspecified length */
	ptr[len - 4] = 0x00;
	ptr[len - 3] = 0xF0;
	ptr[len - 2] = 0xFF;
	ptr[len - 1] = 0x7F;

	/* fill out header */
	*ptr++ = 'R';
	*ptr++ = 'I';
	*ptr++ = 'F';
	*ptr++ = 'F';

	/* total chunk size - unknown */

	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;
	*ptr++ = 0;

	*ptr++ = 'W';
	*ptr++ = 'A';
	*ptr++ = 'V';
	*ptr++ = 'E';
	*ptr++ = 'f';
	*ptr++ = 'm';
	*ptr++ = 't';
	*ptr++ = ' ';

	/* make sure header fits in PCM block */
	len -= 28;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* audioformat = PCM */

	*ptr++ = 0x01;
	*ptr++ = 0x00;

	/* number of channels */

	len = pvc->channels;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* sample rate */

	len = pvc->sample_rate;

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* byte rate */

	len = pvc->sample_rate * pvc->channels * vclient_sample_bytes(pvc);

	*ptr++ = len;
	*ptr++ = len >> 8;
	*ptr++ = len >> 16;
	*ptr++ = len >> 24;

	/* block align */

	len = pvc->channels * vclient_sample_bytes(pvc);

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* bits per sample */

	len = vclient_sample_bytes(pvc) * 8;

	*ptr++ = len;
	*ptr++ = len >> 8;

	/* check if alignment is correct */
	if (r_start >= buflen && (r_start % mod) != 0)
		return (2);

	dummy_len = pvc->sample_rate * pvc->channels * vclient_sample_bytes(pvc);
	dummy_len *= VOSS_HTTPD_MAX_STREAM_TIME;

	/* fixup end */
	if (r_end >= dummy_len)
		r_end = dummy_len - 1;

	delta = r_end - r_start + 1;

	if (is_partial) {
		fprintf(io, "HTTP/1.1 206 Partial Content\r\n"
		    "Content-Type: audio/wav\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "Connection: Close\r\n"
		    "Content-Range: bytes %ju-%ju/%ju\r\n"
		    "Content-Length: %ju\r\n"
		    "\r\n", r_start, r_end, dummy_len, delta);
	} else {
		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: audio/wav\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "Connection: Close\r\n"
		    "Content-Length: %ju\r\n"
		    "\r\n", dummy_len);
	}

	/* check if we should insert a header */
	if (r_start < buflen) {
		buflen -= r_start;
		if (buflen > delta)
			buflen = delta;
		/* send data */
		if (fwrite(buffer + r_start, buflen, 1, io) != 1)
			return (-1);
		/* check if all data was read */
		if (buflen == delta)
			return (1);
	}
	return (0);
}

static void
voss_httpd_handle_connection(vclient_t *pvc, int fd, const struct sockaddr_in *sa)
{
	char linebuffer[2048];
	uintmax_t r_start = 0;
	uintmax_t r_end = -1ULL;
	bool is_partial = false;
	char *line;
	FILE *io;
	size_t x;
	int page;

	io = fdopen(fd, "r+");
	if (io == NULL)
		goto done;

	page = -1;

	/* dump HTTP request header */
	while (1) {
		line = voss_httpd_read_line(io, linebuffer, sizeof(linebuffer));
		if (line == NULL)
			goto done;
		if (line[0] == 0)
			break;
		if (page < 0 && (strstr(line, "GET / ") == line ||
		    strstr(line, "GET /index.html") == line)) {
			page = 0;
		} else if (page < 0 && strstr(line, "GET /stream.wav") == line) {
			page = 1;
		} else if (page < 0 && strstr(line, "GET /stream.m3u") == line) {
			page = 2;
		} else if (strstr(line, "Range: bytes=") == line &&
		    sscanf(line, "Range: bytes=%ju-%ju", &r_start, &r_end) >= 1) {
			is_partial = true;
		}
	}

	switch (page) {
	case 0:
		x = voss_httpd_usage(pvc);

		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: text/html\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "\r\n"
		    "<html><head><title>Welcome to live streaming</title>"
		    "<meta http-equiv=\"Cache-Control\" content=\"no-cache, no-store, must-revalidate\" />"
		    "<meta http-equiv=\"Pragma\" content=\"no-cache\" />"
		    "<meta http-equiv=\"Expires\" content=\"0\" />"
		    "</head>"
		    "<body>"
		    "<h1>Live HD stream</h1>"
		    "<br>"
		    "<br>"
		    "<h2>Alternative 1 (recommended)</h2>"
		    "<ol type=\"1\">"
		    "<li>Install <a href=\"https://www.videolan.org\">VideoLanClient (VLC)</a>, from App- or Play-store free of charge</li>"
		    "<li>Open VLC and select Network Stream</li>"
		    "<li>Enter, copy or share this network address to VLC: <a href=\"http://%s:%s/stream.m3u\">http://%s:%s/stream.m3u</a></li>"
		    "</ol>"
		    "<br>"
		    "<br>"
		    "<h2>Alternative 2 (on your own)</h2>"
		    "<br>"
		    "<br>"
		    "<audio id=\"audio\" controls=\"true\" src=\"stream.wav\" preload=\"none\"></audio>"
		    "<br>"
		    "<br>",
		    pvc->profile->http.host, pvc->profile->http.port,
		    pvc->profile->http.host, pvc->profile->http.port);

		if (x == pvc->profile->http.nstate)
			fprintf(io, "<h2>There are currently no free slots (%zu active). Try again later!</h2>", x);
		else
			fprintf(io, "<h2>There are %zu free slots (%zu active)</h2>", pvc->profile->http.nstate - x, x);

		fprintf(io, "</body></html>");
		break;
	case 1:
		for (x = 0; x < pvc->profile->http.nstate; x++) {
			if (pvc->profile->http.state[x].fd >= 0)
				continue;
			switch (voss_http_generate_wav_header(pvc, io, r_start, r_end, is_partial)) {
				static const int enable = 1;

			case 0:
				fflush(io);
				fdclose(io, NULL);
				if (ioctl(fd, FIONBIO, &enable) != 0) {
					close(fd);
					return;
				}
				pvc->profile->http.state[x].ts =
				    virtual_oss_timestamp() - 1000000000ULL;
				pvc->profile->http.state[x].fd = fd;
				return;
			case 1:
				fclose(io);
				return;
			case 2:
				fprintf(io, "HTTP/1.1 416 Range Not Satisfiable\r\n"
				    "Server: virtual_oss/1.0\r\n"
				    "\r\n");
				goto done;
			default:
				goto done;
			}
		}
		fprintf(io, "HTTP/1.0 503 Out of Resources\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "\r\n");
		break;
	case 2:
		fprintf(io, "HTTP/1.0 200 OK\r\n"
		    "Content-Type: audio/mpegurl\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "Cache-Control: no-cache, no-store\r\n"
		    "Expires: Mon, 26 Jul 1997 05:00:00 GMT\r\n"
		    "\r\n");
		if (sa->sin_family == AF_INET && pvc->profile->http.rtp_port != NULL) {
			fprintf(io, "rtp://239.255.0.1:%s\r\n", pvc->profile->http.rtp_port);
		} else {
			fprintf(io, "http://%s:%s/stream.wav\r\n",
			    pvc->profile->http.host, pvc->profile->http.port);
		}
		break;
	default:
		fprintf(io, "HTTP/1.0 404 Not Found\r\n"
		    "Content-Type: text/html\r\n"
		    "Server: virtual_oss/1.0\r\n"
		    "\r\n"
		    "<html><head><title>virtual_oss</title></head>"
		    "<body>"
		    "<h1>Invalid page requested! "
		    "<a HREF=\"index.html\">Click here to go back</a>.</h1><br>"
		    "</body>"
		    "</html>");
		break;
	}
done:
	if (io != NULL)
		fclose(io);
	else
		close(fd);
}

static int
voss_httpd_do_listen(vclient_t *pvc, const char *host, const char *port,
    struct pollfd *pfd, int num_sock, int buffer)
{
	static const struct timeval timeout = {.tv_sec = 1};
	struct addrinfo hints = {};
	struct addrinfo *res;
	struct addrinfo *res0;
	int error;
	int flag;
	int s;
	int ns = 0;

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	if ((error = getaddrinfo(host, port, &hints, &res)))
		return (-1);

	res0 = res;

	do {
		if ((s = socket(res0->ai_family, res0->ai_socktype,
		    res0->ai_protocol)) < 0)
			continue;

		flag = 1;
		setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &flag, (int)sizeof(flag));
		setsockopt(s, SOL_SOCKET, SO_SNDBUF, &buffer, (int)sizeof(buffer));
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, &buffer, (int)sizeof(buffer));
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &timeout, (int)sizeof(timeout));
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, (int)sizeof(timeout));

		if (bind(s, res0->ai_addr, res0->ai_addrlen) == 0) {
			if (listen(s, pvc->profile->http.nstate) == 0) {
				if (ns < num_sock) {
					pfd[ns++].fd = s;
					continue;
				}
				close(s);
				break;
			}
		}
		close(s);
	} while ((res0 = res0->ai_next) != NULL);

	freeaddrinfo(res);

	return (ns);
}

static size_t
voss_httpd_buflimit(vclient_t *pvc)
{
	/* don't buffer more than 250ms */
	return ((pvc->sample_rate / 4) *
	    pvc->channels * vclient_sample_bytes(pvc));
};

static void
voss_httpd_server(vclient_t *pvc)
{
	const size_t bufferlimit = voss_httpd_buflimit(pvc);
	const char *host = pvc->profile->http.host;
	const char *port = pvc->profile->http.port;
	struct sockaddr sa = {};
	struct pollfd fds[VOSS_HTTPD_BIND_MAX] = {};
	int nfd;

	nfd = voss_httpd_do_listen(pvc, host, port, fds, VOSS_HTTPD_BIND_MAX, bufferlimit);
	if (nfd < 1) {
		errx(EX_SOFTWARE, "Could not bind to "
		    "'%s' and '%s'", host, port);
	}

	while (1) {
		struct sockaddr_in si;
		int ns = nfd;
		int c;
		int f;

		for (c = 0; c != ns; c++) {
			fds[c].events = (POLLIN | POLLRDNORM | POLLRDBAND | POLLPRI |
			    POLLERR | POLLHUP | POLLNVAL);
			fds[c].revents = 0;
		}
		if (poll(fds, ns, -1) < 0)
			errx(EX_SOFTWARE, "Polling failed");

		for (c = 0; c != ns; c++) {
			socklen_t socklen = sizeof(sa);

			if (fds[c].revents == 0)
				continue;
			f = accept(fds[c].fd, &sa, &socklen);
			if (f < 0)
				continue;
			memcpy(&si, &sa, sizeof(sa));
			voss_httpd_handle_connection(pvc, f, &si);
		}
	}
}

static void
voss_httpd_streamer(vclient_t *pvc)
{
	const size_t bufferlimit = voss_httpd_buflimit(pvc);
	uint8_t *ptr;
	size_t len;
	uint64_t ts;
	size_t x;

	atomic_lock();
	while (1) {
		if (vclient_export_read_locked(pvc) != 0) {
			atomic_wait();
			continue;
		}
		vring_get_read(&pvc->rx_ring[1], &ptr, &len);
		if (len == 0) {
			/* try to avoid ring wraps */
			vring_reset(&pvc->rx_ring[1]);
			atomic_wait();
			continue;
		}
		atomic_unlock();

		ts = virtual_oss_timestamp();

		/* check if we should send RTP data, if any */
		if (pvc->profile->http.rtp_fd > -1) {
			voss_httpd_send_rtp(pvc, pvc->profile->http.rtp_fd,
			    ptr, len, pvc->profile->http.rtp_ts);
		}

		/* send HTTP data, if any */
		for (x = 0; x < pvc->profile->http.nstate; x++) {
			int fd = pvc->profile->http.state[x].fd;
			uint64_t delta = ts - pvc->profile->http.state[x].ts;
			uint8_t buf[1];
			int write_len;

			if (fd < 0) {
				/* do nothing */
			} else if (delta >= (8ULL * 1000000000ULL)) {
				/* no data for 8 seconds - terminate */
				pvc->profile->http.state[x].fd = -1;
				close(fd);
			} else if (read(fd, buf, sizeof(buf)) != -1 || errno != EWOULDBLOCK) {
				pvc->profile->http.state[x].fd = -1;
				close(fd);
			} else if (ioctl(fd, FIONWRITE, &write_len) < 0) {
				pvc->profile->http.state[x].fd = -1;
				close(fd);
			} else if ((ssize_t)(bufferlimit - write_len) < (ssize_t)len) {
				/* do nothing */
			} else if (write(fd, ptr, len) != (ssize_t)len) {
				pvc->profile->http.state[x].fd = -1;
				close(fd);
			} else {
				/* update timestamp */
				pvc->profile->http.state[x].ts = ts;
			}
		}

		atomic_lock();
		vring_inc_read(&pvc->rx_ring[1], len);
	}
}

const char *
voss_httpd_start(vprofile_t *pvp)
{
	vclient_t *pvc;
	pthread_t td;
	int error;
	size_t x;

	if (pvp->http.host == NULL || pvp->http.port == NULL || pvp->http.nstate == 0)
		return (NULL);

	pvp->http.state = malloc(sizeof(pvp->http.state[0]) * pvp->http.nstate);
	if (pvp->http.state == NULL)
		return ("Could not allocate HTTP states");

	for (x = 0; x != pvp->http.nstate; x++) {
		pvp->http.state[x].fd = -1;
		pvp->http.state[x].ts = 0;
	}

	pvc = vclient_alloc();
	if (pvc == NULL)
		return ("Could not allocate client for HTTP server");

	pvc->profile = pvp;

	if (pvp->http.rtp_ifname != NULL) {
		const char *perr;

		if (pvc->channels > 2)
			return ("RTP only supports 44.1kHz, 1 or 2 channels at 16-bit depth");

		/* bind to UDP port */
		perr = voss_httpd_bind_rtp(pvc, pvp->http.rtp_ifname,
		    &pvp->http.rtp_fd);
		if (perr != NULL)
			return (perr);

		/* setup buffers */
		error = vclient_setup_buffers(pvc, 0, 0,
		    pvp->channels, AFMT_S16_LE, 44100);
	} else {
		pvp->http.rtp_fd = -1;

		/* setup buffers */
		error = vclient_setup_buffers(pvc, 0, 0, pvp->channels,
		    vclient_get_default_fmt(pvp, VTYPE_WAV_HDR),
		    voss_dsp_sample_rate);
	}

	if (error != 0) {
		vclient_free(pvc);
		return ("Could not allocate buffers for HTTP server");
	}

	/* trigger enabled */
	pvc->rx_enabled = 1;

	pvc->type = VTYPE_OSS_DAT;

	atomic_lock();
	TAILQ_INSERT_TAIL(&pvp->head, pvc, entry);
	atomic_unlock();

	if (pthread_create(&td, NULL, (void *)&voss_httpd_server, pvc))
		return ("Could not create HTTP daemon thread");
	if (pthread_create(&td, NULL, (void *)&voss_httpd_streamer, pvc))
		return ("Could not create HTTP streamer thread");

	return (NULL);
}
