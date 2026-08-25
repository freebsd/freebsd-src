.include <src.opts.mk>

PACKAGE=	tests

ATF_TESTS_C=	cap_main_test

.if ${MK_CASPER} != "no"
LIBADD+=	casper
CFLAGS+=	-DWITH_CASPER
.endif
LIBADD+=	nv

.include <bsd.test.mk>
