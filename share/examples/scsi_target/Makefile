# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5.30.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
