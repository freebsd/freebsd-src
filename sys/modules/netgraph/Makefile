# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD$

SUBDIR=	UI \
	async \
	atm \
	${_bluetooth} \
	bpf \
	bridge \
	cisco \
	echo \
	eiface \
	etf \
	ether \
	fec \
	frame_relay \
	gif \
	gif_demux \
	hole \
	hub \
	iface \
	ip_input \
	ksocket \
	l2tp \
	lmi \
	${_mppc} \
	netflow \
	netgraph \
	one2many \
	ppp \
	pppoe \
	pptpgre \
	rfc1490 \
	socket \
	split \
	sppp \
	sync_ar \
	sync_sr \
	tee \
	tty \
	vjc \
	vlan

.if !defined(NO_BLUETOOTH) || defined(ALL_MODULES)
_bluetooth=	bluetooth
.endif

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
_mppc=		mppc
.endif

.include <bsd.subdir.mk>
