# $FreeBSD$
#
# The user can override the default language to build and install
# with the RELNOTES_LANG variable.
# 
.if defined(RELNOTES_LANG) && !empty(RELNOTES_LANG)
SUBDIR+= 	${RELNOTES_LANG}
.else
SUBDIR+=	en_US.ISO8859-1
.endif
SUBDIR+=	share/sgml

RELN_ROOT?= ${.CURDIR}

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.subdir.mk"
