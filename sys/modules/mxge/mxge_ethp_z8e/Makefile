# $FreeBSD$

MXGE= ${.CURDIR}/../../../dev/mxge
.PATH= ${MXGE}
KMOD=	mxge_ethp_z8e
FIRMWS=	ethp_z8e.dat:mxge_ethp_z8e
CLEANFILES+= ethp_z8e.dat

ethp_z8e.dat: ${MXGE}/ethp_z8e.dat.gz.uu
	uudecode -p < ${MXGE}/ethp_z8e.dat.gz.uu \
		| gzip -dc > ${.TARGET}

.include <bsd.kmod.mk>
