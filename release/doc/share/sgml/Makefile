# $FreeBSD$

RELN_ROOT?= ${.CURDIR}/../..

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"

dev-auto.sgml: ${MAN4PAGES} ${ARCHLIST} ${RELN_ROOT}/share/misc/man2hwnotes.pl catalog-auto
	${PERL} ${RELN_ROOT}/share/misc/man2hwnotes.pl -a ${ARCHLIST} \
		${MAN4PAGES} > ${.TARGET}

catalog-auto:
	${ECHO_CMD} 'PUBLIC "-//FreeBSD//ENTITIES Auto Generated Device Lists//EN"' \
		'"dev-auto.sgml"' > ${.TARGET}

all: dev-auto.sgml
