.if exists(${.CURDIR}/obj)
MAN1=  ${.CURDIR}/obj/whatis.1
TARG=  ${.CURDIR}/obj/whatis
.else
MAN1=  ${.CURDIR}/whatis.1
TARG=  ${.CURDIR}/whatis
.endif

MANDEPEND=	${MAN1}

all:	${TARG} ${MAN1}

depend rcsfreeze tags all:
	@echo -n

cleandir: clean
	cd ${.CURDIR}; rm -rf obj;

clean:
	@rm -f ${TARG} ${MAN1}

${TARG}: ${.CURDIR}/whatis.sh
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		${.CURDIR}/whatis.sh > ${TARG}

${MAN1}: ${.CURDIR}/whatis.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	${.CURDIR}/whatis.man > ${MAN1}

install: ${TARG} maninstall
	install -c -o bin -g bin -m 555 ${TARG} ${DESTDIR}/usr/bin


.include "../Makefile.inc"
.if make(maninstall) || make(install)
.if !defined(NOMAN)
.include <bsd.man.mk>
.else
maninstall:
.endif
.endif
