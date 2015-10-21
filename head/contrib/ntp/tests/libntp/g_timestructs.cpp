/*
 * timestructs.cpp -- test bed adaptors for time structs.
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 */
#include "g_libntptest.h"
#include "g_timestructs.h"

extern "C" {
#include "timetoa.h"
#include "timevalops.h"
#include "timespecops.h"
}

namespace timeStruct {

std::ostream&
operator << (std::ostream& os, const timeStruct::l_fp_wrap& val)
{
	// raw data formatting
	os << "0x" << std::hex << val.V.l_ui << ':'
	   << std::setfill('0') << std::setw(8) << val.V.l_uf
	   << std::dec;
	// human-readable format
	os << '[' << lfptoa(&val.V, 10) << ']';
	return os;
}

std::ostream&
operator << (std::ostream& os, const timeStruct::timeval_wrap& val)
{
	// raw data formatting
	os << val.V.tv_sec << ':' << val.V.tv_usec;
	// human-readable format
	os << '['
	   << format_time_fraction(val.V.tv_sec, val.V.tv_usec, 6)
	   << ']';
	return os;
}

std::ostream&
operator << (std::ostream& os, const timeStruct::timespec_wrap& val)
{
	// raw data formatting
	os << val.V.tv_sec << ':' << val.V.tv_nsec;
	// human-readable format
	os << '['
	   << format_time_fraction(val.V.tv_sec, val.V.tv_nsec, 9)
	   << ']';
	return os;
}

// Implementation of the l_fp closeness predicate

AssertFpClose::AssertFpClose(
	u_int32 hi,
	u_int32 lo
	)
{
	limit.l_ui = hi;
	limit.l_uf = lo;
}

::testing::AssertionResult
AssertFpClose::operator()(
	const char* m_expr,
	const char* n_expr,
	const l_fp & m,
	const l_fp & n
	)
{
	l_fp diff;

	if (L_ISGEQ(&m, &n)) {
		diff = m;
		L_SUB(&diff, &n);
	} else {
		diff = n;
		L_SUB(&diff, &m);
	}
	if (L_ISGEQ(&limit, &diff))
		return ::testing::AssertionSuccess();

	return ::testing::AssertionFailure()
	    << m_expr << " which is " << l_fp_wrap(m)
	    << "\nand\n"
	    << n_expr << " which is " << l_fp_wrap(n)
	    << "\nare not close; diff=" << l_fp_wrap(diff);
}

// Implementation of the timeval closeness predicate

AssertTimevalClose::AssertTimevalClose(
	time_t hi,
	int32  lo
	)
{
	limit.tv_sec = hi;
	limit.tv_usec = lo;
}

::testing::AssertionResult
AssertTimevalClose::operator()(
	const char* m_expr,
	const char* n_expr,
	const struct timeval & m,
	const struct timeval & n
	)
{
	struct timeval diff;

	diff = abs_tval(sub_tval(m, n));
	if (cmp_tval(limit, diff) >= 0)
		return ::testing::AssertionSuccess();

	return ::testing::AssertionFailure()
	    << m_expr << " which is " << timeval_wrap(m)
	    << "\nand\n"
	    << n_expr << " which is " << timeval_wrap(n)
	    << "\nare not close; diff=" << timeval_wrap(diff);
}

// Implementation of the timespec closeness predicate

AssertTimespecClose::AssertTimespecClose(
	time_t hi,
	int32  lo
	)
{
	limit.tv_sec = hi;
	limit.tv_nsec = lo;
}

::testing::AssertionResult
AssertTimespecClose::operator()(
	const char* m_expr,
	const char* n_expr,
	const struct timespec & m,
	const struct timespec & n
	)
{
	struct timespec diff;

	diff = abs_tspec(sub_tspec(m, n));
	if (cmp_tspec(limit, diff) >= 0)
		return ::testing::AssertionSuccess();

	return ::testing::AssertionFailure()
	    << m_expr << " which is " << timespec_wrap(m)
	    << "\nand\n"
	    << n_expr << " which is " << timespec_wrap(n)
	    << "\nare not close; diff=" << timespec_wrap(diff);
}

} // namespace timeStruct
