# $Id: strip-unneeded-1.sh 2081 2011-10-27 04:28:29Z jkoshy $
inittest strip-unneeded-1 tc/strip-unneeded-1
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${STRIP} --strip-unneeded -o sections.o.1 sections.o" work true
rundiff true
