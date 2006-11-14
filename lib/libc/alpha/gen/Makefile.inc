# $FreeBSD$

SRCS+=	_setjmp.S _set_tp.c fabs.S infinity.c ldexp.c modf.c setjmp.S
SRCS+=	flt_rounds.c fpgetmask.c fpgetround.c fpgetsticky.c fpsetmask.c \
	fpsetround.c

SRCS+=	sigsetjmp.S
SRCS+=	__divqu.S __divq.S __divlu.S __divl.S
SRCS+=	__remqu.S __remq.S __remlu.S __reml.S
SRCS+=	rfork_thread.S
SRCS+=	_ctx_start.S makecontext.c signalcontext.c

CLEANFILES+=	__divqu.S __divq.S __divlu.S __divl.S
CLEANFILES+=	__remqu.S __remq.S __remlu.S __reml.S


__divqu.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__divqu -DOP=div -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divq.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__divq -DOP=div -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__divlu.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__divlu -DOP=div -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__divl.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__divl -DOP=div -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__remqu.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__remqu -DOP=rem -DS=false -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remq.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__remq -DOP=rem -DS=true -DWORDSIZE=64 \
		${.ALLSRC} > ${.TARGET}

__remlu.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__remlu -DOP=rem -DS=false -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}

__reml.S: ${.CURDIR}/alpha/gen/divrem.m4
	m4 -DNAME=__reml -DOP=rem -DS=true -DWORDSIZE=32 \
		${.ALLSRC} > ${.TARGET}
