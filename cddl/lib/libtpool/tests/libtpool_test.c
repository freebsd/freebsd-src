#include <sys/stdtypes.h>
#include <sys/sysctl.h>
#include <errno.h>
#include <pthread.h>

#include <thread_pool.h>

#include <atf-c.h>

static void
tp_delay(void *arg)
{
	pthread_barrier_t *barrier = arg;

	/* Block this task until all thread pool workers have been created. */
	pthread_barrier_wait(barrier);
}

/*
 * NB: we could reduce the test's resource cost by using rctl(4).  But that
 * isn't enabled by default.  And even with a thread limit of 1500, it takes <
 * 0.1s to run on my machine.  So I don't think it's worth optimizing for the
 * case where rctl is available.
 */
ATF_TC(complete_exhaustion);
ATF_TC_HEAD(complete_exhaustion, tc)
{
	atf_tc_set_md_var(tc, "descr",
	    "A thread pool should fail to schedule tasks if it is completely impossible to spawn any threads.");
}

ATF_TC_BODY(complete_exhaustion, tc)
{
	pthread_barrier_t barrier;
	tpool_t *tp0, *tp1;
	size_t len;
	int max_threads_per_proc = 0;
	int nworkers;
	int r, i;


	len = sizeof(max_threads_per_proc);
	r = sysctlbyname("kern.threads.max_threads_per_proc",
	    &max_threads_per_proc, &len, NULL, 0);
	ATF_REQUIRE_EQ_MSG(r, 0, "sysctlbyname: %s", strerror(errno));
	nworkers = max_threads_per_proc - 1;
	pthread_barrier_init(&barrier, NULL, max_threads_per_proc);

	/*
	 * Create the first thread pool and spawn the maximum allowed number of
	 * processes.
	 */
	tp0 = tpool_create(nworkers, nworkers, 1, NULL);
	ATF_REQUIRE(tp0 != NULL);
	for (i = 0; i < nworkers; i++) {
		ATF_REQUIRE_EQ(tpool_dispatch(tp0, tp_delay, &barrier), 0);
	}

	/*
	 * Now create a second thread pool.  Unable to create new threads, the
	 * dispatch function should return an error.
	 */
	tp1 = tpool_create(nworkers, 2 * nworkers, 1, NULL);
	ATF_REQUIRE(tp1 != NULL);
	ATF_REQUIRE_EQ(tpool_dispatch(tp1, tp_delay, NULL), -1);

	/* Cleanup */
	ATF_REQUIRE_EQ(pthread_barrier_wait(&barrier), 0);
	tpool_wait(tp1);
	tpool_wait(tp0);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, complete_exhaustion);

	return (atf_no_error());
}
