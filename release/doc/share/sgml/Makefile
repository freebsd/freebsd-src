# $FreeBSD: src/release/doc/share/sgml/Makefile,v 1.3.6.1 2008/11/25 02:59:29 kensmith Exp $

RELN_ROOT?= ${.CURDIR}/../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

dev-auto.sgml: ${MAN4PAGES} ${ARCHLIST} ${MAN2HWNOTES_CMD} catalog-auto
	${PERL} ${MAN2HWNOTES_CMD} ${MAN2HWNOTES_FLAGS} -a ${ARCHLIST} -o ${.TARGET} ${MAN4PAGES}

catalog-auto:
	${ECHO_CMD} 'PUBLIC "-//FreeBSD//ENTITIES Auto Generated Device Lists//EN"' \
		'"dev-auto.sgml"' > ${.TARGET}

all: dev-auto.sgml
