# $FreeBSD$

.if ${MACHINE_ARCH} == "sparc64"
SUBDIR  = audiocs es137x
.else
SUBDIR  = ad1816 ak452x als4000 atiixp cmi cs4281 csa ds1 emu10k1 envy24 es137x
SUBDIR += ess fm801 ich maestro maestro3 mss neomagic sb16 sb8 sbc solo
SUBDIR += t4dwave via8233 via82c686 vibes
SUBDIR += driver uaudio
.endif

.include <bsd.subdir.mk>
