# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD$

SUBDIR=	async \
	atm \
	${_bluetooth} \
	bpf \
	bridge \
	cisco \
	device \
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
	ipfw \
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
	${_sync_ar} \
	${_sync_sr} \
	tee \
	tty \
	UI \
	vjc \
	vlan

.if ${MACHINE_ARCH} == "i386"
_sync_ar=	sync_ar
_sync_sr=	sync_sr
.endif

.if !defined(NO_BLUETOOTH) || defined(ALL_MODULES)
_bluetooth=	bluetooth
.endif

.if !defined(NO_CRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
_mppc=		mppc
.endif

.include <bsd.subdir.mk>
