PROG=	manpath
SRCS=	manpath.c

.if exists(${.CURDIR}/../lib/obj)
LDADD=  -L${.CURDIR}/../lib/obj -lman
.else
LDADD=  -L${.CURDIR}/../lib/ -lman
.endif

.if exists(${.CURDIR}/obj)
MAN1=${.CURDIR}/obj/manpath.1
.else
MAN1=${.CURDIR}/manpath.1
.endif

DPADD+=  ${MAN1}
CFLAGS+= -I${.CURDIR}/../lib -DMAIN -DSTDC_HEADERS -DPOSIX -DHAS_TROFF -DDO_UNCOMPRESS -DALT_SYSTEMS

${MAN1}: ${.CURDIR}/manpath.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	${.CURDIR}/manpath.man > ${MAN1}

afterinstall:
	install -c -o bin -g bin -m 555 ${.CURDIR}/manpath.config ${DESTDIR}${manpath_config_file}

.include <bsd.prog.mk>
