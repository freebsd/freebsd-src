# $FreeBSD$

PROG=	manpath

DPADD=	${LIBMAN}
LDADD=	${LIBMAN}

CFLAGS+= -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS
CFLAGS+= -I${.CURDIR}/../lib -I${.OBJDIR}/../lib
CLEANFILES+=	manpath.1

manpath.1: ${.CURDIR}/manpath.man
	@${ECHO} Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%manpath_config_file%,/etc/manpath.config,' \
	${.ALLSRC} > ${.TARGET}

.include <bsd.prog.mk>
