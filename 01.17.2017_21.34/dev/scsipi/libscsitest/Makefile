#	$NetBSD: Makefile,v 1.3 2014/04/26 18:53:21 christos Exp $
#

.include <bsd.own.mk>

RUMPTOP= ${NETBSDSRCDIR}/sys/rump

LIB=	rumpdev_scsitest
IOCONF=	SCSITEST.ioconf
LIBISPRIVATE= #defined

SRCS=	scsitest.c
SRCS+=	scsitest_component.c

CPPFLAGS+= -I${.CURDIR} -I${RUMPTOP}/librump/rumpkern

.include "${RUMPTOP}/Makefile.rump"
.include <bsd.lib.mk>
.include <bsd.klinks.mk>
