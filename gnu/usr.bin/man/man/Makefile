PROG=	man
SRCS=	man.c manpath.c glob.c

.if exists(${.OBJDIR}/../lib)
LDADD=  -L${.OBJDIR}/../lib -lman
CFLAGS+= -I${.OBJDIR}/../lib
.else
LDADD=  -L${.CURDIR}/../lib/ -lman
.endif

.if exists(${.OBJDIR})
MAN1=   ${.OBJDIR}/man.1
.else
MAN1=   ${.CURDIR}/man.1
.endif

DPADD+=  ${MAN1}
CFLAGS+= -I${.CURDIR}/../lib -DSTDC_HEADERS -DPOSIX -DHAS_TROFF
CFLAGS+= -DDO_COMPRESS -DALT_SYSTEMS -DSETREUID -DCATMODE=0664
CLEANFILES+=	${MAN1}
MANDEPEND+=	${MAN1}

${MAN1}: ${.CURDIR}/man.man
	@${ECHO} echo Making ${.TARGET:T} from ${.ALLSRC:T}; \
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	${.CURDIR}/man.man > ${MAN1}

.include <bsd.prog.mk>
