# $FreeBSD$

.PATH:	${.CURDIR}/../../dev/md
KMOD=	md
SRCS=	md.c opt_mfs.h opt_md.h
NOMAN=
CLEANFILES +=	opt_mfs.h opt_md.h

opt_md.h:
	touch opt_md.h

opt_mfs.h:
	touch opt_mfs.h

.include <bsd.kmod.mk>


