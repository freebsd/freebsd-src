/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 The FreeBSD Foundation
 *
 * This software was developed by Christos Margiolis <christos@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/event.h>
#include <sys/soundcard.h>

#include <atf-c.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <poll.h>
#include <unistd.h>

#define	FMT_ERR(s)	s ": %s", strerror(errno)

static int
oss_init(void)
{
	int fd, tmp, rc;

	fd = open("/dev/dsp.dummy", O_RDWR);
	ATF_REQUIRE_MSG(fd >= 0, FMT_ERR("open"));

	tmp = 2;
	rc = ioctl(fd, SNDCTL_DSP_CHANNELS, &tmp);
	ATF_REQUIRE_EQ_MSG(rc, 0, FMT_ERR("ioctl"));

	tmp = AFMT_S16_LE;
	rc = ioctl(fd, SNDCTL_DSP_SETFMT, &tmp);
	ATF_REQUIRE_EQ_MSG(rc, 0, FMT_ERR("ioctl"));

	tmp = 48000;
	rc = ioctl(fd, SNDCTL_DSP_SPEED, &tmp);
	ATF_REQUIRE_EQ_MSG(rc, 0, FMT_ERR("ioctl"));

	/*
	 * See http://manuals.opensound.com/developer/SNDCTL_DSP_SETTRIGGER.html
	 */
	tmp = PCM_ENABLE_INPUT | PCM_ENABLE_OUTPUT;
	rc = ioctl(fd, SNDCTL_DSP_SETTRIGGER, &tmp);
	ATF_REQUIRE_EQ_MSG(rc, 0, FMT_ERR("ioctl"));

	return (fd);
}

ATF_TC(poll_kqueue);
ATF_TC_HEAD(poll_kqueue, tc)
{
	atf_tc_set_md_var(tc, "descr", "kqueue(2) test");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}

ATF_TC_BODY(poll_kqueue, tc)
{
	struct kevent ev;
	int16_t buf[32];
	int fd, kq;

	fd = oss_init();

	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, FMT_ERR("kqueue"));
	EV_SET(&ev, fd, EVFILT_READ, EV_ADD | EV_CLEAR, 0, 0, 0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));

	ATF_REQUIRE_MSG(kevent(kq, NULL, 0, &ev, 1, NULL) == 1,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG((ev.flags & EV_ERROR) == 0, "EV_ERROR is set");
	ATF_REQUIRE_MSG(ev.data != 0, "data is %" PRId64, ev.data);
	ATF_REQUIRE_MSG(read(fd, buf, sizeof(buf)) > 0, FMT_ERR("read"));

	EV_SET(&ev, fd, EVFILT_READ, EV_DELETE, 0, 0, 0);
	close(kq);

	kq = kqueue();
	ATF_REQUIRE_MSG(kq >= 0, FMT_ERR("kqueue"));
	EV_SET(&ev, fd, EVFILT_WRITE, EV_ADD | EV_CLEAR, 0, 0, 0);
	ATF_REQUIRE_MSG(kevent(kq, &ev, 1, NULL, 0, NULL) == 0,
	    FMT_ERR("kevent"));

	ATF_REQUIRE_MSG(kevent(kq, NULL, 0, &ev, 1, NULL) == 1,
	    FMT_ERR("kevent"));
	ATF_REQUIRE_MSG((ev.flags & EV_ERROR) == 0, "EV_ERROR is set");
	ATF_REQUIRE_MSG(ev.data != 0, "data is %" PRId64, ev.data);
	ATF_REQUIRE_MSG(write(fd, buf, sizeof(buf)) > 0, FMT_ERR("write"));

	EV_SET(&ev, fd, EVFILT_WRITE, EV_DELETE, 0, 0, 0);
	close(kq);

	close(fd);
}

ATF_TC(poll_poll);
ATF_TC_HEAD(poll_poll, tc)
{
	atf_tc_set_md_var(tc, "descr", "poll(2) test");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}

ATF_TC_BODY(poll_poll, tc)
{
	struct pollfd pfd[2];
	int16_t buf[32];
	int fd;
	bool rd = false;
	bool wr = false;

	fd = oss_init();

	while (!rd || !wr) {
		pfd[0].fd = fd;
		pfd[0].events = POLLIN;
		pfd[1].fd = fd;
		pfd[1].events = POLLOUT;
		ATF_REQUIRE_MSG(poll(pfd, sizeof(pfd) / sizeof(struct pollfd),
		    -1) > 0, FMT_ERR("poll"));

		if (pfd[0].revents) {
			ATF_REQUIRE_MSG(read(fd, buf, sizeof(buf)) > 0,
			    FMT_ERR("read"));
			rd = true;
		}
		if (pfd[1].revents) {
			ATF_REQUIRE_MSG(write(fd, buf, sizeof(buf)) > 0,
			    FMT_ERR("write"));
			wr = true;
		}
	}
	close(fd);
}

ATF_TC(poll_select);
ATF_TC_HEAD(poll_select, tc)
{
	atf_tc_set_md_var(tc, "descr", "select(2) test");
	atf_tc_set_md_var(tc, "require.kmods", "snd_dummy");
}

ATF_TC_BODY(poll_select, tc)
{
	fd_set fds[2];
	int16_t buf[32];
	int fd;
	bool rd = false;
	bool wr = false;

	fd = oss_init();

	while (!rd || !wr) {
		FD_ZERO(&fds[0]);
		FD_ZERO(&fds[1]);
		FD_SET(fd, &fds[0]);
		FD_SET(fd, &fds[1]);
		ATF_REQUIRE_MSG(select(fd + 2, &fds[0], &fds[1], NULL, NULL) > 0,
		    FMT_ERR("select"));
		if (FD_ISSET(fd, &fds[0])) {
			ATF_REQUIRE_MSG(read(fd, buf, sizeof(buf)) > 0,
			    FMT_ERR("read"));
			rd = true;
		}
		if (FD_ISSET(fd, &fds[1])) {
			ATF_REQUIRE_MSG(write(fd, buf, sizeof(buf)) > 0,
			    FMT_ERR("write"));
			wr = true;
		}
	}
	close(fd);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, poll_kqueue);
	ATF_TP_ADD_TC(tp, poll_poll);
	ATF_TP_ADD_TC(tp, poll_select);

	return (atf_no_error());
}
