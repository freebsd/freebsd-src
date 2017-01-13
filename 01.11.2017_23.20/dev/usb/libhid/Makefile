#	$NetBSD: Makefile,v 1.1 2016/01/05 17:22:39 jakllsch Exp $
#

.include <bsd.own.mk>

RUMPTOP= ${NETBSDSRCDIR}/sys/rump
.PATH:	${.CURDIR}/../../../../sys/dev/usb

LIB=	rumpdev_hid
LIBISPRIVATE= #defined

SRCS=	hid.c

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
