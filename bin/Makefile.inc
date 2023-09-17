#	@(#)Makefile.inc	8.1 (Berkeley) 5/31/93

.include <src.opts.mk>

BINDIR?=	/bin

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
