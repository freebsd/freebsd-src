# $Id: strip-all-4.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-4 tc/strip-all-4
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} vi" work true
rundiff true
