# $FreeBSD$

PROG=	manpath

DPADD=	${LIBMAN}
LDADD=	${LIBMAN}

CFLAGS+= -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS
CFLAGS+= -DALT_SYSTEMS -I${.CURDIR}/../lib -I${.OBJDIR}/../lib
CLEANFILES+=	${MAN1}

.include <bsd.prog.mk>

${MAN1}: ${.CURDIR}/manpath.man
	@${ECHO} Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%manpath_config_file%,/etc/manpath.config,' \
	${.ALLSRC} > ${.TARGET}
