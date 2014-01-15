# $Id: strip-debug-3.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-debug-3 tc/strip-debug-3
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -g -o ls.1 ls" work true
rundiff true
