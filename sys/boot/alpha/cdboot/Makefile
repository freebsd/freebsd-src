# $FreeBSD$

BASE= 		cdboot
PROG= 		${BASE}
NOMAN=
NEWVERSWHAT=	"SRM CD9660 boot" alpha
LOAD_ADDRESS=	${PRIMARY_LOAD_ADDRESS}

CFLAGS+=	-ffreestanding -DLOADER_CDROM_SUPPORT

.include <${.CURDIR}/../common/Makefile.common>
