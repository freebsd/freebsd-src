#	$FreeBSD: src/sys/boot/alpha/cdboot/Makefile,v 1.3 1999/09/03 19:42:14 msmith Exp $

BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-DLOADER_CDROM_SUPPORT

.include	<${.CURDIR}/../common/Makefile.common>
