#	@(#)Makefile.inc	8.1 (Berkeley) 6/4/93
# $FreeBSD$

SRCS+=	\
	__aeabi_read_tp.S \
	_ctx_start.S \
	_set_tp.c \
	_setjmp.S \
	alloca.S \
	arm_initfini.c \
	fabs.c \
	flt_rounds.c \
	getcontextx.c \
	infinity.c \
	ldexp.c \
	makecontext.c \
	setjmp.S \
	signalcontext.c \
	sigsetjmp.S \

.if ${MACHINE_ARCH:Marmv[67]*} && (!defined(CPUTYPE) || ${CPUTYPE:M*soft*} == "")

SRCS+=	\
	fpgetmask_vfp.c \
	fpgetround_vfp.c \
	fpgetsticky_vfp.c \
	fpsetmask_vfp.c \
	fpsetround_vfp.c \
	fpsetsticky_vfp.c \

.endif
