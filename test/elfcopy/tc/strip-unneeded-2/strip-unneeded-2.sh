# $Id: strip-unneeded-2.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-unneeded-2 tc/strip-unneeded-2
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} --strip-unneeded -o elfcopy.1 elfcopy" work true
rundiff true
