
.include <src.opts.mk>

BINDIR?=	/bin

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
