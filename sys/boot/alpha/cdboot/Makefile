#	$FreeBSD$

BINDIR=		/boot
BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
STRIP=

${BASE}: ${BASE}-4.0R.gz.uu
	uudecode -p ${.CURDIR}/${BASE}-4.0R.gz.uu | gunzip > ${BASE}

.include <bsd.prog.mk>
