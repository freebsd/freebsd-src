.include <src.opts.mk>

PACKAGE=	certctl
PROG=	certctl
MAN=	certctl.8
LIBADD=	crypto util
HAS_TESTS=
SUBDIR.${MK_TESTS}=	tests

.ifdef BOOTSTRAPPING
CFLAGS+=-DBOOTSTRAPPING
.endif

.include <bsd.prog.mk>
