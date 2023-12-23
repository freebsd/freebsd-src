/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 The FreeBSD Foundation
 *
 * This software was developed by Björn Zeeb under sponsorship from
 * the FreeBSD Foundation.
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

/*
 * First get scan results in a hurry.
 * Pick a random BSSID and try to assoc.
 * Hopefully this is enough to trigger the newstate race along with the
 * (*iv_update_bss)() logic.
 *
 * Alternatively pass IF SSID BSSID in and just try that.
 */

#include <err.h>
#include <stdio.h>
#include <string.h>
#include <sysexits.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/ioctl.h>

#include <net/if.h>
#include <net/ethernet.h>

#include <net80211/ieee80211.h>
#include <net80211/ieee80211_ioctl.h>

static int
if_up(int sd, const char *ifnam)
{
	struct ifreq ifr;
	int error;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, ifnam, sizeof(ifr.ifr_name));

	error = ioctl(sd, SIOCGIFFLAGS, &ifr);
	if (error == -1) {
		warn("SIOCGIFFLAGS");
		return (error);
	}

	if (ifr.ifr_flags & IFF_UP)
		return (0);

	ifr.ifr_flags |= IFF_UP;

	error = ioctl(sd, SIOCSIFFLAGS, &ifr);
	if (error == -1) {
		warn("SIOCSIFFLAGS");
		return (error);
	}

	return (0);
}

static int
try_mlme_assoc(int sd, const char *ifnam, uint8_t *ssid, uint8_t ssid_len, uint8_t *bssid)
{
	struct ieee80211req ireq;
	struct ieee80211req_mlme mlme;
	int error;

	memset(&mlme, 0, sizeof(mlme));
	mlme.im_op = IEEE80211_MLME_ASSOC;
	if (ssid != NULL)
		memcpy(mlme.im_ssid, ssid, ssid_len);
	mlme.im_ssid_len = ssid_len;
	if (bssid != NULL)
		memcpy(mlme.im_macaddr, bssid, IEEE80211_ADDR_LEN);

	memset(&ireq, 0, sizeof(ireq));
	strlcpy(ireq.i_name, ifnam, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_MLME;
	ireq.i_val = 0;
	ireq.i_data = (void *)&mlme;
	ireq.i_len = sizeof(mlme);

	error = ioctl(sd, SIOCS80211, &ireq);
	if (error == -1) {
		warn("SIOCS80211, %#x", ireq.i_type);
		return (error);
	}

	return (0);
}

static int
mlme_assoc_scan_results(int sd, const char *ifnam)
{
	struct ieee80211req ireq;
	struct ieee80211req_scan_result *sr;
	uint8_t buf[32 * 1024], *p;
	ssize_t len;
	int error;

	memset(&ireq, 0, sizeof(ireq));
	strlcpy(ireq.i_name, ifnam, sizeof(ireq.i_name));
	ireq.i_type = IEEE80211_IOC_SCAN_RESULTS;
	ireq.i_data = (void *)buf;
	ireq.i_len = sizeof(buf);

	error = ioctl(sd, SIOCG80211, &ireq);
	if (error == -1 || ireq.i_len < 0) {
		warn("SIOCG80211, %#x", ireq.i_type);
		return (error);
	}

	p = buf;
	len = ireq.i_len;
	while (len > (ssize_t)sizeof(*sr)) {
		sr = (struct ieee80211req_scan_result *)(void *)p;
		p += sr->isr_len;
		len -= sr->isr_len;

		error = try_mlme_assoc(sd, ifnam, (void *)(sr + 1), sr->isr_ssid_len,
		    sr->isr_bssid);
		if (error != 0) {
			warnx("try_mlme_assoc");
			return (error);
		}

		usleep(100000);
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	const char *ifnam;
	uint8_t *ssid, *bssid;
	struct ether_addr ea;
	int error, sd;

	ifnam = "wlan0";
	ssid = NULL;
	bssid = NULL;

	if (argc == 4) {
		ifnam = argv[1];
		ssid = (uint8_t *)argv[2];
		bssid = (uint8_t *)ether_aton_r(argv[3], &ea);
		if (bssid == NULL)
			warnx("ether_aton_r, ignoring BSSID");
	} else if (argc == 2) {
		ifnam = argv[1];
	}

	sd = socket(AF_LOCAL, SOCK_DGRAM, 0);
	if (sd == -1)
		errx(EX_UNAVAILABLE, "socket");

	error = if_up(sd, ifnam);
	if (error != 0)
		errx(EX_UNAVAILABLE, "if_up");

	if (argc == 4) {
		error = try_mlme_assoc(sd, ifnam, ssid, strlen((const char *)ssid), bssid);
		if (error != 0)
			errx(EX_UNAVAILABLE, "try_mlme_assoc");

	} else {
		error = mlme_assoc_scan_results(sd, ifnam);
		if (error != 0)
			errx(EX_UNAVAILABLE, "mlme_assoc_scan_results");
	}

	close(sd);

	return (0);
}
