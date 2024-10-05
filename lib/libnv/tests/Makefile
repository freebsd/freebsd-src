.include <src.opts.mk>

ATF_TESTS_C= \
	nvlist_send_recv_test

.PATH: ${SRCTOP}/lib/libnv
SRCS.nvlist_send_recv_test= msgio.c nvlist_send_recv_test.c
CFLAGS.nvlist_send_recv_test+=-I${SRCTOP}/sys/contrib/libnv
CFLAGS.nvlist_send_recv_test+=-I${SRCTOP}/lib/libnv
.if ${MK_ASAN} != "yes"
CFLAGS.nvlist_send_recv_test+=-DNO_ASAN
.endif

ATF_TESTS_CXX=	\
	cnv_tests \
	dnv_tests \
	nv_array_tests \
	nv_tests

TAP_TESTS_C+=	nvlist_add_test
TAP_TESTS_C+=	nvlist_append_test
TAP_TESTS_C+=	nvlist_exists_test
TAP_TESTS_C+=	nvlist_free_test
TAP_TESTS_C+=	nvlist_get_test
TAP_TESTS_C+=	nvlist_move_test

LIBADD+=	nv

WARNS?=		3

.include <bsd.test.mk>
