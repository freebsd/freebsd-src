PROG=	man
SRCS=	man.c manpath.c glob.c
MAN1=   man.1
BINOWN= man
BINMODE=4555
INSTALLFLAGS+= -fschg

CFLAGS+= -I${LIBDESTDIR}
DPADD=	${LIBMAN}
LDADD=	${LIBMAN}

CFLAGS+= -I${.CURDIR}/../lib -DSTDC_HEADERS -DPOSIX -DHAS_TROFF
CFLAGS+= -DDO_COMPRESS -DALT_SYSTEMS -DSETREUID -DCATMODE=0644
CLEANFILES+=	${MAN1}
MANDEPEND+=	${MAN1}

.PATH:	${.CURDIR}/../manpath

${MAN1}: ${.CURDIR}/man.man
	@${ECHO} echo Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	${.CURDIR}/man.man > ${MAN1}

.include <bsd.prog.mk>
