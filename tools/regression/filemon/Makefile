# $FreeBSD$

BINS=	\
	filemontest \
	timed-forkb

bins: filemontest timed-forkb
all: bins

NO_MAN=

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../../sys

# Should be "WITHOUT_CTF=" below, but stupid infastrurture fails:
# "/usr/share/mk/bsd.own.mk", line 489: WITH_CTF and WITHOUT_CTF can't both be set.
WITHOUT_CDDL=

CLEANFILES+=	${BINS}

tests: bins
	kldstat | grep filemon
	${MAKE} test
	./timed-forkb
	@echo "filemon(4) tests passed."

# Cannot use .OBJDIR -- 'filemontest' expects 'test_script.sh' in .
#FILEMONTEST32=	filemontest32
test: filemontest clean-test
.for BIN in filemontest ${FILEMONTEST32}
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
