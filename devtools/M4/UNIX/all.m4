divert(-1)
#
# Copyright (c) 1999-2000, 2006 Proofpoint, Inc. and its suppliers.
#	All rights reserved.
#
# By using this file, you agree to the terms and conditions set
# forth in the LICENSE file which can be found at the top level of
# the sendmail distribution.
#
#
#  Definitions for Makefile construction for sendmail
#
#	$Id: all.m4,v 8.22 2013-11-22 20:51:22 ca Exp $
#
divert(0)dnl
ALL=${BEFORE} ${LINKS} bldTARGETS

all: ${ALL}

clean: bldCLEAN_TARGETS

define(`bldADD_SRC', ${$1SRCS} )dnl
SRCS=bldFOREACH(`bldADD_SRC(', bldC_PRODUCTS)
define(`bldADD_OBJS', ${$1OBJS} )dnl
OBJS=bldFOREACH(`bldADD_OBJS(', bldC_PRODUCTS)

ifdef(`bldCHECK_PROGRAMS',`dnl
check_PROGRAMS=bldCHECK_PROGRAMS')

ifdef(`bldCHECK_TARGETS',`dnl
TESTS=bldCHECK_TARGETS')

VPATH=${srcdir}:${srcdir}/tests
changequote([[, ]])
check-TESTS: $(TESTS)
	@failed=0; all=0; xfail=0; xpass=0; skip=0; \
	list='$(TESTS)'; \
	srcdir=$(srcdir); export srcdir; \
	if test -n "$$list"; then \
	  for tst in $$list; do \
	    if test -f ./$$tst; then dir=./; \
	    elif test -f "$(srcdir)/tests/$$tst"; then dir="$(srcdir)/tests/"; \
	    elif test -f $$tst; then dir=; \
	    else dir="$(srcdir)/"; fi; \
	    if $(TESTS_ENVIRONMENT) $${dir}$$tst; then \
	      all=`expr $$all + 1`; \
	      case " $(XFAIL_TESTS) " in \
	      *" $$tst "*) \
	        xpass=`expr $$xpass + 1`; \
	        failed=`expr $$failed + 1`; \
	        echo "XPASS: $$tst"; \
	      ;; \
	      *) \
	        echo "PASS: $$tst"; \
	      ;; \
	      esac; \
	    elif test $$? -ne 77; then \
	      all=`expr $$all + 1`; \
	      case " $(XFAIL_TESTS) " in \
	      *" $$tst "*) \
	        xfail=`expr $$xfail + 1`; \
	        echo "XFAIL: $$tst"; \
	      ;; \
	      *) \
	        failed=`expr $$failed + 1`; \
	        echo "FAIL: $$tst"; \
	      ;; \
	      esac; \
	    else \
	      skip=`expr $$skip + 1`; \
	      res=SKIP; \
	    fi; \
	  done; \
	  if test "$$failed" -eq 0; then \
	    if test "$$xfail" -eq 0; then \
	      banner="All $$all tests passed"; \
	    else \
	      banner="All $$all tests behaved as expected ($$xfail expected failures)"; \
	    fi; \
	  else \
	    if test "$$xpass" -eq 0; then \
	      banner="$$failed of $$all tests failed"; \
	    else \
	      banner="$$failed of $$all tests did not behave as expected ($$xpass unexpected passes)"; \
	    fi; \
	  fi; \
	  skipped=""; \
	  dashes="$$banner"; \
	  if test "$$skip" -ne 0; then \
	    if test "$$skip" -eq 1; then \
	      skipped="($$skip test was not run)"; \
	    else \
	      skipped="($$skip tests were not run)"; \
	    fi; \
	    test `echo "$$skipped" | wc -c` -le `echo "$$banner" | wc -c` || \
	      dashes="$$skipped"; \
	  fi; \
	  dashes=`echo "$$dashes" | sed s/./=/g`; \
	  test -z "$$skipped" || echo "$$skipped"; \
	  echo "$$dashes"; \
	  echo "$$banner"; \
	  echo "$$dashes"; \
	  test "$$failed" -eq 0; \
	fi
changequote(`, ')

check-am: make-test all
	$(MAKE) $(check_PROGRAMS)
	$(MAKE) check-TESTS
check: check-am
make-test:
	ifdef(`confTEST_PRGS', `(cd ${SRCDIR}/test && $(MAKE) confTEST_PRGS)')

define(`bldADD_SRC_CHK', ${$1SRCS_CHK} )dnl
SRCS_CHK=bldFOREACH(`bldADD_SRC_CHK(', bldC_CHECKS)
define(`bldADD_OBJS_CHK', ${$1OBJS_CHK} )dnl
OBJS_CHK=bldFOREACH(`bldADD_OBJS(', bldC_CHECKS)

ifdef(`bldNO_INSTALL', `divert(-1)')
install: bldINSTALL_TARGETS

install-strip: bldINSTALL_TARGETS ifdef(`bldSTRIP_TARGETS', `bldSTRIP_TARGETS')
ifdef(`bldNO_INSTALL', `divert(0)')

ifdef(`confREQUIRE_SM_OS_H',`
ifdef(`confSM_OS_HEADER',
`sm_os.h: ${SRCDIR}/inc`'lude/sm/os/confSM_OS_HEADER.h
	${RM} ${RMOPTS} sm_os.h
	${LN} ${LNOPTS} ${SRCDIR}/inc`'lude/sm/os/confSM_OS_HEADER.h sm_os.h',
`sm_os.h:
	${CP} /dev/null sm_os.h')')

divert(bldDEPENDENCY_SECTION)
################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
divert(0)
