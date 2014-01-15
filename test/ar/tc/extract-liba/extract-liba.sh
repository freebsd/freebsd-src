# $Id: extract-liba.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest extract-liba tc/extract-liba
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} x liba.a" work true
rundiff true
