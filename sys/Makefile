#	$Id: Makefile,v 1.5 1996/06/14 10:56:36 asami Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.else
SUBDIR=	${MACHINE}/boot
.endif

.include <bsd.subdir.mk>
