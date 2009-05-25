# $FreeBSD$

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		kgzldr libi386 libfirewire loader

.if defined(LOADER_ZFS_SUPPORT)
SUBDIR+=		zfsboot gptzfsboot
.endif

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.include <bsd.subdir.mk>
