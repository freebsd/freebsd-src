# $Id$

.include "../Makefile.inc0"

.PATH: ${SRCDIR}/gas/doc ${SRCDIR}/ld ${SRCDIR}/bfd/doc

INFO = as ld
INFOSECTION=	"Programming & development tools."
INFOENTRY_as=	"* As: (as).            The GNU assembler."
INFOENTRY_ld=	"* Ld: (ld).		The GNU linker."

MAKEINFOFLAGS+= --no-validate
MAKEINFOFLAGS+= -I ${SRCDIR}/gas/doc -I ${SRCDIR}/ld -I ${SRCDIR}/bfd/doc

as.info:	as.texinfo asconfig.texi c-i386.texi
ld.info:	ld.texinfo bfdsumm.texi

.include <bsd.info.mk>
