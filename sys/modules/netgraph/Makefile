# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD$

SUBDIR=	UI \
	async \
	bpf \
	bridge \
	cisco \
	echo \
	eiface \
	etf \
	ether \
	frame_relay \
	gif \
	gif_demux \
	hole \
	iface \
	ip_input \
	ksocket \
	l2tp \
	lmi \
	netgraph \
	one2many \
	ppp \
	pppoe \
	pptpgre \
	rfc1490 \
	socket \
	split \
	sync_ar \
	sync_sr \
	tee \
	tty \
	vjc

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
SUBDIR+=	mppc
.endif

.if ${MACHINE_ARCH} == "i386"
# Things that don't compile on alpha or are aout specific:
SUBDIR+= bluetooth
.endif

.include <bsd.subdir.mk>
