/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 James Gritton <jamie@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/jail.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <jail.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <atf-c.h>

#define NJAILS 3		/* One master jail and two to race. */
#define NROUNDS 10000		/* Number of attempts to make race happen. */

struct jailinfo {
	int jfd;
	ino_t ino;
	char path[MAXPATHLEN];
};

static pthread_barrier_t barrier;
static struct jailinfo jinfo[NJAILS];

/* Attach a thread to a jail with jail_attach_jd. */
static void *
thread_jail_attach_jd(void *arg)
{
	int error;

	/*
	 * Synchronize to get as close as possible to the same time,
	 * then attach to a jail.
	 */
	error = pthread_barrier_wait(&barrier);
	ATF_REQUIRE_MSG(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD,
	    "pthread_barrier_wait: %s", strerror(errno));
	ATF_REQUIRE_MSG(jail_attach_jd(jinfo[(size_t)arg].jfd) == 0,
	    "jail_attach_jd: %s", strerror(errno));
	return (NULL);
}

/* Attach a thread to a jail with jail_setv. */
static void *
thread_jail_setv(void *arg)
{
	int error;
	char jdescstr[16];

	error = pthread_barrier_wait(&barrier);
	ATF_REQUIRE_MSG(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD,
	    "pthread_barrier_wait: %s", strerror(errno));
	snprintf(jdescstr, sizeof(jdescstr), "%d", jinfo[(size_t)arg].jfd);
	ATF_REQUIRE_MSG(jail_setv(JAIL_UPDATE | JAIL_ATTACH | JAIL_USE_DESC,
	    "desc", jdescstr, NULL) > 0,
	    "jail_setv: %s", jail_errmsg[0] ? jail_errmsg : strerror(errno));
	return (NULL);
}

/* Attach a thread to a jail with chroot. */
static void *
thread_chroot(void *arg)
{
	int error;

	/* This is a race between jail_attach and chroot. */
	if ((size_t)arg > 1)
		return thread_jail_attach_jd(arg);
	error = pthread_barrier_wait(&barrier);
	ATF_REQUIRE_MSG(error == 0 || error == PTHREAD_BARRIER_SERIAL_THREAD,
	    "pthread_barrier_wait: %s", strerror(errno));
	ATF_REQUIRE_MSG(chroot(jinfo[(size_t)arg].path) == 0 || errno == ENOENT,
	    "chroot: %s", strerror(errno));
	return (NULL);
}

static void
thread_attach_test(void *(*thread_handler)(void*), const char *jail_name,
    const char *syscall_name, bool jail_reset)
{
	int ri, spn, mixed_jails;
	size_t ji, ti, ji_hostname, ji_ino, jail_namelen;
	char *cwd;
	struct stat st;
	char jnamestr[64], jdescstr[16];
	pthread_t threads[NJAILS];

	if (jinfo[0].jfd == 0) {
		/* Start with a master jail, so we can return to real root. */
		jdescstr[0] = '\0';
		ATF_REQUIRE_MSG(jail_setv(JAIL_CREATE | JAIL_OWN_DESC,
		    "name", jail_name,
		    "path", "/",
		    "desc", jdescstr,
		    "persist", "true",
		    NULL) > 0,
		    "jail_setv jail 0: %s",
		    jail_errmsg[0] ? jail_errmsg : strerror(errno));
		jinfo[0].jfd = strtol(jdescstr, NULL, 10);
		/* Make enough jails to cause contention. */
		cwd = getcwd(NULL, MAXPATHLEN);
		ATF_REQUIRE_MSG(cwd != NULL, "getcwd: %s", strerror(errno));
		for (ji = 1; ji < NJAILS; ++ji) {
			snprintf(jnamestr, sizeof(jnamestr),
			    "%s%zu", jail_name, ji);
			spn = snprintf(jinfo[ji].path, MAXPATHLEN,
			    "%s/jail%zu", cwd, ji);
			ATF_REQUIRE_MSG((size_t)spn < MAXPATHLEN,
			    "snprintf exceeded MAXPATHLEN: %d", spn);
			ATF_REQUIRE_MSG(
			    mkdir(jinfo[ji].path, 0755) == 0 || errno == EEXIST,
			    "mkdir %s: %s", jinfo[ji].path, strerror(errno));
			ATF_REQUIRE_MSG(stat(jinfo[ji].path, &st) == 0,
			    "stat %s: %s", jinfo[ji].path, strerror(errno));
			jinfo[ji].ino = st.st_ino;
			jdescstr[0] = '\0';
			ATF_REQUIRE_MSG(jail_setv(JAIL_CREATE | JAIL_OWN_DESC,
			    "name", jnamestr,
			    "host.hostname", jnamestr,
			    "path", jinfo[ji].path,
			    "desc", jdescstr,
			    "persist", "true",
			    NULL) > 0,
			    "jail_setv: %s",
			    jail_errmsg[0] ? jail_errmsg : strerror(errno));
			jinfo[ji].jfd = strtol(jdescstr, NULL, 10);
		}
	} else
		ATF_REQUIRE_MSG(jail_attach_jd(jinfo[0].jfd) == 0,
		    "jail_attach_jd: %s", strerror(errno));

	/* Check the different system calls that can race. */
	jail_namelen = strlen(jail_name);
	mixed_jails = 0;
	for (ri = 0; ri < NROUNDS; ++ri) {
		ATF_REQUIRE_MSG(
		    pthread_barrier_init(&barrier, NULL, NJAILS - 1) == 0,
		    "pthread_barrier_init: %s", strerror(errno));
		for (ti = 1; ti < NJAILS; ++ti)
			ATF_REQUIRE_MSG(
			    pthread_create(&threads[ti], NULL, thread_handler,
			    (void*)ti) == 0,
			    "pthread_create: %s", strerror(errno));
		for (ti = 1; ti < NJAILS; ++ti)
			ATF_REQUIRE_MSG(
			    pthread_join(threads[ti], NULL) == 0,
			    "pthread_join: %s", strerror(errno));
		ATF_REQUIRE_MSG(pthread_barrier_destroy(&barrier) == 0,
		    "pthread_barrier_destroy: %s", strerror(errno));
		/*
		 * Find the current jail from the hostname, and also
		 * by the root inode.  They should be the same.
		 */
		ATF_REQUIRE_MSG(
		    gethostname(jnamestr, sizeof(jnamestr)) == 0,
		    "gethostname: %s", strerror(errno));
		ATF_REQUIRE_MSG(strncmp(jnamestr, jail_name, jail_namelen) == 0,
		    "unexpected jail hostname %s", jnamestr);
		ji_hostname = strtol(jnamestr + jail_namelen, NULL, 10);
		ATF_REQUIRE_MSG(stat("/", &st) == 0,
		    "stat /: %s", strerror(errno));
		for (ji_ino = 1; ji_ino < NJAILS; ++ji_ino)
			if (jinfo[ji_ino].ino == st.st_ino)
				break;
		ATF_REQUIRE_MSG(ji_ino < NJAILS,
		    "unexpected jail root inode %lu",
		    (unsigned long)st.st_ino);
		mixed_jails += ji_hostname != ji_ino;
		/* Reset to the master jail, required for chroot. */
		if (jail_reset)
			ATF_REQUIRE_MSG(jail_attach_jd(jinfo[0].jfd) == 0,
			    "jail_attach_jd: %s", strerror(errno));
	}
	/* It's an error if any of the rounds had a mismatch. */
	ATF_REQUIRE_MSG(mixed_jails == 0,
	    "%d of %d %s races with different root and "
	    "credentials", mixed_jails, NROUNDS, syscall_name);
}

#define JAIL_NAME "jail_thread_attach_test"


ATF_TC(jail_thread_attach);
ATF_TC_HEAD(jail_thread_attach, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_thread_attach, tc)
{
	thread_attach_test(thread_jail_attach_jd,
	    "jail_thread_attach_test", "jail_attach_jd", false);
}

ATF_TC(jail_thread_attach_jail_set);
ATF_TC_HEAD(jail_thread_attach_jail_set, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_thread_attach_jail_set, tc)
{
	thread_attach_test(thread_jail_setv,
	    "jail_thread_attach_test_jail_set", "jail_set", false);
}

ATF_TC(jail_thread_attach_chroot);
ATF_TC_HEAD(jail_thread_attach_chroot, tc)
{
	atf_tc_set_md_var(tc, "require.user", "root");
}
ATF_TC_BODY(jail_thread_attach_chroot, tc)
{
	thread_attach_test(thread_chroot,
	    "jail_thread_attach_test_chroot", "chroot", true);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, jail_thread_attach);
	ATF_TP_ADD_TC(tp, jail_thread_attach_jail_set);
	ATF_TP_ADD_TC(tp, jail_thread_attach_chroot);
	return (atf_no_error());
}
