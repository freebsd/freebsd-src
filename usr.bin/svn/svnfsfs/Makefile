# $FreeBSD$

MAN=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svnfsfs

PROG=	svn${SVNLITE}fsfs

SRCS=	dump-index-cmd.c load-index-cmd.c stats-cmd.c svnfsfs.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDADD=	-L${LIBSVN_REPOSDIR} -lsvn_repos \
	-L${LIBSVN_FSDIR} -lsvn_fs \
	-L${LIBSVN_FS_FSDIR} -lsvn_fs_fs \
	-L${LIBSVN_FS_XDIR} -lsvn_fs_x \
	-L${LIBSVN_FS_UTILDIR} -lsvn_fs_util \
	-L${LIBSVN_DELTADIR} -lsvn_delta \
	-L${LIBSVN_SUBRDIR} -lsvn_subr \
	-L${LIBAPR_UTILDIR} -lapr-util \
	-L${LIBAPRDIR} -lapr
LIBADD+=	bsdxml sqlite3 z pthread

DPADD=	${LIBSVN_REPOS} ${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} \
	${LIBSVN_DELTA} ${LIBSVN_SUBR} ${LIBAPR_UTIL} \
	${LIBAPR}

.include <bsd.prog.mk>
