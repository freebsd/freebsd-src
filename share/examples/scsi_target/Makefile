# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.30.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
