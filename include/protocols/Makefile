# $FreeBSD$

.include <src.opts.mk>

NO_OBJ=
INCS=	dumprestore.h timed.h
INCSDIR=${INCLUDEDIR}/protocols

.if ${MK_RCMDS} != "no"
INCS+=	rwhod.h
.endif

.if ${MK_ROUTED} != "no"
INCS+=	routed.h
.endif

.if ${MK_TALK} != "no"
INCS+=	talkd.h
.endif

.include <bsd.prog.mk>
