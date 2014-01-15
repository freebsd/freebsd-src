# $Id: strip-debug-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-debug-2 tc/strip-debug-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -g -o symbols.o.1 symbols.o" work true
rundiff true
