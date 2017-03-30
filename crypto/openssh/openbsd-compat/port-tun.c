/*
 * Copyright (c) 2005 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "includes.h"

#include <sys/types.h>
#include <sys/ioctl.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "openbsd-compat/sys-queue.h"
#include "log.h"
#include "misc.h"
#include "sshbuf.h"
#include "channels.h"
#include "ssherr.h"

/*
 * This is the portable version of the SSH tunnel forwarding, it
 * uses some preprocessor definitions for various platform-specific
 * settings.
 *
 * SSH_TUN_LINUX	Use the (newer) Linux tun/tap device
 * SSH_TUN_FREEBSD	Use the FreeBSD tun/tap device
 * SSH_TUN_COMPAT_AF	Translate the OpenBSD address family
 * SSH_TUN_PREPEND_AF	Prepend/remove the address family
 */

/*
 * System-specific tunnel open function
 */

#if defined(SSH_TUN_LINUX)
#include <linux/if.h>
#include <linux/if_tun.h>

int
sys_tun_open(int tun, int mode)
{
	struct ifreq ifr;
	int fd = -1;
	const char *name = NULL;

	if ((fd = open("/dev/net/tun", O_RDWR)) == -1) {
		debug("%s: failed to open tunnel control interface: %s",
		    __func__, strerror(errno));
		return (-1);
	}

	bzero(&ifr, sizeof(ifr));

	if (mode == SSH_TUNMODE_ETHERNET) {
		ifr.ifr_flags = IFF_TAP;
		name = "tap%d";
	} else {
		ifr.ifr_flags = IFF_TUN;
		name = "tun%d";
	}
	ifr.ifr_flags |= IFF_NO_PI;

	if (tun != SSH_TUNID_ANY) {
		if (tun > SSH_TUNID_MAX) {
			debug("%s: invalid tunnel id %x: %s", __func__,
			    tun, strerror(errno));
			goto failed;
		}
		snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), name, tun);
	}

	if (ioctl(fd, TUNSETIFF, &ifr) == -1) {
		debug("%s: failed to configure tunnel (mode %d): %s", __func__,
		    mode, strerror(errno));
		goto failed;
	}

	if (tun == SSH_TUNID_ANY)
		debug("%s: tunnel mode %d fd %d", __func__, mode, fd);
	else
		debug("%s: %s mode %d fd %d", __func__, ifr.ifr_name, mode, fd);

	return (fd);

 failed:
	close(fd);
	return (-1);
}
#endif /* SSH_TUN_LINUX */

#ifdef SSH_TUN_FREEBSD
#include <sys/socket.h>
#include <net/if.h>

#ifdef HAVE_NET_IF_TUN_H
#include <net/if_tun.h>
#endif

int
sys_tun_open(int tun, int mode)
{
	struct ifreq ifr;
	char name[100];
	int fd = -1, sock, flag;
	const char *tunbase = "tun";

	if (mode == SSH_TUNMODE_ETHERNET) {
#ifdef SSH_TUN_NO_L2
		debug("%s: no layer 2 tunnelling support", __func__);
		return (-1);
#else
		tunbase = "tap";
#endif
	}

	/* Open the tunnel device */
	if (tun <= SSH_TUNID_MAX) {
		snprintf(name, sizeof(name), "/dev/%s%d", tunbase, tun);
		fd = open(name, O_RDWR);
	} else if (tun == SSH_TUNID_ANY) {
		for (tun = 100; tun >= 0; tun--) {
			snprintf(name, sizeof(name), "/dev/%s%d",
			    tunbase, tun);
			if ((fd = open(name, O_RDWR)) >= 0)
				break;
		}
	} else {
		debug("%s: invalid tunnel %u\n", __func__, tun);
		return (-1);
	}

	if (fd < 0) {
		debug("%s: %s open failed: %s", __func__, name,
		    strerror(errno));
		return (-1);
	}

	/* Turn on tunnel headers */
	flag = 1;
#if defined(TUNSIFHEAD) && !defined(SSH_TUN_PREPEND_AF)
	if (mode != SSH_TUNMODE_ETHERNET &&
	    ioctl(fd, TUNSIFHEAD, &flag) == -1) {
		debug("%s: ioctl(%d, TUNSIFHEAD, 1): %s", __func__, fd,
		    strerror(errno));
		close(fd);
	}
#endif

	debug("%s: %s mode %d fd %d", __func__, name, mode, fd);

	/* Set the tunnel device operation mode */
	snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s%d", tunbase, tun);
	if ((sock = socket(PF_UNIX, SOCK_STREAM, 0)) == -1)
		goto failed;

	if (ioctl(sock, SIOCGIFFLAGS, &ifr) == -1)
		goto failed;
	if ((ifr.ifr_flags & IFF_UP) == 0) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(sock, SIOCSIFFLAGS, &ifr) == -1)
			goto failed;
	}

	close(sock);
	return (fd);

 failed:
	if (fd >= 0)
		close(fd);
	if (sock >= 0)
		close(sock);
	debug("%s: failed to set %s mode %d: %s", __func__, name,
	    mode, strerror(errno));
	return (-1);
}
#endif /* SSH_TUN_FREEBSD */

/*
 * System-specific channel filters
 */

#if defined(SSH_TUN_FILTER)
#define OPENBSD_AF_INET		2
#define OPENBSD_AF_INET6	24

int
sys_tun_infilter(struct Channel *c, char *buf, int len)
{
#if defined(SSH_TUN_PREPEND_AF)
	char rbuf[CHAN_RBUF];
	struct ip *iph;
#endif
	u_int32_t *af;
	char *ptr = buf;
	int r;

#if defined(SSH_TUN_PREPEND_AF)
	if (len <= 0 || len > (int)(sizeof(rbuf) - sizeof(*af)))
		return (-1);
	ptr = (char *)&rbuf[0];
	bcopy(buf, ptr + sizeof(u_int32_t), len);
	len += sizeof(u_int32_t);
	af = (u_int32_t *)ptr;

	iph = (struct ip *)(ptr + sizeof(u_int32_t));
	switch (iph->ip_v) {
	case 6:
		*af = AF_INET6;
		break;
	case 4:
	default:
		*af = AF_INET;
		break;
	}
#endif

#if defined(SSH_TUN_COMPAT_AF)
	if (len < (int)sizeof(u_int32_t))
		return (-1);

	af = (u_int32_t *)ptr;
	if (*af == htonl(AF_INET6))
		*af = htonl(OPENBSD_AF_INET6);
	else
		*af = htonl(OPENBSD_AF_INET);
#endif

	if ((r = sshbuf_put_string(&c->input, ptr, len)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	return (0);
}

u_char *
sys_tun_outfilter(struct Channel *c, u_char **data, u_int *dlen)
{
	u_char *buf;
	u_int32_t *af;
	int r;
	size_t xxx_dlen;

	/* XXX new API is incompatible with this signature. */
	if ((r = sshbuf_get_string(&c->output, data, &xxx_dlen)) != 0)
		fatal("%s: buffer error: %s", __func__, ssh_err(r));
	if (dlen != NULL)
		*dlen = xxx_dlen;
	if (*dlen < sizeof(*af))
		return (NULL);
	buf = *data;

#if defined(SSH_TUN_PREPEND_AF)
	*dlen -= sizeof(u_int32_t);
	buf = *data + sizeof(u_int32_t);
#elif defined(SSH_TUN_COMPAT_AF)
	af = ntohl(*(u_int32_t *)buf);
	if (*af == OPENBSD_AF_INET6)
		*af = htonl(AF_INET6);
	else
		*af = htonl(AF_INET);
#endif

	return (buf);
}
#endif /* SSH_TUN_FILTER */
