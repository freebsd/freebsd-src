# $FreeBSD: src/sys/boot/i386/Makefile,v 1.26.2.3.2.1 2010/06/14 02:09:06 kensmith Exp $

.include <bsd.own.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot kgzldr \
		libi386 libfirewire loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.if ${MK_ZFS} != "no"
SUBDIR+=	zfsboot gptzfsboot zfsloader
.endif

.include <bsd.subdir.mk>
