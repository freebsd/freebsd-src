#
# $FreeBSD$
#

.include <bsd.own.mk>

NETSURF_DISTDIR?= ${.CURDIR}/../../../contrib/netsurf

LIB=	hubbub

.PATH:		${NETSURF_DISTDIR}/libhubbub
SRCS=		src/parser.c \
		src/charset/detect.c \
		src/tokeniser/entities.c \
		src/tokeniser/tokeniser.c \
		src/treebuilder/treebuilder.c \
		src/treebuilder/initial.c \
		src/treebuilder/before_html.c \
		src/treebuilder/before_head.c \
		src/treebuilder/in_head.c \
		src/treebuilder/in_head_noscript.c \
		src/treebuilder/after_head.c \
		src/treebuilder/in_body.c \
		src/treebuilder/in_caption.c \
		src/treebuilder/in_column_group.c \
		src/treebuilder/in_table.c \
		src/treebuilder/in_table_body.c \
		src/treebuilder/in_row.c \
		src/treebuilder/in_cell.c \
		src/treebuilder/in_select.c \
		src/treebuilder/in_select_in_table.c \
		src/treebuilder/in_foreign_content.c \
		src/treebuilder/after_body.c \
		src/treebuilder/in_frameset.c \
		src/treebuilder/after_frameset.c \
		src/treebuilder/after_after_body.c \
		src/treebuilder/after_after_frameset.c \
		src/treebuilder/generic_rcdata.c \
		src/utils/errors.c \
		src/utils/string.c

CFLAGS+=	-I${NETSURF_DISTDIR}/libhubbub/include \
		-I${NETSURF_DISTDIR}/libhubbub/src \
		-I${NETSURF_DISTDIR}/libparserutils/include \
		-I${.CURDIR}/src/tokeniser \
		'-D_ALIGNED=__attribute__((aligned))' -DNDEBUG

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
