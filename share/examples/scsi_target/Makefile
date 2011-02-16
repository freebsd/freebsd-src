# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.30.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
