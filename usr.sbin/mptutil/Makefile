# $FreeBSD: src/usr.sbin/mptutil/Makefile,v 1.1.4.2.2.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	mptutil
SRCS=	mptutil.c mpt_cam.c mpt_cmd.c mpt_config.c mpt_drive.c mpt_evt.c \
	mpt_show.c mpt_volume.c
#	mpt_flash.c
MAN=	mptutil.8

WARNS?= 3

DPADD+=	${LIBCAM} ${LIBUTIL}
LDADD+=	-lcam -lutil

# Here be dragons
.ifdef DEBUG
CFLAGS+= -DDEBUG
.endif

.include <bsd.prog.mk>
