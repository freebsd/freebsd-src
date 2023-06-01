#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/cpuset.h>

#include <stdio.h>
#include <libutil.h>
#include <atf-c.h>

ATF_TC(invalid);
ATF_TC_HEAD(invalid, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test invalid cpu");
}

ATF_TC_BODY(invalid, tc)
{
	cpuset_t mask;
	char testvalue[BUFSIZ];

	snprintf(testvalue, sizeof(testvalue), "%d", CPU_SETSIZE + 1);

	ATF_CHECK_EQ(cpuset_parselist(testvalue,  &mask), CPUSET_PARSE_INVALID_CPU);
}

ATF_TC(invalidchar);
ATF_TC_HEAD(invalidchar, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test invalid char");
}

ATF_TC_BODY(invalidchar, tc)
{
	cpuset_t mask;

	ATF_CHECK_EQ(cpuset_parselist("1+3",  &mask), CPUSET_PARSE_ERROR);
}

ATF_TC(all);
ATF_TC_HEAD(all, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test 'all' special cpu-list");
}

ATF_TC_BODY(all, tc)
{
	cpuset_t mask;

	ATF_CHECK_EQ(cpuset_parselist("all",  &mask), CPUSET_PARSE_OK);
}

ATF_TC(normalsyntax);
ATF_TC_HEAD(normalsyntax, tc)
{
	atf_tc_set_md_var(tc, "descr", "Test normal cpu-list syntax");
}

ATF_TC_BODY(normalsyntax, tc)
{
	cpuset_t mask;

	ATF_CHECK_EQ(cpuset_parselist("1-3,6",  &mask), CPUSET_PARSE_OK);
}

ATF_TP_ADD_TCS(tp)
{
	ATF_TP_ADD_TC(tp, invalid);
	ATF_TP_ADD_TC(tp, invalidchar);
	ATF_TP_ADD_TC(tp, all);
	ATF_TP_ADD_TC(tp, normalsyntax);
	return (atf_no_error());
}
