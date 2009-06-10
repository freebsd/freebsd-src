# $FreeBSD$
.include <bsd.own.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		kgzldr libi386 libfirewire loader

.if ${MK_ZFS} != "no"
SUBDIR+=		zfsboot gptzfsboot
.endif

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.include <bsd.subdir.mk>
