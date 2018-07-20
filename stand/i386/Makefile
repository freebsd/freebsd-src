# $FreeBSD$

NO_OBJ=t

.include <bsd.init.mk>

SUBDIR.yes=	mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		isoboot libi386

SUBDIR.${MK_LOADER_FIREWIRE}+=	libfirewire

SUBDIR.yes+=	loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR.yes+=	pxeldr

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR.yes+=	kgzldr
.endif

SUBDIR.${MK_ZFS}+=	zfsboot gptzfsboot

.include <bsd.subdir.mk>
