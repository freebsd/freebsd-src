#	$FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.3.2.1 2000/11/27 02:16:39 obrien Exp $

BINDIR=		/boot
BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
STRIP=

${BASE}: ${BASE}-4.0R.gz.uu
	uudecode -p ${.CURDIR}/${BASE}-4.0R.gz.uu | gunzip > ${BASE}

.include <bsd.prog.mk>
