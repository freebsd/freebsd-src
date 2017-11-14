# $FreeBSD$

SYSDIR?=${.CURDIR}/../../../..

test-dts:
.for dts in ${DTS}
	@env MACHINE=`basename ${.CURDIR}` ${SYSDIR}/tools/fdt/make_dtb.sh ${SYSDIR} ${dts} /tmp

.endfor

