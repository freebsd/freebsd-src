# $FreeBSD$
# Original revision: 1.4

RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

JADEFLAGS+=	-V %generate-article-toc%

# SGML content
SRCS+=	article.sgml 
SRCS+=	proc-sparc64.sgml 
SRCS+=	../common/hw.ent
SRCS+=	../common/artheader.sgml
SRCS+=	../common/dev.sgml
SRCS+=	${DEV-AUTODIR}/catalog-auto
SRCS+=	${DEV-AUTODIR}/dev-auto.sgml
SRCS+=	../../share/sgml/dev-auto-ja.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
