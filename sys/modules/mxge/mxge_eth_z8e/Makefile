# $FreeBSD$

MXGE= ${.CURDIR}/../../../dev/mxge
.PATH= ${MXGE}
KMOD=	mxge_eth_z8e
FIRMWS=	eth_z8e.dat:mxge_eth_z8e
CLEANFILES+= eth_z8e.dat

eth_z8e.dat: ${MXGE}/eth_z8e.dat.gz.uu
	uudecode -p < ${MXGE}/eth_z8e.dat.gz.uu \
		| gzip -dc > ${.TARGET}

.include <bsd.kmod.mk>
