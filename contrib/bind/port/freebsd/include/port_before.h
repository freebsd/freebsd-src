#define WANT_IRS_NIS
#define WANT_IRS_PW
#define WANT_IRS_GR
#define SIG_FN void
#define ts_sec tv_sec
#define ts_nsec tv_nsec

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#if defined (__FreeBSD__) && __FreeBSD__>=3
#define SETPWENT_VOID
#endif
