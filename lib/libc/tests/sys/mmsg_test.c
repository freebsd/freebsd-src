/*
 * Copyright (c) 2026 Yeatol
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#define	_WANT_P_OSREL
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/user.h>

#include <atf-c.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

ATF_TC_WITHOUT_HEAD(mmsg_batch);
ATF_TC_BODY(mmsg_batch, tc)
{
	struct mmsghdr tx[2], rx[3];
	struct iovec tiov[2], riov[3];
	struct sigaction sa;
	char out[2] = { 'x', 'y' }, in[3] = {0};
	int sv[2];

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = SIG_DFL;
	ATF_REQUIRE_EQ(0, sigemptyset(&sa.sa_mask));
	ATF_REQUIRE_EQ(0, sigaction(SIGSYS, &sa, NULL));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	memset(tx, 0, sizeof(tx));
	memset(rx, 0, sizeof(rx));
	for (int i = 0; i < 2; i++) {
		tiov[i].iov_base = &out[i];
		tiov[i].iov_len = 1;
		tx[i].msg_hdr.msg_iov = &tiov[i];
		tx[i].msg_hdr.msg_iovlen = 1;
		tx[i].msg_len = -1;
	}
	for (int i = 0; i < 3; i++) {
		riov[i].iov_base = &in[i];
		riov[i].iov_len = 1;
		rx[i].msg_hdr.msg_iov = &riov[i];
		rx[i].msg_hdr.msg_iovlen = 1;
		rx[i].msg_len = -1;
	}
	ATF_REQUIRE_EQ(2, sendmmsg(sv[0], tx, 2, 0));
	ATF_REQUIRE_EQ(2, recvmmsg(sv[1], rx, 3, MSG_WAITFORONE, NULL));
	for (int i = 0; i < 2; i++) {
		ATF_CHECK_EQ(1, tx[i].msg_len);
		ATF_CHECK_EQ(1, rx[i].msg_len);
		ATF_CHECK_EQ(out[i], in[i]);
	}
	ATF_CHECK_EQ(-1, rx[2].msg_len);
	ATF_REQUIRE_ERRNO(EAGAIN,
	    recvmmsg(sv[1], rx, 1, MSG_DONTWAIT, NULL) == -1);
	close(sv[0]);
	close(sv[1]);
}

ATF_TC_WITHOUT_HEAD(mmsg_zero_length);
ATF_TC_BODY(mmsg_zero_length, tc)
{
	ATF_REQUIRE_EQ(0, sendmmsg(-1, NULL, 0, 0));
	ATF_REQUIRE_EQ(0, recvmmsg(-1, NULL, 0, 0, NULL));
}

struct cancel_context {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int sd;
	int error;
	int ready;
	lwpid_t tid;
	int partial;
	ssize_t received;
	ssize_t lengths[2];
	char data[2];
};

static void *
cancel_receiver(void *arg)
{
	struct cancel_context *ctx = arg;
	struct timespec timeout = { .tv_sec = 10 };
	struct mmsghdr msgs[2];
	struct iovec iov[2];
	size_t vlen = ctx->partial ? 2 : 1;

	ATF_REQUIRE_EQ(0, pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL));
	ATF_REQUIRE_EQ(0, pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL));
	memset(msgs, 0, sizeof(msgs));
	for (size_t i = 0; i < vlen; i++) {
		iov[i].iov_base = &ctx->data[i];
		iov[i].iov_len = sizeof(ctx->data[i]);
		msgs[i].msg_hdr.msg_iov = &iov[i];
		msgs[i].msg_hdr.msg_iovlen = 1;
		msgs[i].msg_len = -1;
	}
	ATF_REQUIRE_EQ(0, pthread_mutex_lock(&ctx->mutex));
	ctx->tid = pthread_getthreadid_np();
	ctx->ready = 1;
	ATF_REQUIRE_EQ(0, pthread_cond_signal(&ctx->cond));
	while (ctx->ready == 1)
		ATF_REQUIRE_EQ(0, pthread_cond_wait(&ctx->cond, &ctx->mutex));
	ATF_REQUIRE_EQ(0, pthread_mutex_unlock(&ctx->mutex));
	ATF_REQUIRE_EQ(0, pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL));
	ctx->received = recvmmsg(ctx->sd, msgs, vlen, 0, &timeout);
	ctx->error = errno;
	for (size_t i = 0; i < vlen; i++)
		ctx->lengths[i] = msgs[i].msg_len;
	/* A successful partial batch must be observable before cancellation. */
	if (ctx->partial)
		pthread_testcancel();
	return (arg);
}

static void
wait_for_receiver(lwpid_t tid)
{
	struct kinfo_proc threads[8];
	struct timespec deadline, now;
	size_t len;
	int mib[] = {
		CTL_KERN, KERN_PROC, KERN_PROC_PID | KERN_PROC_INC_THREAD, getpid(),
	};

	ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_MONOTONIC, &deadline));
	deadline.tv_sec += 5;
	for (;;) {
		len = sizeof(threads);
		ATF_REQUIRE_EQ(0, sysctl(mib, nitems(mib), threads, &len,
		    NULL, 0));
		for (size_t i = 0; i < len / sizeof(threads[0]); i++) {
			if (threads[i].ki_tid == tid &&
			    strcmp(threads[i].ki_wmesg, "sbwait") == 0)
				return;
		}
		ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_MONOTONIC, &now));
		ATF_REQUIRE_MSG(now.tv_sec < deadline.tv_sec ||
		    (now.tv_sec == deadline.tv_sec &&
		    now.tv_nsec < deadline.tv_nsec),
		    "receiver did not block waiting for a datagram");
		usleep(1000);
	}
}

static void
check_cancel(bool pending, bool partial)
{
	struct cancel_context ctx;
	struct timespec deadline;
	pthread_t thread;
	size_t len;
	void *result = NULL;
	int error, osreldate, sv[2];
	char ch = 'x';

	if (!pending) {
		len = sizeof(osreldate);
		ATF_REQUIRE_EQ(0, sysctlbyname("kern.osreldate", &osreldate,
		    &len, NULL, 0));
		if (osreldate < P_OSREL_MMSG)
			atf_tc_skip("Requires native recvmmsg cancellation");
	}
	memset(&ctx, 0, sizeof(ctx));
	ATF_REQUIRE_EQ(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sv));
	ctx.sd = sv[1];
	ctx.partial = partial;
	ctx.received = -1;
	if (partial)
		ATF_REQUIRE_EQ(1, send(sv[0], &ch, sizeof(ch), 0));
	ATF_REQUIRE_EQ(0, pthread_mutex_init(&ctx.mutex, NULL));
	ATF_REQUIRE_EQ(0, pthread_cond_init(&ctx.cond, NULL));
	ATF_REQUIRE_EQ(0, pthread_create(&thread, NULL, cancel_receiver, &ctx));
	ATF_REQUIRE_EQ(0, pthread_mutex_lock(&ctx.mutex));
	while (ctx.ready == 0)
		ATF_REQUIRE_EQ(0, pthread_cond_wait(&ctx.cond, &ctx.mutex));
	if (pending)
		ATF_REQUIRE_EQ(0, pthread_cancel(thread));
	ctx.ready = 2;
	ATF_REQUIRE_EQ(0, pthread_cond_signal(&ctx.cond));
	ATF_REQUIRE_EQ(0, pthread_mutex_unlock(&ctx.mutex));
	if (!pending) {
		wait_for_receiver(ctx.tid);
		ATF_REQUIRE_EQ(0, pthread_cancel(thread));
	}
	ATF_REQUIRE_EQ(0, clock_gettime(CLOCK_REALTIME, &deadline));
	deadline.tv_sec += 2;
	error = pthread_timedjoin_np(thread, &result, &deadline);
	ATF_REQUIRE_MSG(error == 0 || error == ETIMEDOUT,
	    "pthread_timedjoin_np failed: %s", strerror(error));
	if (error == ETIMEDOUT) {
		ATF_REQUIRE_EQ(1, send(sv[0], &ch, sizeof(ch), 0));
		ATF_REQUIRE_EQ(0, pthread_join(thread, &result));
	}
	ATF_CHECK_MSG(error == 0 && result == PTHREAD_CANCELED,
	    "timed recvmmsg did not honor deferred cancellation: "
	    "join=%d, canceled=%d, receive errno=%d",
	    error, result == PTHREAD_CANCELED, ctx.error);
	if (partial) {
		ATF_CHECK_EQ(1, ctx.received);
		ATF_CHECK_EQ(1, ctx.lengths[0]);
		ATF_CHECK_EQ('x', ctx.data[0]);
		ATF_CHECK_EQ(-1, ctx.lengths[1]);
	}
	ATF_REQUIRE_EQ(0, pthread_cond_destroy(&ctx.cond));
	ATF_REQUIRE_EQ(0, pthread_mutex_destroy(&ctx.mutex));
	ATF_REQUIRE_EQ(0, close(sv[0]));
	ATF_REQUIRE_EQ(0, close(sv[1]));
}

ATF_TC(mmsg_cancel);
ATF_TC_HEAD(mmsg_cancel, tc)
{
	atf_tc_set_md_var(tc, "timeout", "15");
}
ATF_TC_BODY(mmsg_cancel, tc)
{
	check_cancel(true, false);
}

ATF_TC(mmsg_cancel_blocked);
ATF_TC_HEAD(mmsg_cancel_blocked, tc)
{
	atf_tc_set_md_var(tc, "timeout", "15");
}
ATF_TC_BODY(mmsg_cancel_blocked, tc)
{
	check_cancel(false, false);
}

ATF_TC(mmsg_cancel_partial);
ATF_TC_HEAD(mmsg_cancel_partial, tc)
{
	atf_tc_set_md_var(tc, "timeout", "15");
}
ATF_TC_BODY(mmsg_cancel_partial, tc)
{
	check_cancel(false, true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, mmsg_batch);
	ATF_TP_ADD_TC(tp, mmsg_zero_length);
	ATF_TP_ADD_TC(tp, mmsg_cancel);
	ATF_TP_ADD_TC(tp, mmsg_cancel_blocked);
	ATF_TP_ADD_TC(tp, mmsg_cancel_partial);
	return (atf_no_error());
}
