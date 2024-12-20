.include <src.opts.mk>

PROG=	wc
LIBADD=	xo

.if ${MK_CASPER} != "no"
LIBADD+=        casper
LIBADD+=        cap_fileargs
CFLAGS+=-DWITH_CASPER
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}=	tests

.include <bsd.prog.mk>
