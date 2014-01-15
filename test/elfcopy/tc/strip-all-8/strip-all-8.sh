# $Id: strip-all-8.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-8 tc/strip-all-8
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -o sections.o.debug.1 sections.o.debug" work true
rundiff true
