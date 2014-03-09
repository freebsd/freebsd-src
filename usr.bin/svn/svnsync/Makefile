# $FreeBSD$

NO_MAN=	svnsync.1

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svnsync

PROG=	svn${SVNLITE}sync

SRCS=	svnsync.c sync.c

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
	-I${APRU}/include/private \
	-I${APRU}/include

LDADD=	-L${LIBSVN_RADIR} -lsvn_ra \
	-L${LIBSVN_RA_LOCALDIR} -lsvn_ra_local \
	-L${LIBSVN_RA_SVNDIR} -lsvn_ra_svn \
	-L${LIBSVN_RA_SERFDIR} -lsvn_ra_serf \
	-L${LIBSVN_REPOSDIR} -lsvn_repos \
	-L${LIBSVN_FSDIR} -lsvn_fs \
	-L${LIBSVN_FS_FSDIR} -lsvn_fs_fs \
	-L${LIBSVN_FS_UTILDIR} -lsvn_fs_util \
	-L${LIBSVN_DELTADIR} -lsvn_delta \
	-L${LIBSVN_SUBRDIR} -lsvn_subr \
	-L${LIBSERFDIR} -lserf \
	-L${LIBAPR_UTILDIR} -lapr-util \
	-lbsdxml \
	-L${LIBAPRDIR} -lapr \
	-L${LIBSQLITEDIR} -lsqlite3 \
	-lz -lcrypt -lmagic -lcrypto -lssl -lpthread

DPADD=	${LIBSVN_RA} ${LIBSVN_RA_LOCAL} ${LIBSVN_RA_SVN} ${LIBSVN_RA_SERF} \
	${LIBSVN_REPOS} ${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} \
	${LIBSVN_DELTA} ${LIBSVN_SUBR} ${LIBSERF} ${LIBAPR_UTIL} \
	${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} ${LIBCRYPT} ${LIBMAGIC} \
	${LIBCRYPTO} ${LIBSSL} ${LIBPTHREAD}

.include <bsd.prog.mk>
