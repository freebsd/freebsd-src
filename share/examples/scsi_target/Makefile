# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
