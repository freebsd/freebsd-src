/* $FreeBSD$ */
#define WANT_IRS_NIS
#define WANT_IRS_PW
#define HAVE_PW_CLASS
#define WANT_IRS_GR
#define SIG_FN void
#define HAS_PTHREADS
#define ts_sec tv_sec
#define ts_nsec tv_nsec

#if defined(HAS_PTHREADS) && defined(_REENTRANT)
#define DO_PTHREADS
#endif

#if defined (__FreeBSD__) && __FreeBSD__>=3
#define SETPWENT_VOID
#endif

#include <sys/param.h>
#include <sys/types.h>

#define GROUP_R_RETURN struct group *
#define GROUP_R_SET_RETURN void
#undef GROUP_R_SET_RESULT /*empty*/
#define GROUP_R_END_RETURN void
#define GROUP_R_END_RESULT(x) /*empty*/
#define GROUP_R_ARGS char *buf, int buflen
#define GROUP_R_ENT_ARGS void
#define GROUP_R_OK gptr
#define GROUP_R_BAD NULL
#if defined(__FreeBSD_version) && __FreeBSD_version >= 500030
#define GETGROUPLIST_ARGS const char *name, gid_t basegid, gid_t *groups, \
			  int *ngroups
#else
#define GETGROUPLIST_ARGS const char *name, int basegid, int *groups, \
			  int *ngroups
#endif

#define HOST_R_RETURN struct hostent *
#define HOST_R_SET_RETURN void
#undef HOST_R_SET_RESULT /*empty*/
#define HOST_R_END_RETURN void
#define HOST_R_END_RESULT(x) /*empty*/
#define HOST_R_ARGS char *buf, int buflen, int *h_errnop
#undef HOST_R_ENT_ARGS /*empty*/
#define HOST_R_COPY buf, buflen
#define HOST_R_COPY_ARGS char *buf, int buflen
#define HOST_R_ERRNO *h_errnop = h_errno
#define HOST_R_OK hptr
#define HOST_R_BAD NULL

#define NET_R_RETURN struct netent *
#define NET_R_SET_RETURN void
#undef NET_R_SET_RESULT /*empty*/
#define NET_R_END_RETURN void
#define NET_R_END_RESULT(x) /*empty*/
#define NET_R_ARGS char *buf, int buflen
#undef NET_R_ENT_ARGS /*empty*/
#define NET_R_COPY buf, buflen
#define NET_R_COPY_ARGS NET_R_ARGS
#define NET_R_OK nptr
#define NET_R_BAD NULL

#define NGR_R_RETURN int
#define NGR_R_SET_RETURN void
#undef NGR_R_SET_RESULT /*empty*/
#define NGR_R_END_RETURN void
#define NGR_R_END_RESULT(x) return
#define NGR_R_ARGS char *buf, int buflen
#undef NGR_R_ENT_ARGS /*empty*/
#define NGR_R_COPY buf, buflen
#define NGR_R_COPY_ARGS NGR_R_ARGS
#define NGR_R_OK 1
#define NGR_R_BAD (0)

#define PROTO_R_RETURN struct protoent *
#define PROTO_R_SET_RETURN void
#undef PROTO_R_SET_RESULT /*empty*/
#define PROTO_R_END_RETURN void
#define PROTO_R_END_RESULT(x) /*empty*/
#define PROTO_R_ARGS char *buf, int buflen
#undef PROTO_R_ENT_ARGS /*empty*/
#define PROTO_R_COPY buf, buflen
#define PROTO_R_COPY_ARGS PROTO_R_ARGS
#define PROTO_R_OK pptr
#define PROTO_R_BAD NULL

#define PASS_R_RETURN struct passwd *
#define PASS_R_SET_RETURN void
#undef PASS_R_SET_RESULT /*empty*/
#define PASS_R_END_RETURN void
#define PASS_R_END_RESULT(x) /*empty*/
#define PASS_R_ARGS char *buf, int buflen
#undef PASS_R_ENT_ARGS /*empty*/
#define PASS_R_COPY buf, buflen
#define PASS_R_COPY_ARGS PASS_R_ARGS
#define PASS_R_OK pwptr
#define PASS_R_BAD NULL

#define SERV_R_RETURN struct servent *
#define SERV_R_SET_RETURN void
#undef SERV_R_SET_RESULT /*empty*/
#define SERV_R_END_RETURN void
#define SERV_R_END_RESULT(x) /*empty*/
#define SERV_R_ARGS char *buf, int buflen
#undef SERV_R_ENT_ARGS /*empty*/
#define SERV_R_COPY buf, buflen
#define SERV_R_COPY_ARGS SERV_R_ARGS
#define SERV_R_OK sptr
#define SERV_R_BAD NULL

#define IRS_LCL_SV_DB
#define ISC_SOCKLEN_T int
#ifdef __GNUC__
#define ISC_FORMAT_PRINTF(fmt, args) \
	__attribute__((__format__(__printf__, fmt, args)))
#else
#define ISC_FORMAT_PRINTF(fmt, args)
#endif
