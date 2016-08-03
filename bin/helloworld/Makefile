# $FreeBSD$

.include <src.opts.mk>

PROG=	helloworld

MAN=
NEED_CHERI=	pure
WANT_DUMP=	yes

NO_WERROR=	YES

.if ${MK_CHERI_SHARED} == "yes"
# Always build shared if we're building shared libs.
MK_CHERI_SHARED_PROG=yes
.endif

.include <bsd.prog.mk>
