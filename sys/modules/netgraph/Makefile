# $Whistle: Makefile,v 1.5 1999/01/24 06:48:37 archie Exp $
# $FreeBSD: src/sys/modules/netgraph/Makefile,v 1.8.2.3 2000/09/13 04:23:38 archie Exp $

SUBDIR=	async bpf bridge cisco echo ether frame_relay hole iface ksocket lmi \
	netgraph ppp pppoe pptpgre rfc1490 socket tee tty UI vjc

.if !defined(NOCRYPT) && exists(${.CURDIR}/../../crypto/rc4/rc4.c)
SUBDIR+=	mppc
.endif

.include <bsd.subdir.mk>
