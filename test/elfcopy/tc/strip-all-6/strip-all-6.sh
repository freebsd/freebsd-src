# $Id: strip-all-6.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-all-6 tc/strip-all-6
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} -o mcs.o.1 mcs.o" work true
rundiff true
