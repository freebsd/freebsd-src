# Build a bootstrap version of m4 (needed in order to build libelf and lex)
.PATH: ${SRCTOP}/usr.bin/m4 ${.CURDIR}

# Avoid using lex or yacc to generate sources
LEX:=/this/should/not/be/used

tokenizer.c: bootstrap_m4_tokenizer
	test -e ${.TARGET}

# This target is used as a marker in usr.bin/m4/Makefile to not add the
# lex and yacc includes. Therefore we must define it before including
# the other Makefile.
bootstrap_m4_tokenizer: inittokenizer.c
.for _f in tokenizer.c
	@cmp -s ${.CURDIR}/init${_f} ${_f} || { \
		echo "Bootstrapping ${_f}" ; \
		${CP} ${.CURDIR}/init${_f} ${_f} ; \
	}
.endfor

BINDIR=	/usr/bin
.include "${SRCTOP}/usr.bin/m4/Makefile"

regen:
	(cd ${SRCTOP}/usr.bin/m4 && lex -t ${SRCTOP}/usr.bin/m4/tokenizer.l) | grep -v '#line' > inittokenizer.c
