#	$Id: Makefile,v 1.4 1995/12/26 13:57:32 bde Exp $

.if defined(MACHINE_PC98) && ${MACHINE_PC98} == "yes"
SUBDIR=	pc98/boot
.else
SUBDIR=	${MACHINE}/boot
.endif

.include <bsd.subdir.mk>
