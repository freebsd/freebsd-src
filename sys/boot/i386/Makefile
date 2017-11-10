# $FreeBSD$

.include <bsd.init.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		libi386

.if ${MK_LOADER_FIREWIRE} == "yes"
SUBDIR+=	libfirewire
.endif

SUBDIR+=	loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	kgzldr
.endif

.if ${MK_ZFS} != "no"
SUBDIR+=	zfsboot gptzfsboot zfsloader
.endif

.include <bsd.subdir.mk>
