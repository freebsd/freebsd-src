# $FreeBSD$

.if ${MACHINE_ARCH} == "amd64"
BI_BITS=
.endif

_BINS=	\
	filemontest \
	timed-forkb \
	sizeof_long

BINS=	${_BINS}
.if defined(BI_BITS)
BINS+=	${_BINS:C/$/32/g}
.endif

bins: ${BINS}
all: bins

MAN=

WARNS?=	6
CFLAGS+= -I${.CURDIR}/../../../sys

# We don't want to genreate CTF files. Force that here.
MK_CTF=no

CLEANFILES+=	${BINS}


.for f32 in ${BINS}
${f32}32: ${f32}.c
	${CC} -m32 -DBIT=\"32\" -o ${.TARGET} ${CFLAGS} ${.ALLSRC}
.endfor

tests:
	kldstat | grep filemon
	@echo ""
	${MAKE} test01
	${MAKE} test02
.if defined(BI_BITS)
	${MAKE} test32
.endif
	@echo "filemon(4) tests passed."

# Cannot use .OBJDIR -- 'filemontest' expects 'test_script.sh' in .
test01: ${BINS:Mfilemontest*} ${BINS:Msizeof_long*} clean-test
.for BIN in ${BINS:Mfilemontest}
	cd ${.CURDIR} ; \
		for A in 1 2 3 4 5 6 7 8 9 0; do \
		for B in 1 2 3 4 5 6 7 8 9 0; do \
		for C in 1 2 3 4 5 6 7 8 9 0; do \
			test -x ${BIN} && ${.OBJDIR}/${BIN} ;\
		done ;\
		done ;\
		done
	@cd ${.CURDIR} ; set +e ; egrep '(Start|Stop) .*\.' filemon_log.* | \
	    grep -q -v '\.[0-9][0-9][0-9][0-9][0-9][0-9]$$' || printf "Time stamp format OK\n\n"
.endfor
	@cd ${.CURDIR} ; set +e ; for F in filemon_log.* ; do \
	    tail -1 $$F | grep -q '# Bye bye' || echo "$$F missing filemon bye-bye" ; \
	    NL=`wc -l $$F | awk '{print $$1}'` ; \
	    if [ "$${NL}" != 97 ]; then echo "$$F BAD, contains $${NL} lines" ; exit 1 ; fi ; done

test02: ${BINS:Mtimed-forkb*}
	@echo "Without filemon(4) active:"
	./timed-forkb
	@echo "With filemon(4) active:"
	script -f typescript-timed-forkb ./timed-forkb
	ls -l typescript-timed-forkb.filemon

test32: ${BINS:M*32*}
	script -f typescript.${.TARGET} ./sizeof_long32 >/dev/null
	@tail -1 typescript.test32.filemon | grep -q '# Bye bye' || (echo '32-bit comapt filemon Missing "bye-bye"' ; exit 1)
	@egrep -q '^X [0-9]+ 0$$' typescript.test32.filemon || (echo "32-bit binary exit ERROR" ; exit 1)
	@printf "filemon(4) 32bit FreeBSD support passed.\n\n"

CLEANFILES+=	typescript-timed-forkb typescript-timed-forkb.filemon

clean-test:
	cd ${.CURDIR} ; rm -f filemon_log*.*

clean-tests: clean-test

.include <bsd.prog.mk>
