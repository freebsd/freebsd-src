/*-
 * Copyright (c) 2017 Spectra Logic Corp
 * All rights reserved.
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

#define _WANT_ALL_LIO_OPCODES

#include <sys/param.h>
#include <sys/event.h>
#include <sys/uio.h>

#include <aio.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdlib.h>

#include <atf-c.h>

#include "local.h"
#include "freebsd_test_suite/macros.h"

static sem_t completions; 


static void
handler(int sig __unused)
{
	ATF_REQUIRE_EQ(0, sem_post(&completions));
}

static void
thr_handler(union sigval sv __unused)
{
	ATF_REQUIRE_EQ(0, sem_post(&completions));
}

/* 
 * If lio_listio is unable to enqueue any requests at all, it should return
 * EAGAIN.
 */
ATF_TC_WITHOUT_HEAD(lio_listio_eagain_kevent);
ATF_TC_BODY(lio_listio_eagain_kevent, tc)
{
	int fd, i, j, kq, max_queue_per_proc, ios_per_call;
	size_t max_queue_per_proc_size;
	struct aiocb *aiocbs[2];
	struct aiocb **list[2];
	struct sigevent sev[2];
	char *buffer;
	const char *path="tempfile";
	void *udata[2];

	ATF_REQUIRE_KERNEL_MODULE("aio");
	ATF_REQUIRE_UNSAFE_AIO();

	max_queue_per_proc_size = sizeof(max_queue_per_proc);
	ATF_REQUIRE_EQ(sysctlbyname("vfs.aio.max_aio_queue_per_proc",
	    &max_queue_per_proc, &max_queue_per_proc_size, NULL, 0), 0);
	ios_per_call = max_queue_per_proc;

	fd = open(path, O_RDWR|O_CREAT, 0666);
	ATF_REQUIRE(fd >= 0);

	kq = kqueue();
	ATF_REQUIRE(kq > 0);

	buffer = calloc(1, 4096);
	ATF_REQUIRE(buffer != NULL);

	/*
	 * Call lio_listio twice, each with the maximum number of operations.
	 * The first call should succeed and the second should fail.
	 */
	for (i = 0; i < 2; i++) {
		aiocbs[i] = calloc(ios_per_call, sizeof(struct aiocb));
		ATF_REQUIRE(aiocbs[i] != NULL);
		list[i] = calloc(ios_per_call, sizeof(struct aiocb*));
		ATF_REQUIRE(list[i] != NULL);
		udata[i] = (void*)((caddr_t)0xdead0000 + i);
		sev[i].sigev_notify = SIGEV_KEVENT;
		sev[i].sigev_notify_kqueue = kq;
		sev[i].sigev_value.sival_ptr = udata[i];
		for (j = 0; j < ios_per_call; j++) {
			aiocbs[i][j].aio_fildes = fd;
			aiocbs[i][j].aio_offset = (i * ios_per_call + j) * 4096;
			aiocbs[i][j].aio_buf = buffer;
			aiocbs[i][j].aio_nbytes = 4096;
			aiocbs[i][j].aio_lio_opcode = LIO_WRITE;
			list[i][j] = &aiocbs[i][j];
		}
	}

	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, list[0], ios_per_call, &sev[0]));
	ATF_REQUIRE_EQ(-1, lio_listio(LIO_NOWAIT, list[1], ios_per_call, &sev[1]));
	/*
	 * The second lio_listio call should fail with EAGAIN.  Bad timing may
	 * mean that some requests did get enqueued, but the result should
	 * still be EAGAIN.
	 */
	ATF_REQUIRE_EQ(errno, EAGAIN);
}


/* With LIO_WAIT, an empty lio_listio should return immediately */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_wait);
ATF_TC_BODY(lio_listio_empty_wait, tc)
{
	struct aiocb *list = NULL;

	ATF_REQUIRE_EQ(0, lio_listio(LIO_WAIT, &list, 0, NULL));
}

/* With LIO_NOWAIT, an empty lio_listio should return immediately */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait);
ATF_TC_BODY(lio_listio_empty_nowait, tc)
{
	struct aiocb *list = NULL;

	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, &list, 0, NULL));
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_kevent);
ATF_TC_BODY(lio_listio_empty_nowait_kevent, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;
	struct kevent kq_returned;
	int kq, result;
	void *udata = (void*)0xdeadbeefdeadbeef;

	atf_tc_expect_timeout("Bug 251515 - lio_listio(2) never sends"
			" kevent if nent==0");
	kq = kqueue();
	ATF_REQUIRE(kq > 0);
	sev.sigev_notify = SIGEV_KEVENT;
	sev.sigev_notify_kqueue = kq;
	sev.sigev_value.sival_ptr = udata;
	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, &list, 0, &sev));
	result = kevent(kq, NULL, 0, &kq_returned, 1, NULL);
	ATF_REQUIRE_MSG(result == 1, "Never got completion notification");
	ATF_REQUIRE_EQ((uintptr_t)list, kq_returned.ident);
	ATF_REQUIRE_EQ(EVFILT_LIO, kq_returned.filter);
	ATF_REQUIRE_EQ(udata, kq_returned.udata);
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_signal);
ATF_TC_BODY(lio_listio_empty_nowait_signal, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;

	ATF_REQUIRE_EQ(0, sem_init(&completions, false, 0));
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = SIGUSR1;
	ATF_REQUIRE(SIG_ERR != signal(SIGUSR1, handler));
	ATF_REQUIRE_EQ(0, lio_listio(LIO_NOWAIT, &list, 0, &sev));
	ATF_REQUIRE_EQ(0, sem_wait(&completions));
	ATF_REQUIRE_EQ(0, sem_destroy(&completions));
}

/*
 * With LIO_NOWAIT, an empty lio_listio should send completion notification
 * immediately
 */
ATF_TC_WITHOUT_HEAD(lio_listio_empty_nowait_thread);
ATF_TC_BODY(lio_listio_empty_nowait_thread, tc)
{
	struct aiocb *list = NULL;
	struct sigevent sev;

	ATF_REQUIRE_EQ(0, sem_init(&completions, false, 0));
	bzero(&sev, sizeof(sev));
	sev.sigev_notify = SIGEV_THREAD;
	sev.sigev_notify_function = thr_handler;
	sev.sigev_notify_attributes = NULL;
	ATF_REQUIRE_MSG(0 == lio_listio(LIO_NOWAIT, &list, 0, &sev),
	    "lio_listio: %s", strerror(errno));
	ATF_REQUIRE_EQ(0, sem_wait(&completions));
	ATF_REQUIRE_EQ(0, sem_destroy(&completions));
}

/*
 * A simple check that the allowed operations work.
 */
ATF_TC_WITHOUT_HEAD(lio_listio_opcodes);
ATF_TC_BODY(lio_listio_opcodes, tc)
{
	struct aiocb write_cb, read_cb, writev_cb, readv_cb;
	struct aiocb *list[] = {&write_cb, &read_cb, &writev_cb, &readv_cb};
	struct iovec writev_iov[2];
	struct iovec readv_iov[2];
	char buffer[6];
	int fd;

	fd = open("testfile", O_CREAT | O_RDWR, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "open: %s", strerror(errno));

	/* We start with numbers in a file and letters in memory... */
	ATF_CHECK_EQ(6, write(fd, "123456", 6));
	memcpy(buffer, "abcdef", 6);

	/* a -> 1 */
	bzero(&write_cb, sizeof(write_cb));
	write_cb.aio_sigevent.sigev_notify = SIGEV_NONE;
	write_cb.aio_fildes = fd;
	write_cb.aio_lio_opcode = LIO_WRITE;
	write_cb.aio_buf = &buffer[0];
	write_cb.aio_nbytes = 1;
	write_cb.aio_offset = 0;

	/* b <- 2 */
	bzero(&read_cb, sizeof(read_cb));
	read_cb.aio_sigevent.sigev_notify = SIGEV_NONE;
	read_cb.aio_fildes = fd;
	read_cb.aio_lio_opcode = LIO_READ;
	read_cb.aio_buf = &buffer[1];
	read_cb.aio_nbytes = 1;
	read_cb.aio_offset = 1;

	/* d -> 3, c -> 4 */
	writev_iov[0].iov_base = &buffer[3];
	writev_iov[0].iov_len = 1;
	writev_iov[1].iov_base = &buffer[2];
	writev_iov[1].iov_len = 1;
	bzero(&writev_cb, sizeof(writev_cb));
	writev_cb.aio_sigevent.sigev_notify = SIGEV_NONE;
	writev_cb.aio_fildes = fd;
	writev_cb.aio_lio_opcode = LIO_WRITEV;
	writev_cb.aio_iov = &writev_iov;
	writev_cb.aio_iovcnt = 2;
	writev_cb.aio_offset = 2;

	/* f <- 5, e <- 6 */
	readv_iov[0].iov_base = &buffer[5];
	readv_iov[0].iov_len = 1;
	readv_iov[1].iov_base = &buffer[4];
	readv_iov[1].iov_len = 1;
	bzero(&readv_cb, sizeof(readv_cb));
	readv_cb.aio_sigevent.sigev_notify = SIGEV_NONE;
	readv_cb.aio_fildes = fd;
	readv_cb.aio_lio_opcode = LIO_READV;
	readv_cb.aio_iov = &readv_iov;
	readv_cb.aio_iovcnt = 2;
	readv_cb.aio_offset = 4;

	ATF_CHECK_EQ(0, lio_listio(LIO_WAIT, list, nitems(list), NULL));
	ATF_CHECK_EQ(0, aio_error(&write_cb));
	ATF_CHECK_EQ(1, aio_return(&write_cb));
	ATF_CHECK_EQ(0, aio_error(&read_cb));
	ATF_CHECK_EQ(1, aio_return(&read_cb));
	ATF_CHECK_EQ(0, aio_error(&writev_cb));
	ATF_CHECK_EQ(2, aio_return(&writev_cb));
	ATF_CHECK_EQ(0, aio_error(&readv_cb));
	ATF_CHECK_EQ(2, aio_return(&readv_cb));

	ATF_CHECK_EQ(0, memcmp(buffer, "a2cd65", 6));
	ATF_CHECK_EQ(6, pread(fd, buffer, 6, 0));
	ATF_CHECK_EQ(0, memcmp(buffer, "a2dc56", 6));

	close(fd);
}


/*
 * Only select opcodes are allowed with lio_listio
 */
ATF_TC_WITHOUT_HEAD(lio_listio_invalid_opcode);
ATF_TC_BODY(lio_listio_invalid_opcode, tc)
{
	struct aiocb sync_cb, mlock_cb;
	struct aiocb *list[] = {&sync_cb, &mlock_cb};
	int fd;

	fd = open("testfile", O_CREAT | O_RDWR, 0666);
	ATF_REQUIRE_MSG(fd >= 0, "open: %s", strerror(errno));

	bzero(&sync_cb, sizeof(sync_cb));
	sync_cb.aio_fildes = fd;
	sync_cb.aio_lio_opcode = LIO_SYNC;

	bzero(&mlock_cb, sizeof(mlock_cb));
	mlock_cb.aio_lio_opcode = LIO_MLOCK;

	ATF_CHECK_ERRNO(EIO, lio_listio(LIO_WAIT, list, nitems(list), NULL));
	ATF_CHECK_EQ(EINVAL, aio_error(&sync_cb));
	ATF_CHECK_ERRNO(EINVAL, aio_return(&sync_cb) < 0);
	ATF_CHECK_EQ(EINVAL, aio_error(&mlock_cb));
	ATF_CHECK_ERRNO(EINVAL, aio_return(&mlock_cb) < 0);

	close(fd);
}


ATF_TP_ADD_TCS(tp)
{

	ATF_TP_ADD_TC(tp, lio_listio_eagain_kevent);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_kevent);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_signal);
	ATF_TP_ADD_TC(tp, lio_listio_empty_nowait_thread);
	ATF_TP_ADD_TC(tp, lio_listio_empty_wait);
	ATF_TP_ADD_TC(tp, lio_listio_opcodes);
	ATF_TP_ADD_TC(tp, lio_listio_invalid_opcode);

	return (atf_no_error());
}
