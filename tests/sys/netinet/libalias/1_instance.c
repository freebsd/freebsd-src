#include <atf-c.h>
#include <alias.h>
#include <stdio.h>
#include <stdlib.h>

static int randcmp(const void *a, const void *b) {
	int res, r = rand();

	(void)a;
	(void)b;
	res = (r/4 < RAND_MAX/9) ? 1
	    : (r/5 < RAND_MAX/9) ? 0
	    : -1;
	return (res);
}

ATF_TC(2_destroynull);
ATF_TC_HEAD(2_destroynull, env)
{
	atf_tc_set_md_var(env, "descr", "Destroy the NULL instance");
}
ATF_TC_BODY(2_destroynull, dummy)
{
	atf_tc_expect_death("Code expects valid pointer.");
	LibAliasUninit(NULL);
}

ATF_TC(1_singleinit);
ATF_TC_HEAD(1_singleinit, env)
{
	atf_tc_set_md_var(env, "descr", "Create an instance");
}
ATF_TC_BODY(1_singleinit, dummy)
{
	struct libalias *la;

	la = LibAliasInit(NULL);
	ATF_CHECK_MSG(la != NULL, "Creating an instance failed.");
	LibAliasUninit(la);
}

ATF_TC(3_multiinit);
ATF_TC_HEAD(3_multiinit, env)
{
	atf_tc_set_md_var(env, "descr", "Recreate an instance multiple times");
}
ATF_TC_BODY(3_multiinit, dummy)
{
	struct libalias *la;
	int i;

	la = LibAliasInit(NULL);
	for(i = 1; i < 30; i++) {
		struct libalias *lo = la;

		la = LibAliasInit(la);
		ATF_CHECK_MSG(la == lo, "Recreating moved the instance around: %d", i);
	}
	LibAliasUninit(la);
}

ATF_TC(4_multiinstance);
ATF_TC_HEAD(4_multiinstance, env)
{
	atf_tc_set_md_var(env, "descr", "Create and destoy multiple instances.");
}
ATF_TC_BODY(4_multiinstance, dummy)
{
	struct libalias *la[300];
	int const num_instances = sizeof(la) / sizeof(*la);
	int i;

	for (i = 0; i < num_instances; i++) {
		la[i] = LibAliasInit(NULL);
		ATF_CHECK_MSG(la[i] != NULL, "Creating instance %d failed.", i);
	}

	qsort(la, num_instances, sizeof(*la), randcmp);

	for (i = 0; i < num_instances; i++)
		LibAliasUninit(la[i]);
}

ATF_TP_ADD_TCS(instance)
{
	/* Use "dd if=/dev/random bs=2 count=1 | od -x" to reproduce */
	srand(0x5ac4);

	ATF_TP_ADD_TC(instance, 2_destroynull);
	ATF_TP_ADD_TC(instance, 1_singleinit);
	ATF_TP_ADD_TC(instance, 3_multiinit);
	ATF_TP_ADD_TC(instance, 4_multiinstance);

	return atf_no_error();
}
