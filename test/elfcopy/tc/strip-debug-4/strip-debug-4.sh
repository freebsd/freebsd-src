# $Id: strip-debug-4.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-debug-4 tc/strip-debug-4
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -g -o elfcopy.1 elfcopy" work true
rundiff true
