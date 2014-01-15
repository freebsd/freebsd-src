# $Id: strip-all-10.sh 2543 2012-08-12 19:09:34Z kaiwang27 $
inittest strip-all-10 tc/strip-all-10
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -o make.1 make" work true
rundiff true
