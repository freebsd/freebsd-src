# $FreeBSD$

PROG=	filemontest

NO_MAN=

WARNS?=	6
CFLAGS+= -I${.CURDIR}/..

# Cannot use .OBJDIR -- 'filemontest' expects 'test_script.sh' in .
test: ${PROG} clean-test
	cd ${.CURDIR} ; \
		for A in 1 2 3 4 5 6 7 8 9 0; do \
		for B in 1 2 3 4 5 6 7 8 9 0; do \
		for C in 1 2 3 4 5 6 7 8 9 0; do \
			${.OBJDIR}/${PROG} ;\
		done ;\
		done ;\
		done
	@cd ${.CURDIR} ; set +e ; egrep '(Start|Stop) .*\.' filemon_log.* | \
	    grep -q -v '\.[0-9][0-9][0-9][0-9][0-9][0-9]$$' || echo "Time stamp format OK"

clean-test:
	cd ${.CURDIR} ; rm -f filemon_log.*

.include <bsd.prog.mk>
