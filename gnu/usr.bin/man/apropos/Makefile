# $Id: Makefile,v 1.7 1994/06/05 21:57:03 csgr Exp $

.if exists(${.CURDIR}/obj)
MAN1=  ${.CURDIR}/obj/apropos.1
TARG=  ${.CURDIR}/obj/apropos
.else
MAN1=  ${.CURDIR}/apropos.1
TARG=  ${.CURDIR}/apropos
.endif

MANDEPEND=	${MAN1}

all:	${TARG} ${MAN1}

depend rcsfreeze tags all:
	@echo -n

cleandir: clean
	cd ${.CURDIR}; rm -rf obj;

clean:
	@rm -f ${TARG} ${MAN1}

${TARG}: ${.CURDIR}/apropos.sh
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		${.CURDIR}/apropos.sh > $@

${MAN1}: ${.CURDIR}/apropos.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	${.CURDIR}/apropos.man > $@

install: ${TARG} maninstall
	install -c -o bin -g bin -m 555 ${TARG} ${DESTDIR}/usr/bin


.include "../Makefile.inc"

.if make(maninstall) || make(install)
.if !defined(NOMAN)
.include <bsd.man.mk>
.elif !target(maninstall)
maninstall:
.endif
.endif
