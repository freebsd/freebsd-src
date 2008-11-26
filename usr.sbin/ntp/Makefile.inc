# $FreeBSD: src/usr.sbin/ntp/Makefile.inc,v 1.8.2.1.2.1 2008/10/02 02:57:24 kensmith Exp $

DEFS_LOCAL= -DPARSE -DHAVE_CONFIG_H
NTPDEFS=   -DSYS_FREEBSD
# CLOCKDEFS=
#	-DLOCAL_CLOCK -DPST -DWWVB -DAS2201 -DGOES -DGPSTM -DOMEGA \
#	-DLEITCH -DTRAK -DACTS -DATOM -DDATUM -DHEATH -DMSFEES \
#	-DMX4200 -DNMEA -DBOEDER
CFLAGS+= ${NTPDEFS} ${DEFS_LOCAL} ${CLOCKDEFS}

.if !defined(NO_CRYPT) && !defined(NO_OPENSSL) && !defined(RELEASE_CRUNCH)
CFLAGS+= -DOPENSSL
.endif

LIBOPTS=	${.OBJDIR}/../libopts/libopts.a
LIBPARSE=	${.OBJDIR}/../libparse/libparse.a
LIBNTP=	${.OBJDIR}/../libntp/libntp.a

.include "../Makefile.inc"
