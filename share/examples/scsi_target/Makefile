# $FreeBSD$

PROG=	scsi_target
SRCS=	scsi_target.h scsi_target.c scsi_cmds.c
DPADD=	${LIBCAM}
LDADD=	-lcam

MAN=	scsi_target.8

.include <bsd.prog.mk>
