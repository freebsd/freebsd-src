#	$Id$

.if exists(${.OBJDIR}/../../include)
INCLUDEOBJDIR=	${.OBJDIR}/../include
.else
INCLUDEOBJDIR=	${.CURDIR}/../include
.endif

depend lint tags:

.include <bsd.prog.mk>

#${INCLUDEOBJDIR}/protos.h: ${KRB4DIR}/include/protos.H
#	cd ${INCLUDEOBJDIR} ; ln -sf ${.ALLSRC} ${.TARGET} 

#all:	${INCLUDEOBJDIR}/protos.h

#CLEANFILES= ${INCLUDEOBJDIR}/protos.h
