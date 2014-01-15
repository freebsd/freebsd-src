# $Id: strip-all-7.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-7 tc/strip-all-7
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -o sections.o.1 sections.o" work true
rundiff true
