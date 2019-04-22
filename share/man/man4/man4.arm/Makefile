# $FreeBSD$

PACKAGE=runtime-manuals

MAN=	cgem.4 \
	devcfg.4 \
	imx6_ahci.4 \
	imx6_snvs.4 \
	imx_wdog.4 \
	mge.4 \
	npe.4 \
	ti_adc.4

MLINKS= cgem.4 if_cgem.4
MLINKS+= imx_wdog.4 imxwdt.4
MLINKS+= mge.4 if_mge.4
MLINKS+=npe.4 if_npe.4

MANSUBDIR=/arm

.include <bsd.prog.mk>
