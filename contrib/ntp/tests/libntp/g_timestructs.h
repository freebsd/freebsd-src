/*
 * timestructs.h -- test bed adaptors for time structs.
 *
 * Written by Juergen Perlinger (perlinger@ntp.org) for the NTP project.
 * The contents of 'html/copyright.html' apply.
 *
 * Some wrapper classes and a closeness predicate that are used to
 * bridge the gap between the goggletest framework and the structs used
 * for representing time stamps (l_fp, struct timeval, struct timespec).
 *
 * Some ostream conversion operators are provided to give diagnostic
 * output on errors. The normal string conversion functions will give
 * HRVs (human readable values) but we might also be interested in the
 * machine representation for diagnostic purposes.
 */
#ifndef TIMESTRUCTS_H
#define TIMESTRUCTS_H

extern "C" {
#include "ntp_fp.h"
}

namespace timeStruct {

// wrap a l_fp struct with common operations
class l_fp_wrap {
  public:
	l_fp V;
	
	l_fp_wrap()
		{ ZERO(V); }
	l_fp_wrap(u_int32 hi, u_int32 lo)
		{ V.l_ui = hi; V.l_uf = lo; }
	l_fp_wrap(const l_fp &rhs)
		{ V = rhs; }
	bool operator == (const l_fp_wrap& rhs) const
		{ return L_ISEQU(&V, &rhs.V); }
	operator l_fp* () 
		{ return &V; }
	operator l_fp& ()
		{ return V; }
	l_fp_wrap & operator = (const l_fp_wrap& rhs)
		{ V = rhs.V; return *this; }
	l_fp_wrap& operator = (const l_fp& rhs)
		{ V = rhs; return *this; }
	};
	
// wrap a 'struct timeval' with common operations
class timeval_wrap {
public:
	struct timeval V;

	timeval_wrap()
		{ ZERO(V); }
	timeval_wrap(time_t hi, long lo)
		{ V.tv_sec = hi; V.tv_usec = lo; }
	timeval_wrap(const struct timeval & rhs)
		{ V = rhs; }
	timeval_wrap(const timeval_wrap & rhs)
		{ V = rhs.V; }
	bool operator == (const timeval_wrap& rhs) const
		{ return V.tv_sec == rhs.V.tv_sec &&
			 V.tv_usec == rhs.V.tv_usec ; }
	bool valid() const
		{ return V.tv_usec >= 0 && V.tv_usec < 1000000; }
	operator struct timeval* () 
		{ return &V; }
	operator struct timeval& ()
		{ return V; }
	timeval_wrap& operator = (const timeval_wrap& rhs)
		{ V = rhs.V; return *this; }
	timeval_wrap& operator = (const struct timeval& rhs)
		{ V = rhs; return *this; }
};

// wrap a 'struct timespec' with common operations
class timespec_wrap {
public:
	struct timespec V;

	timespec_wrap()
		{ ZERO(V); }
	timespec_wrap(time_t hi, long lo)
		{ V.tv_sec = hi; V.tv_nsec = lo; }
	timespec_wrap(const struct timespec & rhs)
		{ V = rhs; }
	timespec_wrap(const timespec_wrap & rhs)
		{ V = rhs.V; }
	bool operator == (const timespec_wrap& rhs) const
		{ return V.tv_sec == rhs.V.tv_sec &&
			 V.tv_nsec == rhs.V.tv_nsec ; }
	bool valid() const
		{ return V.tv_nsec >= 0 && V.tv_nsec < 1000000000; }
	operator struct timespec* () 
		{ return &V; }
	operator struct timespec& ()
		{ return V;	}
	timespec_wrap& operator = (const timespec_wrap& rhs)
		{ V = rhs.V; return *this; }
	timespec_wrap& operator = (const struct timespec& rhs)
		{ V = rhs; return *this; }
};

// l_fp closeness testing predicate
//
// This predicate is used for the closeness ('near') testing of l_fp
// values. Once constructed with a limit, it can be used to check the
// absolute difference of two l_fp structs against that limit; if the
// difference is less or equal to this limit, the test passes.
class AssertFpClose {
private:
	l_fp limit;

public:
	AssertFpClose(u_int32 hi, u_int32 lo);

	::testing::AssertionResult
	operator()(const char* m_expr, const char* n_expr,
		   const l_fp & m, const l_fp & n);
};


// timeval closeness testing predicate
//
// CAVEAT: This class uses the timevalops functions
// - sub_tval
// - abs_tval
// - cmp_tval
//
// This creates a dependency loop of sorts. The loop is defused by the
// fact that these basic operations can be tested by exact value tests,
// so once the basic timeval operations passed it's safe to use this
// predicate.
class AssertTimevalClose {
private:
	struct timeval limit;

public:
	// note: (hi,lo) should be a positive normalised timeval;
	// the constructor does not normalise the values!
	AssertTimevalClose(time_t hi, int32 lo);

	::testing::AssertionResult
	operator()(const char* m_expr, const char* n_expr,
		   const struct timeval & m, const struct timeval & n);
};


// timespec closeness testing predicate
//
// CAVEAT: This class uses the timespecops functions
// - sub_tspec
// - abs_tspec
// - cmp_tspec
//
// See the equivalent timeval helper.
class AssertTimespecClose {
private:
	struct timespec limit;

public:
	// note: (hi,lo) should be a positive normalised timespec;
	// the constructor does not normalise the values!
	AssertTimespecClose(time_t hi, int32 lo);

	::testing::AssertionResult
	operator()(const char* m_expr, const char* n_expr,
		   const struct timespec & m, const struct timespec & n);
};


// since googletest wants to string format items, we declare the
// necessary operators. Since all adaptors have only public members
// there is need for friend declarations anywhere.

extern std::ostream& operator << (std::ostream& os,
				  const timeStruct::l_fp_wrap& val);
extern std::ostream& operator << (std::ostream& os,
				  const timeStruct::timeval_wrap& val);
extern std::ostream& operator << (std::ostream& os,
				  const timeStruct::timespec_wrap& val);

} // namespace timeStruct

#endif // TIMESTRUCTS_H
