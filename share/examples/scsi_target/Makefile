# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
