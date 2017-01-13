#	$NetBSD: Makefile,v 1.1 2016/01/08 17:27:48 jakllsch Exp $
#

PROG= t_hid
NOMAN=

.PATH: ${.CURDIR}/..

CPPFLAGS.t_hid.c= -I${.CURDIR}/../../../../sys/dev/usb

.include <bsd.own.mk>

BINDIR= ${TESTSBASE}/dev/usb

LIBHIDDIR!= cd ${.CURDIR}/../libhid && ${PRINTOBJDIR}
LDFLAGS+= -L${LIBHIDDIR}
LDADD+= -Wl,--whole-archive -lrumpdev_hid -Wl,--no-whole-archive
DPADD+= ${LIBHIDDIR}/librumpdev_hid.a
DPADD+= ${ATF_C}

LDADD+= -latf-c
LDADD+= -lrump
LDADD+= -lrumpuser
LDADD+= -lpthread

.include <bsd.prog.mk>
