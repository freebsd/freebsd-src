.if exists(${.CURDIR}/obj)
MANP=  ${.CURDIR}/obj/whatis.1
TARG=  ${.CURDIR}/obj/whatis
.else
MANP=  ${.CURDIR}/whatis.1
TARG=  ${.CURDIR}/whatis
.endif

all:	${TARG} ${MANP}

depend rcsfreeze tags all:
	@echo -n

cleandir: clean
	cd ${.CURDIR}; rm -rf obj;

clean:
	@rm -f ${TARG} ${MANP}

${TARG}: ${.CURDIR}/whatis.sh
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' \
		${.CURDIR}/whatis.sh > ${TARG}

${MANP}: ${.CURDIR}/whatis.man
	sed -e 's,%libdir%,${libdir},' -e 's,%bindir%,${bindir},' \
	    -e 's,%pager%,${pager},' -e 's,%troff%,${troff},' \
	    -e 's,%manpath_config_file%,${manpath_config_file},' \
	${.CURDIR}/whatis.man > ${MANP}

install: ${TARG} maninstall
	install -c -o bin -g bin -m 555 ${TARG} /usr/bin

maninstall: ${MANP}
	install -c -o bin -g bin -m 444 ${MANP} /usr/share/man/man1

.include "../Makefile.inc"
