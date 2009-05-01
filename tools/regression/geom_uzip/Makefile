#
# $FreeBSD: src/tools/regression/geom_uzip/Makefile,v 1.2.8.1 2009/04/15 03:14:26 kensmith Exp $
#
# Regression test for geom_ugz.
#

IMAGE=		test-1.img
ZIMAGE=		${IMAGE}.uzip
UZIMAGE=	${ZIMAGE}.uue

test:
	@sh runtests.sh

image:
	makefs -s 1048576 ${IMAGE} etalon
	printf "#\n# $$" >${UZIMAGE}
	printf "FreeBSD$$\n#\n\n" >> ${UZIMAGE}
	mkuzip -o ${ZIMAGE} ${IMAGE}
	uuencode ${ZIMAGE} ${ZIMAGE} >>${UZIMAGE}
	rm ${ZIMAGE}

clean:
	rm -f ${IMAGE} ${ZIMAGE}
