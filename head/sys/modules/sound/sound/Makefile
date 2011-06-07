# $FreeBSD$

.PATH: ${.CURDIR}/../../../dev/sound
.PATH: ${.CURDIR}/../../../dev/sound/pcm
.PATH: ${.CURDIR}/../../../dev/sound/midi
.PATH: ${.CURDIR}/../../../dev/sound/isa

KMOD=	sound
SRCS=	device_if.h bus_if.h isa_if.h pci_if.h opt_isa.h
SRCS+=	ac97_if.h channel_if.h feeder_if.h mixer_if.h
SRCS+=	ac97_if.c channel_if.c feeder_if.c mixer_if.c
SRCS+=	feeder.c feeder_rate.c feeder_volume.c
SRCS+=	feeder_chain.c feeder_eq.c feeder_format.c
SRCS+=	feeder_matrix.c feeder_mixer.c
SRCS+=	feeder_eq_gen.h feeder_rate_gen.h snd_fxdiv_gen.h
SRCS+=	mpu_if.h mpufoi_if.h synth_if.h
SRCS+=	mpu_if.c mpufoi_if.c synth_if.c
SRCS+=	ac97.c ac97_patch.c buffer.c channel.c clone.c dsp.c
SRCS+=	mixer.c sndstat.c sound.c unit.c vchan.c
SRCS+=	midi.c mpu401.c sequencer.c

.if !exists(@)
feeder_eq_gen.h:	@
.else
feeder_eq_gen.h:	@/tools/sound/feeder_eq_mkfilter.awk
.endif
	${AWK} -f @/tools/sound/feeder_eq_mkfilter.awk -- ${FEEDER_EQ_PRESETS} > ${.TARGET}

.if !exists(@)
feeder_rate_gen.h:	@
.else
feeder_rate_gen.h:	@/tools/sound/feeder_rate_mkfilter.awk
.endif
	${AWK} -f @/tools/sound/feeder_rate_mkfilter.awk -- ${FEEDER_RATE_PRESETS} > ${.TARGET}

.if !exists(@)
snd_fxdiv_gen.h:	@
.else
snd_fxdiv_gen.h:	@/tools/sound/snd_fxdiv_gen.awk
.endif
	${AWK} -f @/tools/sound/snd_fxdiv_gen.awk -- > ${.TARGET}

CLEANFILES+=	feeder_eq_gen.h feeder_rate_gen.h snd_fxdiv_gen.h

EXPORT_SYMS=	YES	# XXX evaluate

.if ${MACHINE_CPUARCH} == "sparc64" || ${MACHINE_CPUARCH} == "powerpc"
# Create an empty opt_isa.h in order to keep kmod.mk from linking in an
# existing one from KERNBUILDDIR which possibly has DEV_ISA defined so
# sound.ko is always built without isadma support.
opt_isa.h:
	:> ${.TARGET}
.else
.if !defined(KERNBUILDDIR)
SRCS+=	sndbuf_dma.c

opt_isa.h:
	echo "#define DEV_ISA 1" > ${.TARGET}
.else
DEV_ISA!= sed -n '/DEV_ISA/p' ${KERNBUILDDIR}/opt_isa.h
.if !empty(DEV_ISA)
SRCS+=	sndbuf_dma.c
.endif
.endif
.endif

.include <bsd.kmod.mk>
