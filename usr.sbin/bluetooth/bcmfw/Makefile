# $Id: Makefile,v 1.2 2003/04/27 00:44:54 max Exp $
# $FreeBSD$

DESTDIR=	/usr/sbin/
MANDIR=		../share/man/man
PROG=		bcmfw
MAN8=		bcmfw.8
WARNS?=		2
CFLAGS+=	-g -I${.CURDIR}/../../../sys/netgraph/bluetooth/include
SRCS=		bcmfw.c 

#FILESDIR=	../local/etc/
#FILES+=		BCM2033-FW.bin BCM2033-MD.hex
#CLEANFILES+=	BCM2033-FW.bin BCM2033-MD.hex
#
#BCM2033-FW.bin: BCM2033-FW.bin.uue
#	uudecode BCM2033-FW.bin.uue
#
#BCM2033-MD.hex: BCM2033-MD.hex.uue
#	uudecode BCM2033-MD.hex.uue
#
.include <bsd.prog.mk>
#.include <bsd.files.mk>
