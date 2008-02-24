# $FreeBSD: src/sys/modules/smbfs/Makefile,v 1.12 2006/03/05 22:52:17 yar Exp $

.PATH:	${.CURDIR}/../../crypto/des \
	${.CURDIR}/../../crypto/des/arch/${MACHINE_ARCH} \
	${.CURDIR}/../../kern \
	${.CURDIR}/../../libkern \
	${.CURDIR}/../../netsmb \
	${.CURDIR}/../../fs/smbfs

KMOD=	smbfs

SRCS=	vnode_if.h \
	opt_inet.h opt_ipx.h \
	opt_netsmb.h \
	iconv_converter_if.h \
	md4c.c \
	smb_conn.c smb_dev.c smb_trantcp.c smb_smb.c smb_subr.c smb_rq.c \
	smb_usr.c smb_crypt.c smb_iod.c \
	smbfs_vfsops.c smbfs_node.c smbfs_io.c smbfs_vnops.c \
	smbfs_subr.c smbfs_smb.c

# NETSMBCRYPTO
SRCS+=	des_ecb.c des_setkey.c
.if ${MACHINE_ARCH} == "i386"
SRCS+=	des_enc.S
.else
SRCS+=	des_enc.c
.endif

# Build with IPX support (1|0)
SMB_IPX?=	0

# Build with INET support (1|0)
SMB_INET?=	1

CFLAGS+= ${KDEBUG}

.if defined(VNPRINT)
CFLAGS+= -DVNPRINT
.endif

.if !defined(KERNBUILDDIR)
.if ${SMB_INET} > 0
opt_inet.h:
	echo "#define INET 1" > ${.TARGET}
.endif

.if ${SMB_IPX} > 0
opt_ipx.h:
	echo "#define IPX 1" > ${.TARGET}
.endif
.endif

.include <bsd.kmod.mk>
