.if exists(${.CURDIR}/obj)
TARG=  ${.CURDIR}/obj/makewhatis
.else
TARG=  ${.CURDIR}/makewhatis
.endif

all:	${TARG}

depend rcsfreeze tags all:
	@echo -n

cleandir: clean
	cd ${.CURDIR}; rm -rf obj;

clean:
	@rm -f ${TARG}

install:
	install -c -o bin -g bin -m 555 ${TARG} ${DESTDIR}/usr/bin

${TARG}: ${.CURDIR}/makewhatis.sh
	sed -e 's/%sections%/ "1", "n", "l", "6", "8", "2", "3", "4", "5", "7", "p", "o", NULL/' ${.CURDIR}/makewhatis.sh > ${TARG}

.include "../Makefile.inc"
