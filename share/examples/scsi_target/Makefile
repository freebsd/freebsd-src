# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.5 2004/02/04 10:15:26 ru Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM} ${LIBSBUF}
LDADD=	-lcam -lsbuf

MAN=	scsi_target.8

.include <bsd.prog.mk>
