# $FreeBSD$

.include <src.opts.mk>

NO_OBJ=
INCS=	dumprestore.h routed.h rwhod.h timed.h
INCSDIR=${INCLUDEDIR}/protocols

.if ${MK_TALK} == "no"
INCS+=	talkd.h
.endif

.include <bsd.prog.mk>
