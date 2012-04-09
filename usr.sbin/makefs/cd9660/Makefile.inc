#	$FreeBSD: src/usr.sbin/makefs/cd9660/Makefile.inc,v 1.1.2.2.2.1 2012/03/03 06:15:13 kensmith Exp $
#

.PATH:	${.CURDIR}/cd9660 ${.CURDIR}/../../sys/fs/cd9660/

CFLAGS+=-I${.CURDIR}/../../sys/fs/cd9660/

SRCS+=	cd9660_strings.c cd9660_debug.c cd9660_eltorito.c \
	cd9660_write.c cd9660_conversion.c iso9660_rrip.c cd9660_archimedes.c
