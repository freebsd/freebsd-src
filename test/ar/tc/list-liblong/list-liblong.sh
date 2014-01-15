# $Id: list-liblong.sh 2079 2011-10-27 04:10:55Z jkoshy $
inittest list-liblong tc/list-liblong
extshar ${TESTDIR}
extshar ${RLTDIR}
runcmd "${AR} t liblong.a" work true
rundiff true
