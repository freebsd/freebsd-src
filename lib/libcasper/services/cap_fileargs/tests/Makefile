.include <src.opts.mk>

ATF_TESTS_C=	fileargs_test

CFLAGS+=	-I${SRCTOP}/tests

.if ${MK_CASPER} != "no"
LIBADD+=	casper
LIBADD+=	cap_fileargs
CFLAGS+=-DWITH_CASPER
.endif
LIBADD+=	nv

WARNS?=		3

.include <bsd.test.mk>
