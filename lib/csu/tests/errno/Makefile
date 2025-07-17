PLAIN_TESTS_C=	errno_test		\
		errno_static_test	\
		errno_thr_test		\
		errno_thr_static_test

SRCS.errno_static_test= errno_test.c
LDFLAGS.errno_static_test= -static

SRCS.errno_thr_test= errno_test.c
LIBADD.errno_thr_test= pthread

SRCS.errno_thr_static_test= errno_test.c
LDFLAGS.errno_thr_static_test= -static
LIBADD.errno_thr_static_test= pthread

MK_PIE:= no

.include <bsd.test.mk>
