#
# $FreeBSD$
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
