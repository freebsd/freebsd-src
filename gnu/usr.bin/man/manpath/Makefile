# $FreeBSD: src/gnu/usr.bin/man/manpath/Makefile,v 1.22.2.1 2000/12/05 08:11:12 ru Exp $

PROG=	manpath
SRCS=	manpath.c
MAN1=	manpath.1

DPADD=	${LIBMAN}
LDADD=	${LIBMAN}

CFLAGS+= -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS
CFLAGS+= -DALT_SYSTEMS -I${.CURDIR}/../lib -I${.OBJDIR}/../lib
CLEANFILES+=	${MAN1}
MANDEPEND+=	${MAN1}

${MAN1}: ${.CURDIR}/manpath.man
	@${ECHO} Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%manpath_config_file%,/etc/manpath.config,' \
	${.CURDIR}/manpath.man > ${MAN1}

.include <bsd.prog.mk>
