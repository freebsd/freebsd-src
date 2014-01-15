# $Id: strip-all-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-2 tc/strip-all-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} ps" work true
rundiff true
