# $FreeBSD$

MAINTAINER=	roberto

DEFS_LOCAL= -DPARSE -DHAVE_CONFIG_H
NTPDEFS=   -DSYS_FREEBSD
CLOCKDEFS=
#	-DLOCAL_CLOCK -DPST -DWWVB -DAS2201 -DGOES -DGPSTM -DOMEGA \
#	-DLEITCH -DTRAK -DACTS -DATOM -DDATUM -DHEATH -DMSFEES \
#	-DMX4200 -DNMEA -DBOEDER
CFLAGS+= ${NTPDEFS} ${DEFS_LOCAL} ${CLOCKDEFS}

.if exists(${.OBJDIR}/../libparse)
LIBPARSE=	${.OBJDIR}/../libparse/libparse.a
.else
LIBPARSE=	${.CURDIR}/../libparse/libparse.a
.endif

.if exists(${.OBJDIR}/../libntp)
LIBNTP=	${.OBJDIR}/../libntp/libntp.a
.else
LIBNTP=	${.CURDIR}/../libntp/libntp.a
.endif

.include "../Makefile.inc"
