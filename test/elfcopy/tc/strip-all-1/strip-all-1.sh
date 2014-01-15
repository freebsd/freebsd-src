# $Id: strip-all-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-1 tc/strip-all-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} ls" work true
rundiff true
