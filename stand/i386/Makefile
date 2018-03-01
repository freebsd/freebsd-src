# $FreeBSD$

NO_OBJ=t

.include <bsd.init.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		libi386

SUBDIR.${MK_LOADER_FIREWIRE}+=	libfirewire

SUBDIR+=	loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	kgzldr
.endif

SUBDIR.${MK_ZFS}+=	zfsboot gptzfsboot zfsloader

.include <bsd.subdir.mk>
