.include <src.opts.mk>

PACKAGE=runtime
CONFS=	sysctl.conf
PROG=	sysctl
WARNS?=	3
MAN=	sysctl.8

.if ${MK_JAIL} != "no" && !defined(RESCUE)
CFLAGS+=	-DJAIL
LIBADD+=	jail
.endif

HAS_TESTS=
SUBDIR.${MK_TESTS}+=	tests

.include <bsd.prog.mk>
