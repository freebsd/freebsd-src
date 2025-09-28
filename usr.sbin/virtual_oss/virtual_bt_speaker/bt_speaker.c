/*-
 * Copyright (c) 2019 Google LLC, written by Richard Kralovic <riso@google.com>
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
#include <sys/rtprio.h>
#include <sys/soundcard.h>

#include <dlfcn.h>
#include <err.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>
#include <time.h>
#include <unistd.h>
#include <poll.h>
#include <getopt.h>

#define	L2CAP_SOCKET_CHECKED
#include <bluetooth.h>
#include <sdp.h>

#include "avdtp_signal.h"
#include "bt.h"
#include "utils.h"

static int (*bt_receive_f)(struct bt_config *, void *, int, int);
static int (*avdtpACPHandlePacket_f)(struct bt_config *cfg);
static void (*avdtpACPFree_f)(struct bt_config *);

static int bt_in_background;

static void
message(const char *fmt,...)
{
	va_list list;

	if (bt_in_background)
		return;

	va_start(list, fmt);
	vfprintf(stderr, fmt, list);
	va_end(list);
}

struct bt_audio_receiver {
	const char *devname;
	const char *sdp_socket_path;
	uint16_t l2cap_psm;
	int	fd_listen;
	void   *sdp_session;
	uint32_t sdp_handle;
};

static int
register_sdp(struct bt_audio_receiver *r)
{
	struct sdp_audio_sink_profile record = {};

	r->sdp_session = sdp_open_local(r->sdp_socket_path);
	if (r->sdp_session == NULL || sdp_error(r->sdp_session)) {
		sdp_close(r->sdp_session);
		r->sdp_session = NULL;
		return (0);
	}

	record.psm = r->l2cap_psm;
	record.protover = 0x100;
	record.features = 0x01;		/* player only */

	if (sdp_register_service(r->sdp_session, SDP_SERVICE_CLASS_AUDIO_SINK,
	    NG_HCI_BDADDR_ANY, (const uint8_t *)&record, sizeof(record),
	    &r->sdp_handle)) {
		message("SDP failed to register: %s\n",
		    strerror(sdp_error(r->sdp_session)));
		sdp_close(r->sdp_session);
		r->sdp_session = NULL;
		return (0);
	}
	return (1);
}

static void
unregister_sdp(struct bt_audio_receiver *r)
{
	sdp_unregister_service(r->sdp_session, r->sdp_handle);
	sdp_close(r->sdp_session);
	r->sdp_session = NULL;
}

static int
start_listen(struct bt_audio_receiver *r)
{
	struct sockaddr_l2cap addr = {};

	r->fd_listen = socket(PF_BLUETOOTH, SOCK_SEQPACKET, BLUETOOTH_PROTO_L2CAP);
	if (r->fd_listen < 0)
		return (0);

	addr.l2cap_len = sizeof(addr);
	addr.l2cap_family = AF_BLUETOOTH;
	addr.l2cap_psm = r->l2cap_psm;

	if (bind(r->fd_listen, (struct sockaddr *)&addr, sizeof(addr)) < 0 ||
	    listen(r->fd_listen, 4) < 0) {
		close(r->fd_listen);
		return (0);
	}
	return (1);
}

static void
stop_listen(struct bt_audio_receiver *r)
{
	close(r->fd_listen);
}

struct bt_audio_connection {
	struct bt_audio_receiver *r;
	struct sockaddr_l2cap peer_addr;
	struct bt_config cfg;
	int	oss_fd;
};

static void
close_connection(struct bt_audio_connection *c)
{
	avdtpACPFree_f(&c->cfg);
	if (c->cfg.fd != -1)
		close(c->cfg.fd);
	if (c->cfg.hc != -1)
		close(c->cfg.hc);
	if (c->oss_fd != -1)
		close(c->oss_fd);
	free(c);
}

static struct bt_audio_connection *
wait_for_connection(struct bt_audio_receiver *r)
{
	struct bt_audio_connection *c =
	malloc(sizeof(struct bt_audio_connection));
	socklen_t addrlen;

	memset(c, 0, sizeof(*c));

	c->r = r;
	c->cfg.fd = -1;
	c->oss_fd = -1;

	addrlen = sizeof(c->peer_addr);
	c->cfg.hc = accept(r->fd_listen, (struct sockaddr *)&c->peer_addr, &addrlen);

	message("Accepted control connection, %d\n", c->cfg.hc);
	if (c->cfg.hc < 0) {
		close_connection(c);
		return NULL;
	}
	c->cfg.sep = 0;			/* to be set later */
	c->cfg.media_Type = mediaTypeAudio;
	c->cfg.chmode = MODE_DUAL;
	c->cfg.aacMode1 = 0;		/* TODO: support AAC */
	c->cfg.aacMode2 = 0;
	c->cfg.acceptor_state = acpInitial;

	return (c);
}

static void
setup_oss(struct bt_audio_connection *c)
{
	c->oss_fd = open(c->r->devname, O_WRONLY);

	if (c->oss_fd < 0)
		goto err;

	int v;

	switch (c->cfg.chmode) {
	case MODE_STEREO:
	case MODE_JOINT:
	case MODE_DUAL:
		v = 2;
		break;
	case MODE_MONO:
		v = 1;
		break;
	default:
		message("Wrong chmode\n");
		goto err;
	}

	if (ioctl(c->oss_fd, SNDCTL_DSP_CHANNELS, &v) < 0) {
		message("SNDCTL_DSP_CHANNELS failed\n");
		goto err;
	}
	v = AFMT_S16_NE;
	if (ioctl(c->oss_fd, SNDCTL_DSP_SETFMT, &v) < 0) {
		message("SNDCTL_DSP_SETFMT failed\n");
		goto err;
	}
	switch (c->cfg.freq) {
	case FREQ_16K:
		v = 16000;
		break;
	case FREQ_32K:
		v = 32000;
		break;
	case FREQ_44_1K:
		v = 44100;
		break;
	case FREQ_48K:
		v = 48000;
		break;
	default:
		message("Wrong freq\n");
		goto err;
	}

	if (ioctl(c->oss_fd, SNDCTL_DSP_SPEED, &v) < 0) {
		message("SNDCTL_DSP_SETFMT failed\n");
		goto err;
	}
	v = (2 << 16) | 15;		/* 2 fragments of 32k each */
	if (ioctl(c->oss_fd, SNDCTL_DSP_SETFRAGMENT, &v) < 0) {
		message("SNDCTL_DSP_SETFRAGMENT failed\n");
		goto err;
	}
	return;

err:
	c->oss_fd = -1;
	message("Cannot open oss device %s\n", c->r->devname);
}

static void
process_connection(struct bt_audio_connection *c)
{
	struct pollfd pfd[3] = {};
	time_t oss_attempt = 0;

	while (c->cfg.acceptor_state != acpStreamClosed) {
		int np;

		pfd[0].fd = c->r->fd_listen;
		pfd[0].events = POLLIN | POLLRDNORM;
		pfd[0].revents = 0;

		pfd[1].fd = c->cfg.hc;
		pfd[1].events = POLLIN | POLLRDNORM;
		pfd[1].revents = 0;

		pfd[2].fd = c->cfg.fd;
		pfd[2].events = POLLIN | POLLRDNORM;
		pfd[2].revents = 0;

		if (c->cfg.fd != -1)
			np = 3;
		else
			np = 2;

		if (poll(pfd, np, INFTIM) < 0)
			return;

		if (pfd[1].revents != 0) {
			int retval;

			message("Handling packet: state = %d, ",
			    c->cfg.acceptor_state);
			retval = avdtpACPHandlePacket_f(&c->cfg);
			message("retval = %d\n", retval);
			if (retval < 0)
				return;
		}
		if (pfd[0].revents != 0) {
			socklen_t addrlen = sizeof(c->peer_addr);
			int fd = accept4(c->r->fd_listen,
			    (struct sockaddr *)&c->peer_addr, &addrlen,
			    SOCK_NONBLOCK);

			if (fd < 0)
				return;

			if (c->cfg.fd < 0) {
				if (c->cfg.acceptor_state == acpStreamOpened) {
					socklen_t mtusize = sizeof(uint16_t);
					c->cfg.fd = fd;

					if (getsockopt(c->cfg.fd, SOL_L2CAP, SO_L2CAP_IMTU, &c->cfg.mtu, &mtusize) == -1) {
						message("Could not get MTU size\n");
						return;
					}

					int temp = c->cfg.mtu * 32;

					if (setsockopt(c->cfg.fd, SOL_SOCKET, SO_RCVBUF, &temp, sizeof(temp)) == -1) {
						message("Could not set send buffer size\n");
						return;
					}

					temp = 1;
					if (setsockopt(c->cfg.fd, SOL_SOCKET, SO_RCVLOWAT, &temp, sizeof(temp)) == -1) {
						message("Could not set low water mark\n");
						return;
					}
					message("Accepted data connection, %d\n", c->cfg.fd);
				}
			} else {
				close(fd);
			}
		}
		if (pfd[2].revents != 0) {
			uint8_t data[65536];
			int len;

			if ((len = bt_receive_f(&c->cfg, data, sizeof(data), 0)) < 0) {
				return;
			}
			if (c->cfg.acceptor_state != acpStreamSuspended &&
			    c->oss_fd < 0 &&
			    time(NULL) != oss_attempt) {
				message("Trying to open dsp\n");
				setup_oss(c);
				oss_attempt = time(NULL);
			}
			if (c->oss_fd > -1) {
				uint8_t *end = data + len;
				uint8_t *ptr = data;
				unsigned delay;
				unsigned jitter_limit;

				switch (c->cfg.freq) {
				case FREQ_16K:
					jitter_limit = (16000 / 20);
					break;
				case FREQ_32K:
					jitter_limit = (32000 / 20);
					break;
				case FREQ_44_1K:
					jitter_limit = (44100 / 20);
					break;
				default:
					jitter_limit = (48000 / 20);
					break;
				}

				if (c->cfg.chmode == MODE_MONO) {
					if (len >= 2 &&
					    ioctl(c->oss_fd, SNDCTL_DSP_GETODELAY, &delay) == 0 &&
					    delay < (jitter_limit * 2)) {
						uint8_t jitter[jitter_limit * 4] __aligned(4);
						size_t x;

						/* repeat last sample */
						for (x = 0; x != sizeof(jitter); x++)
							jitter[x] = ptr[x % 2];

						write(c->oss_fd, jitter, sizeof(jitter));
					}
				} else {
					if (len >= 4 &&
					    ioctl(c->oss_fd, SNDCTL_DSP_GETODELAY, &delay) == 0 &&
					    delay < (jitter_limit * 4)) {
						uint8_t jitter[jitter_limit * 8] __aligned(4);
						size_t x;

						/* repeat last sample */
						for (x = 0; x != sizeof(jitter); x++)
							jitter[x] = ptr[x % 4];

						write(c->oss_fd, jitter, sizeof(jitter));
					}
				}
				while (ptr != end) {
					int written = write(c->oss_fd, ptr, end - ptr);

					if (written < 0) {
						if (errno != EINTR && errno != EAGAIN)
							break;
						written = 0;
					}
					ptr += written;
				}
				if (ptr != end) {
					message("Not all written, closing dsp\n");
					close(c->oss_fd);
					c->oss_fd = -1;
					oss_attempt = time(NULL);
				}
			}
		}

		if (c->cfg.acceptor_state == acpStreamSuspended &&
		    c->oss_fd > -1) {
			close(c->oss_fd);
			c->oss_fd = -1;
		}
	}
}

static struct option bt_speaker_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"sdp_socket_path", required_argument, NULL, 'p'},
	{"rtprio", required_argument, NULL, 'i'},
	{"background", no_argument, NULL, 'B'},
	{"help", no_argument, NULL, 'h'},
	{NULL, 0, NULL, 0}
};

static void
usage(void)
{
	fprintf(stderr, "Usage: virtual_bt_speaker -d /dev/dsp\n"
	    "\t" "-d, --device [device]\n"
	    "\t" "-p, --sdp_socket_path [path]\n"
	    "\t" "-i, --rtprio [priority]\n"
	    "\t" "-B, --background\n"
	);
	exit(EX_USAGE);
}

int
main(int argc, char **argv)
{
	struct bt_audio_receiver r = {};
	struct rtprio rtp = {};
	void *hdl;
	int ch;

	r.devname = NULL;
	r.sdp_socket_path = NULL;
	r.l2cap_psm = SDP_UUID_PROTOCOL_AVDTP;

	while ((ch = getopt_long(argc, argv, "p:i:d:Bh", bt_speaker_opts, NULL)) != -1) {
		switch (ch) {
		case 'd':
			r.devname = optarg;
			break;
		case 'p':
			r.sdp_socket_path = optarg;
			break;
		case 'B':
			bt_in_background = 1;
			break;
		case 'i':
			rtp.type = RTP_PRIO_REALTIME;
			rtp.prio = atoi(optarg);
			if (rtprio(RTP_SET, getpid(), &rtp) != 0) {
				message("Cannot set realtime priority\n");
			}
			break;
		default:
			usage();
			break;
		}
	}

	if (r.devname == NULL)
		errx(EX_USAGE, "No devicename specified");

	if (bt_in_background) {
		if (daemon(0, 0) != 0)
			errx(EX_SOFTWARE, "Cannot become daemon");
	}

	if ((hdl = dlopen("/usr/lib/virtual_oss/voss_bt.so", RTLD_NOW)) == NULL)
		errx(1, "%s", dlerror());
	if ((bt_receive_f = dlsym(hdl, "bt_receive")) == NULL)
		goto err_dlsym;
	if ((avdtpACPHandlePacket_f = dlsym(hdl, "avdtpACPHandlePacket")) ==
	    NULL)
		goto err_dlsym;
	if ((avdtpACPFree_f = dlsym(hdl, "avdtpACPFree")) == NULL)
		goto err_dlsym;

	while (1) {
		message("Starting to listen\n");
		if (!start_listen(&r)) {
			message("Failed to initialize server socket\n");
			goto err_listen;
		}
		message("Registering service via SDP\n");
		if (!register_sdp(&r)) {
			message("Failed to register in SDP\n");
			goto err_sdp;
		}
		while (1) {
			message("Waiting for connection...\n");
			struct bt_audio_connection *c = wait_for_connection(&r);

			if (c == NULL) {
				message("Failed to get connection\n");
				goto err_conn;
			}
			message("Got connection...\n");

			process_connection(c);

			message("Connection finished...\n");

			close_connection(c);
		}
err_conn:
		message("Unregistering service\n");
		unregister_sdp(&r);
err_sdp:
		stop_listen(&r);
err_listen:
		sleep(5);
	}
	return (0);

err_dlsym:
	warnx("%s", dlerror());
	dlclose(hdl);
	exit(EXIT_FAILURE);
}
