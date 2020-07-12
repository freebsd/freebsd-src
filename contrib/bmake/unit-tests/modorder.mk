# $NetBSD: modorder.mk,v 1.3 2020/06/09 01:48:17 sjg Exp $

LIST=		one two three four five six seven eight nine ten
LISTX=		${LIST:Ox}
LISTSX:=	${LIST:Ox}
TEST_RESULT= && echo Ok || echo Failed

# unit-tests have to produce the same results on each run
# so we cannot actually include :Ox output.
all:
	@echo "LIST      = ${LIST}"
	@echo "LIST:O    = ${LIST:O}"
	@echo "LIST:Or    = ${LIST:Or}"
	# Note that 1 in every 10! trials two independently generated
	# randomized orderings will be the same.  The test framework doesn't
	# support checking probabilistic output, so we accept that each of the
	# 3 :Ox tests will incorrectly fail with probability 2.756E-7, which
	# lets the whole test fail once in 1.209.600 runs, on average.
	@echo "LIST:Ox   = `test '${LIST:Ox}' != '${LIST:Ox}' ${TEST_RESULT}`"
	@echo "LIST:O:Ox = `test '${LIST:O:Ox}' != '${LIST:O:Ox}' ${TEST_RESULT}`"
	@echo "LISTX     = `test '${LISTX}' != '${LISTX}' ${TEST_RESULT}`"
	@echo "LISTSX    = `test '${LISTSX}' = '${LISTSX}' ${TEST_RESULT}`"
	@echo "BADMOD 1  = ${LIST:OX}"
	@echo "BADMOD 2  = ${LIST:OxXX}"
