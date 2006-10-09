# $FreeBSD$

PROG=	doexec
NO_MAN=

RP=	./${PROG}
TD=	${.CURDIR}/tests

TESTSCRIPTS=	nonexistshell devnullscript badinterplen goodscript \
		scriptarg scriptarg-nospace
CLEANFILES=	goodaout truncaout sparseaout empty ${TESTSCRIPTS}

all:		${PROG} goodaout ${TESTSCRIPTS}

.for x in ${TESTSCRIPTS}
${x}: ${TD}/${x}
	cp ${TD}/${x} .
	chmod +x ${x}
.endfor

regress:	test-empty test-nonexist test-nonexistshell \
		test-devnullscript test-badinterplen test-goodscript \
		test-scriptarg test-scriptarg-nospace test-goodaout \
		test-truncaout test-sparseaout

test-empty:	${PROG}
	rm -f empty
	touch empty
	chmod +x empty
	${RP} empty | grep 'Exec format error'

test-nonexist:	${PROG}
	${RP} ${TD}/nonexistent | grep 'No such file or directory'

test-nonexistshell: ${PROG} nonexistshell
	${RP} nonexistshell | grep 'No such file or directory'

test-devnullscript: ${PROG} devnullscript
	${RP} devnullscript | grep 'Permission denied'

test-badinterplen: ${PROG} badinterplen
	${RP} badinterplen | grep 'No such file or directory'

test-goodscript: ${PROG} goodscript
	${RP} goodscript | grep 'succeeded'

test-scriptarg: ${PROG} scriptarg
	${RP} scriptarg 2>&1 | grep '+ echo succeeded'

test-scriptarg-nospace: ${PROG} scriptarg-nospace
	${RP} scriptarg-nospace 2>&1 | grep '+ echo succeeded'

goodaout: ${TD}/goodaout.c
	${CC} -static -o ${.TARGET} ${TD}/goodaout.c

test-goodaout: ${PROG} goodaout
	${RP} goodaout | grep 'succeeded'

test-truncaout: ${PROG} goodaout
	truncate -s 16 truncaout
	chmod a+x truncaout
	${RP} truncaout | grep 'Exec format error'

test-sparseaout: ${PROG}
	/bin/rm -rf sparseaout
	truncate -s 20480 sparseaout
	chmod a+x sparseaout
	${RP} sparseaout | grep 'Exec format error'

.include <bsd.prog.mk>
