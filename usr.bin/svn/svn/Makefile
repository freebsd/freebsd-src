# $FreeBSD$

MAN=

.include "${.CURDIR}/../Makefile.inc"

.PATH:	${SVNDIR}/svn

PROG=	svn${SVNLITE}

NO_PIE=	yes

SRCS=	add-cmd.c blame-cmd.c cat-cmd.c changelist-cmd.c checkout-cmd.c \
	cl-conflicts.c cleanup-cmd.c commit-cmd.c conflict-callbacks.c \
	copy-cmd.c delete-cmd.c deprecated.c diff-cmd.c export-cmd.c \
	file-merge.c help-cmd.c import-cmd.c info-cmd.c list-cmd.c \
	lock-cmd.c log-cmd.c merge-cmd.c mergeinfo-cmd.c mkdir-cmd.c \
	move-cmd.c notify.c patch-cmd.c propdel-cmd.c propedit-cmd.c \
	propget-cmd.c proplist-cmd.c props.c propset-cmd.c relocate-cmd.c \
	resolve-cmd.c resolved-cmd.c revert-cmd.c status-cmd.c status.c \
	svn.c switch-cmd.c unlock-cmd.c update-cmd.c upgrade-cmd.c util.c 

CFLAGS+=-I${SVNDIR}/include -I${SVNDIR} -I${.CURDIR}/.. \
	-I${.CURDIR}/../lib/libapr \
	-I${APR}/include/arch/unix \
	-I${APR}/include \
	-I${.CURDIR}/../lib/libapr_util \
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

DPADD=	${LIBSVN_CLIENT} ${LIBSVN_WC} ${LIBSVN_RA} ${LIBSVN_RA_LOCAL} \
	${LIBSVN_RA_SVN} ${LIBSVN_RA_SERF} ${LIBSVN_REPOS} \
	${LIBSVN_FS} ${LIBSVN_FS_FS} ${LIBSVN_FS_UTIL} ${LIBSVN_DELTA} \
	${LIBSVN_DIFF} ${LIBSVN_SUBR} ${LIBSERF} ${LIBAPR_UTIL} \
	${LIBBSDXML} ${LIBAPR} ${LIBSQLITE} ${LIBZ} ${LIBCRYPT} ${LIBMAGIC} \
	${LIBCRYPTO} ${LIBSSL} ${LIBPTHREAD}

.if(defined(ORGANIZATION) && !empty(ORGANIZATION))
DPSRCS+=	freebsd-organization.h
CLEANFILES+=	freebsd-organization.h
CFLAGS+=	-I. -DHAS_ORGANIZATION_NAME
freebsd-organization.h:
	@echo '#define ORGANIZATION_NAME ${ORGANIZATION}' \
	    > freebsd-organization.h
.endif

.include <bsd.prog.mk>
