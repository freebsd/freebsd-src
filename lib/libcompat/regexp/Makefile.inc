.PATH: ${.CURDIR}/regexp

SRCS += regexp.c regsub.c regerror.c
CFLAGS += -I ${.CURDIR}/regexp

MAN3 += regexp/regexp.3

# We need a different suffix for the man page links, so
# we could have `man 3 regcomp' telling about both
# libraries.
# <bsd.man.mk> is not clever enough to handle this...

M3COMPATLINKS = regcomp.3c regexec.3c regsub.3c regerror.3c

afterinstall:
	for link in ${M3COMPATLINKS} ; do \
		rm -f ${DESTDIR}${MANDIR}3${MANSUBDIR}/$$link; \
		ln ${DESTDIR}${MANDIR}3${MANSUBDIR}/regexp.3 \
			${DESTDIR}${MANDIR}3${MANSUBDIR}/$$link; \
	done
