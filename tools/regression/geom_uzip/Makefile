#
# $FreeBSD$
#
# Regression test for geom_ugz.
#

IMAGE=		test-1.img
ZIMAGE=		${IMAGE}.uzip
UZIMAGE=	${ZIMAGE}.uue

test:
	prove -rv ./test-1.t

image:
	makefs -s 1048576 ${IMAGE} etalon
	printf "#\n# $$" >${UZIMAGE}
	printf "FreeBSD$$\n#\n\n" >> ${UZIMAGE}
	mkuzip -o ${ZIMAGE} ${IMAGE}
	uuencode ${ZIMAGE} ${ZIMAGE} >>${UZIMAGE}
	rm ${ZIMAGE}

clean:
	rm -f ${IMAGE} ${ZIMAGE}
