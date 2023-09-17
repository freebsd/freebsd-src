#	@(#)Makefile.inc	8.1 (Berkeley) 6/8/93

.include <src.opts.mk>

BINDIR?=	/sbin

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
