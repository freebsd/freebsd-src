# $FreeBSD$

MAN=

.include "${.CURDIR:H}/Makefile.inc"

.PATH:	${SVNDIR}/svnmucc

PROG=	svn${SVNLITE}mucc

SRCS=	svnmucc.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR:H} \
	-I${.CURDIR:H}/lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR:H}/lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDADD=	-L${LIBSVN_CLIENTDIR} -lsvn_client \
	-L${LIBSVN_WCDIR} -lsvn_wc \
	-L${LIBSVN_RADIR} -lsvn_ra \
	-L${LIBSVN_RA_LOCALDIR} -lsvn_ra_local \
	-L${LIBSVN_RA_SVNDIR} -lsvn_ra_svn \
	-L${LIBSVN_RA_SERFDIR} -lsvn_ra_serf \
	-L${LIBSVN_REPOSDIR} -lsvn_repos \
	-L${LIBSVN_FSDIR} -lsvn_fs \
	-L${LIBSVN_FS_FSDIR} -lsvn_fs_fs \
	-L${LIBSVN_FS_XDIR} -lsvn_fs_x \
	-L${LIBSVN_FS_UTILDIR} -lsvn_fs_util \
	-L${LIBSVN_DELTADIR} -lsvn_delta \
	-L${LIBSVN_DIFFDIR} -lsvn_diff \
	-L${LIBSVN_SUBRDIR} -lsvn_subr \
	-L${LIBSERFDIR} -lserf \
	-L${LIBAPR_UTILDIR} -lapr-util \
	-lbsdxml \
	-L${LIBAPRDIR} -lapr \
	-L${LIBSQLITEDIR} -lsqlite3 \
	-lz -lcrypt -lmagic -lcrypto -lssl -lpthread

DPADD=	${LIBSVN_CLIENT} ${LIBSVN_RA} ${LIBSVN_RA_LOCAL} ${LIBSVN_RA_SVN} \
	${LIBSVN_RA_SERF} ${LIBSVN_REPOS} ${LIBSVN_FS} ${LIBSVN_FS_FS} \
	${LIBSVN_FS_UTIL} ${LIBSVN_DELTA} ${LIBSVN_SUBR} ${LIBSERF} \
	${LIBAPR_UTIL} ${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} \
	${LIBCRYPT} ${LIBMAGIC} ${LIBCRYPTO} ${LIBSSL} ${LIBPTHREAD}

.include <bsd.prog.mk>
