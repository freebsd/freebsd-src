# $Id: strip-all-5.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-5 tc/strip-all-5
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} pkill" work true
rundiff true
