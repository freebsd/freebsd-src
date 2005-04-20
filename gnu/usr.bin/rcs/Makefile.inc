# Location of librcs

.if exists(${.OBJDIR}/../lib)
LIBRCS=	${.OBJDIR}/../lib/librcs.a
.else
LIBRCS=	${.CURDIR}/../lib/librcs.a
.endif
