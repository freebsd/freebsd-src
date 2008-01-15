#
# $FreeBSD: src/tools/regression/geom_uzip/Makefile,v 1.1 2004/08/13 09:53:52 fjoe Exp $
#
# Regression test for geom_ugz.
#

IMAGE=		test-1.img
ZIMAGE=		${IMAGE}.gz
UZIMAGE=	${ZIMAGE}.uue

test:
	@sh runtests.sh

image:
	makefs -s 1048576 ${IMAGE} etalon
	printf "#\n# $$" >${UZIMAGE}
	printf "FreeBSD$$\n#\n\n" >> ${UZIMAGE}
	create_compressed_fs ${IMAGE} 65536 | uuencode ${ZIMAGE} >>${UZIMAGE}

clean:
	rm -f ${IMAGE} ${ZIMAGE}
