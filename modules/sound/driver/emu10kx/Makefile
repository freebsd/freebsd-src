# $FreeBSD$
.PATH: ${.CURDIR}/../../../../dev/sound/pci \
       ${.CURDIR}/../../../../gnu/dev/sound/pci

KMOD=	snd_emu10kx

SRCS=	device_if.h bus_if.h pci_if.h
SRCS+=	isa_if.h channel_if.h ac97_if.h mixer_if.h mpufoi_if.h
SRCS+=	vnode_if.h
# Master, PCM and MIDI devices
SRCS+=	emu10kx.c
SRCS+=	emu10kx-pcm.c
SRCS+=	emu10kx-midi.c
# de-GPLed Makefiles
SRCS+=	emu10k1-alsa%diked.h
SRCS+=	p16v-alsa%diked.h
SRCS+=	p17v-alsa%diked.h

emu10k1-alsa%diked.h: emu10k1-alsa.h
	grep -v '#include' ${.OODATE} | $(CC) -E -D__KERNEL__ -dM - \
	    | awk -F"[ 	(]" '/define/ \
	    { print "#ifndef " $$2 ; print ; print "#endif" }' \
	    >${.TARGET}
p16v-alsa%diked.h: p16v-alsa.h
	grep -v '#include' ${.OODATE} | $(CC) -E -D__KERNEL__ -dM - \
	    | awk -F"[ 	(]" '/define/ \
	    { print "#ifndef " $$2 ; print ; print "#endif" }' \
	    >${.TARGET}
p17v-alsa%diked.h: p17v-alsa.h
	grep -v '#include' ${.OODATE} | $(CC) -E -D__KERNEL__ -dM - \
	    | awk -F"[ 	(]" '/define/ \
	    { print "#ifndef " $$2 ; print ; print "#endif" }' \
	    >${.TARGET}

CLEANFILES+=	emu10k1-alsa%diked.h
CLEANFILES+=	p16v-alsa%diked.h
CLEANFILES+=	p17v-alsa%diked.h

.include <bsd.kmod.mk>
