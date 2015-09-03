# $FreeBSD$

.include <bsd.own.mk>

PROG=	fstyp
SRCS=	fstyp.c ext2fs.c cd9660.c msdosfs.c ntfs.c ufs.c
MAN=	fstyp.8

WARNS=	6

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
