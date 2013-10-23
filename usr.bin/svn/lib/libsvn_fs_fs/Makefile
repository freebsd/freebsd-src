# $FreeBSD$

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/libsvn_fs_fs

INTERNALLIB=	yes
LIB=		svn_fs_fs

SRCS=		caching.c dag.c fs.c fs_fs.c id.c key-gen.c lock.c \
		rep-cache.c temp_serializer.c tree.c

CFLAGS+=	-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/../.. \
		-I${.CURDIR}/../libapr \
		-I${APR}/include/arch/unix \
		-I${APR}/include \
		-I${.CURDIR}/../libapr_util \
		-I${APRU}/include/private \
		-I${APRU}/include

.include <bsd.lib.mk>
