PROG=	manpath
SRCS=	manpath.c

.if exists(${.OBJDIR}/../lib)
LIBDESTDIR=	${.OBJDIR}/../lib
.else
LIBDESTDIR=	${.CURDIR}/../lib
.endif
DPADD=	${LIBDESTDIR}/libman.a
LDADD=	-L${LIBDESTDIR} -lman

.if exists(${.OBJDIR})
MAN1=${.OBJDIR}/manpath.1
.else
MAN1=${.CURDIR}/manpath.1
.endif

CFLAGS+= -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS
CFLAGS+= -DALT_SYSTEMS -I${.CURDIR}/../lib -I${.OBJDIR}/../lib
CLEANFILES+=	${MAN1}
MANDEPEND+=	${MAN1}

${MAN1}: ${.CURDIR}/manpath.man
	@${ECHO} Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	${.CURDIR}/manpath.man > ${MAN1}

afterinstall:
	${INSTALL} -c -o bin -g bin -m 644 ${.CURDIR}/manpath.config \
	    ${DESTDIR}${manpath_config_file}.sample

.include <bsd.prog.mk>
