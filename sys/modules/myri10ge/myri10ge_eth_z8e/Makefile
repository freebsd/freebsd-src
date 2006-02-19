# $FreeBSD$

MYRI10GE= ${.CURDIR}/../../../dev/myri10ge
.PATH= ${MYRI10GE}
KMOD=	myri10ge_eth_z8e
FIRMWS=	eth_z8e.dat:myri10ge_eth_z8e
CLEANFILES+= eth_z8e.dat

eth_z8e.dat: ${MYRI10GE}/eth_z8e.dat.gz.uu
	uudecode -p < ${MYRI10GE}/eth_z8e.dat.gz.uu \
		| gzip -dc > ${.TARGET}

.include <bsd.kmod.mk>
