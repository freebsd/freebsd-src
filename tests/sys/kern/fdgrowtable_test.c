/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Rob Wing
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

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <atf-c.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* linked libraries */
#include <kvm.h>
#include <libutil.h>
#include <libprocstat.h>
#include <pthread.h>

/* test-case macro */
#define AFILE "afile"

/*
 * The following macros, struct freetable, struct fdescenttbl0
 * and struct filedesc0 are copied from sys/kern/kern_descrip.c
 */
#define NDFILE		20
#define NDSLOTSIZE	sizeof(NDSLOTTYPE)
#define	NDENTRIES	(NDSLOTSIZE * __CHAR_BIT)
#define NDSLOT(x)	((x) / NDENTRIES)
#define NDBIT(x)	((NDSLOTTYPE)1 << ((x) % NDENTRIES))
#define	NDSLOTS(x)	(((x) + NDENTRIES - 1) / NDENTRIES)

struct freetable {
	struct fdescenttbl *ft_table;
	SLIST_ENTRY(freetable) ft_next;
};

struct fdescenttbl0 {
	int	fdt_nfiles;
	struct	filedescent fdt_ofiles[NDFILE];
};

struct filedesc0 {
	struct filedesc fd_fd;
	SLIST_HEAD(, freetable) fd_free;
	struct	fdescenttbl0 fd_dfiles;
	NDSLOTTYPE fd_dmap[NDSLOTS(NDFILE)];
};

static void
openfiles(int n)
{
	int i, fd;

	ATF_REQUIRE((fd = open(AFILE, O_CREAT, 0644)) != -1);
	close(fd);
	for (i = 0; i < n; i++)
		ATF_REQUIRE((fd = open(AFILE, O_RDONLY, 0644)) != -1);
}

/*
 * Get a count of the old file descriptor tables on the freelist.
 */
static int
old_tables(kvm_t *kd, struct kinfo_proc *kp)
{
	struct filedesc0 fdp0;
	struct freetable *ft, tft;
	int counter;

	counter = 0;

	ATF_REQUIRE(kvm_read(kd, (unsigned long) kp->ki_fd, &fdp0, sizeof(fdp0)) > 0);

	SLIST_FOREACH(ft, &fdp0.fd_free, ft_next) {
		ATF_REQUIRE(kvm_read(kd, (unsigned long) ft, &tft, sizeof(tft)) > 0 );
		ft = &tft;
		counter++;
	}

	return (counter);
}

/*
 *  The returning struct kinfo_proc stores kernel addresses that will be
 *  used by kvm_read to retrieve information for the current process.
 */
static struct kinfo_proc *
read_kinfo(kvm_t *kd)
{
	struct kinfo_proc *kp;
	int procs_found;

	ATF_REQUIRE((kp = kvm_getprocs(kd, KERN_PROC_PID, (int) getpid(), &procs_found)) != NULL);
	ATF_REQUIRE(procs_found == 1);

	return (kp);
}

/*
 * Test a single threaded process that doesn't have a shared
 * file descriptor table. The old tables should be freed.
 */
ATF_TC(free_oldtables);
ATF_TC_HEAD(free_oldtables, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(free_oldtables, tc)
{
	kvm_t *kd;
	struct kinfo_proc *kp;

	ATF_REQUIRE((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) != NULL);
	openfiles(128);
	kp = read_kinfo(kd);
	ATF_CHECK(old_tables(kd,kp) == 0);
}

static _Noreturn void *
exec_thread(void *args)
{
	openfiles(128);
	for (;;)
		sleep(1);
}

/*
 * Test a process with two threads that doesn't have a shared file
 * descriptor table. The old tables should not be freed.
 */
ATF_TC(oldtables_shared_via_threads);
ATF_TC_HEAD(oldtables_shared_via_threads, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(oldtables_shared_via_threads, tc)
{
	kvm_t *kd;
	struct kinfo_proc *kp;
	pthread_t thread;

	ATF_REQUIRE((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) != NULL);
	ATF_REQUIRE(pthread_create(&thread, NULL, exec_thread, NULL) == 0);

	openfiles(128);

	kp = read_kinfo(kd);
	ATF_CHECK(kp->ki_numthreads > 1);
	ATF_CHECK(old_tables(kd,kp) > 1);

	ATF_REQUIRE(pthread_cancel(thread) == 0);
	ATF_REQUIRE(pthread_join(thread, NULL) == 0);
}

/*
 * Get the reference count of a file descriptor table.
 */
static int
filedesc_refcnt(kvm_t *kd, struct kinfo_proc *kp)
{
	struct filedesc fdp;

	ATF_REQUIRE(kvm_read(kd, (unsigned long) kp->ki_fd, &fdp, sizeof(fdp)) > 0);

	return (fdp.fd_refcnt);
}

/*
 * Test a single threaded process that shares a file descriptor
 * table with another process. The old tables should not be freed.
 */
ATF_TC(oldtables_shared_via_process);
ATF_TC_HEAD(oldtables_shared_via_process, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}

ATF_TC_BODY(oldtables_shared_via_process, tc)
{
	kvm_t *kd;
	struct kinfo_proc *kp;
	int status;
	pid_t child, wpid;

	ATF_REQUIRE((kd = kvm_open(NULL, NULL, NULL, O_RDONLY, NULL)) != NULL);

	/* share the file descriptor table */
	ATF_REQUIRE((child = rfork(RFPROC)) != -1);

	if (child == 0) {
		openfiles(128);
		raise(SIGSTOP);
		exit(127);
	}

	/* let parent process open some files too */
	openfiles(128);

	/* get current status of child */
	wpid = waitpid(child, &status, WUNTRACED);
	ATF_REQUIRE(wpid == child);

	/* child should be stopped */
	ATF_REQUIRE(WIFSTOPPED(status));

	/*
	 * We want to read kernel data
	 * before the child exits
	 * otherwise we'll lose a reference count
	 * to the file descriptor table
	 */
	kp = read_kinfo(kd);

	ATF_CHECK(filedesc_refcnt(kd,kp) > 1);
	ATF_CHECK(old_tables(kd,kp) > 1);

	kill(child, SIGCONT);

	/* child should have exited */
	wpid = waitpid(child, &status, 0);
	ATF_REQUIRE(wpid == child);
	ATF_REQUIRE(WIFEXITED(status));
	ATF_REQUIRE(WEXITSTATUS(status) == 127);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, free_oldtables);
	ATF_TP_ADD_TC(tp, oldtables_shared_via_threads);
	ATF_TP_ADD_TC(tp, oldtables_shared_via_process);
	return (atf_no_error());
}
