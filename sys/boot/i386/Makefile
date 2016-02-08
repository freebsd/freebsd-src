# $FreeBSD$

.include <src.opts.mk>

SUBDIR=		mbr pmbr boot0 boot0sio btx boot2 cdboot gptboot \
		libi386 libfirewire loader

# special boot programs, 'self-extracting boot2+loader'
SUBDIR+=	pxeldr

.if ${MACHINE_CPUARCH} == "i386"
SUBDIR+=	kgzldr
.endif

.if ${MK_ZFS} != "no"
SUBDIR+=	zfsboot gptzfsboot zfsloader
.endif

.include <bsd.subdir.mk>
