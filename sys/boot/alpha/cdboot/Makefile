#	$FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.3.2.2 2001/08/01 20:52:58 mjacob Exp $

BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_CDROM_SUPPORT

.include	<${.CURDIR}/../common/Makefile.common>
