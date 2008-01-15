# $FreeBSD: src/sys/modules/sound/driver/Makefile,v 1.16.2.2 2007/05/13 21:11:40 ariff Exp $

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR  = audiocs es137x
.else
SUBDIR  = als4000 ad1816 atiixp cmi cs4281 csa ds1 emu10k1 envy24
SUBDIR += envy24ht es137x ess fm801 hda ich maestro maestro3 mss neomagic
SUBDIR += sb16 sb8 sbc solo spicds t4dwave via8233 via82c686 vibes
SUBDIR += driver uaudio
.endif

.include <bsd.subdir.mk>
