PROG=	man
SRCS=	man.c manpath.c glob.c
BINMODE=6555

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lman
CFLAGS+= -I${.CURDIR}/../lib/obj
.else
LDADD=  -L${.CURDIR}/../lib/ -lman
.endif

.if exists(${.CURDIR}/obj)
MAN1=   ${.CURDIR}/obj/man.1
.else
MAN1=   ${.CURDIR}/man.1
.endif

DPADD+=  ${MAN1}
CFLAGS+= -I${.CURDIR}/../lib -DSTDC_HEADERS -DPOSIX -DHAS_TROFF 
CFLAGS+= -DDO_COMPRESS -DALT_SYSTEMS -DSETREUID -DCATMODE=0664
CLEANFILES+=	${MAN1}

${MAN1}: ${.CURDIR}/man.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	    -e 's,%compress%,${compress},' \
	${.CURDIR}/man.man > ${MAN1}

.include <bsd.prog.mk>
