# Location of librcs

.if exists(${.CURDIR}/../lib/obj)
LIBRCS=	${.CURDIR}/../lib/obj/librcs.a
.else
LIBRCS=	${.CURDIR}/../lib/librcs.a
.endif
