# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.24.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
