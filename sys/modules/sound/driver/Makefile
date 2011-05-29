# $FreeBSD$

SUBDIR=		ad1816 als4000 atiixp cs4281 csa ds1 emu10k1 emu10kx
SUBDIR+=	envy24 envy24ht es137x ess fm801 hda ich maestro maestro3
SUBDIR+=	neomagic sb16 sb8 sbc solo spicds t4dwave via8233
SUBDIR+=	via82c686 vibes driver uaudio

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	cmi mss
.endif

.if ${MACHINE_CPUARCH} == "sparc64"
.if ${MACHINE} == "sparc64"
SUBDIR+=	audiocs
.endif
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
SUBDIR+=	ai2s davbus
.endif

.include <bsd.subdir.mk>
