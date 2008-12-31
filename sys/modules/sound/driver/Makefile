# $FreeBSD: src/sys/modules/sound/driver/Makefile,v 1.24.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $

.if ${MACHINE_ARCH} == "sparc64"
.if ${MACHINE} == "sparc64"
audiocs=	audiocs
.endif
SUBDIR=	${audiocs} es137x
.else
SUBDIR=		ad1816 als4000 atiixp cmi cs4281 csa ds1 emu10k1 emu10kx
SUBDIR+=	envy24 envy24ht es137x ess fm801 hda ich maestro maestro3
SUBDIR+=	mss neomagic sb16 sb8 sbc solo spicds t4dwave via8233
SUBDIR+=	via82c686 vibes driver uaudio
.endif

.include <bsd.subdir.mk>
