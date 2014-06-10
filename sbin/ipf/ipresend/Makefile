#	$FreeBSD$

PROG=		ipresend
SRCS=		ipresend.c ip.c resend.c sbpf.c sock.c 44arp.c
MAN=		ipresend.1

NO_PIE=	yes

.PATH:		${.CURDIR}/../../../contrib/ipfilter/ipsend

.include <bsd.prog.mk>
