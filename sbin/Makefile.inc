
.include <src.opts.mk>

BINDIR?=	/sbin

.if ${MK_DYNAMICROOT} == "no"
NO_SHARED?=	YES
.endif
