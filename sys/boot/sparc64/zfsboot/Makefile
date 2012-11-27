# $FreeBSD$

.PATH: ${.CURDIR}/../boot1

PROGNAME=	zfsboot
CFLAGS+=	-DZFSBOOT
FILES=		zfsboot
CLEANFILES+=	zfsboot

zfsboot: boot1
	ln -s ${.ALLSRC} ${.TARGET}

.include "${.CURDIR}/../boot1/Makefile"
