.include <src.opts.mk>

PROG=	cut

.if ${MK_CASPER} != "no" && !defined(BOOTSTRAPPING)
LIBADD+=	casper
LIBADD+=	cap_fileargs
CFLAGS+=	-DWITH_CASPER
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+= tests

.include <bsd.prog.mk>
