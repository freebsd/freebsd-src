# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.2.2.1 2003/02/18 22:07:10 njl Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM}
LDADD=	-lcam

MAN=	scsi_target.8

.include <bsd.prog.mk>
