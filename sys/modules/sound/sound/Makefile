# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/sound/pcm
.PATH: ${.CURDIR}/../../../dev/sound/isa

KMOD=	sound
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h opt_isa.h
SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h
SRCS+=	ac97_if.c channel_if.c feeder_if.c mixer_if.c
SRCS+=	ac97.c ac97_patch.c buffer.c channel.c dsp.c
SRCS+=	fake.c feeder.c feeder_fmt.c feeder_rate.c feeder_volume.c
SRCS+=	mixer.c sndstat.c sound.c vchan.c

EXPORT_SYMS=	YES	# XXX evaluate

.if ${MACHINE_ARCH} == "sparc64"
# Create an empty opt_isa.h in order to keep kmod.mk from linking in an
# existing one from KERNBUILDDIR which possibly has DEV_ISA defined so
# sound.ko is always built without isadma support.
opt_isa.h:
	touch ${.TARGET}
.else
SRCS+=	sndbuf_dma.c

opt_isa.h:
	echo "#define DEV_ISA 1" > ${.TARGET}
.endif

.include <bsd.kmod.mk>
