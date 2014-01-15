# $Id: strip-all-9.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-9 tc/strip-all-9
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -o elfcopy.1 elfcopy" work true
rundiff true
