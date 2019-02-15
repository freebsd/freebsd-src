# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_delta

INTERNALLIB=	yes
LIB=		svn_delta

SRCS=		cancel.c compat.c compose_delta.c debug_editor.c \
		default_editor.c deprecated.c depth_filter_editor.c editor.c \
		path_driver.c svndiff.c text_delta.c version.c xdelta.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
