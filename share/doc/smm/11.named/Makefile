#	from: @(#)Makefile	6.4 (Berkeley) 6/30/90
#	Makefile,v 1.4 1993/07/31 15:31:45 mycroft Exp

DIR=	smm/11.named
DOC=	11.named

SRCS=	00macs.me 00title.me intro.me build.me types.me setup.me \
	files.me named.boot.primary named.boot.secondary named.boot.cache \
	resolv.conf root.cache named.local ucbhosts ucbhosts.rev manage.me
MACROS=	-me

.include <bsd.doc.mk>

${DOC}.${PRINTER}: ${SRCS}
	( cd ${.CURDIR} ; ${TBL} ${SRCS} ) | ${ROFF} ${COMPAT} > ${.TARGET}
