# $FreeBSD: src/share/examples/scsi_target/Makefile,v 1.4 2002/11/25 19:29:05 njl Exp $

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM}
LDADD=	-lcam

MAN=	scsi_target.8

.include <bsd.prog.mk>
