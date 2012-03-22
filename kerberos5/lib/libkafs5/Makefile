# $FreeBSD$

LIB=	kafs5
LDADD=	-lasn1 -lroken
DPADD=	${LIBASN1} ${LIBKRB5} ${LIBROKEN}
INCS=	kafs.h
MAN=	kafs5.3

#
# Linking with libkrb5 uncovers a bug in binutils.
# See http://repo.or.cz/w/binutils.git/commit/ee05170bf71819c99cb5a36a44735c231ae03c56 .
#
.if ${MACHINE_ARCH} != "mipsn32eb" && ${MACHINE_ARCH} != "mipsel" && ${MACHINE_ARCH} != "mipseb"
LDADD+=	-lkrb5
LDFLAGS=	-Wl,--no-undefined
.endif

MLINKS=	kafs5.3 k_afs_cell_of_file.3 \
	kafs5.3 k_hasafs.3 \
	kafs5.3 k_pioctl.3 \
	kafs5.3 k_setpag.3 \
	kafs5.3 k_unlog.3 \
	kafs5.3 kafs.3 \
	kafs5.3 kafs_set_verbose.3 \
	kafs5.3 kafs_settoken.3 \
	kafs5.3 kafs_settoken5.3 \
	kafs5.3 kafs_settoken_rxkad.3 \
	kafs5.3 krb5_afslog.3 \
	kafs5.3 krb5_afslog_uid.3 \
	kafs5.3 krb_afslog.3 \
	kafs5.3 krb_afslog_uid.3

SRCS=	afssys.c afskrb5.c common.c krb5_err.h
SRCS=	afssys.c \
	afskrb5.c \
	common.c \
	kafs_locl.h \
	afssysdefs.h \
	roken_rename.h
CFLAGS+=	-I${KRB5DIR}/lib/kafs \
		-I${KRB5DIR}/lib/krb5 \
		-I${.OBJDIR}/../libkrb5/ \
		-I${KRB5DIR}/lib/roken

CLEANFILES= kafs5.3

kafs5.3: kafs.3
	sed -e 's/libkafs, -lkafs/libkafs5, -lkafs5/g' ${.ALLSRC} >${.TARGET}

.include <bsd.lib.mk>

.PATH: ${KRB5DIR}/lib/kafs
