#
#     The FreeBSD Documentation Project
#     The FreeBSD French Documentation Project
#     $Id$
#
#     $FreeBSD$
#     Original revision: 1.1
#


RELN_ROOT?= ${.CURDIR}/../../..

DOC?= article
FORMATS?= html
INSTALL_COMPRESSED?= gz
INSTALL_ONLY_COMPRESSED?=

# SGML content
SRCS+=	article.sgml
SRCS+=	install.sgml
SRCS+=	../common/artheader.sgml
SRCS+=	../common/install.sgml
SRCS+=	../common/layout.sgml

.include "${RELN_ROOT}/share/mk/doc.relnotes.mk"
.include "${DOC_PREFIX}/share/mk/doc.project.mk"
