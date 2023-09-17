
SYSDIR?=${SRCTOP}/sys
.include "${SYSDIR}/conf/kern.opts.mk"

# Modules that include binary-only blobs of microcode should be selectable by
# MK_SOURCELESS_UCODE option (see below).

SUBDIR=		als4000 atiixp cs4281 ${_csa} emu10k1 emu10kx
SUBDIR+=	envy24 envy24ht es137x fm801 hda hdspe ich
SUBDIR+=	${_maestro3} neomagic solo spicds t4dwave via8233
SUBDIR+=	via82c686 vibes driver uaudio

.if ${MK_SOURCELESS_UCODE} != "no"
_csa=		csa
_maestro3=	maestro3
.endif

.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
SUBDIR+=	cmi
.endif

.if ${MACHINE_CPUARCH} == "powerpc"
SUBDIR+=	ai2s davbus
.endif

.include <bsd.subdir.mk>
