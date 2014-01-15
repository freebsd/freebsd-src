# $Id: extract-liblong.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest extract-liblong tc/extract-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} x liblong.a" work true
rundiff true
