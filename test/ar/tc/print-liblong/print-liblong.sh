# $Id: print-liblong.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest print-liblong tc/print-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} p liblong.a" work true
rundiff true
