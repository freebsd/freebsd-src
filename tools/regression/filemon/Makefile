# $FreeBSD$

PROG=	filemontest

NO_MAN=

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../../sys

# Cannot use .OBJDIR -- 'filemontest' expects 'test_script.sh' in .
test: ${PROG} clean-test
.for BIN in ${PROG} ${PROG}32
	cd ${.CURDIR} ; \
		for A in 1 2 3 4 5 6 7 8 9 0; do \
		for B in 1 2 3 4 5 6 7 8 9 0; do \
		for C in 1 2 3 4 5 6 7 8 9 0; do \
			test -x ${BIN} && ${.OBJDIR}/${BIN} ;\
		done ;\
		done ;\
		done
	@cd ${.CURDIR} ; set +e ; egrep '(Start|Stop) .*\.' filemon_log.* | \
	    grep -q -v '\.[0-9][0-9][0-9][0-9][0-9][0-9]$$' || echo "Time stamp format OK"
.endfor

clean-test:
	cd ${.CURDIR} ; rm -f filemon_log.*

.include <bsd.prog.mk>
