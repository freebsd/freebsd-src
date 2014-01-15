# $Id: strip-all-3.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-3 tc/strip-all-3
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} tcsh" work true
rundiff true
